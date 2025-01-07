#include <aws/core/Aws.h>
#include <aws/lambda-runtime/runtime.h>
#include <aws/s3/S3Client.h>
#include <aws/sqs/SQSClient.h> 
#include <aws/sqs/model/SendMessageRequest.h>
#include <zip.h>
#include "serviceS3.h"
#include "serviceSqs.h"
#include "DecompressS3Upload.h"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

using namespace Aws;
using namespace Aws::Auth;
using namespace aws::lambda_runtime;

static invocation_response my_handler(
    invocation_request const& req, 
    ServiceS3& serviceS3, 
    ServiceSqs& serviceSqs) {

    // Parse input event JSON to get bucket and key
    auto payload = Aws::Utils::Json::JsonValue(req.payload);
    //auto reqMessages = Aws::Utils::Json::JsonValue(payload.View().GetArray("Records").GetItem(0).GetString("body"));
    auto records = payload.View().GetArray("Records");
    auto bucket = records[0].GetObject("s3").GetObject("bucket").GetString("name");
    auto key = records[0].GetObject("s3").GetObject("object").GetString("key");

    if (!ends_with(key, ".zip")) {
        return invocation_response::failure(
            "The file uploaded is not a .zip file.", "InvalidFileUploaded");
    }

    std::string path = "/tmp/";
    serviceS3.getObject(key, bucket, path);
    std::cout << "Successfully retrieved S3 service" << std::endl;
    std::string zipPath = path + key;
    const char* zipSource = zipPath.c_str();
    std::cout << "Successfully copied zipSource" << std::endl;
    zip_t* zipFile;
    int err;

    if ((zipFile = zip_open(zipSource, 0, &err)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        return invocation_response::failure(
            "Cannot open zip archive, error from consistancy check.", "FileConsistencyFail");
    }
    std::cout << "Successfully opened zipfile" << std::endl;

    Aws::String sqsUrl = "https://sqs.ap-southeast-1.amazonaws.com/046259933227/csvProcessQueue";

    // Check each file in the zip archive and send note to csv processing queue if matched
    zip_int64_t numFiles = zip_get_num_entries(zipFile, 0);
    auto fileData = Aws::MakeShared<Aws::StringStream>("PutObjectInputStream",
        std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    for (zip_int32_t i = 0; i < numFiles; ++i) {
        const char* fileName = zip_get_name(zipFile, i, 0);
        if (!fileName) {
            std::cerr << "Error: Could not get file name in zip archive." << std::endl;
            continue;
        }
        std::cout << "Successfully retrieved name of file index "<< i << std::endl;

        std::string fileNameStr(fileName);
        if (ends_with(fileNameStr, ".csv")) {
            std::string outputPath = "/tmp/csv/" + fileNameStr;
            if (!extractFile(zipFile, i, fileData)) {
                return invocation_response::failure(
                    "Cannot open extract " + fileNameStr + " from zip file.",
                    "FileExtractionFail");
            }
            std::cout << "Successfully downloaded csv file to " << outputPath << std::endl;

            if (!serviceS3.putStringToObject(bucket, fileNameStr, fileData)) {
                return invocation_response::failure(
                    "Cannot upload file " + fileNameStr + " from zip file to S3 bucket.",
                    "S3UploadFail");
            }

            Aws::String message = "{\"bucket\": \""+ 
                bucket + "\",\"file\": \""+ 
                fileNameStr + "\"}";
            if (!serviceSqs.sendMessage(sqsUrl, message)) {
                return invocation_response::failure(
                    "Cannot send message to csv process queue about " + fileNameStr + " from zip file.",
                    "SQSMessageFail");
            }
        }
    }
    zip_close(zipFile);
    std::filesystem::remove_all("/tmp/csv");
    std::filesystem::remove(zipSource);


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
        auto handler_fn = [&serviceS3,&serviceSqs](aws::lambda_runtime::invocation_request const& req) {
            return my_handler(req, serviceS3, serviceSqs);
        };
        run_handler(handler_fn);
        serviceS3.free();
        serviceSqs.free();
    }

    Aws::ShutdownAPI(options);
    return result;
}
