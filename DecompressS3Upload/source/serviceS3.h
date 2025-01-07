
#pragma once

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/Object.h>
//#include "json.hpp"

using namespace Aws;
using namespace Aws::Auth;
using namespace Aws::Utils::Json;

/*
 *  A "Hello S3" starter application which initializes an Amazon Simple Storage Service (Amazon S3) client
 *  and lists the Amazon S3 buckets in the selected region.
 */

 /**
 *  An AWS service class to encapsulate simple operations with S3 buckets and objects
 */
class ServiceS3 {
private:
    /**
    *  S3 Client instance
    */
    Aws::S3::S3Client* s3_client;

public:

    /**
    * Constructor method
    */
    ServiceS3(const Aws::Client::ClientConfiguration& clientConfig)
    {
        s3_client = new Aws::S3::S3Client(clientConfig);

    }
    /**
    * Put an object into an Amazon S3 bucket
    */
    bool putStringToObject(const Aws::String& bucketName,
        const Aws::String& objectName,
        const std::shared_ptr<Aws::StringStream> inputData)
    {
        Aws::S3::Model::PutObjectRequest objectRequest;

        objectRequest.SetBucket(bucketName);
        objectRequest.SetKey(objectName);
        /*
        const std::shared_ptr<Aws::IOStream> inputData =
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag",
                fileName.c_str(),
                std::ios_base::in | std::ios_base::binary);
        */
        objectRequest.SetBody(inputData);

        // Put the object
        auto put_object_outcome = s3_client->PutObject(objectRequest);
        if (!put_object_outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = put_object_outcome.GetError();
            throw std::runtime_error(err.GetExceptionName() + ": " + err.GetMessage());
            return false;
        }
        return true;
        // snippet-end:[s3.cpp.put_object.code]
    }

    bool putObject(const Aws::String& bucketName,
        const Aws::String& objectName,
        const std::string& fileName)
    {
        // Verify fileName exists
        if (!std::filesystem::exists(fileName)) {
            std::cout << "ERROR: NoSuchFile: The specified file does not exist: "<< fileName
                << std::endl;
            //return false;
        }

        Aws::S3::Model::PutObjectRequest objectRequest;

        objectRequest.SetBucket(bucketName);
        objectRequest.SetKey(objectName);
        const std::shared_ptr<Aws::IOStream> inputData =
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag",
                fileName.c_str(),
                std::ios_base::in | std::ios_base::binary);
        objectRequest.SetBody(inputData);

        // Put the object
        auto put_object_outcome = s3_client->PutObject(objectRequest);
        if (!put_object_outcome.IsSuccess()) {
            auto error = put_object_outcome.GetError();
            std::cout << "ERROR: " << error.GetExceptionName() << ": "
                << error.GetMessage() << std::endl;
            return false;
        }
        return true;
    }

    std::vector<Aws::String> listObjects(const Aws::String& bucketName)
    {
        Aws::S3::Model::ListObjectsRequest request;
        request.WithBucket(bucketName);

        auto outcome = s3_client->ListObjects(request);

        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            throw std::runtime_error(err.GetExceptionName() + ": " + err.GetMessage());
        }
        else {
            Aws::Vector<Aws::S3::Model::Object> objects =
                outcome.GetResult().GetContents();

            std::vector<Aws::String> objectList;
            for (Aws::S3::Model::Object& object : objects) {
                objectList.push_back(object.GetKey());
            }
            return objectList;
        }

        //return NULL;
    }

    auto listBuckets()
    {
        auto outcome = s3_client->ListBuckets();

        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            throw std::runtime_error(err.GetExceptionName() + ": " + err.GetMessage());
        }
        else {
            std::cout << "Found " << outcome.GetResult().GetBuckets().size()
                << " buckets\n";
            Aws::String bucketList[outcome.GetResult().GetBuckets().size()];
            int count = 0;
            for (auto& bucket : outcome.GetResult().GetBuckets()) {
                std::cout << bucket.GetName() << std::endl;
                bucketList[count] = bucket.GetName();
                ++count;
            }
            return bucketList;
        }
        //return NULL;
    }

    bool getObject(const Aws::String& objectKey,
        const Aws::String& fromBucket, std::string downloadPath)
    {
        Aws::S3::Model::GetObjectRequest request;
        request.SetBucket(fromBucket);
        request.SetKey(objectKey);

        Aws::S3::Model::GetObjectOutcome outcome =
            s3_client->GetObject(request);

        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            std::cerr << "Error: GetObject: " <<
                err.GetExceptionName() << ": " << err.GetMessage() << std::endl;
        }
        else {
            std::cout << "Successfully retrieved '" << objectKey << "' from '"
                << fromBucket << "'." << std::endl;

            std::string local_fileName = downloadPath + objectKey;
            std::ofstream local_file(local_fileName, std::ios::binary);
            auto& retrieved = outcome.GetResult().GetBody();
            local_file << retrieved.rdbuf();
            std::cout << "Done!";
        }

        return outcome.IsSuccess();
    }

    std::ostringstream getStringFromJsonObject(const Aws::String& objectKey,
        const Aws::String& fromBucket)
    {
        Aws::S3::Model::GetObjectRequest request;
        request.SetBucket(fromBucket);
        request.SetKey(objectKey);

        Aws::S3::Model::GetObjectOutcome outcome =
            s3_client->GetObject(request);

        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            throw std::runtime_error(err.GetExceptionName() + ": " + err.GetMessage());
        }
        else {
            std::cout << "Successfully retrieved '" << objectKey << "' from '"
                << fromBucket << "'." << std::endl;
            auto& retrieved = outcome.GetResult().GetBody();
            std::ostringstream ss;
            ss << retrieved.rdbuf();
            return ss;
        }
        //return NULL;
    }

    /**
    * Free up resources in class
    */
    void free()
    {
        delete s3_client;
    }
};

