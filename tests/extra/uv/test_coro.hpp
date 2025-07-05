#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atom/extra/uv/coro.hpp"

using namespace std::chrono_literals;
using ::testing::HasSubstr;
using ::testing::StartsWith;

namespace fs = std::filesystem;

class UvCoroTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test files directory
        test_dir_path_ = fs::temp_directory_path() / "uv_coro_test";
        fs::create_directories(test_dir_path_);

        // Create test file
        test_file_path_ = test_dir_path_ / "test_file.txt";
        std::ofstream test_file(test_file_path_);
        test_file << "This is a test file.\nIt has multiple lines.\nUsed for "
                     "testing file operations.";
        test_file.close();

        // Create an HTTP echo server for testing
        startEchoServer();
    }

    void TearDown() override {
        // Clean up test directory and files
        fs::remove_all(test_dir_path_);

        // Stop the echo server
        stopEchoServer();
    }

    void startEchoServer() {
        echo_server_port_ = 8123;  // Use a port unlikely to be in use

        echo_server_thread_ = std::thread([this]() {
            uv_loop_t loop;
            uv_loop_init(&loop);

            // Setup TCP server
            uv_tcp_t server;
            uv_tcp_init(&loop, &server);

            struct sockaddr_in addr;
            uv_ip4_addr("127.0.0.1", echo_server_port_, &addr);

            uv_tcp_bind(&server,
                        reinterpret_cast<const struct sockaddr*>(&addr), 0);

            // Start listening
            uv_listen(
                reinterpret_cast<uv_stream_t*>(&server), 128,
                [](uv_stream_t* server, int status) {
                    if (status < 0)
                        return;

                    // Accept client connection
                    uv_tcp_t* client = new uv_tcp_t;
                    uv_tcp_init(server->loop, client);

                    if (uv_accept(server, reinterpret_cast<uv_stream_t*>(
                                              client)) == 0) {
                        // Setup read callback to echo data back
                        uv_read_start(
                            reinterpret_cast<uv_stream_t*>(client),
                            []([[maybe_unused]] uv_handle_t* handle,
                               size_t suggested_size, uv_buf_t* buf) {
                                buf->base = new char[suggested_size];
                                buf->len = suggested_size;
                            },
                            [](uv_stream_t* client, ssize_t nread,
                               const uv_buf_t* buf) {
                                if (nread > 0) {
                                    // Echo the data back
                                    uv_write_t* req = new uv_write_t;
                                    uv_buf_t write_buf =
                                        uv_buf_init(buf->base, nread);

                                    uv_write(
                                        req, client, &write_buf, 1,
                                        []([[maybe_unused]] uv_write_t* req,
                                           [[maybe_unused]] int status) {
                                            delete req;
                                        });
                                } else if (nread < 0) {
                                    // EOF or error, close the connection
                                    uv_close(
                                        reinterpret_cast<uv_handle_t*>(client),
                                        [](uv_handle_t* handle) {
                                            delete reinterpret_cast<uv_tcp_t*>(
                                                handle);
                                        });
                                }

                                if (buf->base) {
                                    delete[] buf->base;
                                }
                            });
                    } else {
                        uv_close(reinterpret_cast<uv_handle_t*>(client),
                                 [](uv_handle_t* handle) {
                                     delete reinterpret_cast<uv_tcp_t*>(handle);
                                 });
                    }
                });

            // Mark server as running
            server_running_.store(true);

            // Run the event loop
            while (server_running_.load() && !uv_loop_alive(&loop)) {
                uv_run(&loop, UV_RUN_DEFAULT);
            }

            // Cleanup
            uv_close(reinterpret_cast<uv_handle_t*>(&server), nullptr);

            // Let remaining handles close
            int closed = 0;
            while (uv_run(&loop, UV_RUN_NOWAIT) || closed < 5) {
                closed++;
                std::this_thread::sleep_for(100ms);
            }

            uv_loop_close(&loop);
        });

        // Wait for server to start
        std::this_thread::sleep_for(500ms);
    }

    void stopEchoServer() {
        if (server_running_.load()) {
            server_running_.store(false);
            if (echo_server_thread_.joinable()) {
                echo_server_thread_.join();
            }
        }
    }

    // Create simple HTTP response for testing
    std::string createHttpResponse(
        int status_code, const std::string& body,
        const std::vector<std::pair<std::string, std::string>>& headers = {}) {
        std::string response = "HTTP/1.1 " + std::to_string(status_code) + " ";

        // Status text
        if (status_code == 200)
            response += "OK";
        else if (status_code == 404)
            response += "Not Found";
        else if (status_code == 500)
            response += "Internal Server Error";
        else
            response += "Status";

        response += "\r\n";

        // Default headers
        response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
        response += "Connection: close\r\n";

        // Custom headers
        for (const auto& header : headers) {
            response += header.first + ": " + header.second + "\r\n";
        }

        // End of headers
        response += "\r\n";

        // Body
        response += body;

        return response;
    }

    void runEventLoopUntilDone(std::function<bool()> isDoneCheck,
                               std::chrono::milliseconds timeout = 5s) {
        auto start = std::chrono::steady_clock::now();
        while (!isDoneCheck()) {
            // Run one iteration of the event loop
            uv_coro::get_scheduler().run_once();

            // Check for timeout
            auto now = std::chrono::steady_clock::now();
            if (now - start > timeout) {
                FAIL() << "Timed out waiting for operation to complete";
                break;
            }

            // Small sleep to prevent CPU spinning
            std::this_thread::sleep_for(10ms);
        }
    }

    fs::path test_dir_path_;
    fs::path test_file_path_;
    int echo_server_port_;
    std::thread echo_server_thread_;
    std::atomic<bool> server_running_{false};
};

