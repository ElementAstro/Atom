#ifndef ATOM_CONNECTION_ASYNC_FIFOSERVER_HPP
#define ATOM_CONNECTION_ASYNC_FIFOSERVER_HPP

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace atom::async::connection {

/**
 * @brief A high-performance, thread-safe server for FIFO (Named Pipe)
 * communication.
 *
 * This class provides a modern C++ interface for asynchronous I/O operations on
 * a FIFO, designed for robust, scalable performance. It listens for incoming
 * client connections and handles messages asynchronously.
 */
class FifoServer {
public:
    /**
     * @brief A handler for processing incoming messages.
     * @param data The message data received from a client.
     */
    using MessageHandler = std::function<void(std::string_view data)>;

    /**
     * @brief A handler for processing errors.
     * @param ec The error code.
     */
    using ErrorHandler = std::function<void(const std::error_code &ec)>;

    /**
     * @brief An enum representing client events.
     */
    enum class ClientEvent {
        Connected,
        Disconnected,
    };

    /**
     * @brief A handler for processing client events.
     * @param event The client event.
     */
    using ClientHandler = std::function<void(ClientEvent event)>;

    /**
     * @brief Constructs a FifoServer with the specified FIFO path.
     * @param fifoPath The filesystem path to the FIFO.
     */
    explicit FifoServer(std::string_view fifoPath);

    /**
     * @brief Destroys the FifoServer, stops it, and cleans up resources.
     */
    ~FifoServer();

    FifoServer(const FifoServer &) = delete;
    auto operator=(const FifoServer &) -> FifoServer & = delete;
    FifoServer(FifoServer &&) noexcept = default;
    auto operator=(FifoServer &&) noexcept -> FifoServer & = default;

    /**
     * @brief Starts the server and begins listening for client connections.
     * @param handler The message handler to process incoming data.
     * @throws std::runtime_error if the server fails to start.
     */
    void start(MessageHandler handler);

    /**
     * @brief Stops the server and closes any active connections.
     */
    void stop();

    /**
     * @brief Sets the client event handler.
     * @param handler The client event handler.
     */
    void setClientHandler(ClientHandler handler);

    /**
     * @brief Sets the error handler.
     * @param handler The error handler.
     */
    void setErrorHandler(ErrorHandler handler);

    /**
     * @brief Asynchronously writes data to the connected client.
     * @param data The data to write.
     * @return A future that will be true if the write was successful, false
     * otherwise.
     */
    auto write(std::string_view data) -> std::future<bool>;

    /**
     * @brief Synchronously writes data to the connected client.
     * @param data The data to write.
     * @return true if the write was successful, false otherwise.
     */
    auto writeSync(std::string_view data) -> bool;

    /**
     * @brief Checks if the server is currently running.
     * @return true if the server is running, false otherwise.
     */
    [[nodiscard]] auto isRunning() const -> bool;

    /**
     * @brief Gets the path of the FIFO.
     * @return The path of the FIFO.
     */
    [[nodiscard]] auto getPath() const -> std::string;

    /**
     * @brief Cancels all pending asynchronous operations.
     */
    void cancel();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace atom::async::connection

#endif  // ATOM_CONNECTION_ASYNC_FIFOSERVER_HPP
