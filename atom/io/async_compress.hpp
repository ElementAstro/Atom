#ifndef ASYNC_COMPRESS_HPP
#define ASYNC_COMPRESS_HPP

#include <zlib.h>
#include <array>
#include <atomic>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

#include <spdlog/spdlog.h>
#include <asio.hpp>

namespace fs = std::filesystem;
#ifdef _WIN32
#include <windows.h>
using StreamHandle = asio::windows::stream_handle;
#else
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

constexpr std::size_t CHUNK = 32768;

/**
 * @brief Base class for compression operations.
 */
class BaseCompressor {
public:
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

    asio::io_context& io_context_;          ///< The ASIO I/O context.
    StreamHandle output_stream_;            ///< The output stream handle.
    std::array<char, CHUNK> out_buffer_{};  ///< Buffer for compressed data.
    z_stream zlib_stream_{};                ///< Zlib stream for compression.
    bool is_initialized_ = false;  ///< Flag to track initialization status.
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
    std::array<char, CHUNK> in_buffer_{};  ///< Buffer for input data.
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
    std::array<char, CHUNK> in_buffer_{};    ///< Buffer for input data.
    std::size_t total_bytes_processed_ = 0;  ///< Total bytes processed.
};

/**
 * @brief Base class for decompression operations.
 */
class BaseDecompressor {
public:
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

    asio::io_context& io_context_;         ///< The ASIO I/O context.
    StreamHandle* out_stream_{};           ///< The output stream handle.
    std::array<char, CHUNK> in_buffer_{};  ///< Buffer for input data.
    gzFile in_file_{};                     ///< The input gzFile.
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
}  // namespace atom::async::io

#endif  // ASYNC_COMPRESS_HPP
