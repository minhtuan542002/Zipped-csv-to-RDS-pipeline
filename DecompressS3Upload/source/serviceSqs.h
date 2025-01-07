#pragma once

#include <string>
#include <aws/sqs/SQSClient.h> 
#include <aws/sqs/model/SendMessageRequest.h>

class ServiceSqs {
private:
    /**
    *  SQS Client instance
    */
    Aws::SQS::SQSClient* sqsClient;

public:
    /**
    * Constructor method
    */
    ServiceSqs(const Aws::Client::ClientConfiguration& clientConfig)
    {
        sqsClient = new Aws::SQS::SQSClient(clientConfig);
    }

    /**
    * Send message to SQS queue
    */
    bool sendMessage(const Aws::String& queueUrl,
        const Aws::String& messageBody) {

        Aws::SQS::Model::SendMessageRequest request;
        request.SetQueueUrl(queueUrl);
        request.SetMessageBody(messageBody);

        const Aws::SQS::Model::SendMessageOutcome outcome = sqsClient->SendMessage(request);
        if (outcome.IsSuccess()) {
            std::cout << "Successfully sent message to " << queueUrl <<
                std::endl;
        }
        else {
            std::cerr << "Error sending message to " << queueUrl << ": " <<
                outcome.GetError().GetMessage() << std::endl;
        }

        return outcome.IsSuccess();
    }

    void free()
    {
        delete sqsClient;
    }
};