// Test for Task and coroutine functionality
TEST_F(UvCoroTest, BasicTaskCreationAndCompletion) {
    // Simple task that returns an integer
    auto create_task = []() -> uv_coro::Task<int> { co_return 42; };

    auto task = create_task();

    // Verify task result
    EXPECT_EQ(42, task.get_result());
}

// Test for Task exception handling
TEST_F(UvCoroTest, TaskExceptionHandling) {
    // Task that throws an exception
    auto create_throwing_task = []() -> uv_coro::Task<int> {
        throw std::runtime_error("Test exception");
        co_return 0;  // Never reached
    };

    auto task = create_throwing_task();

    // Verify exception is propagated
    EXPECT_THROW(
        {
            try {
                task.get_result();
            } catch (const std::runtime_error& e) {
                EXPECT_STREQ("Test exception", e.what());
                throw;
            }
        },
        std::runtime_error);
}

// Test for void Task
TEST_F(UvCoroTest, VoidTask) {
    bool task_executed = false;

    // Simple void task
    auto create_void_task = [&task_executed]() -> uv_coro::Task<void> {
        task_executed = true;
        co_return;
    };

    auto task = create_void_task();

    // Verify task executed
    EXPECT_TRUE(task_executed);

    // Should not throw
    EXPECT_NO_THROW(task.get_result());
}

// Test for co_await functionality between tasks
TEST_F(UvCoroTest, CoAwaitBetweenTasks) {
    // Helper task
    auto helper_task = []() -> uv_coro::Task<std::string> {
        co_return "Hello from helper";
    };

    // Main task that awaits helper_task
    auto main_task = [&helper_task]() -> uv_coro::Task<std::string> {
        std::string result = co_await helper_task();
        co_return result + " and main";
    };

    auto task = main_task();

    // Verify combined result
    EXPECT_EQ("Hello from helper and main", task.get_result());
}

// Test for TimeoutAwaiter
TEST_F(UvCoroTest, TimeoutAwaiter) {
    auto start_time = std::chrono::steady_clock::now();
    bool completed = false;

    // Task that sleeps
    auto sleep_task = [&completed]() -> uv_coro::Task<void> {
        co_await uv_coro::sleep_for(500);  // 500ms
        completed = true;
        co_return;
    };

    auto task = sleep_task();

    // Run event loop until the timeout completes
    runEventLoopUntilDone([&completed]() { return completed; });

    auto elapsed = std::chrono::steady_clock::now() - start_time;

    // Verify timing (with some tolerance)
    EXPECT_GE(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
        450);
    EXPECT_TRUE(completed);
}

