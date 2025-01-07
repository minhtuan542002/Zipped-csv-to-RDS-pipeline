#include <aws/core/Aws.h>
#include <aws/lambda-runtime/runtime.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/sqs/SQSClient.h> 
#include "mysql_connection.h"
#include "mysql_driver.h"
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include "aspell.h"

#include "serviceSqs.h"
#include "ProcessCsvToDatabase.h"
#include <iostream>
#include <regex>
#include <vector>
#include <cmath>
#include <sys/stat.h>

using namespace aws::lambda_runtime;

invocation_response my_handler(
    invocation_request const& req, 
    ServiceSqs& serviceSqs,
    AspellSpeller* spellChecker) 
{
    // RDS settings
    const std::string rds_host = getEnvironmentValue("RDS_PROXY_HOST");
    const std::string db_username = getEnvironmentValue("USERNAME");
    const std::string db_password = getEnvironmentValue("PASSWORD");
    const std::string db_name = getEnvironmentValue("DB_NAME");
    const std::string sqs_url = getEnvironmentValue("SQS_URL");
    const Aws::String sqsUrl(sqs_url.c_str(), sqs_url.size());

    auto payload = Aws::Utils::Json::JsonValue(req.payload);
    auto records = payload.View().GetArray("Records");

    sql::Driver* driver = sql::mysql::get_driver_instance();
    
    std::unique_ptr<sql::Connection> con(driver->connect(
        rds_host, db_username, db_password));
    con->setSchema(db_name);

    std::unique_ptr<sql::Statement> stmt(con->createStatement());
    stmt->execute(
        "CREATE TABLE IF NOT EXISTS book ("
        "book_id INT AUTO_INCREMENT PRIMARY KEY,"
        "title VARCHAR(128) NOT NULL,"
        "author VARCHAR(128),"
        "genre VARCHAR(128),"
        "publisher VARCHAR(128),"
        "isbn VARCHAR(128),"
        "language VARCHAR(32),"
        "rating DECIMAL(2, 1) CHECK(rating BETWEEN 0 AND 5));"
    );
    std::cout << "Connected to the database and added book table" << std::endl;

    std::vector<std::string> fields = {
            "author", "genre", "isbn", "language", "publisher", "title", "rating" };
    std::vector<std::string> values;
    const std::vector<size_t> fieldSizes = {
            128,      128,     128,    32,         128,         128,     0};

    for (size_t record = 0; record < records.GetLength(); ++record) {
        std::cout << "Starting a new record..." << std::endl;
        auto books = records[record].GetArray("books");
        std::string errorName, errorDescription, suggestion;
        std::string message = "{\"errors\":[";
        for (size_t x = 0; x < books.GetLength(); ++x) {
            bool errorLine = false;
            //Check for fatal errors
            std::cout << "Checking errors..." << std::endl;
            if (!books[x].ValueExists("title") || 
                !books[x].GetObject("title").IsString())
            {
                errorLine = true;
                errorName = "TitleFormatError";
                errorDescription = 
                    "This book entry is missing a title or has invalid title format "
                    "(should be string).";
            }
            else if (books[x].GetDouble("rating") < 0.0 || 
                books[x].GetDouble("rating") > 5.0) 
            {
                errorLine = true;
                errorName = "RatingValueError";
                errorDescription =
                    "This book entry rating has invalid value range "
                    "(should be between 0 and 5).";
            }
            //Spell check on title if provided language is English
            else if (!books[x].ValueExists("noSpellCheck") && spellChecker != 0) {
                if (books[x].ValueExists("language") &&
                    books[x].GetObject("language").IsString()) {
                    if (isEnglish(books[x].GetString("language"))) {
                        auto bookTitle = books[x].GetString("title");
                        std::string titleString(bookTitle.c_str(), bookTitle.size());
                        suggestion = "{";
                        std::regex r(R"([^\W_]+(?:['_][^\W_]+)*)");

                        for (std::sregex_iterator i = std::sregex_iterator(
                                titleString.begin(), titleString.end(), r);
                            i != std::sregex_iterator();
                            ++i)
                        {
                            std::smatch match = *i;
                            std::string word = match.str();
                            std::string suggestWord = checkSpelling(word, spellChecker);
                            if (suggestWord != "") {
                                errorLine = true;
                                suggestion.append(
                                    "\"" + word + "\":\"" + suggestWord + "\",");
                            }
                        }
                        suggestion.pop_back();
                        if (errorLine) {
                            suggestion.append("}");
                        }
                        else {
                            suggestion = "";
                        }
                    }
                }
            }
            //Check if the size of each field fit allowable size
            else {                 
                for (size_t i = 0; i < fields.size(); ++i) { 
                    if (fieldSizes[i] > 0 && 
                        (books[x].ValueExists(fields[i]) &&
                        books[x].GetString(fields[i]).size() > fieldSizes[i])) 
                    { 
                        errorLine = true; 
                        errorName = "SizeError";
                        errorDescription = 
                            "The size of " + fields[i] + 
                            " string of this book entry exceeds allowed size " +
                            "(should be less than " + 
                            std::to_string(fieldSizes[i]) + " characters)."; 
                        break; 
                    } 
                }
                //Round up rating to one decimal digit
                double rating = books[x].GetDouble("rating");
                double roundedRating = std::ceil(rating * 10.0) / 10.0;
                for (size_t i = 0; i < fields.size(); ++i) {
                    Aws::String value = books[x].GetObject(fields[i]).AsString();
                    std::string valueStr(value.c_str(), value.size());
                    if (valueStr == "") {
                        valueStr = "NULL";
                    }
                    else {
                        valueStr = "\"" + valueStr + "\"";
                    }
                    if (fields[i] == "rating") {
                        valueStr = std::to_string(roundedRating);
                    }
                    values.push_back(valueStr);
                }
            }
            //Check for duplicates
            if (!errorLine) {
                std::string query = "SELECT * FROM book WHERE isbn = '" +
                    values[2] + "' OR (title = '" + values[5] +
                    "' AND author = '" + values[0] + "');";
                    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
                        query.c_str()));
                if (res->next()) {
                    errorLine = true;
                    errorName = "DuplicateError";
                    errorDescription =
                        "This book entry has already existed in the database.";
                }
            }

            //Insert value to table
            if (!errorLine) {
                std::cout << "Inserting new row" << std::endl;
                try {
                    std::string preparedStatement = "INSERT INTO book (";
                    for (size_t i = 0; i < fields.size(); ++i) {
                        preparedStatement.append(fields[i] + ",");
                    }
                    preparedStatement.pop_back();
                    preparedStatement.append(") VALUES (");
                    for (size_t i = 0; i < fields.size(); ++i) {
                        preparedStatement.append("?,");
                    }
                    preparedStatement.pop_back();
                    preparedStatement.append(")");
                    std::unique_ptr<sql::PreparedStatement> pstmt(
                        con->prepareStatement(preparedStatement.c_str())); 
                    for (size_t i = 0; i < fields.size(); ++i) {
                        pstmt->setString(i + 1, values[i].c_str());
                    }
                    pstmt->executeUpdate();
                } catch (sql::SQLException& e) { 
                    errorLine = true;
                    errorName = "SQLException";
                    errorDescription = e.what();
                    errorDescription = "SQLException: " + errorDescription;
                }                
            }

            if (errorLine) {
                message.append("{\"errorName\":\"" + errorName + "\"");
                message.append(",\"errorDescription\":\"" + errorDescription + "\"");
                message.append(",\"book\":" + books[x].WriteCompact());
                if (suggestion != "") {
                    message.append(",\"suggestion\":" + suggestion);
                }
                message.append("},");
                std::cout << "Error:" << message << std::endl;
            }            
        }
        std::cout << "Sending errors..." << std::endl;
        if (message.back() == ',') {
            message.pop_back();
            message.append("]}");
            Aws::String sqsMessage(message.c_str(), message.size());
            std::cout << "Error:" << message << std::endl;
            if (!serviceSqs.sendMessage(sqsUrl, sqsMessage)) {
                return invocation_response::failure(
                    "Cannot send error message to error queue.",
                    "SQSMessageFail");
            }
        }
    }
    return invocation_response::success(
        "Added to database successfully", "application/json");
}

int main() {
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    int result = 0;
    {
        AspellConfig* spellConfig = new_aspell_config();
        aspell_config_replace(spellConfig, "lang", "en");
        AspellCanHaveError* possibleErr = new_aspell_speller(spellConfig);
        AspellSpeller* spellChecker = 0;

        if (aspell_error_number(possibleErr) != 0)
            puts(aspell_error_message(possibleErr));
        else spellChecker = to_aspell_speller(possibleErr);

        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = Aws::Region::AP_SOUTHEAST_1;
        clientConfig.scheme = Aws::Http::Scheme::HTTPS;
        clientConfig.caFile = "/etc/pki/tls/certs/ca-bundle.crt";

        ServiceSqs serviceSqs(clientConfig);
        auto handler_fn = [&serviceSqs, spellChecker](
            aws::lambda_runtime::invocation_request const& req) {
                return my_handler(req, serviceSqs, spellChecker);
            };
        run_handler(handler_fn);

        serviceSqs.free();
        delete_aspell_speller(spellChecker); 
        delete_aspell_config(spellConfig);
    }

    Aws::ShutdownAPI(options);
    return result;
}
