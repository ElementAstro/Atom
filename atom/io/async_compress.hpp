#ifndef ASYNC_COMPRESS_HPP
#define ASYNC_COMPRESS_HPP

#include <zlib.h>
#include <atomic>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <limits>

#include <spdlog/spdlog.h>
#include <asio.hpp>

#ifdef _WIN32
#include <asio/windows/stream_handle.hpp>
using StreamHandle = asio::windows::stream_handle;
#else
#include <asio/posix/stream_descriptor.hpp>
#include <fcntl.h>
using StreamHandle = asio::posix::stream_descriptor;
#endif

namespace atom::async::io {

template <typename T>
concept FileSizeType = std::integral<T> || std::floating_point<T>;

template <typename T>
concept PathLike = requires(T t) {
    { std::filesystem::path(t) } -> std::same_as<std::filesystem::path>;
};

// Configuration constants with better defaults
constexpr std::size_t DEFAULT_CHUNK_SIZE = 65536;  // 64KB - better for modern systems
constexpr std::size_t MIN_CHUNK_SIZE = 4096;       // 4KB minimum
constexpr std::size_t MAX_CHUNK_SIZE = 1048576;    // 1MB maximum

// Forward declarations for callback types
namespace fs = std::filesystem;

// File filter callback type for selective compression
using FileFilterCallback = std::function<bool(const fs::path& file_path, std::size_t file_size)>;

// Compression configuration structure
struct CompressionConfig {
    std::size_t chunk_size = DEFAULT_CHUNK_SIZE;
    int compression_level = Z_DEFAULT_COMPRESSION;  // More balanced default
    bool enable_progress_reporting = false;
    std::size_t progress_update_interval = 1024 * 1024;  // Update every 1MB
    bool enable_statistics = true;
    bool use_memory_mapping = false;  // For large files
    std::size_t memory_mapping_threshold = 100 * 1024 * 1024;  // 100MB

    // Advanced features
    bool enable_parallel_compression = false;  // Parallel compression for large files
    std::size_t parallel_threshold = 50 * 1024 * 1024;  // 50MB threshold for parallel
    std::size_t max_parallel_chunks = 4;  // Maximum parallel chunks
    bool enable_integrity_check = true;  // Verify compressed data integrity
    bool enable_resume = false;  // Support for resuming interrupted operations
    std::string resume_file_suffix = ".resume";  // Suffix for resume files

    // File filtering
    FileFilterCallback file_filter;  // Custom file filter for selective compression
    std::vector<std::string> exclude_extensions = {".tmp", ".log"};  // Extensions to exclude
    std::vector<std::string> include_extensions;  // If not empty, only include these extensions
    std::size_t min_file_size = 0;  // Minimum file size to compress
    std::size_t max_file_size = std::numeric_limits<std::size_t>::max();  // Maximum file size

    // Performance tuning
    bool use_buffer_pool = true;  // Use buffer pooling for better performance
    std::size_t io_thread_count = 1;  // Number of I/O threads for parallel operations
    bool enable_compression_cache = false;  // Cache compression results for identical files
};

// Compression statistics
struct CompressionStats {
    std::size_t bytes_processed = 0;
    std::size_t bytes_compressed = 0;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    double compression_ratio = 0.0;
    double throughput_mbps = 0.0;

    void updateRatio() {
        if (bytes_processed > 0) {
            compression_ratio = static_cast<double>(bytes_processed) / bytes_compressed;
        }
    }

    void updateThroughput() {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        if (duration > 0) {
            throughput_mbps = (static_cast<double>(bytes_processed) / (1024 * 1024)) /
                             (duration / 1000.0);
        }
    }
};

// Progress callback type
using ProgressCallback = std::function<void(std::size_t bytes_processed, std::size_t total_bytes, double percentage)>;

// Completion callback type
using CompletionCallback = std::function<void(const std::error_code& ec, const CompressionStats& stats)>;

// Error callback type for detailed error reporting
using ErrorCallback = std::function<void(const std::string& operation, const std::error_code& ec, const std::string& details)>;

/**
 * @brief Base class for compression operations.
 */
class BaseCompressor {
public:
    /**
     * @brief Constructs a BaseCompressor.
     * @param io_context The ASIO I/O context.
     * @param output_file The path to the output file.
     * @param config Compression configuration.
     * @throws std::runtime_error If initialization fails.
     */
    BaseCompressor(asio::io_context& io_context, const fs::path& output_file,
                   const CompressionConfig& config = {});

