#include <chrono>
#include <iostream>
#include <thread>
#include <span>

#include "atom/connection/tcpclient.hpp"

// Function to handle connection success
void onConnected() {
    std::cout << "Successfully connected to the server." << std::endl;
}

// Function to handle disconnection
void onDisconnected() {
    std::cout << "Disconnected from the server." << std::endl;
}

// Function to handle incoming data
void onDataReceived(std::span<const char> data) {
    std::string received(data.begin(), data.end());
    std::cout << "Received data: " << received << std::endl;
}

// Function to handle errors
void onError(const std::system_error& error) {
    std::cerr << "Error: " << error.what() << std::endl;
}

// Function to run the TCP client
void runTcpClient(const std::string& host, int port) {
    atom::connection::TcpClient::Options options{};
    atom::connection::TcpClient tcpClient(options);

    // Set callbacks for various events
    tcpClient.setOnConnectedCallback(onConnected);
    tcpClient.setOnDisconnectedCallback(onDisconnected);
    tcpClient.setOnDataReceivedCallback(onDataReceived);
    tcpClient.setOnErrorCallback(onError);

    // Try to connect to the server
    auto connectResult = tcpClient.connect(host, static_cast<uint16_t>(port), std::chrono::milliseconds(5000));
    if (!connectResult.has_value()) {
        std::cerr << "Failed to connect to the server." << std::endl;
        return;
    }

    // Sending a message to the server
    std::string message = "Hello, Server!";
    std::span<const char> data_span(message.data(), message.size());
    auto sendResult = tcpClient.send(data_span);
    if (sendResult.has_value()) {
        std::cout << "Sent message: " << message << " (" << sendResult.value() << " bytes)" << std::endl;
    } else {
        std::cerr << "Failed to send message." << std::endl;
    }

    // Start receiving data in a separate thread
    tcpClient.startReceiving(
        1024);  // Start receiving with buffer size of 1024 bytes

    // Wait for some time to receive data from server
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Stop receiving before disconnecting
    tcpClient.stopReceiving();

    // Disconnect from the server
    tcpClient.disconnect();
}

int main() {
    const std::string host =
        "127.0.0.1";        // Replace with the server's IP address or hostname
    const int port = 8080;  // Replace with the server's port

    // Run the TCP client
    runTcpClient(host, port);

    return 0;
}