// Test for TcpClient basic connectivity
TEST_F(UvCoroTest, TcpClientConnect) {
    bool connect_completed = false;
    bool exception_thrown = false;

    // Task to test TCP connection
    auto tcp_connect_task = [this, &connect_completed,
                             &exception_thrown]() -> uv_coro::Task<void> {
        try {
            auto client = uv_coro::make_tcp_client();
            co_await client.connect("127.0.0.1", echo_server_port_);
            connect_completed = true;
            client.close();
        } catch (const uv_coro::UvError& e) {
            exception_thrown = true;
            // Log error for debugging
            std::cerr << "TCP connect error: " << e.what() << " (code "
                      << e.error_code() << ")" << std::endl;
        }
        co_return;
    };

    auto task = tcp_connect_task();

    // Run event loop until connection completes or fails
    runEventLoopUntilDone([&connect_completed, &exception_thrown]() {
        return connect_completed || exception_thrown;
    });

    // Verify connection succeeded
    EXPECT_TRUE(connect_completed);
    EXPECT_FALSE(exception_thrown);
}

// Test for TcpClient echo (read/write)
TEST_F(UvCoroTest, TcpClientEcho) {
    bool echo_completed = false;
    std::string echo_result;

    // Task to test TCP echo
    auto tcp_echo_task = [this, &echo_completed,
                          &echo_result]() -> uv_coro::Task<void> {
        try {
            auto client = uv_coro::make_tcp_client();
            co_await client.connect("127.0.0.1", echo_server_port_);

            // Write data
            std::string test_data = "Hello, Echo Server!";
            co_await client.write(test_data);

            // Read response
            echo_result = co_await client.read();
            echo_completed = true;

            client.close();
        } catch (const uv_coro::UvError& e) {
            std::cerr << "TCP echo error: " << e.what() << " (code "
                      << e.error_code() << ")" << std::endl;
        }
        co_return;
    };

    auto task = tcp_echo_task();

    // Run event loop until echo completes
    runEventLoopUntilDone([&echo_completed]() { return echo_completed; });

    // Verify echo result
    EXPECT_TRUE(echo_completed);
    EXPECT_EQ("Hello, Echo Server!", echo_result);
}

// Test for FileSystem read_file
TEST_F(UvCoroTest, FileSystemReadFile) {
    bool read_completed = false;
    std::string file_content;

    // Task to test file reading
    auto file_read_task = [this, &read_completed,
                           &file_content]() -> uv_coro::Task<void> {
        try {
            auto fs = uv_coro::make_file_system();
            file_content = co_await fs.read_file(test_file_path_.string());
            read_completed = true;
        } catch (const uv_coro::UvError& e) {
            std::cerr << "File read error: " << e.what() << " (code "
                      << e.error_code() << ")" << std::endl;
        }
        co_return;
    };

    auto task = file_read_task();

    // Run event loop until read completes
    runEventLoopUntilDone([&read_completed]() { return read_completed; });

    // Verify file content
    EXPECT_TRUE(read_completed);
    EXPECT_THAT(file_content, StartsWith("This is a test file."));
    EXPECT_THAT(file_content, HasSubstr("multiple lines"));
}

// Test for FileSystem write_file
TEST_F(UvCoroTest, FileSystemWriteFile) {
    bool write_completed = false;
    bool read_completed = false;
    std::string read_content;
    std::string test_content = "This is new content.\nWritten by the test.";
    fs::path write_file_path = test_dir_path_ / "write_test.txt";

    // Task to test file writing
    auto file_write_task = [&write_completed, &read_completed, &read_content,
                            &write_file_path,
                            &test_content]() -> uv_coro::Task<void> {
        try {
            auto fs = uv_coro::make_file_system();

            // Write file
            co_await fs.write_file(write_file_path.string(), test_content);
            write_completed = true;

            // Read it back to verify
            read_content = co_await fs.read_file(write_file_path.string());
            read_completed = true;
        } catch (const uv_coro::UvError& e) {
            std::cerr << "File write/read error: " << e.what() << " (code "
                      << e.error_code() << ")" << std::endl;
        }
        co_return;
    };

    auto task = file_write_task();

    // Run event loop until operations complete
    runEventLoopUntilDone([&write_completed, &read_completed]() {
        return write_completed && read_completed;
    });

    // Verify file was written and read correctly
    EXPECT_TRUE(write_completed);
    EXPECT_TRUE(read_completed);
    EXPECT_EQ(test_content, read_content);
}