    virtual ~BaseCompressor() noexcept;

    /**
     * @brief Starts the compression process.
     */
    virtual void start() = 0;

    /**
     * @brief Cancels the compression process.
     */
    virtual void cancel();

    /**
     * @brief Sets progress callback.
     * @param callback The progress callback function.
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Sets completion callback.
     * @param callback The completion callback function.
     */
    void setCompletionCallback(CompletionCallback callback);

    /**
     * @brief Gets current compression statistics.
     * @return Current compression statistics.
     */
    [[nodiscard]] const CompressionStats& getStats() const noexcept;

protected:
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

    /**
     * @brief Updates progress and calls progress callback if set.
     * @param bytes_processed Number of bytes processed.
     */
    void updateProgress(std::size_t bytes_processed);

    /**
     * @brief Calls completion callback with final statistics.
     * @param ec Error code from operation.
     */
    void notifyCompletion(const std::error_code& ec);

    asio::io_context& io_context_;          ///< The ASIO I/O context.
    StreamHandle output_stream_;            ///< The output stream handle.
    std::vector<char> out_buffer_;          ///< Dynamic buffer for compressed data.
    z_stream zlib_stream_{};                ///< Zlib stream for compression.
    bool is_initialized_ = false;           ///< Flag to track initialization status.
    std::atomic<bool> cancelled_ = false;   ///< Cancellation flag.

    CompressionConfig config_;              ///< Compression configuration.
    CompressionStats stats_;                ///< Compression statistics.
    ProgressCallback progress_callback_;    ///< Progress callback.
    CompletionCallback completion_callback_; ///< Completion callback.
    std::size_t total_size_estimate_ = 0;   ///< Estimated total size for progress.
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
     * @param config Compression configuration.
     * @throws std::runtime_error If initialization fails.
     */
    SingleFileCompressor(asio::io_context& io_context,
                         const fs::path& input_file,
                         const fs::path& output_file,
                         const CompressionConfig& config = {});

    /**
     * @brief Starts the compression process.
     */
    void start() override;

    /**
     * @brief Cancels the compression process.
     */
    void cancel() override;

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

    StreamHandle input_stream_;            ///< The input stream handle.
    std::vector<char> in_buffer_;          ///< Dynamic buffer for input data.
    fs::path input_file_;                  ///< Input file path for reference.
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
     * @param config Compression configuration.
     * @throws std::runtime_error If initialization fails.
     */
    DirectoryCompressor(asio::io_context& io_context, fs::path input_dir,
                        const fs::path& output_file,
                        const CompressionConfig& config = {});

    /**
     * @brief Starts the compression process.
     */
    void start() override;

    /**
     * @brief Cancels the compression process.
     */
    void cancel() override;

private:
    /**
     * @brief Asynchronously scans directory for files to compress.
     */
    void scanDirectoryAsync();

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
    fs::path current_file_;                    ///< The current file being compressed.
    std::ifstream input_stream_;               ///< Input stream for the current file.
    std::vector<char> in_buffer_;              ///< Dynamic buffer for input data.
    std::size_t total_bytes_processed_ = 0;    ///< Total bytes processed.
    std::size_t current_file_index_ = 0;       ///< Current file index for progress.
};

/**
 * @brief Streaming compressor for real-time data compression.
 */
class StreamingCompressor : public BaseCompressor {
public:
    /**
     * @brief Constructs a StreamingCompressor.
     * @param io_context The ASIO I/O context.
     * @param output_file The path to the output file.
     * @param config Compression configuration.
     */
    StreamingCompressor(asio::io_context& io_context,
                       const fs::path& output_file,
                       const CompressionConfig& config = {});

    /**
     * @brief Starts the streaming compression process.
     */
    void start() override;

