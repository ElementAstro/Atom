#include "atom/async/message_bus.hpp"

#include <iostream>
#include <thread>
#include <utility>

#include <asio/io_context.hpp>

using namespace atom::async;

// Example message type
struct ExampleMessage {
    std::string content;
};

// Example handler function
void exampleHandler(const ExampleMessage& message) {
    std::cout << "Received message: " << message.content << std::endl;
}

int main() {
    // Create an Asio io_context
    asio::io_context io_context;

    // Create a MessageBus instance
    auto messageBus = MessageBus::createShared(io_context);

    // Subscribe to a message
    auto token = messageBus->subscribe<ExampleMessage>("example.message", exampleHandler);

    // Publish a message
    ExampleMessage message{"Hello, World!"};
    messageBus->publish("example.message", message);

    // Run the io_context to process asynchronous operations
    std::thread ioThread([&io_context]() { io_context.run(); });

    // Wait for a short duration to ensure the message is processed
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Unsubscribe from the message
    messageBus->unsubscribe<ExampleMessage>(token);

    // Publish another message to demonstrate unsubscription
    messageBus->publish("example.message", ExampleMessage{"This should not be received"});

    // Wait for a short duration to ensure the message is processed
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Clear all subscribers
    messageBus->clearAllSubscribers();

    // Publish a global message
    messageBus->publishGlobal(ExampleMessage{"Global message"});

    // Wait for a short duration to ensure the message is processed
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Stop the io_context and join the thread
    io_context.stop();
    ioThread.join();

    return 0;
}