#ifndef ATOM_IO_ASYNC_ASYNC_COMPRESSOR_HPP
#define ATOM_IO_ASYNC_ASYNC_COMPRESSOR_HPP

#include <zlib.h>
#include <array>
#include <atomic>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <functional>
#include <system_error>
#include <vector>

#include <spdlog/spdlog.h>
#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

namespace fs = std::filesystem;
#ifdef _WIN32
#include <windows.h>
using StreamHandle = asio::windows::stream_handle;
#else
#include <fcntl.h>
using StreamHandle = asio::posix::stream_descriptor;
#endif

namespace atom::io::async {

constexpr std::size_t COMPRESS_CHUNK = 32768;

/**
 * @brief Base class for compression operations.
 */
class BaseCompressor {
public:
    using CompletionHandler =
        std::function<void(const std::error_code&, std::size_t)>;

    /**
     * @brief Constructs a BaseCompressor.
     * @param io_context The ASIO I/O context.
     * @param output_file The path to the output file.
     * @throws std::runtime_error If initialization fails.
     */
    BaseCompressor(asio::io_context& io_context, const fs::path& output_file);

    virtual ~BaseCompressor() noexcept;

    /**
     * @brief Starts the compression process.
     */
    virtual void start() = 0;

protected:
    void setCompletionHandler(CompletionHandler handler);
    void notifyCompletion(const std::error_code& ec = {},
                          std::size_t bytes = 0);

    /**
     * @brief Opens the output file for writing.
     * @param output_file The path to the output file.
     * @throws std::runtime_error If file opening fails.
     */
    void openOutputFile(const fs::path& output_file);

    /**
     * @brief Performs the compression operation.
     */
    void doCompress();

    /**
     * @brief Called after writing data to the output file.
     */
    virtual void onAfterWrite() = 0;

    /**
     * @brief Finishes the compression process.
     */
    void finishCompression();

    asio::io_context& io_context_;                    ///< The ASIO I/O context.
    StreamHandle output_stream_;                      ///< The output stream handle.
    std::array<char, COMPRESS_CHUNK> out_buffer_{};   ///< Buffer for compressed data.
    z_stream zlib_stream_{};                          ///< Zlib stream for compression.
    bool is_initialized_ = false;  ///< Flag to track initialization status.
    CompletionHandler completion_handler_{};
    std::atomic<bool> completion_notified_{false};
};

/**
 * @brief Compressor for single files.
 */
class SingleFileCompressor : public BaseCompressor {
public:
    /**
     * @brief Constructs a SingleFileCompressor.
     * @param io_context The ASIO I/O context.
     * @param input_file The path to the input file.
     * @param output_file The path to the output file.
     * @throws std::runtime_error If initialization fails.
     */
    SingleFileCompressor(asio::io_context& io_context,
                         const fs::path& input_file,
                         const fs::path& output_file);

    /**
     * @brief Starts the compression process.
     */
    void start() override;

    void start(CompletionHandler handler) {
        setCompletionHandler(std::move(handler));
        start();
    }

private:
    /**
     * @brief Opens the input file for reading.
     * @param input_file The path to the input file.
     * @throws std::runtime_error If file opening fails.
     */
    void openInputFile(const fs::path& input_file);

    /**
     * @brief Reads data from the input file.
     */
    void doRead();

    /**
     * @brief Called after writing data to the output file.
     */
    void onAfterWrite() override;

    StreamHandle input_stream_;                      ///< The input stream handle.
    std::array<char, COMPRESS_CHUNK> in_buffer_{};   ///< Buffer for input data.
};

/**
 * @brief Compressor for directories.
 */
class DirectoryCompressor : public BaseCompressor {
public:
    /**
     * @brief Constructs a DirectoryCompressor.
     * @param io_context The ASIO I/O context.
     * @param input_dir The path to the input directory.
     * @param output_file The path to the output file.
     * @throws std::runtime_error If initialization fails.
     */
    DirectoryCompressor(asio::io_context& io_context, fs::path input_dir,
                        const fs::path& output_file);

    /**
     * @brief Starts the compression process.
     */
    void start() override;

    void start(CompletionHandler handler) {
        setCompletionHandler(std::move(handler));
        start();
    }

private:
    /**
     * @brief Compresses the next file in the directory.
     */
    void doCompressNextFile();

    /**
     * @brief Reads data from the current file.
     */
    void doRead();

    /**
     * @brief Called after writing data to the output file.
     */
    void onAfterWrite() override;

    fs::path input_dir_;                       ///< The input directory path.
    std::vector<fs::path> files_to_compress_;  ///< List of files to compress.
    fs::path current_file_;       ///< The current file being compressed.
    std::ifstream input_stream_;  ///< Input stream for the current file.
    std::array<char, COMPRESS_CHUNK> in_buffer_{};   ///< Buffer for input data.
    std::size_t total_bytes_processed_ = 0;  ///< Total bytes processed.
};

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_COMPRESSOR_HPP
