#include "http.hpp"

#include <gtest/gtest.h>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
namespace fs = std::filesystem;

class HttpClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
        ioc_ = std::make_unique<net::io_context>();
        client_ = std::make_unique<HttpClient>(*ioc_);

        // Create a temporary directory for file tests
        temp_dir_ = fs::temp_directory_path() / "http_client_test";
        if (fs::exists(temp_dir_)) {
            fs::remove_all(temp_dir_);
        }
        fs::create_directories(temp_dir_);

        // Create a test file for upload tests
        test_file_path_ = temp_dir_ / "test_upload.txt";
        std::ofstream test_file(test_file_path_);
        test_file << "This is test content for file upload";
        test_file.close();

        // Start mock server in a separate thread
        server_thread_ = std::thread([this]() { runMockServer(); });

        // Wait a moment for the server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        // Cleanup
        server_running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        // Cleanup temporary directory
        try {
            fs::remove_all(temp_dir_);
        } catch (...) {
            // Ignore errors during cleanup
        }

        client_.reset();
        ioc_.reset();
    }

    // Helper method to run a mock HTTP server
    void runMockServer() {
        try {
            auto const address = net::ip::make_address("127.0.0.1");
            tcp::acceptor acceptor(*ioc_, {address, 8080});
            server_running_ = true;

            while (server_running_) {
                tcp::socket socket(*ioc_);
                acceptor.accept(socket);

                beast::flat_buffer buffer;
                http::request<http::string_body> req;
                http::read(socket, buffer, req);

                http::response<http::string_body> res{http::status::ok,
                                                      req.version()};
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/plain");

                // Mock different endpoints
                if (req.target() == "/get") {
                    res.body() = "GET response";
                } else if (req.target() == "/post") {
                    res.body() = "POST response: " + req.body();
                } else if (req.target() == "/json") {
                    res.set(http::field::content_type, "application/json");
                    res.body() =
                        "{\"status\":\"success\",\"message\":\"JSON "
                        "response\"}";
                } else if (req.target() == "/upload") {
                    res.body() = "File uploaded successfully";
                } else if (req.target() == "/download") {
                    res.body() = "This is content for download test";
                } else if (req.target() == "/retry") {
                    static int retry_count = 0;
                    if (retry_count++ < 2) {
                        res.result(http::status::service_unavailable);
                        res.body() = "Service temporarily unavailable";
                    } else {
                        res.result(http::status::ok);
                        res.body() = "Success after retries";
                        retry_count = 0;
                    }
                } else if (req.target() == "/timeout") {
                    // Simulate timeout by delaying the response
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    res.body() = "Response after delay";
                } else if (req.target() == "/error") {
                    res.result(http::status::internal_server_error);
                    res.body() = "Internal server error";
                } else {
                    res.result(http::status::not_found);
                    res.body() = "Not found";
                }

                res.prepare_payload();
                http::write(socket, res);

                beast::error_code ec;
                socket.shutdown(tcp::socket::shutdown_both, ec);
                if (ec && ec != beast::errc::not_connected) {
                    // Ignore this error
                }
            }
        } catch (const std::exception& e) {
            // Ignore exceptions during shutdown
            if (server_running_) {
                std::cerr << "Server error: " << e.what() << std::endl;
            }
        }
    }

    std::unique_ptr<net::io_context> ioc_;
    std::unique_ptr<HttpClient> client_;
    std::thread server_thread_;
    std::atomic<bool> server_running_{false};
    fs::path temp_dir_;
    fs::path test_file_path_;
    const std::string test_host = "127.0.0.1";
    const std::string test_port = "8080";
};

// Test basic HTTP GET request
TEST_F(HttpClientTest, BasicGetRequest) {
    auto response =
        client_->request(http::verb::get, test_host, test_port, "/get");
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response.body(), "GET response");
}

// Test basic HTTP POST request
TEST_F(HttpClientTest, BasicPostRequest) {
    auto response =
        client_->request(http::verb::post, test_host, test_port, "/post", 11,
                         "text/plain", "Test POST data");
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response.body(), "POST response: Test POST data");
}

// Test custom headers
TEST_F(HttpClientTest, CustomHeaders) {
    std::unordered_map<std::string, std::string> headers = {
        {"X-Custom-Header", "CustomValue"}};

    client_->setDefaultHeader("X-Default-Header", "DefaultValue");

    auto response = client_->request(http::verb::get, test_host, test_port,
                                     "/get", 11, "", "", headers);
    EXPECT_EQ(response.result(), http::status::ok);
}