// Test for FileSystem append_file
TEST_F(UvCoroTest, FileSystemAppendFile) {
    bool append_completed = false;
    bool read_completed = false;
    std::string read_content;
    std::string append_content = "\nThis is appended content.";

    // Task to test file appending
    auto file_append_task = [this, &append_completed, &read_completed,
                             &read_content,
                             &append_content]() -> uv_coro::Task<void> {
        try {
            auto fs = uv_coro::make_file_system();

            // Append to file
            co_await fs.append_file(test_file_path_.string(), append_content);
            append_completed = true;

            // Read it back to verify
            read_content = co_await fs.read_file(test_file_path_.string());
            read_completed = true;
        } catch (const uv_coro::UvError& e) {
            std::cerr << "File append/read error: " << e.what() << " (code "
                      << e.error_code() << ")" << std::endl;
        }
        co_return;
    };

    auto task = file_append_task();

    // Run event loop until operations complete
    runEventLoopUntilDone([&append_completed, &read_completed]() {
        return append_completed && read_completed;
    });

    // Verify file was appended and read correctly
    EXPECT_TRUE(append_completed);
    EXPECT_TRUE(read_completed);
    EXPECT_THAT(read_content, testing::EndsWith("This is appended content."));
    EXPECT_THAT(read_content, HasSubstr("multiple lines"));
}

// Test for error handling with non-existent file
TEST_F(UvCoroTest, FileSystemNonExistentFile) {
    bool operation_completed = false;
    bool exception_caught = false;
    int error_code = 0;

    // Path to non-existent file
    fs::path non_existent_file = test_dir_path_ / "does_not_exist.txt";

    // Task to test reading non-existent file
    auto file_error_task = [&non_existent_file, &operation_completed,
                            &exception_caught,
                            &error_code]() -> uv_coro::Task<void> {
        try {
            auto fs = uv_coro::make_file_system();
            std::string content =
                co_await fs.read_file(non_existent_file.string());
            operation_completed = true;
        } catch (const uv_coro::UvError& e) {
            exception_caught = true;
            error_code = e.error_code();
        }
        co_return;
    };

    auto task = file_error_task();

    // Run event loop until operation completes
    runEventLoopUntilDone([&operation_completed, &exception_caught]() {
        return operation_completed || exception_caught;
    });

    // Verify error was caught
    EXPECT_FALSE(operation_completed);
    EXPECT_TRUE(exception_caught);

    // Check that it's a "file not found" type error (varies by platform)
#ifdef _WIN32
    EXPECT_EQ(UV_ENOENT, error_code);
#else
    EXPECT_TRUE(error_code == UV_ENOENT || error_code == UV_EACCES);
#endif
}