    /**
     * @brief Compresses data chunk asynchronously.
     * @param data The data to compress.
     * @param callback Callback called when compression is complete.
     */
    void compressChunk(const std::vector<char>& data,
                      std::function<void(const std::error_code&)> callback);

    /**
     * @brief Finishes the streaming compression.
     */
    void finish();

    /**
     * @brief Cancels the streaming compression.
     */
    void cancel() override;

private:
    struct PendingChunk {
        std::vector<char> data;
        std::function<void(const std::error_code&)> callback;
    };

    void onAfterWrite() override;
    void processNextChunk();

    std::queue<PendingChunk> pending_chunks_;
    std::mutex chunks_mutex_;
    bool is_processing_ = false;
    bool is_finished_ = false;
};

/**
 * @brief Parallel compressor for large files using multiple threads.
 */
class ParallelCompressor {
public:
    /**
     * @brief Constructs a ParallelCompressor.
     * @param io_context The ASIO I/O context.
     * @param input_file The path to the input file.
     * @param output_file The path to the output file.
     * @param config Compression configuration.
     */
    ParallelCompressor(asio::io_context& io_context,
                      const fs::path& input_file,
                      const fs::path& output_file,
                      const CompressionConfig& config = {});

    /**
     * @brief Starts the parallel compression process.
     */
    void start();

    /**
     * @brief Cancels the parallel compression.
     */
    void cancel();

    /**
     * @brief Sets progress callback.
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Sets completion callback.
     */
    void setCompletionCallback(CompletionCallback callback);

private:
    struct ChunkInfo {
        std::size_t offset;
        std::size_t size;
        std::size_t chunk_id;
    };

    void processChunk(const ChunkInfo& chunk);
    void mergeCompressedChunks();

    asio::io_context& io_context_;
    fs::path input_file_;
    fs::path output_file_;
    CompressionConfig config_;
    std::vector<ChunkInfo> chunks_;
    std::atomic<std::size_t> completed_chunks_{0};
    std::atomic<bool> cancelled_{false};
    ProgressCallback progress_callback_;
    CompletionCallback completion_callback_;
    CompressionStats stats_;
};

/**
 * @brief Base class for decompression operations.
 */
class BaseDecompressor {
public:
    /**
     * @brief Constructs a BaseDecompressor.
     * @param io_context The ASIO I/O context.
     * @param config Decompression configuration.
     */
    explicit BaseDecompressor(asio::io_context& io_context,
                             const CompressionConfig& config = {}) noexcept;

    virtual ~BaseDecompressor() noexcept = default;

    /**
     * @brief Starts the decompression process.
     */
    virtual void start() = 0;

    /**
     * @brief Cancels the decompression process.
     */
    virtual void cancel();

    /**
     * @brief Sets progress callback.
     * @param callback The progress callback function.
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Sets completion callback.
     * @param callback The completion callback function.
     */
    void setCompletionCallback(CompletionCallback callback);

    /**
     * @brief Gets current decompression statistics.
     * @return Current decompression statistics.
     */
    [[nodiscard]] const CompressionStats& getStats() const noexcept;

protected:
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

    /**
     * @brief Updates progress and calls progress callback if set.
     * @param bytes_processed Number of bytes processed.
     */
    void updateProgress(std::size_t bytes_processed);

    /**
     * @brief Calls completion callback with final statistics.
     * @param ec Error code from operation.
     */
    void notifyCompletion(const std::error_code& ec);

    asio::io_context& io_context_;         ///< The ASIO I/O context.
    StreamHandle* out_stream_{};           ///< The output stream handle.
    std::vector<char> in_buffer_;          ///< Dynamic buffer for input data.
    gzFile in_file_{};                     ///< The input gzFile.
    std::atomic<bool> cancelled_ = false;  ///< Cancellation flag.

    CompressionConfig config_;             ///< Decompression configuration.
    CompressionStats stats_;               ///< Decompression statistics.
    ProgressCallback progress_callback_;   ///< Progress callback.
    CompletionCallback completion_callback_; ///< Completion callback.
    std::size_t total_size_estimate_ = 0;  ///< Estimated total size for progress.
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
     * @param config Decompression configuration.
     */
    SingleFileDecompressor(asio::io_context& io_context, fs::path input_file,
                           fs::path output_folder,
                           const CompressionConfig& config = {});

