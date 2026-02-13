#ifndef ATOM_CONNECTION_ASYNC_FIFOCLIENT_HPP
#define ATOM_CONNECTION_ASYNC_FIFOCLIENT_HPP

#include <memory>
#include <string>
#include <string_view>

#include "fifo_common.hpp"

namespace atom::connection {

/**
 * @brief Asynchronous FIFO client using ASIO for non-blocking I/O.
 *
 * This class provides an async interface for FIFO pipe communication,
 * complementing the synchronous FifoClient class.
 */
class AsyncFifoClient {
public:
    /**
     * @brief Constructs an AsyncFifoClient with the specified FIFO path.
     *
     * @param fifoPath The path to the FIFO file to be used for communication.
     */
    explicit AsyncFifoClient(std::string fifoPath);

    /**
     * @brief Constructs an AsyncFifoClient with custom configuration.
     *
     * @param fifoPath The path to the FIFO file.
     * @param config Custom client configuration.
     */
    AsyncFifoClient(std::string fifoPath, const ClientConfig& config);

    /**
     * @brief Destroys the AsyncFifoClient and closes the FIFO if it is open.
     */
    ~AsyncFifoClient();

    // Non-copyable
    AsyncFifoClient(const AsyncFifoClient&) = delete;
    AsyncFifoClient& operator=(const AsyncFifoClient&) = delete;

    // Movable
    AsyncFifoClient(AsyncFifoClient&&) noexcept;
    AsyncFifoClient& operator=(AsyncFifoClient&&) noexcept;

    /**
     * @brief Writes data to the FIFO.
     *
     * @param data The data to be written to the FIFO, as a string view.
     * @param timeout Optional timeout for the write operation, in milliseconds.
     * @return FifoResult with bytes written or error.
     */
    auto write(std::string_view data,
               std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> FifoResult<size_t>;

    /**
     * @brief Reads data from the FIFO.
     *
     * @param timeout Optional timeout for the read operation, in milliseconds.
     * @return FifoResult with read data or error.
     */
    auto read(std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> FifoResult<std::string>;

    /**
     * @brief Checks if the FIFO is currently open.
     *
     * @return true if the FIFO is open, false otherwise.
     */
    [[nodiscard]] auto isOpen() const -> bool;

    /**
     * @brief Gets the FIFO path.
     */
    [[nodiscard]] auto getPath() const -> std::string;

    /**
     * @brief Closes the FIFO.
     */
    void close();

    /**
     * @brief Gets the current configuration.
     */
    [[nodiscard]] auto getConfig() const -> ClientConfig;

    /**
     * @brief Updates the configuration.
     */
    auto updateConfig(const ClientConfig& config) -> bool;

    /**
     * @brief Gets current statistics.
     */
    [[nodiscard]] auto getStatistics() const -> FifoStats;

    /**
     * @brief Resets statistics.
     */
    void resetStatistics();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_ASYNC_FIFOCLIENT_HPP
