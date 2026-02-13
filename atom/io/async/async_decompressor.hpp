#ifndef ATOM_IO_ASYNC_ASYNC_DECOMPRESSOR_HPP
#define ATOM_IO_ASYNC_ASYNC_DECOMPRESSOR_HPP

#include <zlib.h>
#include <array>
#include <atomic>
#include <filesystem>
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

constexpr std::size_t DECOMPRESS_CHUNK = 32768;

/**
 * @brief Base class for decompression operations.
 */
class BaseDecompressor {
public:
    using CompletionHandler =
        std::function<void(const std::error_code&, std::size_t)>;

    /**
     * @brief Constructs a BaseDecompressor.
     * @param io_context The ASIO I/O context.
     */
    explicit BaseDecompressor(asio::io_context& io_context) noexcept;

    virtual ~BaseDecompressor() noexcept = default;

    /**
     * @brief Starts the decompression process.
     */
    virtual void start() = 0;

protected:
    void setCompletionHandler(CompletionHandler handler);
    void notifyCompletion(const std::error_code& ec = {},
                          std::size_t bytes = 0);

    /**
     * @brief Decompresses data from the source file to the output stream.
     * @param source The source gzFile.
     * @param output_stream The output stream handle.
     */
    void decompress(gzFile source, StreamHandle& output_stream);

    /**
     * @brief Reads data from the source file.
     */
    void doRead();

    /**
     * @brief Called when decompression is done.
     */
    virtual void done() = 0;

    asio::io_context& io_context_;                      ///< The ASIO I/O context.
    StreamHandle* out_stream_{};                         ///< The output stream handle.
    std::array<char, DECOMPRESS_CHUNK> in_buffer_{};    ///< Buffer for input data.
    gzFile in_file_{};                                  ///< The input gzFile.
    CompletionHandler completion_handler_{};
    std::atomic<bool> completion_notified_{false};
};

/**
 * @brief Decompressor for single files.
 */
class SingleFileDecompressor : public BaseDecompressor {
public:
    /**
     * @brief Constructs a SingleFileDecompressor.
     * @param io_context The ASIO I/O context.
     * @param input_file The path to the input file.
     * @param output_folder The path to the output folder.
     */
    SingleFileDecompressor(asio::io_context& io_context, fs::path input_file,
                           fs::path output_folder);

    ~SingleFileDecompressor() override = default;

    /**
     * @brief Starts the decompression process.
     */
    void start() override;

    void start(CompletionHandler handler) {
        setCompletionHandler(std::move(handler));
        start();
    }

private:
    /**
     * @brief Called when decompression is done.
     */
    void done() override;

    fs::path input_file_;         ///< The input file path.
    fs::path output_folder_;      ///< The output folder path.
    StreamHandle output_stream_;  ///< The output stream handle.
};

/**
 * @brief Decompressor for directories.
 */
class DirectoryDecompressor : public BaseDecompressor {
public:
    /**
     * @brief Constructs a DirectoryDecompressor.
     * @param io_context The ASIO I/O context.
     * @param input_dir The path to the input directory.
     * @param output_folder The path to the output folder.
     */
    DirectoryDecompressor(asio::io_context& io_context,
                          const fs::path& input_dir,
                          const fs::path& output_folder);

    ~DirectoryDecompressor() override = default;
    /**
     * @brief Starts the decompression process.
     */
    void start() override;

    void start(CompletionHandler handler) {
        setCompletionHandler(std::move(handler));
        start();
    }

private:
    /**
     * @brief Decompresses the next file in the directory.
     */
    void decompressNextFile();

    /**
     * @brief Called when decompression is done.
     */
    void done() override;

    fs::path input_dir_;          ///< The input directory path.
    fs::path output_folder_;      ///< The output folder path.
    StreamHandle output_stream_;  ///< The output stream handle.
    std::vector<fs::path>
        files_to_decompress_;  ///< List of files to decompress.
    fs::path current_file_;    ///< The current file being decompressed.
};

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_DECOMPRESSOR_HPP