// Test for HttpClient basic GET
TEST_F(UvCoroTest, HttpClientBasicGet) {
    // Setup a simple HTTP response server
    int http_port = 8124;
    std::atomic<bool> request_received{false};
    std::string request_path;

    // Start HTTP server thread
    std::thread http_server_thread([&http_port, &request_received,
                                    &request_path]() {
        uv_loop_t loop;
        uv_loop_init(&loop);

        uv_tcp_t server;
        uv_tcp_init(&loop, &server);

        struct sockaddr_in addr;
        uv_ip4_addr("127.0.0.1", http_port, &addr);
        uv_tcp_bind(&server, reinterpret_cast<const struct sockaddr*>(&addr),
                    0);

        uv_listen(
            reinterpret_cast<uv_stream_t*>(&server), 128,
            [&request_path, &request_received](uv_stream_t* server,
                                               int status) {
                if (status < 0)
                    return;

                struct ClientData {
                    std::string request;
                    uv_buf_t response;
                };

                auto* client_data = new ClientData;

                uv_tcp_t* client = new uv_tcp_t;
                uv_tcp_init(server->loop, client);
                client->data = client_data;

                if (uv_accept(server, reinterpret_cast<uv_stream_t*>(client)) ==
                    0) {
                    uv_read_start(
                        reinterpret_cast<uv_stream_t*>(client),
                        [](uv_handle_t* handle, size_t suggested_size,
                           uv_buf_t* buf) {
                            buf->base = new char[suggested_size];
                            buf->len = suggested_size;
                        },
                        [&request_path, &request_received](
                            uv_stream_t* client, ssize_t nread,
                            const uv_buf_t* buf) {
                            auto* client_data =
                                static_cast<ClientData*>(client->data);

                            if (nread > 0) {
                                // Append request data
                                client_data->request.append(buf->base, nread);

                                // Check if we have the full request
                                if (client_data->request.find("\r\n\r\n") !=
                                    std::string::npos) {
                                    // Extract path from request
                                    size_t path_start =
                                        client_data->request.find("GET ") + 4;
                                    size_t path_end = client_data->request.find(
                                        " HTTP", path_start);
                                    request_path = client_data->request.substr(
                                        path_start, path_end - path_start);
                                    request_received = true;

                                    // Create response
                                    std::string response_body =
                                        "{ \"message\": \"Test success\", "
                                        "\"path\": \"" +
                                        request_path + "\" }";
                                    std::string response_str =
                                        "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: application/json\r\n"
                                        "Content-Length: " +
                                        std::to_string(response_body.length()) +
                                        "\r\n"
                                        "Connection: close\r\n\r\n" +
                                        response_body;

                                    char* response_data =
                                        new char[response_str.length()];
                                    memcpy(response_data, response_str.c_str(),
                                           response_str.length());

                                    client_data->response = uv_buf_init(
                                        response_data, response_str.length());

                                    // Send response
                                    uv_write_t* write_req = new uv_write_t;
                                    write_req->data = client_data;

                                    uv_write(
                                        write_req, client,
                                        &client_data->response, 1,
                                        [](uv_write_t* req, int status) {
                                            // Close connection after write
                                            auto* client_data =
                                                static_cast<ClientData*>(
                                                    req->data);
                                            uv_close(
                                                reinterpret_cast<uv_handle_t*>(
                                                    req->handle),
                                                [](uv_handle_t* handle) {
                                                    auto* client_data =
                                                        static_cast<
                                                            ClientData*>(
                                                            handle->data);
                                                    delete[] client_data
                                                        ->response.base;
                                                    delete client_data;
                                                    delete reinterpret_cast<
                                                        uv_tcp_t*>(handle);
                                                });
                                            delete req;
                                        });
                                }
                            }

                            if (buf->base) {
                                delete[] buf->base;
                            }

                            if (nread < 0) {
                                // Handle EOF or error
                                uv_close(reinterpret_cast<uv_handle_t*>(client),
                                         [](uv_handle_t* handle) {
                                             auto* client_data =
                                                 static_cast<ClientData*>(
                                                     handle->data);
                                             delete client_data;
                                             delete reinterpret_cast<uv_tcp_t*>(
                                                 handle);
                                         });
                            }
                        });
                }
            });

        // Run the loop for a limited time
        uv_timer_t shutdown_timer;
        uv_timer_init(&loop, &shutdown_timer);
        uv_timer_start(
            &shutdown_timer, [](uv_timer_t* timer) { uv_stop(timer->loop); },
            10000, 0);

        uv_run(&loop, UV_RUN_DEFAULT);

        // Cleanup
        uv_close(reinterpret_cast<uv_handle_t*>(&shutdown_timer), nullptr);
        uv_close(reinterpret_cast<uv_handle_t*>(&server), nullptr);

        while (uv_run(&loop, UV_RUN_NOWAIT)) {
        }

        uv_loop_close(&loop);
    });

    // Wait for server to start
    std::this_thread::sleep_for(500ms);

    bool http_completed = false;
    uv_coro::HttpClient::HttpResponse response;

    // Task to test HTTP GET
    auto http_get_task = [&http_port, &http_completed,
                          &response]() -> uv_coro::Task<void> {
        try {
            auto client = uv_coro::make_http_client();
            response = co_await client.get(
                "http://127.0.0.1:" + std::to_string(http_port) + "/test/path");
            http_completed = true;
        } catch (const std::exception& e) {
            std::cerr << "HTTP GET error: " << e.what() << std::endl;
        }
        co_return;
    };

    auto task = http_get_task();

    // Run event loop until HTTP request completes
    runEventLoopUntilDone([&http_completed]() { return http_completed; });

    // Cleanup HTTP server
    if (http_server_thread.joinable()) {
        http_server_thread.join();
    }

    // Verify HTTP request and response
    EXPECT_TRUE(http_completed);
    EXPECT_TRUE(request_received);
    EXPECT_EQ("/test/path", request_path);
    EXPECT_EQ(200, response.status_code);
    EXPECT_THAT(response.body, HasSubstr("Test success"));
    EXPECT_THAT(response.body, HasSubstr("/test/path"));
    EXPECT_TRUE(response.headers.find("Content-Type") !=
                response.headers.end());
    if (response.headers.find("Content-Type") != response.headers.end()) {
        EXPECT_EQ("application/json", response.headers["Content-Type"]);
    }
}

