#include "async_zip.hpp"

#include <zlib.h>

#include <spdlog/spdlog.h>

#include <minizip-ng/mz_compat.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_strm_buf.h>
#include <minizip-ng/mz_strm_mem.h>
#include <minizip-ng/mz_strm_split.h>
#include <minizip-ng/mz_strm_zlib.h>
#include <minizip-ng/mz_zip.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace atom::io::async {

ListFilesInZip::ListFilesInZip(asio::io_context& io_context,
                               std::string_view zip_file)
    : io_context_(io_context), zip_file_(zip_file) {
    if (zip_file.empty()) {
        spdlog::error("ZIP file path cannot be empty");
        throw std::invalid_argument("ZIP file path cannot be empty");
    }
}

void ListFilesInZip::start() {
    auto result = std::make_shared<std::future<void>>(
        std::async(std::launch::async, &ListFilesInZip::listFiles, this));

    asio::post(io_context_, [result = std::move(result)]() mutable {
        try {
            result->get();
        } catch (const std::exception& e) {
            spdlog::error("Exception during ZIP file listing: {}", e.what());
        }
    });
}

std::vector<std::string> ListFilesInZip::getFileList() const noexcept {
    std::lock_guard lock(fileListMutex_);
    return fileList_;
}

void ListFilesInZip::listFiles() {
    if (!fs::exists(zip_file_)) {
        spdlog::error("ZIP file does not exist: {}", zip_file_);
        return;
    }

    unzFile zipReader = unzOpen(zip_file_.data());
    if (zipReader == nullptr) {
        spdlog::error("Failed to open ZIP file: {}", zip_file_);
        return;
    }

    auto zipCloser = [](unzFile z) { unzClose(z); };
    std::unique_ptr<void, decltype(zipCloser)> zipReaderGuard(zipReader,
                                                              zipCloser);

    if (unzGoToFirstFile(zipReader) != UNZ_OK) {
        spdlog::error("Failed to read first file in ZIP: {}", zip_file_);
        return;
    }

    std::vector<std::string> tempFileList;
    tempFileList.reserve(100);

    do {
        std::array<char, 256> filename;
        unz_file_info fileInfo;
        if (unzGetCurrentFileInfo(zipReader, &fileInfo, filename.data(),
                                  filename.size(), nullptr, 0, nullptr,
                                  0) != UNZ_OK) {
            spdlog::error("Failed to get file info in ZIP: {}", zip_file_);
            return;
        }
        tempFileList.emplace_back(filename.data());
    } while (unzGoToNextFile(zipReader) != UNZ_END_OF_LIST_OF_FILE);

    {
        std::lock_guard<std::mutex> lock(fileListMutex_);
        fileList_ = std::move(tempFileList);
    }
}

// FileExistsInZip implementation
FileExistsInZip::FileExistsInZip(asio::io_context& io_context,
                                 std::string_view zip_file,
                                 std::string_view file_name)
    : io_context_(io_context), zip_file_(zip_file), file_name_(file_name) {
    if (zip_file.empty()) {
        spdlog::error("ZIP file path cannot be empty");
        throw std::invalid_argument("ZIP file path cannot be empty");
    }

    if (file_name.empty()) {
        spdlog::error("File name cannot be empty");
        throw std::invalid_argument("File name cannot be empty");
    }
}

void FileExistsInZip::start() {
    auto result = std::make_shared<std::future<void>>(std::async(
        std::launch::async, &FileExistsInZip::checkFileExists, this));

    asio::post(io_context_, [result = std::move(result)]() mutable {
        try {
            result->get();
        } catch (const std::exception& e) {
            spdlog::error("Exception during file existence check: {}",
                          e.what());
        }
    });
}

bool FileExistsInZip::found() const noexcept {
    return fileExists_.load(std::memory_order_acquire);
}

