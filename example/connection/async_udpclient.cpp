#include "atom/connection/async_udpclient.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::async::connection;

int main() {
    // Create an instance of UdpClient
    UdpClient client;

    // Bind the client to a specific port
    int port = 12345;
    if (!client.bind(port)) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        return 1;
    }
    std::cout << "Successfully bound to port " << port << std::endl;

    // Set the callback for data received
    client.setOnDataReceivedCallback([](const std::vector<char>& data,
                                        const std::string& remoteHost,
                                        int remotePort) {
        std::cout << "Received data from " << remoteHost << ":" << remotePort
                  << " - ";
        std::cout.write(data.data(), data.size());
        std::cout << std::endl;
    });

    // Set the callback for errors
    client.setOnErrorCallback([](const std::string& error) {
        std::cerr << "Error: " << error << std::endl;
    });

    // Start receiving data with a buffer size of 1024 bytes
    client.startReceiving(1024);

    // Send data to a remote host
    std::string remoteHost = "127.0.0.1";
    int remotePort = 54321;
    std::vector<char> dataToSend = {'H', 'e', 'l', 'l', 'o', ' ',
                                    'W', 'o', 'r', 'l', 'd'};
    if (!client.send(remoteHost, remotePort, dataToSend)) {
        std::cerr << "Failed to send data to " << remoteHost << ":"
                  << remotePort << std::endl;
        return 1;
    }
    std::cout << "Data sent to " << remoteHost << ":" << remotePort
              << std::endl;

    // Receive data with a timeout of 5 seconds
    std::string receivedHost;
    int receivedPort;
    std::vector<char> receivedData = client.receive(
        1024, receivedHost, receivedPort, std::chrono::milliseconds(5000));
    if (!receivedData.empty()) {
        std::cout << "Synchronously received data from " << receivedHost << ":"
                  << receivedPort << " - ";
        std::cout.write(receivedData.data(), receivedData.size());
        std::cout << std::endl;
    } else {
        std::cout << "No data received within the timeout period" << std::endl;
    }

    // Stop receiving data
    client.stopReceiving();
    std::cout << "Stopped receiving data" << std::endl;

    return 0;
}