// Test for error handling in HTTP client
TEST_F(UvCoroTest, HttpClientErrorHandling) {
    bool operation_completed = false;
    bool exception_caught = false;
    std::string error_message;

    // Task to test HTTP error handling (connection refused)
    auto http_error_task = [&operation_completed, &exception_caught,
                            &error_message]() -> uv_coro::Task<void> {
        try {
            auto client = uv_coro::make_http_client();
            // Use a port where nothing is running
            auto response = co_await client.get("http://127.0.0.1:54321/");
            operation_completed = true;
        } catch (const uv_coro::UvError& e) {
            exception_caught = true;
            error_message = e.what();
        } catch (const std::exception& e) {
            exception_caught = true;
            error_message = e.what();
        }
        co_return;
    };

    auto task = http_error_task();

    // Run event loop until operation completes
    runEventLoopUntilDone([&operation_completed, &exception_caught]() {
        return operation_completed || exception_caught;
    });

    // Verify error was caught
    EXPECT_FALSE(operation_completed);
    EXPECT_TRUE(exception_caught);
    EXPECT_FALSE(error_message.empty());
}

// Test for complex coroutine chaining
TEST_F(UvCoroTest, ComplexCoroutineChaining) {
    bool operation_completed = false;
    std::string final_result;

    // Task to test chaining multiple operations
    auto complex_task = [this, &operation_completed,
                         &final_result]() -> uv_coro::Task<void> {
        try {
            // Step 1: Read a file
            auto fs = uv_coro::make_file_system();
            std::string file_content =
                co_await fs.read_file(test_file_path_.string());

            // Step 2: Sleep for a bit
            co_await uv_coro::sleep_for(200);

            // Step 3: Write to a new file
            fs::path output_path = test_dir_path_ / "complex_output.txt";
            co_await fs.write_file(output_path.string(),
                                   "Original: " + file_content);

            // Step 4: Read it back
            std::string processed_content =
                co_await fs.read_file(output_path.string());

            // Step 5: Set the result
            final_result = processed_content;
            operation_completed = true;
        } catch (const std::exception& e) {
            std::cerr << "Complex task error: " << e.what() << std::endl;
        }
        co_return;
    };

    auto task = complex_task();

    // Run event loop until complex task completes
    runEventLoopUntilDone(
        [&operation_completed]() { return operation_completed; });

    // Verify result
    EXPECT_TRUE(operation_completed);
    EXPECT_THAT(final_result, StartsWith("Original: This is a test file."));
}

// Test for UvError
TEST_F(UvCoroTest, UvErrorTest) {
    // Create a UvError with a known error code
    uv_coro::UvError error(UV_ECONNREFUSED);

    // Verify error code and message
    EXPECT_EQ(UV_ECONNREFUSED, error.error_code());
    EXPECT_STREQ(uv_strerror(UV_ECONNREFUSED), error.what());
}

// Test for Scheduler
TEST_F(UvCoroTest, SchedulerTest) {
    // Get the global scheduler
    auto& scheduler = uv_coro::get_scheduler();

    // Get the loop from the scheduler
    uv_loop_t* loop = scheduler.get_loop();

    // Verify the loop is not null
    EXPECT_NE(nullptr, loop);

    // Run the scheduler once to ensure it works
    EXPECT_NO_THROW(scheduler.run_once());
}

// Main function to run the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
