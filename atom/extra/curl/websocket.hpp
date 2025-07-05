#ifndef ATOM_EXTRA_CURL_WEBSOCKET_HPP
#define ATOM_EXTRA_CURL_WEBSOCKET_HPP

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <thread>

#include <curl/curl.h>

namespace atom::extra::curl {
/**
 * @brief A class for creating and managing WebSocket connections using libcurl.
 *
 * This class provides a simple interface for establishing WebSocket
 * connections, sending and receiving messages, and handling connection events.
 */
class WebSocket {
public:
    /**
     * @brief Type definition for a message callback function.
     *
     * This callback is invoked when a new message is received from the
     * WebSocket server.
     *
     * @param message The content of the received message.
     * @param binary True if the message is binary, false if it's text.
     */
    using MessageCallback = std::function<void(const std::string&, bool)>;

    /**
     * @brief Type definition for a connect callback function.
     *
     * This callback is invoked when the WebSocket connection is successfully
     * established or when the connection attempt fails.
     *
     * @param success True if the connection was successful, false otherwise.
     */
    using ConnectCallback = std::function<void(bool)>;

    /**
     * @brief Type definition for a close callback function.
     *
     * This callback is invoked when the WebSocket connection is closed, either
     * by the client or the server.
     *
     * @param code The WebSocket close code.
     * @param reason A human-readable reason for the closure.
     */
    using CloseCallback = std::function<void(int, const std::string&)>;

    /**
     * @brief Default constructor for the WebSocket class.
     *
     * Initializes a new WebSocket object with default values.
     */
    WebSocket();

    /**
     * @brief Destructor for the WebSocket class.
     *
     * Closes the WebSocket connection and cleans up any resources.
     */
    ~WebSocket();

    /**
     * @brief Establishes a WebSocket connection to the specified URL.
     *
     * @param url The URL of the WebSocket server.
     * @param headers A map of HTTP headers to send with the connection request.
     * @return True if the connection was successfully established, false
     * otherwise.
     */
    bool connect(const std::string& url,
                 const std::map<std::string, std::string>& headers = {});

    /**
     * @brief Closes the WebSocket connection.
     *
     * @param code The WebSocket close code (default: 1000 - Normal closure).
     * @param reason A human-readable reason for the closure (default: "Normal
     * closure").
     */
    void close(int code = 1000, const std::string& reason = "Normal closure");

    /**
     * @brief Sends a message to the WebSocket server.
     *
     * @param message The content of the message to send.
     * @param binary True if the message is binary, false if it's text (default:
     * false).
     * @return True if the message was successfully sent, false otherwise.
     */
    bool send(const std::string& message, bool binary = false);

    /**
     * @brief Sets the message callback function.
     *
     * @param callback The callback function to be invoked when a new message is
     * received.
     */
    void on_message(MessageCallback callback);

    /**
     * @brief Sets the connect callback function.
     *
     * @param callback The callback function to be invoked when the WebSocket
     * connection is established or when the connection attempt fails.
     */
    void on_connect(ConnectCallback callback);

    /**
     * @brief Sets the close callback function.
     *
     * @param callback The callback function to be invoked when the WebSocket
     * connection is closed.
     */
    void on_close(CloseCallback callback);

    /**
     * @brief Checks if the WebSocket connection is currently established.
     *
     * @return True if the connection is established, false otherwise.
     */
    bool is_connected() const { return connected_; }

private:
    /** @brief The curl easy handle for the WebSocket connection. */
    CURL* handle_;
    /** @brief The URL of the WebSocket server. */
    std::string url_;
    /** @brief A flag indicating whether the WebSocket connection is running. */
    std::atomic<bool> running_;
    /** @brief A flag indicating whether the WebSocket connection is
     * established. */
    std::atomic<bool> connected_;
    /** @brief The thread that receives messages from the WebSocket server. */
    std::thread receive_thread_;
    /** @brief A mutex to protect access to shared resources. */
    std::mutex mutex_;
    /** @brief A condition variable to synchronize the receive thread. */
    std::condition_variable condition_;

    /** @brief The message callback function. */
    MessageCallback message_callback_;
    /** @brief The connect callback function. */
    ConnectCallback connect_callback_;
    /** @brief The close callback function. */
    CloseCallback close_callback_;

    /**
     * @brief The main loop for receiving messages from the WebSocket server.
     */
    void receive_loop();

    /**
     * @brief Sends a WebSocket close frame to the server.
     *
     * @param code The WebSocket close code.
     * @param reason A human-readable reason for the closure.
     */
    void send_close_frame(int code, const std::string& reason);
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_WEBSOCKET_HPP
