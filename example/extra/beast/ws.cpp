#include "atom/extra/beast/ws.hpp"

#include <boost/asio/io_context.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

int main() {
    // Create an I/O context
    boost::asio::io_context ioc;

    // Create an instance of WSClient
    WSClient client(ioc);

    // Set the timeout duration
    client.setTimeout(std::chrono::seconds(30));

    // Set the reconnection options
    client.setReconnectOptions(3, std::chrono::seconds(5));

    // Set the ping interval
    client.setPingInterval(std::chrono::seconds(10));

    // Connect to the WebSocket server
    try {
        client.connect("example.com", "80");
        std::cout << "Connected to WebSocket server" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
    }

    // Send a message to the WebSocket server
    client.send("Hello, WebSocket server!");

    // Receive a message from the WebSocket server
    try {
        std::string message = client.receive();
        std::cout << "Received message: " << message << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Receive failed: " << e.what() << std::endl;
    }

    // Close the WebSocket connection
    client.close();
    std::cout << "WebSocket connection closed" << std::endl;

    // Asynchronous connect to the WebSocket server
    client.asyncConnect("example.com", "80", [](boost::beast::error_code ec) {
        if (ec) {
            std::cerr << "Async connection failed: " << ec.message()
                      << std::endl;
        } else {
            std::cout << "Async connected to WebSocket server" << std::endl;
        }
    });

    // Asynchronous send a message to the WebSocket server
    client.asyncSend(
        "Hello, async WebSocket server!",
        [](boost::beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Async send failed: " << ec.message() << std::endl;
            } else {
                std::cout << "Async sent message (" << bytes_transferred
                          << " bytes)" << std::endl;
            }
        });

    // Asynchronous receive a message from the WebSocket server
    client.asyncReceive([](boost::beast::error_code ec,
                           const std::string& message) {
        if (ec) {
            std::cerr << "Async receive failed: " << ec.message() << std::endl;
        } else {
            std::cout << "Async received message: " << message << std::endl;
        }
    });

    // Asynchronous close the WebSocket connection
    client.asyncClose([](boost::beast::error_code ec) {
        if (ec) {
            std::cerr << "Async close failed: " << ec.message() << std::endl;
        } else {
            std::cout << "Async WebSocket connection closed" << std::endl;
        }
    });

    // Asynchronous send a JSON object to the WebSocket server
    json jdata = {{"key", "value"}};
    client.asyncSendJson(
        jdata, [](boost::beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Async send JSON failed: " << ec.message()
                          << std::endl;
            } else {
                std::cout << "Async sent JSON (" << bytes_transferred
                          << " bytes)" << std::endl;
            }
        });

    // Asynchronous receive a JSON object from the WebSocket server
    client.asyncReceiveJson([](boost::beast::error_code ec, json jdata) {
        if (ec) {
            std::cerr << "Async receive JSON failed: " << ec.message()
                      << std::endl;
        } else {
            std::cout << "Async received JSON: " << jdata.dump(4) << std::endl;
        }
    });

    // Run the I/O context to process asynchronous operations
    ioc.run();

    return 0;
}
