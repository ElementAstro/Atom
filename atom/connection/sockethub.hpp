#ifndef ATOM_CONNECTION_SOCKETHUB_HPP
#define ATOM_CONNECTION_SOCKETHUB_HPP

#include <chrono>
#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace atom::connection {

class SocketHubImpl;

template <typename T>
concept MessageHandler = requires(T h, std::string_view msg) {
    { h(msg) } -> std::same_as<void>;
};

template <typename T>
concept ClientEventHandler =
    requires(T h, int clientId, std::string_view clientAddr) {
        { h(clientId, clientAddr) } -> std::same_as<void>;
    };

/**
 * @struct ClientInfo
 * @brief Information about a connected client
 */
struct ClientInfo {
    int id;
    std::string address;
    std::chrono::steady_clock::time_point connectedTime;
    uint64_t bytesReceived = 0;
    uint64_t bytesSent = 0;
};

/**
 * @class SocketHub
 * @brief High-performance socket connection manager with C++20 features
 */
class SocketHub {
public:
    /**
     * @brief Constructs a SocketHub instance
     * @throws std::bad_alloc If memory allocation fails
     */
    SocketHub();

    /**
     * @brief Destroys the SocketHub instance
     */
    ~SocketHub() noexcept;

    SocketHub(const SocketHub&) = delete;
    SocketHub& operator=(const SocketHub&) = delete;

    SocketHub(SocketHub&&) noexcept;
    SocketHub& operator=(SocketHub&&) noexcept;

    /**
     * @brief Starts the socket service
     * @param port The port number (valid range: 1-65535)
     * @throws std::invalid_argument If port is invalid
     * @throws std::runtime_error If socket creation fails
     */
    void start(int port);

    /**
     * @brief Stops the socket service
     */
    void stop() noexcept;

    /**
     * @brief Adds a message handler
     * @param handler Function to handle incoming messages
     * @throws std::invalid_argument If handler is invalid
     */
    template <MessageHandler H>
    void addHandler(H&& handler) {
        addHandlerImpl(std::forward<H>(handler));
    }

    /**
     * @brief Adds a client connect event handler
     * @param handler Function to handle client connect events
     * @throws std::invalid_argument If handler is invalid
     */
    template <ClientEventHandler H>
    void addConnectHandler(H&& handler) {
        addConnectHandlerImpl(std::forward<H>(handler));
    }

    /**
     * @brief Adds a client disconnect event handler
     * @param handler Function to handle client disconnect events
     * @throws std::invalid_argument If handler is invalid
     */
    template <ClientEventHandler H>
    void addDisconnectHandler(H&& handler) {
        addDisconnectHandlerImpl(std::forward<H>(handler));
    }

    /**
     * @brief Broadcasts a message to all connected clients
     * @param message The message to broadcast
     * @return Number of clients the message was sent to
     */
    [[nodiscard]] size_t broadcast(std::string_view message);

    /**
     * @brief Sends a message to a specific client
     * @param clientId The client ID to send to
     * @param message The message to send
     * @return True if message was sent successfully, false otherwise
     */
    [[nodiscard]] bool sendTo(int clientId, std::string_view message);

    /**
     * @brief Gets information about all connected clients
     * @return Vector of client information structures
     */
    [[nodiscard]] std::vector<ClientInfo> getConnectedClients() const;

    /**
     * @brief Gets the number of connected clients
     * @return The current number of connected clients
     */
    [[nodiscard]] size_t getClientCount() const noexcept;

    /**
     * @brief Checks if the socket service is running
     * @return True if running, false otherwise
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Sets the client timeout duration
     * @param timeout The timeout duration in seconds
     */
    void setClientTimeout(std::chrono::seconds timeout);

    /**
     * @brief Gets the port the server is running on
     * @return The server port, or 0 if not running
     */
    [[nodiscard]] int getPort() const noexcept;

private:
    void addHandlerImpl(std::function<void(std::string_view)> handler);
    void addConnectHandlerImpl(
        std::function<void(int, std::string_view)> handler);
    void addDisconnectHandlerImpl(
        std::function<void(int, std::string_view)> handler);
    std::unique_ptr<SocketHubImpl> impl_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_SOCKETHUB_HPP