void FileExistsInZip::checkFileExists() {
    if (!fs::exists(zip_file_)) {
        spdlog::error("ZIP file does not exist: {}", zip_file_);
        return;
    }

    unzFile zipReader = unzOpen(zip_file_.data());
    if (zipReader == nullptr) {
        spdlog::error("Failed to open ZIP file: {}", zip_file_);
        return;
    }

    auto zipCloser = [](unzFile z) { unzClose(z); };
    std::unique_ptr<void, decltype(zipCloser)> zipReaderGuard(zipReader,
                                                              zipCloser);

    bool exists = (unzLocateFile(zipReader, file_name_.data(), 0) == UNZ_OK);
    fileExists_.store(exists, std::memory_order_release);
}

// RemoveFileFromZip implementation
RemoveFileFromZip::RemoveFileFromZip(asio::io_context& io_context,
                                     std::string_view zip_file,
                                     std::string_view file_name)
    : io_context_(io_context), zip_file_(zip_file), file_name_(file_name) {
    if (zip_file.empty()) {
        spdlog::error("ZIP file path cannot be empty");
        throw std::invalid_argument("ZIP file path cannot be empty");
    }

    if (file_name.empty()) {
        spdlog::error("File name cannot be empty");
        throw std::invalid_argument("File name cannot be empty");
    }
}

void RemoveFileFromZip::start() {
    auto result = std::make_shared<std::future<void>>(
        std::async(std::launch::async, &RemoveFileFromZip::removeFile, this));

    asio::post(io_context_, [result = std::move(result)]() mutable {
        try {
            result->get();
        } catch (const std::exception& e) {
            spdlog::error("Exception during file removal from ZIP: {}",
                          e.what());
        }
    });
}

bool RemoveFileFromZip::isSuccessful() const noexcept {
    return success_.load(std::memory_order_acquire);
}

void RemoveFileFromZip::removeFile() {
    if (!fs::exists(zip_file_)) {
        spdlog::error("ZIP file does not exist: {}", zip_file_);
        return;
    }

    unzFile zipReader = unzOpen(zip_file_.data());
    if (zipReader == nullptr) {
        spdlog::error("Failed to open ZIP file: {}", zip_file_);
        return;
    }

    // Use RAII for the zipReader
    auto zipReaderCloser = [](unzFile z) {
        if (z)
            unzClose(z);
    };
    std::unique_ptr<void, decltype(zipReaderCloser)> zipReaderGuard(
        zipReader, zipReaderCloser);

    if (unzLocateFile(zipReader, file_name_.data(), 0) != UNZ_OK) {
        spdlog::error("File not found in ZIP: {}", file_name_);
        return;
    }

    std::string tempZipFile = std::string(zip_file_) + ".tmp";
    zipFile zipWriter = zipOpen(tempZipFile.c_str(), APPEND_STATUS_CREATE);
    if (zipWriter == nullptr) {
        spdlog::error("Failed to create temporary ZIP file: {}", tempZipFile);
        return;
    }

    // Use RAII for the zipWriter
    auto zipWriterCloser = [](zipFile z) {
        if (z)
            zipClose(z, nullptr);
    };
    std::unique_ptr<void, decltype(zipWriterCloser)> zipWriterGuard(
        zipWriter, zipWriterCloser);

    if (unzGoToFirstFile(zipReader) != UNZ_OK) {
        spdlog::error("Failed to read first file in ZIP: {}", zip_file_);
        return;
    }

    std::vector<std::pair<std::string, std::vector<char>>> files_data;

    do {
        std::array<char, 256> filename{};
        unz_file_info fileInfo;
        if (unzGetCurrentFileInfo(zipReader, &fileInfo, filename.data(),
                                  filename.size(), nullptr, 0, nullptr,
                                  0) != UNZ_OK) {
            spdlog::error("Failed to get file info in ZIP: {}", zip_file_);
            return;
        }

        std::string current_filename = filename.data();
        if (file_name_ == current_filename) {
            continue;
        }

        if (unzOpenCurrentFile(zipReader) != UNZ_OK) {
            spdlog::error("Failed to open file in ZIP: {}", current_filename);
            return;
        }

        // Use RAII for closing the current file
        auto currentFileCloser = [&zipReader](int*) {
            unzCloseCurrentFile(zipReader);
        };
        std::unique_ptr<int, decltype(currentFileCloser)> currentFileGuard(
            new int(0), currentFileCloser);

        // Use a dynamic buffer to efficiently read the file content
        std::vector<char> buffer;
        buffer.resize(1024);
        std::vector<char> file_content;
        int readSize;

        while ((readSize = unzReadCurrentFile(zipReader, buffer.data(),
                                              buffer.size())) > 0) {
            file_content.insert(file_content.end(), buffer.begin(),
                                buffer.begin() + readSize);
        }

        files_data.emplace_back(current_filename, std::move(file_content));

    } while (unzGoToNextFile(zipReader) != UNZ_END_OF_LIST_OF_FILE);

    // Add all files to the new zip (except the one to remove)
    for (const auto& [filename, content] : files_data) {
        zip_fileinfo fileInfoOut = {};

        if (zipOpenNewFileInZip(zipWriter, filename.c_str(), &fileInfoOut,
                                nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                                Z_DEFAULT_COMPRESSION) != ZIP_OK) {
            spdlog::error("Failed to add file to temporary ZIP: {}", filename);
            return;
        }

        // Use RAII for closing the zip file
        auto closeFileInZip = [&zipWriter](int*) {
            zipCloseFileInZip(zipWriter);
        };
        std::unique_ptr<int, decltype(closeFileInZip)> closeFileGuard(
            new int(0), closeFileInZip);

        if (!content.empty()) {
            zipWriteInFileInZip(zipWriter, content.data(), content.size());
        }
    }

    // Close everything explicitly before file operations
    zipReaderGuard.reset();
    zipWriterGuard.reset();

    // Atomic replacement of the original file with the new one
    try {
        if (fs::exists(zip_file_)) {
            fs::remove(zip_file_);
        }
        fs::rename(tempZipFile, zip_file_);
        success_.store(true, std::memory_order_release);
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error during file replacement: {}", e.what());
        if (fs::exists(tempZipFile)) {
            fs::remove(tempZipFile);
        }
        return;
    }
}

