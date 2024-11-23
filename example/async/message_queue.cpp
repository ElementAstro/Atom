#include "atom/async/message_queue.hpp"

#include <iostream>
#include <thread>

#include <asio/io_context.hpp>

using namespace atom::async;

// Example message type
struct ExampleMessage {
    std::string content;
};

// Example callback function
void exampleCallback(const ExampleMessage& message) {
    std::cout << "Received message: " << message.content << std::endl;
}

// Example filter function
bool exampleFilter(const ExampleMessage& message) {
    return message.content.find("filter") != std::string::npos;
}

int main() {
    // Create an Asio io_context
    asio::io_context io_context;

    // Create a MessageQueue instance
    MessageQueue<ExampleMessage> messageQueue(io_context);

    // Subscribe to messages with a callback, filter, and timeout
    messageQueue.subscribe(exampleCallback, "exampleSubscriber", 1,
                           exampleFilter, std::chrono::milliseconds(1000));

    // Publish a message
    ExampleMessage message{"Hello, World!"};
    messageQueue.publish(message);

    // Publish a message that passes the filter
    ExampleMessage filteredMessage{"This message contains filter keyword"};
    messageQueue.publish(filteredMessage);

    // Start processing messages in a separate thread
    std::thread processingThread([&io_context]() { io_context.run(); });

    // Wait for a short duration to ensure the message is processed
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Get the number of messages in the queue
    size_t messageCount = messageQueue.getMessageCount();
    std::cout << "Number of messages in the queue: " << messageCount
              << std::endl;

    // Get the number of subscribers
    size_t subscriberCount = messageQueue.getSubscriberCount();
    std::cout << "Number of subscribers: " << subscriberCount << std::endl;

    // Cancel specific messages that meet a given condition
    messageQueue.cancelMessages([](const ExampleMessage& msg) {
        return msg.content == "Hello, World!";
    });

    // Unsubscribe from messages
    messageQueue.unsubscribe(exampleCallback);

    // Stop processing messages
    messageQueue.stopProcessing();

    // Join the processing thread
    processingThread.join();

    return 0;
}