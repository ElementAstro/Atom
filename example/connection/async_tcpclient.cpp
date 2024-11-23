#include "atom/connection/async_tcpclient.hpp"

#include <iostream>
#include <thread>

using namespace atom::async::connection;

void onConnected() {
    std::cout << "Connected to the server" << std::endl;
}

void onDisconnected() {
    std::cout << "Disconnected from the server" << std::endl;
}

void onDataReceived(const std::vector<char>& data) {
    std::cout << "Data received: " << std::string(data.begin(), data.end()) << std::endl;
}

void onError(const std::string& errorMessage) {
    std::cerr << "Error: " << errorMessage << std::endl;
}

int main() {
    // Create a TcpClient object
    TcpClient tcpClient;

    // Set callback functions
    tcpClient.setOnConnectedCallback(onConnected);
    tcpClient.setOnDisconnectedCallback(onDisconnected);
    tcpClient.setOnDataReceivedCallback(onDataReceived);
    tcpClient.setOnErrorCallback(onError);

    // Connect to the server
    if (tcpClient.connect("127.0.0.1", 12345, std::chrono::milliseconds(5000))) {
        std::cout << "Connection attempt successful" << std::endl;
    } else {
        std::cerr << "Connection attempt failed: " << tcpClient.getErrorMessage() << std::endl;
        return 1;
    }

    // Enable reconnection attempts
    tcpClient.enableReconnection(3);

    // Set heartbeat interval
    tcpClient.setHeartbeatInterval(std::chrono::milliseconds(10000));

    // Send data to the server
    std::vector<char> dataToSend = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'T', 'C', 'P', '!'};
    if (tcpClient.send(dataToSend)) {
        std::cout << "Data sent successfully" << std::endl;
    } else {
        std::cerr << "Failed to send data" << std::endl;
    }

    // Receive data from the server
    auto future = tcpClient.receive(11, std::chrono::milliseconds(5000));
    try {
        auto receivedData = future.get();
        std::cout << "Data received: " << std::string(receivedData.begin(), receivedData.end()) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to receive data: " << e.what() << std::endl;
    }

    // Disconnect from the server
    tcpClient.disconnect();

    // Check if the client is connected
    if (tcpClient.isConnected()) {
        std::cout << "Client is still connected" << std::endl;
    } else {
        std::cout << "Client is disconnected" << std::endl;
    }

    return 0;
}