GetZipFileSize::GetZipFileSize(asio::io_context& io_context,
                               std::string_view zip_file)
    : io_context_(io_context), zip_file_(zip_file) {
    if (zip_file.empty()) {
        spdlog::error("ZIP file path cannot be empty");
        throw std::invalid_argument("ZIP file path cannot be empty");
    }
}

void GetZipFileSize::start() {
    auto result = std::make_shared<std::future<void>>(
        std::async(std::launch::async, &GetZipFileSize::getSize, this));

    asio::post(io_context_, [result = std::move(result)]() mutable {
        try {
            result->get();
        } catch (const std::exception& e) {
            spdlog::error("Exception during ZIP file size retrieval: {}",
                          e.what());
        }
    });
}

size_t GetZipFileSize::getSizeValue() const noexcept {
    return size_.load(std::memory_order_acquire);
}

void GetZipFileSize::getSize() {
    if (!fs::exists(zip_file_)) {
        spdlog::error("ZIP file does not exist: {}", zip_file_);
        return;
    }

    auto file_size = fs::file_size(zip_file_);
    size_.store(file_size, std::memory_order_release);

    // Fall back to traditional method if filesystem API fails
    try {
        std::ifstream inputFile(zip_file_.data(),
                                std::ifstream::ate | std::ifstream::binary);
        if (!inputFile) {
            spdlog::error("Failed to open ZIP file to get size: {}", zip_file_);
            return;
        }
        auto fallback_size = static_cast<size_t>(inputFile.tellg());
        size_.store(fallback_size, std::memory_order_release);
    } catch (const std::exception& nested_e) {
        spdlog::error("Exception in fallback size retrieval: {}",
                      nested_e.what());
    }
}

}  // namespace atom::io::async
