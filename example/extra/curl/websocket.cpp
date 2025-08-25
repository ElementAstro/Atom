#include "atom/extra/curl/websocket.hpp"
#include "atom/extra/curl/error.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using namespace atom::extra::curl;
using namespace std::chrono_literals;

// Simple WebSocket echo server simulation for testing
// Note: This example assumes you have access to a WebSocket echo server
// You can use ws://echo.websocket.org/ for testing

int main() {
    try {
        std::cout << "=== CURL WebSocket Example ===" << std::endl;

        // Note: WebSocket support in libcurl is relatively new and may not be
        // available in all versions. This example demonstrates the API usage.

        // 1. Basic WebSocket connection and messaging
        std::cout << "\n1. Basic WebSocket Connection:" << std::endl;
        {
            try {
                WebSocket ws;

                // Connect to WebSocket echo server
                std::cout << "Connecting to WebSocket echo server..."
                          << std::endl;
                ws.connect("ws://echo.websocket.org/");
                std::cout << "Connected successfully!" << std::endl;

                // Send a simple message
                std::string message = "Hello, WebSocket!";
                std::cout << "Sending message: " << message << std::endl;
                ws.send(message);

                // Receive the echo
                std::string received = ws.receive();
                std::cout << "Received echo: " << received << std::endl;

                // Close connection
                ws.close();
                std::cout << "Connection closed" << std::endl;

            } catch (const Error& e) {
                std::cerr << "WebSocket basic test failed: " << e.what()
                          << std::endl;
                std::cout << "Note: WebSocket support requires libcurl 7.86.0+ "
                             "with WebSocket enabled"
                          << std::endl;
            }
        }

        // 2. WebSocket with multiple messages
        std::cout << "\n2. Multiple Messages:" << std::endl;
        {
            try {
                WebSocket ws;
                ws.connect("ws://echo.websocket.org/");

                std::vector<std::string> messages = {
                    "Message 1: Hello", "Message 2: How are you?",
                    "Message 3: This is a test",
                    "Message 4: WebSocket is working!", "Message 5: Goodbye"};

                for (size_t i = 0; i < messages.size(); ++i) {
                    std::cout << "Sending: " << messages[i] << std::endl;
                    ws.send(messages[i]);

                    std::string received = ws.receive();
                    std::cout << "Received: " << received << std::endl;

                    // Small delay between messages
                    std::this_thread::sleep_for(100ms);
                }

                ws.close();

            } catch (const Error& e) {
                std::cerr << "Multiple messages test failed: " << e.what()
                          << std::endl;
            }
        }

        // 3. WebSocket with JSON messages
        std::cout << "\n3. JSON Messages:" << std::endl;
        {
            try {
                WebSocket ws;
                ws.connect("ws://echo.websocket.org/");

                // Send JSON messages
                std::vector<std::string> json_messages = {
                    R"({"type": "greeting", "message": "Hello", "timestamp": 1234567890})",
                    R"({"type": "data", "values": [1, 2, 3, 4, 5], "count": 5})",
                    R"({"type": "user", "name": "John Doe", "id": 12345, "active": true})",
                    R"({"type": "command", "action": "disconnect", "reason": "test complete"})"};

                for (const auto& json_msg : json_messages) {
                    std::cout << "Sending JSON: " << json_msg << std::endl;
                    ws.send(json_msg);

                    std::string received = ws.receive();
                    std::cout << "Received JSON: " << received << std::endl;

                    std::this_thread::sleep_for(100ms);
                }

                ws.close();

            } catch (const Error& e) {
                std::cerr << "JSON messages test failed: " << e.what()
                          << std::endl;
            }
        }

        // 4. WebSocket with binary data
        std::cout << "\n4. Binary Data:" << std::endl;
        {
            try {
                WebSocket ws;
                ws.connect("ws://echo.websocket.org/");

                // Create binary data
                std::vector<uint8_t> binary_data;
                for (int i = 0; i < 256; ++i) {
                    binary_data.push_back(static_cast<uint8_t>(i));
                }

                std::cout << "Sending binary data (" << binary_data.size()
                          << " bytes)" << std::endl;
                ws.send_binary(binary_data);

                auto received_binary = ws.receive_binary();
                std::cout << "Received binary data (" << received_binary.size()
                          << " bytes)" << std::endl;

                // Verify data integrity
                bool data_matches = (binary_data == received_binary);
                std::cout << "Data integrity check: "
                          << (data_matches ? "PASSED" : "FAILED") << std::endl;

                ws.close();

            } catch (const Error& e) {
                std::cerr << "Binary data test failed: " << e.what()
                          << std::endl;
            }
        }

        // 5. WebSocket with ping/pong
        std::cout << "\n5. Ping/Pong:" << std::endl;
        {
            try {
                WebSocket ws;
                ws.connect("ws://echo.websocket.org/");

                // Send ping
                std::cout << "Sending ping..." << std::endl;
                ws.ping("ping-data");

                // Wait for pong (this would be handled automatically in a real
                // implementation)
                std::cout << "Ping sent (pong handling depends on server "
                             "implementation)"
                          << std::endl;

                ws.close();

            } catch (const Error& e) {
                std::cerr << "Ping/pong test failed: " << e.what() << std::endl;
            }
        }

        // 6. WebSocket with custom headers
        std::cout << "\n6. Custom Headers:" << std::endl;
        {
            try {
                WebSocket ws;

                // Set custom headers for the WebSocket handshake
                ws.set_header("User-Agent", "Atom-WebSocket-Client/1.0");
                ws.set_header("X-Client-Version", "1.0.0");
                ws.set_header("X-Custom-Header", "test-value");

                ws.connect("ws://echo.websocket.org/");

                std::string message = "Message with custom headers";
                ws.send(message);

                std::string received = ws.receive();
                std::cout << "Sent: " << message << std::endl;
                std::cout << "Received: " << received << std::endl;

                ws.close();

            } catch (const Error& e) {
                std::cerr << "Custom headers test failed: " << e.what()
                          << std::endl;
            }
        }

        // 7. WebSocket connection timeout
        std::cout << "\n7. Connection Timeout:" << std::endl;
        {
            try {
                WebSocket ws;
                ws.set_timeout(5s);  // 5 second timeout

                // Try to connect to a non-existent server (should timeout)
                std::cout << "Attempting connection with timeout..."
                          << std::endl;
                ws.connect("ws://non-existent-server.example.com/");

                ws.close();

            } catch (const Error& e) {
                std::cout << "Expected timeout error: " << e.what()
                          << std::endl;
            }
        }

        // 8. WebSocket with subprotocols
        std::cout << "\n8. Subprotocols:" << std::endl;
        {
            try {
                WebSocket ws;

                // Set subprotocols
                ws.set_subprotocol("chat");
                // ws.set_subprotocol("echo");  // Multiple subprotocols if
                // supported

                ws.connect("ws://echo.websocket.org/");

                std::string message = "Message with subprotocol";
                ws.send(message);

                std::string received = ws.receive();
                std::cout << "Sent: " << message << std::endl;
                std::cout << "Received: " << received << std::endl;

                ws.close();

            } catch (const Error& e) {
                std::cerr << "Subprotocol test failed: " << e.what()
                          << std::endl;
            }
        }

        // 9. Asynchronous WebSocket operations (if supported)
        std::cout << "\n9. Asynchronous Operations:" << std::endl;
        {
            try {
                WebSocket ws;
                ws.connect("ws://echo.websocket.org/");

                // Simulate async operations with futures
                std::atomic<bool> keep_running{true};

                // Sender thread
                auto sender =
                    std::async(std::launch::async, [&ws, &keep_running]() {
                        int count = 0;
                        while (keep_running && count < 5) {
                            std::string msg =
                                "Async message " + std::to_string(count);
                            ws.send(msg);
                            std::cout << "Sent async: " << msg << std::endl;
                            std::this_thread::sleep_for(500ms);
                            ++count;
                        }
                    });

                // Receiver thread
                auto receiver =
                    std::async(std::launch::async, [&ws, &keep_running]() {
                        int count = 0;
                        while (keep_running && count < 5) {
                            try {
                                std::string received = ws.receive();
                                std::cout << "Received async: " << received
                                          << std::endl;
                                ++count;
                            } catch (const Error& e) {
                                std::cerr << "Async receive error: " << e.what()
                                          << std::endl;
                                break;
                            }
                        }
                    });

                // Wait for completion
                sender.wait();
                receiver.wait();
                keep_running = false;

                ws.close();

            } catch (const Error& e) {
                std::cerr << "Async operations test failed: " << e.what()
                          << std::endl;
            }
        }

        std::cout << "\n=== WebSocket Example Completed ===" << std::endl;
        std::cout << "\nNote: WebSocket support in libcurl requires version "
                     "7.86.0 or later"
                  << std::endl;
        std::cout << "and must be compiled with WebSocket support enabled."
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