    ~SingleFileDecompressor() override = default;

    /**
     * @brief Starts the decompression process.
     */
    void start() override;

    /**
     * @brief Cancels the decompression process.
     */
    void cancel() override;

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
     * @param config Decompression configuration.
     */
    DirectoryDecompressor(asio::io_context& io_context,
                          const fs::path& input_dir,
                          const fs::path& output_folder,
                          const CompressionConfig& config = {});

    ~DirectoryDecompressor() override = default;

    /**
     * @brief Starts the decompression process.
     */
    void start() override;

    /**
     * @brief Cancels the decompression process.
     */
    void cancel() override;

private:
    /**
     * @brief Asynchronously scans directory for files to decompress.
     */
    void scanDirectoryAsync();

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
        files_to_decompress_;     ///< List of files to decompress.
    fs::path current_file_;       ///< The current file being decompressed.
    std::size_t current_file_index_ = 0; ///< Current file index for progress.
};

class ZipOperation {
public:
    virtual ~ZipOperation() noexcept = default;
    virtual void start() = 0;
};

/**
 * @brief Lists files in a ZIP archive.
 */
class ListFilesInZip : public ZipOperation {
public:
    /**
     * @brief Constructs a ListFilesInZip.
     * @param io_context The ASIO I/O context.
     * @param zip_file The path to the ZIP file.
     * @throws std::invalid_argument If zip_file is empty.
     */
    ListFilesInZip(asio::io_context& io_context, std::string_view zip_file);

    /**
     * @brief Starts the ZIP operation.
     */
    void start() override;

    /**
     * @brief Gets the list of files in the ZIP archive.
     * @return A vector of file names.
     */
    [[nodiscard]] auto getFileList() const noexcept -> std::vector<std::string>;

private:
    /**
     * @brief Lists the files in the ZIP archive.
     */
    void listFiles();

    asio::io_context& io_context_;       ///< The ASIO I/O context.
    std::string zip_file_;               ///< The path to the ZIP file.
    std::vector<std::string> fileList_;  ///< List of files in the ZIP archive.
    mutable std::mutex
        fileListMutex_;  ///< Mutex for thread-safe access to fileList_
};

/**
 * @brief Checks if a file exists in a ZIP archive.
 */
class FileExistsInZip : public ZipOperation {
public:
    /**
     * @brief Constructs a FileExistsInZip.
     * @param io_context The ASIO I/O context.
     * @param zip_file The path to the ZIP file.
     * @param file_name The name of the file to check.
     * @throws std::invalid_argument If zip_file or file_name is empty.
     */
    FileExistsInZip(asio::io_context& io_context, std::string_view zip_file,
                    std::string_view file_name);
    ~FileExistsInZip() override = default;

    /**
     * @brief Starts the ZIP operation.
     */
    void start() override;

    /**
     * @brief Checks if the file was found in the ZIP archive.
     * @return True if the file was found, false otherwise.
     */
    [[nodiscard]] auto found() const noexcept -> bool;

private:
    /**
     * @brief Checks if the file exists in the ZIP archive.
     */
    void checkFileExists();

    asio::io_context& io_context_;  ///< The ASIO I/O context.
    std::string zip_file_;          ///< The path to the ZIP file.
    std::string file_name_;         ///< The name of the file to check.
    std::atomic<bool> fileExists_ =
        false;  ///< Whether the file exists in the ZIP archive.
};

/**
 * @brief Removes a file from a ZIP archive.
 */
class RemoveFileFromZip : public ZipOperation {
public:
    /**
     * @brief Constructs a RemoveFileFromZip.
     * @param io_context The ASIO I/O context.
     * @param zip_file The path to the ZIP file.
     * @param file_name The name of the file to remove.
     * @throws std::invalid_argument If zip_file or file_name is empty.
     */
    RemoveFileFromZip(asio::io_context& io_context, std::string_view zip_file,
                      std::string_view file_name);

    /**
     * @brief Starts the ZIP operation.
     */
    void start() override;

