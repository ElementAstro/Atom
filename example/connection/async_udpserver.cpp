#include "atom/connection/async_udpserver.hpp"

#include <iostream>
#include <string>
#include <thread>

using namespace atom::async::connection;

int main() {
    // Create an instance of UdpSocketHub
    UdpSocketHub server;

    // Define a message handler
    UdpSocketHub::MessageHandler handler = [](const std::string& message,
                                              const std::string& remoteIp,
                                              unsigned short remotePort) {
        std::cout << "Received message from " << remoteIp << ":" << remotePort
                  << " - " << message << std::endl;
    };

    // Add the message handler to the server
    server.addMessageHandler(handler);

    // Start the server on a specific port
    unsigned short port = 12345;
    server.start(port);
    std::cout << "Server started on port " << port << std::endl;

    // Check if the server is running
    if (server.isRunning()) {
        std::cout << "Server is running" << std::endl;
    } else {
        std::cerr << "Server failed to start" << std::endl;
        return 1;
    }

    // Send a message to a remote client
    std::string remoteIp = "127.0.0.1";
    unsigned short remotePort = 54321;
    std::string message = "Hello, UDP client!";
    server.sendTo(message, remoteIp, remotePort);
    std::cout << "Sent message to " << remoteIp << ":" << remotePort << " - "
              << message << std::endl;

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Stop the server
    server.stop();
    std::cout << "Server stopped" << std::endl;

    return 0;
}