// ReadBookCsv.cpp : Defines the entry point for the application.
//AWS libs
#include <aws/core/Aws.h>
#include <aws/lambda-runtime/runtime.h>
#include <aws/s3/S3Client.h>
#include <aws/sqs/SQSClient.h> 
#include <aws/sqs/model/SendMessageRequest.h>
//Local headers
#include "serviceS3.h"
#include "serviceSqs.h"
#include "ReadBookCsv.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <filesystem>

using namespace aws::lambda_runtime;

static invocation_response my_handler(
    invocation_request const& req,
    ServiceS3& serviceS3,
    ServiceSqs& serviceSqs) 
{
    // Parse input event JSON to get bucket and key
    auto payload = Aws::Utils::Json::JsonValue(req.payload);
    auto records = payload.View().GetArray("Records");

    std::string path = "/tmp/";
    const std::string sqsUrlStr = getEnvironmentValue("SQS_URL");
    const Aws::String sqsUrl(sqsUrlStr.c_str(), sqsUrlStr.size());
    
    const int BATCH_SIZE = 50;
    int batchCount = 0;
    std::string message = "{\"books\":[";

    for (size_t record = 0; record < records.GetLength(); ++record) {
        auto bucket = records[record].GetString("bucket");
        auto key = records[record].GetString("file");

        serviceS3.getObject(key, bucket, path);
        std::cout << "Successfully retrieved S3 service" << std::endl;
        std::string filePath = path + key;

        std::ifstream csvFile(filePath);
        if (!csvFile.is_open()) { 
            std::cerr << "Error: Could not open file " << filePath << std::endl; 
            return invocation_response::failure(
                "Cannot open file " + filePath + " to read", "CannotOpenFileError");
        }
        std::cout << "Starting to read "<< filePath << "..." << std::endl;

        std::string line; 
        bool isFirstRow = true;
        std::vector<std::string> headers;
        std::vector<std::string> potentialHeaders = {
            "author", "genre", "isbn", "language", "publisher", "rating", "title"};
        while (std::getline(csvFile, line)) {
            //std::cout << "Staring to read a new row..." << std::endl;
            std::stringstream ss(line); 
            std::string item; 
            std::vector<std::string> row;
            while (std::getline(ss, item, ',')) { 
                row.push_back(item); 
            }

            //Process the header line
            if (isFirstRow) {
                for (auto& col : row) {

                    //Find keywords in table headers
                    std::transform(col.begin(), col.end(), col.begin(), [](unsigned char c) { 
                            return std::tolower(c); 
                        });
                    std::string header = "";
                    for (std::string keyword : potentialHeaders) {
                        if (col.find(keyword) != std::string::npos) {
                            header = keyword;
                            break;
                        }
                    }
                    headers.push_back(header);
                }
                isFirstRow = false;
                std::cout << "Finished header row of " << key << std::endl;
                continue;
            }

            //Process inputs
            message.append("{");

            for (long unsigned int i = 0; i < potentialHeaders.size(); ++i) {
                std::string value = "";
                for (long unsigned int j = 0; j <= row.size(); ++j) {
                    if (potentialHeaders[i] == headers[j]) {
                        value.append(row[j] + "/");
                    }
                }
                if (value != "") { value.pop_back(); }
                message.append("\"" + potentialHeaders[i] + "\": \"" + value + "\",");
            } 
            message.pop_back();
            message.append("},");
            //std::cout << "Added a line."<< message << std::endl;
            ++batchCount;
            if (batchCount == BATCH_SIZE) {
                batchCount = 0;
                message.pop_back();
                message.append("]}");
                Aws::String sqsMessage(message.c_str(), message.size());

                if (!serviceSqs.sendMessage(sqsUrl, sqsMessage)) {
                    return invocation_response::failure(
                        "Cannot send message to save to database queue.",
                        "SQSMessageFail");
                    std::cout << "Sent a line to sqs." << std::endl;
                }
                message = "{\"books\":[";
            }         

        }
        csvFile.close();
        if (!std::filesystem::remove(filePath.c_str())) {
            std::cout << "Cannot delete file: " << filePath << " (file not found)" << std::endl;
        }
        std::cout << "Finished processing file " << key << std::endl;
    }
    if (batchCount > 0) {
        message.pop_back();
        message.append("]}");
        Aws::String sqsMessage(message.c_str(), message.size());

        if (!serviceSqs.sendMessage(sqsUrl, sqsMessage)) {
            return invocation_response::failure(
                "Cannot send message to save to database queue.",
                "SQSMessageFail");
            std::cout << "Sent a line to sqs." << std::endl;
        }
    }

    return invocation_response::success("File processed successfully", "application/json");
}

int main() {
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    int result = 0;
    {
        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = Region::AP_SOUTHEAST_1;
        clientConfig.scheme = Http::Scheme::HTTPS;
        clientConfig.caFile = "/etc/pki/tls/certs/ca-bundle.crt";

        ServiceS3 serviceS3(clientConfig);
        ServiceSqs serviceSqs(clientConfig);
        auto handler_fn = [&serviceS3, &serviceSqs](aws::lambda_runtime::invocation_request const& req) {
            return my_handler(req, serviceS3, serviceSqs);
            };
        run_handler(handler_fn);
        serviceS3.free();
        serviceSqs.free();
    }

    Aws::ShutdownAPI(options);
    return result;
}

