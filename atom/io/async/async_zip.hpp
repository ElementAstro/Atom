#ifndef ATOM_IO_ASYNC_ASYNC_ZIP_HPP
#define ATOM_IO_ASYNC_ASYNC_ZIP_HPP

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>
#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

namespace atom::io::async {

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

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_ZIP_HPP
