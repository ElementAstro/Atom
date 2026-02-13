#ifndef ATOM_IO_ASYNC_ASYNC_BATCH_HPP
#define ATOM_IO_ASYNC_ASYNC_BATCH_HPP

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "async_file.hpp"
#include "async_types.hpp"

namespace atom::io::async {

/**
 * @brief Asynchronous batch file operations
 *
 * Provides efficient parallel batch read/write/delete operations by composing
 * with AsyncFile for individual file operations.
 */
class AsyncBatchOps {
public:
#ifdef ATOM_USE_ASIO
    /**
     * @brief Constructs with ASIO context
     * @param io_context The ASIO I/O context
     * @param context Optional async context for cancellation support
     */
    explicit AsyncBatchOps(
        asio::io_context& io_context,
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#else
    /**
     * @brief Constructs with thread pool
     * @param context Optional async context for cancellation support
     */
    explicit AsyncBatchOps(
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#endif

    /**
     * @brief Efficiently reads multiple files in parallel
     * @param files List of file paths to read
     * @param callback Callback function for the results
     */
    void asyncBatchRead(
        std::span<const std::string> files,
        std::function<void(AsyncResult<std::vector<std::string>>)> callback);

    /**
     * @brief Asynchronously writes multiple files in parallel for optimal
     * performance
     * @param file_data_pairs Pairs of filename and data to write
     * @param callback Callback function for the batch operation result
     */
    void asyncBatchWrite(
        std::span<const std::pair<std::string, std::string>> file_data_pairs,
        std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously deletes multiple files in parallel
     * @param files List of file paths to delete
     * @param callback Callback function for the batch operation result
     */
    void asyncBatchDelete(std::span<const std::string> files,
                          std::function<void(AsyncResult<void>)> callback);

private:
    std::shared_ptr<AsyncFile> file_impl_;
};

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_BATCH_HPP
