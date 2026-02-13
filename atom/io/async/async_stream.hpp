#ifndef ATOM_IO_ASYNC_ASYNC_STREAM_HPP
#define ATOM_IO_ASYNC_ASYNC_STREAM_HPP

#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <string>

#include "async_file.hpp"
#include "async_types.hpp"

namespace atom::io::async {

/**
 * @brief Asynchronous streaming file operations
 *
 * Provides memory-efficient streaming read/write operations by processing
 * files in chunks rather than loading entire contents into memory.
 */
class AsyncStreamOps {
public:
#ifdef ATOM_USE_ASIO
    /**
     * @brief Constructs with ASIO context
     * @param io_context The ASIO I/O context
     * @param context Optional async context for cancellation support
     */
    explicit AsyncStreamOps(
        asio::io_context& io_context,
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#else
    /**
     * @brief Constructs with thread pool
     * @param context Optional async context for cancellation support
     */
    explicit AsyncStreamOps(
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#endif

    /**
     * @brief Asynchronously reads a file in chunks for memory-efficient
     * processing
     * @param filename Path to the file to read
     * @param chunk_size Size of each chunk to read
     * @param chunk_callback Callback for each chunk read
     * @param completion_callback Callback when reading is complete
     */
    template <PathString T>
    void asyncStreamRead(
        T&& filename, size_t chunk_size,
        std::function<void(AsyncResult<std::string>)> chunk_callback,
        std::function<void(AsyncResult<void>)> completion_callback);

    /**
     * @brief Asynchronously writes data to a file in chunks for memory
     * efficiency
     * @param filename Path to the file to write
     * @param data Data to write
     * @param chunk_size Size of each chunk to write
     * @param callback Callback when writing is complete
     */
    template <PathString T>
    void asyncStreamWrite(T&& filename, std::span<const char> data,
                          size_t chunk_size,
                          std::function<void(AsyncResult<void>)> callback);

private:
    std::shared_ptr<AsyncFile> file_impl_;
};

// Template implementations for AsyncStreamOps

template <PathString T>
void AsyncStreamOps::asyncStreamRead(
    T&& filename, size_t chunk_size,
    std::function<void(AsyncResult<std::string>)> chunk_callback,
    std::function<void(AsyncResult<void>)> completion_callback) {
    file_impl_->executeAsync(
        [filename = AsyncFile::toString(std::forward<T>(filename)), chunk_size,
         chunk_callback = std::move(chunk_callback),
         completion_callback =
             std::move(completion_callback)]() mutable {
            try {
                std::ifstream file(filename, std::ios::binary);
                if (!file) {
                    completion_callback(AsyncResult<void>::error_result(
                        "Failed to open file: " + filename));
                    return;
                }

                std::string chunk(chunk_size, '\0');
                while (file.read(chunk.data(), chunk_size) ||
                       file.gcount() > 0) {
                    chunk.resize(file.gcount());
                    chunk_callback(AsyncResult<std::string>::success_result(
                        std::string(chunk)));
                    chunk.resize(chunk_size);
                }

                completion_callback(AsyncResult<void>::success_result());
            } catch (const std::exception& e) {
                completion_callback(AsyncResult<void>::error_result(e.what()));
            }
        });
}

template <PathString T>
void AsyncStreamOps::asyncStreamWrite(
    T&& filename, std::span<const char> data, size_t chunk_size,
    std::function<void(AsyncResult<void>)> callback) {
    file_impl_->executeAsync(
        [filename = AsyncFile::toString(std::forward<T>(filename)),
         data = std::string(data.begin(), data.end()), chunk_size,
         callback = std::move(callback)]() mutable {
            try {
                std::ofstream file(filename, std::ios::binary);
                if (!file) {
                    callback(AsyncResult<void>::error_result(
                        "Failed to open file for writing: " + filename));
                    return;
                }

                size_t written = 0;
                while (written < data.size()) {
                    size_t to_write =
                        std::min(chunk_size, data.size() - written);
                    if (!file.write(data.data() + written, to_write)) {
                        callback(AsyncResult<void>::error_result(
                            "Failed to write chunk to file: " + filename));
                        return;
                    }
                    written += to_write;
                }

                callback(AsyncResult<void>::success_result());
            } catch (const std::exception& e) {
                callback(AsyncResult<void>::error_result(e.what()));
            }
        });
}

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_STREAM_HPP