// Test JSON request
TEST_F(HttpClientTest, JsonRequest) {
    json req_body = {{"key1", "value1"}, {"key2", 42}};

    auto response = client_->jsonRequest(http::verb::post, test_host, test_port,
                                         "/json", req_body);

    EXPECT_EQ(response["status"], "success");
    EXPECT_EQ(response["message"], "JSON response");
}

// Test timeout setting
TEST_F(HttpClientTest, Timeout) {
    // Set a short timeout
    client_->setTimeout(std::chrono::seconds(1));

    // This should timeout
    EXPECT_THROW(
        client_->request(http::verb::get, test_host, test_port, "/timeout"),
        beast::system_error);

    // Reset timeout to a longer value
    client_->setTimeout(std::chrono::seconds(5));

    // This should now succeed
    auto response =
        client_->request(http::verb::get, test_host, test_port, "/timeout");
    EXPECT_EQ(response.result(), http::status::ok);
}

// Test file upload
TEST_F(HttpClientTest, FileUpload) {
    auto response = client_->uploadFile(test_host, test_port, "/upload",
                                        test_file_path_.string(), "file");

    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response.body(), "File uploaded successfully");
}

// Test file download
TEST_F(HttpClientTest, FileDownload) {
    fs::path download_path = temp_dir_ / "downloaded_file.txt";

    client_->downloadFile(test_host, test_port, "/download",
                          download_path.string());

    EXPECT_TRUE(fs::exists(download_path));

    std::ifstream file(download_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_EQ(content, "This is content for download test");
}

// Test request with retry
TEST_F(HttpClientTest, RequestWithRetry) {
    auto response = client_->requestWithRetry(http::verb::get, test_host,
                                              test_port, "/retry", 5);

    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response.body(), "Success after retries");
}

// Test batch request
TEST_F(HttpClientTest, BatchRequest) {
    std::vector<std::tuple<http::verb, std::string, std::string, std::string>>
        requests = {{http::verb::get, test_host, test_port, "/get"},
                    {http::verb::get, test_host, test_port, "/json"}};

    auto responses = client_->batchRequest(requests);

    EXPECT_EQ(responses.size(), 2);
    EXPECT_EQ(responses[0].result(), http::status::ok);
    EXPECT_EQ(responses[0].body(), "GET response");
    EXPECT_EQ(responses[1].result(), http::status::ok);
}

// Test error handling
TEST_F(HttpClientTest, ErrorHandling) {
    // Test 404 error
    auto response =
        client_->request(http::verb::get, test_host, test_port, "/nonexistent");
    EXPECT_EQ(response.result(), http::status::not_found);

    // Test server error
    response =
        client_->request(http::verb::get, test_host, test_port, "/error");
    EXPECT_EQ(response.result(), http::status::internal_server_error);

    // Test invalid host
    EXPECT_THROW(client_->request(http::verb::get, "", test_port, "/get"),
                 std::invalid_argument);

    // Test invalid port
    EXPECT_THROW(client_->request(http::verb::get, test_host, "", "/get"),
                 std::invalid_argument);

    // Test connection error to non-existent server
    EXPECT_THROW(
        client_->request(http::verb::get, "nonexistent.host", "8080", "/get"),
        beast::system_error);
}

// Test thread pool
TEST_F(HttpClientTest, ThreadPool) {
    // This just tests that the method doesn't throw
    EXPECT_NO_THROW(client_->runWithThreadPool(2));

    // Test invalid thread count
    EXPECT_THROW(client_->runWithThreadPool(0), std::invalid_argument);
}

// Test file operations with invalid paths
TEST_F(HttpClientTest, InvalidFilePaths) {
    // Test upload with non-existent file
    EXPECT_THROW(
        client_->uploadFile(test_host, test_port, "/upload",
                            (temp_dir_ / "nonexistent.txt").string(), "file"),
        std::runtime_error);

    // Test download to invalid directory
    EXPECT_THROW(
        client_->downloadFile(test_host, test_port, "/download",
                              "/invalid/path/that/does/not/exist/file.txt"),
        std::runtime_error);

    // Test download with empty path
    EXPECT_THROW(client_->downloadFile(test_host, test_port, "/download", ""),
                 std::invalid_argument);
}

// Test setting invalid values
TEST_F(HttpClientTest, InvalidValues) {
    // Test setting invalid timeout
    EXPECT_THROW(client_->setTimeout(std::chrono::seconds(0)),
                 std::invalid_argument);
    EXPECT_THROW(client_->setTimeout(std::chrono::seconds(-1)),
                 std::invalid_argument);

    // Test setting empty header key
    EXPECT_THROW(client_->setDefaultHeader("", "value"), std::invalid_argument);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
