#include "atom/extra/beast/http.hpp"

#include <boost/asio/io_context.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std::chrono_literals;
using json = nlohmann::json;

int main() {
    // Create an I/O context
    boost::asio::io_context ioc;

    // Create an instance of HttpClient
    HttpClient client(ioc);

    // Set a default header
    client.setDefaultHeader("User-Agent", "HttpClient/1.0");

    // Set a timeout duration
    client.setTimeout(30s);

    // Synchronous HTTP request
    try {
        auto response =
            client.request(http::verb::get, "example.com", "80", "/");
        std::cout << "Synchronous response: " << response << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Synchronous request failed: " << e.what() << std::endl;
    }

    // Asynchronous HTTP request
    client.asyncRequest(
        http::verb::get, "example.com", "80", "/",
        [](boost::beast::error_code ec, http::response<http::string_body> res) {
            if (ec) {
                std::cerr << "Asynchronous request failed: " << ec.message()
                          << std::endl;
            } else {
                std::cout << "Asynchronous response: " << res << std::endl;
            }
        });

    // Synchronous JSON request
    try {
        json requestBody = {{"key", "value"}};
        auto jsonResponse = client.jsonRequest(http::verb::post, "example.com",
                                               "80", "/json", requestBody);
        std::cout << "Synchronous JSON response: " << jsonResponse.dump(4)
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Synchronous JSON request failed: " << e.what()
                  << std::endl;
    }

    // Asynchronous JSON request
    client.asyncJsonRequest(http::verb::post, "example.com", "80", "/json",
                            [](boost::beast::error_code ec, json res) {
                                if (ec) {
                                    std::cerr
                                        << "Asynchronous JSON request failed: "
                                        << ec.message() << std::endl;
                                } else {
                                    std::cout << "Asynchronous JSON response: "
                                              << res.dump(4) << std::endl;
                                }
                            },
                            {{"key", "value"}});

    // Upload a file
    try {
        auto response = client.uploadFile("example.com", "80", "/upload",
                                          "path/to/file.txt");
        std::cout << "File upload response: " << response << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "File upload failed: " << e.what() << std::endl;
    }

    // Download a file
    try {
        client.downloadFile("example.com", "80", "/download",
                            "path/to/save/file.txt");
        std::cout << "File downloaded successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "File download failed: " << e.what() << std::endl;
    }

    // Synchronous request with retry logic
    try {
        auto response = client.requestWithRetry(http::verb::get, "example.com",
                                                "80", "/retry", 3);
        std::cout << "Synchronous response with retry: " << response
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Synchronous request with retry failed: " << e.what()
                  << std::endl;
    }

    // Batch synchronous requests
    std::vector<std::tuple<http::verb, std::string, std::string, std::string>>
        requests = {{http::verb::get, "example.com", "80", "/1"},
                    {http::verb::get, "example.com", "80", "/2"},
                    {http::verb::get, "example.com", "80", "/3"}};
    auto responses = client.batchRequest(requests);
    for (const auto& res : responses) {
        std::cout << "Batch response: " << res << std::endl;
    }

    // Batch asynchronous requests
    client.asyncBatchRequest(
        requests,
        [](const std::vector<http::response<http::string_body>>& responses) {
            for (const auto& res : responses) {
                std::cout << "Batch async response: " << res << std::endl;
            }
        });

    // Run the I/O context with a thread pool
    client.runWithThreadPool(4);

    // Asynchronous file download
    client.asyncDownloadFile(
        "example.com", "80", "/download", "path/to/save/file.txt",
        [](boost::beast::error_code ec, bool success) {
            if (ec) {
                std::cerr << "Asynchronous file download failed: "
                          << ec.message() << std::endl;
            } else if (success) {
                std::cout << "Asynchronous file downloaded successfully"
                          << std::endl;
            } else {
                std::cerr << "Asynchronous file download failed" << std::endl;
            }
        });

    // Run the I/O context to process asynchronous operations
    ioc.run();

    return 0;
}