#include <utility>

#include "atom/connection/async_sockethub.hpp"

#include <iostream>
#include <thread>

using namespace atom::async::connection;

void messageHandler(const std::string& message, size_t client_id) {
    std::cout << "Received message from client " << client_id << ": " << message
              << std::endl;
}

void connectHandler(size_t client_id) {
    std::cout << "Client " << client_id << " connected" << std::endl;
}

void disconnectHandler(size_t client_id) {
    std::cout << "Client " << client_id << " disconnected" << std::endl;
}

int main() {
    // Create a SocketHub object
    SocketHub socketHub;

    // Add handlers for messages, connections, and disconnections
    socketHub.addHandler(messageHandler);
    socketHub.addConnectHandler(connectHandler);
    socketHub.addDisconnectHandler(disconnectHandler);

    // Start the SocketHub on port 12345
    socketHub.start(12345);
    std::cout << "SocketHub started on port 12345" << std::endl;

    // Check if the SocketHub is running
    if (socketHub.isRunning()) {
        std::cout << "SocketHub is running" << std::endl;
    } else {
        std::cout << "SocketHub is not running" << std::endl;
    }

    // Simulate some work by sleeping for a few seconds
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Broadcast a message to all connected clients
    socketHub.broadcastMessage("Hello, clients!");

    // Simulate some work by sleeping for a few seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop the SocketHub
    socketHub.stop();
    std::cout << "SocketHub stopped" << std::endl;

    // Check if the SocketHub is running after stopping
    if (socketHub.isRunning()) {
        std::cout << "SocketHub is still running" << std::endl;
    } else {
        std::cout << "SocketHub is not running" << std::endl;
    }

    return 0;
}