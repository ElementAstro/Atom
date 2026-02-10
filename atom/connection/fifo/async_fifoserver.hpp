#ifndef ATOM_CONNECTION_ASYNC_FIFOSERVER_HPP
#define ATOM_CONNECTION_ASYNC_FIFOSERVER_HPP

#include <future>
#include <memory>
#include <string>
#include <string_view>

#include "fifo_common.hpp"

namespace atom::connection {

/**
 * @brief A high-performance, thread-safe asynchronous FIFO server.
 *
 * This class provides a modern C++ interface for asynchronous I/O operations on
 * a FIFO, designed for robust, scalable performance. It listens for incoming
 * client connections and handles messages asynchronously.
 */
class AsyncFifoServer {
public:
    /**
     * @brief Constructs an AsyncFifoServer with the specified FIFO path.
     * @param fifoPath The filesystem path to the FIFO.
     */
    explicit AsyncFifoServer(std::string_view fifoPath);

    /**
     * @brief Destroys the AsyncFifoServer, stops it, and cleans up resources.
     */
    ~AsyncFifoServer();

    AsyncFifoServer(const AsyncFifoServer &) = delete;
    auto operator=(const AsyncFifoServer &) -> AsyncFifoServer & = delete;
    AsyncFifoServer(AsyncFifoServer &&) noexcept = default;
    auto operator=(AsyncFifoServer &&) noexcept -> AsyncFifoServer & = default;

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

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_ASYNC_FIFOSERVER_HPP
