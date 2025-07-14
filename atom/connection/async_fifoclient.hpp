#ifndef ATOM_CONNECTION_ASYNC_FIFOCLIENT_HPP
#define ATOM_CONNECTION_ASYNC_FIFOCLIENT_HPP

#include <asio.hpp>
#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <asio/windows/stream_handle.hpp>
#else
#include <asio/posix/stream_descriptor.hpp>
#endif

namespace atom::async::connection {

/**
 * @brief A high-performance, thread-safe client for FIFO (Named Pipe)
 * communication.
 *
 * This class provides a modern C++ interface for asynchronous I/O operations on
 * a FIFO, utilizing advanced concurrency primitives for robust and scalable
 * performance on multicore systems. It is suitable for high-throughput,
 * low-latency messaging.
 */
class FifoClient {
public:
  /**
   * @brief Default constructor.
   */
  FifoClient();

  /**
   * @brief Constructs a FifoClient and opens the specified FIFO path.
   * @param fifoPath The filesystem path to the FIFO.
   * @throws std::runtime_error if the FIFO cannot be opened.
   */
  explicit FifoClient(std::string_view fifoPath);

  /**
   * @brief Destroys the FifoClient, closes the FIFO, and cleans up resources.
   */
  ~FifoClient();

  FifoClient(const FifoClient &) = delete;
  auto operator=(const FifoClient &) -> FifoClient & = delete;
  FifoClient(FifoClient &&) noexcept = default;
  auto operator=(FifoClient &&) noexcept -> FifoClient & = default;

  /**
   * @brief Opens the FIFO at the specified path.
   * @param fifoPath The filesystem path to the FIFO.
   * @throws std::runtime_error if the FIFO is already open or cannot be opened.
   */
  void open(std::string_view fifoPath);

  /**
   * @brief Asynchronously writes data to the FIFO.
   * @param data The data to write.
   * @param timeout An optional timeout for the write operation.
   * @return A future that will be true if the write was successful, false
   * otherwise.
   */
  auto write(std::string_view data,
             std::optional<std::chrono::milliseconds> timeout = std::nullopt)
      -> std::future<bool>;

  /**
   * @brief Synchronously writes data to the FIFO.
   * @param data The data to write.
   * @param timeout An optional timeout for the write operation.
   * @return true if the write was successful, false otherwise.
   */
  auto writeSync(std::string_view data,
                 std::optional<std::chrono::milliseconds> timeout = std::nullopt)
      -> bool;

  /**
   * @brief Asynchronously reads data from the FIFO.
   * @param timeout An optional timeout for the read operation.
   * @return A future that will contain the read data, or be empty on timeout
   * or error.
   */
  auto read(std::optional<std::chrono::milliseconds> timeout = std::nullopt)
      -> std::future<std::optional<std::string>>;

  /**
   * @brief Synchronously reads data from the FIFO.
   * @param timeout An optional timeout for the read operation.
   * @return An optional string containing the read data.
   */
  auto readSync(std::optional<std::chrono::milliseconds> timeout = std::nullopt)
      -> std::optional<std::string>;

  /**
   * @brief Checks if the FIFO is currently open and valid.
   * @return true if the FIFO is open, false otherwise.
   */
  [[nodiscard]] auto isOpen() const -> bool;

  /**
   * @brief Closes the FIFO connection.
   */
  void close();

  /**
   * @brief Cancels all pending asynchronous operations.
   */
  void cancel();

  /**
   * @brief Gets the path of the FIFO.
   * @return The path of the FIFO.
   */
  [[nodiscard]] auto getPath() const -> std::string;

private:
  struct Impl;
  std::unique_ptr<Impl> pimpl_;
};

} // namespace atom::async::connection

#endif // ATOM_CONNECTION_ASYNC_FIFOCLIENT_HPP