    /**
     * @brief Checks if the file removal was successful.
     * @return True if the file was successfully removed, false otherwise.
     */
    [[nodiscard]] auto isSuccessful() const noexcept -> bool;

private:
    /**
     * @brief Removes the file from the ZIP archive.
     */
    void removeFile();

    asio::io_context& io_context_;  ///< The ASIO I/O context.
    std::string zip_file_;          ///< The path to the ZIP file.
    std::string file_name_;         ///< The name of the file to remove.
    std::atomic<bool> success_ =
        false;  ///< Whether the file removal was successful.
};

/**
 * @brief Gets the size of a ZIP file.
 */
class GetZipFileSize : public ZipOperation {
public:
    /**
     * @brief Constructs a GetZipFileSize.
     * @param io_context The ASIO I/O context.
     * @param zip_file The path to the ZIP file.
     * @throws std::invalid_argument If zip_file is empty.
     */
    GetZipFileSize(asio::io_context& io_context, std::string_view zip_file);

    /**
     * @brief Starts the ZIP operation.
     */
    void start() override;

    /**
     * @brief Gets the size of the ZIP file.
     * @return The size of the ZIP file.
     */
    [[nodiscard]] auto getSizeValue() const noexcept -> size_t;

private:
    /**
     * @brief Gets the size of the ZIP file.
     */
    void getSize();

    asio::io_context& io_context_;  ///< The ASIO I/O context.
    std::string zip_file_;          ///< The path to the ZIP file.
    std::atomic<size_t> size_ = 0;  ///< The size of the ZIP file.
};

// Memory pool for efficient buffer management
class BufferPool {
public:
    static BufferPool& getInstance();

    std::vector<char> getBuffer(std::size_t size);
    void returnBuffer(std::vector<char>&& buffer);

private:
    BufferPool() = default;
    std::mutex mutex_;
    std::unordered_map<std::size_t, std::vector<std::vector<char>>> pools_;
};

// Compression format detection utility
enum class CompressionFormat {
    UNKNOWN,
    GZIP,
    ZLIB,
    ZIP
};

class FormatDetector {
public:
    static CompressionFormat detectFormat(const fs::path& file_path);
    static CompressionFormat detectFormat(const std::vector<char>& data);

private:
    static bool isGzipFormat(const std::vector<char>& header);
    static bool isZlibFormat(const std::vector<char>& header);
    static bool isZipFormat(const std::vector<char>& header);
};

// Factory functions for easier object creation
namespace factory {

/**
 * @brief Creates a single file compressor with optimal configuration.
 */
std::unique_ptr<SingleFileCompressor> createFileCompressor(
    asio::io_context& io_context,
    const fs::path& input_file,
    const fs::path& output_file,
    const CompressionConfig& config = {});

/**
 * @brief Creates a directory compressor with optimal configuration.
 */
std::unique_ptr<DirectoryCompressor> createDirectoryCompressor(
    asio::io_context& io_context,
    const fs::path& input_dir,
    const fs::path& output_file,
    const CompressionConfig& config = {});

/**
 * @brief Creates a single file decompressor with optimal configuration.
 */
std::unique_ptr<SingleFileDecompressor> createFileDecompressor(
    asio::io_context& io_context,
    const fs::path& input_file,
    const fs::path& output_folder,
    const CompressionConfig& config = {});

/**
 * @brief Creates a directory decompressor with optimal configuration.
 */
std::unique_ptr<DirectoryDecompressor> createDirectoryDecompressor(
    asio::io_context& io_context,
    const fs::path& input_dir,
    const fs::path& output_folder,
    const CompressionConfig& config = {});

} // namespace factory

// Utility functions for common operations
namespace utils {

/**
 * @brief Estimates the total size of files in a directory.
 */
std::size_t estimateDirectorySize(const fs::path& directory);

/**
 * @brief Validates compression configuration.
 */
bool validateConfig(const CompressionConfig& config);

/**
 * @brief Gets optimal chunk size based on file size.
 */
std::size_t getOptimalChunkSize(std::size_t file_size);

/**
 * @brief Creates a default configuration optimized for the given file size.
 */
CompressionConfig createOptimalConfig(std::size_t file_size);

} // namespace utils

}  // namespace atom::async::io

#endif  // ASYNC_COMPRESS_HPP
