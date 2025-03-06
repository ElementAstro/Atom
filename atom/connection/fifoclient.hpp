/*
 * fifoclient.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FIFO Client

*************************************************/

#ifndef ATOM_CONNECTION_FIFOCLIENT_HPP
#define ATOM_CONNECTION_FIFOCLIENT_HPP

#include <chrono>
#include <concepts>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "atom/type/expected.hpp"

namespace atom::connection {

// Error codes specific to FIFO operations
enum class FifoError {
    OpenFailed,
    ReadFailed,
    WriteFailed,
    Timeout,
    InvalidOperation,
    NotOpen
};

/**
 * @brief A concept for types that can be written to a FIFO
 *
 * Requires that the type provides contiguous data storage and has a size
 */
template <typename T>
concept WritableData = requires(T data) {
    { std::span(data) } -> std::convertible_to<std::span<const std::byte>>;
    { data.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief A class for interacting with a FIFO (First In, First Out) pipe.
 *
 * This class provides methods to read from and write to a FIFO pipe,
 * handling timeouts and ensuring proper resource management.
 */
class FifoClient {
public:
    /**
     * @brief Constructs a FifoClient with the specified FIFO path.
     *
     * @param fifoPath The path to the FIFO file to be used for communication.
     * @throws std::system_error If the FIFO cannot be opened
     *
     * This constructor opens the FIFO and prepares the client for
     * reading and writing operations.
     */
    explicit FifoClient(std::string_view fifoPath);

    /**
     * @brief Move constructor
     *
     * @param other The FifoClient to move from
     */
    FifoClient(FifoClient&& other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * @param other The FifoClient to move from
     * @return FifoClient& Reference to this object
     */
    FifoClient& operator=(FifoClient&& other) noexcept;

    // Disable copying
    FifoClient(const FifoClient&) = delete;
    FifoClient& operator=(const FifoClient&) = delete;

    /**
     * @brief Destroys the FifoClient and closes the FIFO if it is open.
     *
     * This destructor ensures that all resources are released and the FIFO
     * is properly closed to avoid resource leaks.
     */
    ~FifoClient();

    /**
     * @brief Writes data to the FIFO.
     *
     * @tparam T Type of data that satisfies WritableData concept
     * @param data The data to be written to the FIFO
     * @param timeout Optional timeout for the write operation, in milliseconds.
     *                If not provided, the default is no timeout.
     * @return type::expected<std::size_t, std::error_code> The number of bytes
     * written or an error
     *
     * This method will attempt to write the specified data to the FIFO.
     * If a timeout is specified, the operation will fail if it cannot complete
     * within the given duration.
     */
    template <WritableData T>
    auto write(const T& data,
               std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<std::size_t, std::error_code>;

    /**
     * @brief Writes string data to the FIFO.
     *
     * @param data The string data to be written to the FIFO
     * @param timeout Optional timeout for the write operation, in milliseconds.
     * @return type::expected<std::size_t, std::error_code> The number of bytes
     * written or an error
     */
    auto write(std::string_view data,
               std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<std::size_t, std::error_code>;

    /**
     * @brief Reads data from the FIFO.
     *
     * @param maxSize Maximum number of bytes to read
     * @param timeout Optional timeout for the read operation, in milliseconds.
     *                If not provided, the default is no timeout.
     * @return type::expected<std::string, std::error_code> The data read or an
     * error
     *
     * This method will read data from the FIFO. If a timeout is specified,
     * it will return an error if the operation cannot complete within the
     * specified time.
     */
    auto read(std::size_t maxSize = 4096,
              std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<std::string, std::error_code>;

    /**
     * @brief Checks if the FIFO is currently open.
     *
     * @return true if the FIFO is open, false otherwise.
     *
     * This method can be used to determine if the FIFO client is ready for
     * operations.
     */
    [[nodiscard]] auto isOpen() const noexcept -> bool;

    /**
     * @brief Gets the path of the FIFO.
     *
     * @return std::string_view The path of the FIFO.
     */
    [[nodiscard]] auto getPath() const noexcept -> std::string_view;

    /**
     * @brief Closes the FIFO.
     *
     * This method closes the FIFO and releases any associated resources.
     * It is good practice to call this when you are done using the FIFO
     * to ensure proper cleanup.
     */
    void close() noexcept;

private:
    struct Impl;  ///< Forward declaration of the implementation details.
    std::unique_ptr<Impl> m_impl;  ///< Pointer to the implementation, using
                                   ///< PImpl idiom for encapsulation.
};

// Template implementation
template <WritableData T>
auto FifoClient::write(const T& data,
                       std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<std::size_t, std::error_code> {
    return write(std::string_view(reinterpret_cast<const char*>(data.data()),
                                  data.size()),
                 timeout);
}

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_FIFOCLIENT_HPP