#include "async_compress.hpp"

#include <spdlog/spdlog.h>

#include <minizip-ng/mz_compat.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_strm_buf.h>
#include <minizip-ng/mz_strm_mem.h>
#include <minizip-ng/mz_strm_split.h>
#include <minizip-ng/mz_strm_zlib.h>
#include <minizip-ng/mz_zip.h>

#include <algorithm>
#include <execution>
#include <future>
#include <mutex>
#include <ranges>
#include <stdexcept>

namespace atom::async::io {

BaseCompressor::BaseCompressor(asio::io_context& io_context,
                               const fs::path& output_file)
    : io_context_(io_context), output_stream_(io_context) {
    spdlog::info("BaseCompressor constructor with output_file: {}",
                 output_file.string());

    if (output_file.empty()) {
        throw std::invalid_argument("Output file path cannot be empty");
    }

    if (!output_file.parent_path().empty() &&
        !fs::exists(output_file.parent_path())) {
        fs::create_directories(output_file.parent_path());
    }

    openOutputFile(output_file);

    zlib_stream_.zalloc = Z_NULL;
    zlib_stream_.zfree = Z_NULL;
    zlib_stream_.opaque = Z_NULL;

    int result = deflateInit2(&zlib_stream_, Z_BEST_SPEED, Z_DEFLATED, 15 | 16,
                              8, Z_DEFAULT_STRATEGY);
    if (result != Z_OK) {
        spdlog::error("Failed to initialize zlib: error code {}", result);
        throw std::runtime_error("Failed to initialize zlib");
    }

    is_initialized_ = true;
}

BaseCompressor::~BaseCompressor() noexcept {
    try {
        if (is_initialized_) {
            deflateEnd(&zlib_stream_);
        }
        if (output_stream_.is_open()) {
            output_stream_.close();
        }
    } catch (...) {
        spdlog::error("Exception during BaseCompressor destruction");
    }
}

void BaseCompressor::openOutputFile(const fs::path& output_file) {
#ifdef _WIN32
    HANDLE fileHandle =
        CreateFile(output_file.string().c_str(), GENERIC_WRITE, 0, NULL,
                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open output file: {} (Error code: {})",
                      output_file.string(), error);
        throw std::runtime_error("Failed to open output file");
    }
    output_stream_.assign(fileHandle);
#else
    int file_descriptor = ::open(output_file.string().c_str(),
                                 O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_descriptor == -1) {
        spdlog::error("Failed to open output file: {} (Error: {})",
                      output_file.string(), strerror(errno));
        throw std::runtime_error("Failed to open output file");
    }
    output_stream_.assign(file_descriptor);
#endif
}

void BaseCompressor::doCompress() {
    zlib_stream_.avail_out = out_buffer_.size();
    zlib_stream_.next_out = reinterpret_cast<Bytef*>(out_buffer_.data());

    int ret = deflate(&zlib_stream_, Z_NO_FLUSH);
    if (ret == Z_STREAM_ERROR) {
        spdlog::error("Zlib stream error during compression");
        throw std::runtime_error("Zlib stream error");
    }

    std::size_t bytesToWrite = out_buffer_.size() - zlib_stream_.avail_out;
    if (bytesToWrite > 0) {
        asio::async_write(
            output_stream_, asio::buffer(out_buffer_, bytesToWrite),
            [this](std::error_code ec, std::size_t /*bytes_written*/) {
                if (!ec) {
                    if (zlib_stream_.avail_in > 0) {
                        doCompress();
                    } else {
                        onAfterWrite();
                    }
                } else {
                    spdlog::error("Error during file write: {}", ec.message());
                }
            });
    } else {
        onAfterWrite();
    }
}

void BaseCompressor::finishCompression() {
    zlib_stream_.avail_in = 0;
    zlib_stream_.next_in = Z_NULL;

    int ret;
    do {
        zlib_stream_.avail_out = out_buffer_.size();
        zlib_stream_.next_out = reinterpret_cast<Bytef*>(out_buffer_.data());
        ret = deflate(&zlib_stream_, Z_FINISH);

        if (ret == Z_STREAM_ERROR) {
            spdlog::error("Zlib stream error during finish compression");
            throw std::runtime_error("Zlib stream error");
        }

        std::size_t bytesToWrite = out_buffer_.size() - zlib_stream_.avail_out;
        if (bytesToWrite == 0)
            continue;

        auto self =
            std::shared_ptr<BaseCompressor>(this, [](BaseCompressor*) {});
        asio::async_write(
            output_stream_, asio::buffer(out_buffer_, bytesToWrite),
            [this, ret, self](std::error_code ec,
                              std::size_t /*bytes_written*/) {
                if (!ec && ret == Z_STREAM_END) {
                    deflateEnd(&zlib_stream_);
                    is_initialized_ = false;
                    spdlog::info("Compression finished successfully");
                } else if (ec) {
                    spdlog::error("Error during file write: {}", ec.message());
                }
            });

    } while (ret != Z_STREAM_END);
}

SingleFileCompressor::SingleFileCompressor(asio::io_context& io_context,
                                           const fs::path& input_file,
                                           const fs::path& output_file)
    : BaseCompressor(io_context, output_file), input_stream_(io_context) {
    if (!fs::exists(input_file)) {
        throw std::invalid_argument("Input file does not exist: " +
                                    input_file.string());
    }

    if (!fs::is_regular_file(input_file)) {
        throw std::invalid_argument("Input is not a regular file: " +
                                    input_file.string());
    }

    openInputFile(input_file);
}

void SingleFileCompressor::start() { doRead(); }

void SingleFileCompressor::openInputFile(const fs::path& input_file) {
#ifdef _WIN32
    HANDLE fileHandle =
        CreateFile(input_file.string().c_str(), GENERIC_READ, 0, NULL,
                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open input file: {} (Error code: {})",
                      input_file.string(), error);
        throw std::runtime_error("Failed to open input file");
    }
    input_stream_.assign(fileHandle);
#else
    int file_descriptor = ::open(input_file.string().c_str(), O_RDONLY);
    if (file_descriptor == -1) {
        spdlog::error("Failed to open input file: {} (Error: {})",
                      input_file.string(), strerror(errno));
        throw std::runtime_error("Failed to open input file");
    }
    input_stream_.assign(file_descriptor);
#endif
}

void SingleFileCompressor::doRead() {
    input_stream_.async_read_some(
        asio::buffer(in_buffer_),
        [this](std::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                zlib_stream_.avail_in = bytes_transferred;
                zlib_stream_.next_in =
                    reinterpret_cast<Bytef*>(in_buffer_.data());
                doCompress();
            } else {
                if (ec != asio::error::eof) {
                    spdlog::error("Error during file read: {}", ec.message());
                }
                finishCompression();
            }
        });
}

void SingleFileCompressor::onAfterWrite() { doRead(); }

DirectoryCompressor::DirectoryCompressor(asio::io_context& io_context,
                                         fs::path input_dir,
                                         const fs::path& output_file)
    : BaseCompressor(io_context, output_file),
      input_dir_(std::move(input_dir)) {
    if (!fs::exists(input_dir_)) {
        throw std::invalid_argument("Input directory does not exist: " +
                                    input_dir_.string());
    }

    if (!fs::is_directory(input_dir_)) {
        throw std::invalid_argument("Input is not a directory: " +
                                    input_dir_.string());
    }
}

void DirectoryCompressor::start() {
    files_to_compress_.clear();
    files_to_compress_.reserve(1000);
    total_bytes_processed_ = 0;

    std::vector<fs::path> all_entries;
    all_entries.reserve(1000);

    if (fs::exists(input_dir_) && fs::is_directory(input_dir_)) {
        for (const auto& entry : fs::recursive_directory_iterator(input_dir_)) {
            all_entries.push_back(entry.path());
        }
    } else {
        spdlog::error(
            "Input directory does not exist or is not a directory: {}",
            input_dir_.string());
        return;
    }

    std::mutex file_list_mutex;
    std::for_each(std::execution::par_unseq, all_entries.begin(),
                  all_entries.end(), [&](const fs::path& path) {
                      if (fs::is_regular_file(path)) {
                          std::lock_guard<std::mutex> lock(file_list_mutex);
                          files_to_compress_.push_back(path);
                      }
                  });

    if (!files_to_compress_.empty()) {
        std::sort(std::execution::par_unseq, files_to_compress_.begin(),
                  files_to_compress_.end(),
                  [](const fs::path& a, const fs::path& b) {
                      try {
                          return fs::file_size(a) < fs::file_size(b);
                      } catch (...) {
                          return false;
                      }
                  });

        doCompressNextFile();
    } else {
        spdlog::warn("No files to compress in directory: {}",
                     input_dir_.string());
    }
}

void DirectoryCompressor::doCompressNextFile() {
    if (files_to_compress_.empty()) {
        spdlog::info("Total bytes processed: {}", total_bytes_processed_);
        finishCompression();
        return;
    }

    current_file_ = files_to_compress_.back();
    files_to_compress_.pop_back();

    if (!fs::exists(current_file_) || !fs::is_regular_file(current_file_)) {
        spdlog::error("File does not exist or is not a regular file: {}",
                      current_file_.string());
        doCompressNextFile();
        return;
    }

    input_stream_.open(current_file_, std::ios::binary);
    if (!input_stream_) {
        spdlog::error("Failed to open file: {}", current_file_.string());
        doCompressNextFile();
        return;
    }

    doRead();
}

void DirectoryCompressor::doRead() {
    input_stream_.read(in_buffer_.data(), in_buffer_.size());
    auto bytesRead = input_stream_.gcount();
    if (bytesRead > 0) {
        total_bytes_processed_ += bytesRead;
        zlib_stream_.avail_in = bytesRead;
        zlib_stream_.next_in = reinterpret_cast<Bytef*>(in_buffer_.data());
        doCompress();
    } else {
        input_stream_.close();
        doCompressNextFile();
    }
}

void DirectoryCompressor::onAfterWrite() { doRead(); }

BaseDecompressor::BaseDecompressor(asio::io_context& io_context) noexcept
    : io_context_(io_context) {}

void BaseDecompressor::decompress(gzFile source, StreamHandle& output_stream) {
    if (!source) {
        spdlog::error("Invalid source gzFile");
        throw std::invalid_argument("Invalid source gzFile");
    }

    in_file_ = source;
    out_stream_ = &output_stream;
    doRead();
}

void BaseDecompressor::doRead() {
    int read_result = gzread(in_file_, in_buffer_.data(), in_buffer_.size());
    if (read_result > 0) {
        std::size_t bytesTransferred = static_cast<std::size_t>(read_result);

        auto self =
            std::shared_ptr<BaseDecompressor>(this, [](BaseDecompressor*) {});
        asio::async_write(
            *out_stream_, asio::buffer(in_buffer_, bytesTransferred),
            [this, self](std::error_code ec, std::size_t /*bytes_written*/) {
                if (!ec) {
                    doRead();
                } else {
                    spdlog::error("Error during file write: {}", ec.message());
                    done();
                }
            });
    } else {
        if (read_result < 0) {
            spdlog::error("Error during file read");
        }
        gzclose(in_file_);
        done();
    }
}

SingleFileDecompressor::SingleFileDecompressor(asio::io_context& io_context,
                                               fs::path input_file,
                                               fs::path output_folder)
    : BaseDecompressor(io_context),
      input_file_(std::move(input_file)),
      output_folder_(std::move(output_folder)),
      output_stream_(io_context) {
    if (input_file_.empty()) {
        throw std::invalid_argument("Input file path cannot be empty");
    }

    if (output_folder_.empty()) {
        throw std::invalid_argument("Output folder path cannot be empty");
    }

    if (!fs::exists(output_folder_)) {
        fs::create_directories(output_folder_);
    }
}

void SingleFileDecompressor::start() {
    if (!fs::exists(input_file_)) {
        spdlog::error("Input file does not exist: {}", input_file_.string());
        return;
    }

    fs::path outputFilePath =
        output_folder_ / input_file_.filename().stem().concat(".out");

    if (!outputFilePath.parent_path().empty() &&
        !fs::exists(outputFilePath.parent_path())) {
        fs::create_directories(outputFilePath.parent_path());
    }

    gzFile inputHandle = gzopen(input_file_.string().c_str(), "rb");
    if (inputHandle == nullptr) {
        spdlog::error("Failed to open compressed file: {}",
                      input_file_.string());
        return;
    }

#ifdef _WIN32
    HANDLE file_handle =
        CreateFile(outputFilePath.string().c_str(), GENERIC_WRITE, 0, NULL,
                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        gzclose(inputHandle);
        spdlog::error("Failed to create decompressed file: {}",
                      outputFilePath.string());
        return;
    }
    output_stream_.assign(file_handle);
#else
    int file_descriptor = ::open(outputFilePath.string().c_str(),
                                 O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_descriptor == -1) {
        gzclose(inputHandle);
        spdlog::error("Failed to create decompressed file: {}",
                      outputFilePath.string());
        return;
    }
    output_stream_.assign(file_descriptor);
#endif

    decompress(inputHandle, output_stream_);
}

void SingleFileDecompressor::done() {
    if (output_stream_.is_open()) {
        output_stream_.close();
    }
}

DirectoryDecompressor::DirectoryDecompressor(asio::io_context& io_context,
                                             const fs::path& input_dir,
                                             const fs::path& output_folder)
    : BaseDecompressor(io_context),
      input_dir_(input_dir),
      output_folder_(output_folder),
      output_stream_(io_context) {
    if (input_dir_.empty()) {
        throw std::invalid_argument("Input directory path cannot be empty");
    }

    if (!fs::exists(input_dir_) || !fs::is_directory(input_dir_)) {
        throw std::invalid_argument(
            "Input directory does not exist or is not a directory: " +
            input_dir_.string());
    }

    if (output_folder_.empty()) {
        throw std::invalid_argument("Output folder path cannot be empty");
    }

    if (!fs::exists(output_folder_)) {
        fs::create_directories(output_folder_);
    }
}

void DirectoryDecompressor::start() {
    files_to_decompress_.clear();
    files_to_decompress_.reserve(1000);

    for (const auto& entry : fs::recursive_directory_iterator(input_dir_)) {
        if (fs::is_regular_file(entry.path())) {
            files_to_decompress_.push_back(entry.path());
        }
    }

    if (!files_to_decompress_.empty()) {
        std::sort(std::execution::par_unseq, files_to_decompress_.begin(),
                  files_to_decompress_.end(),
                  [](const fs::path& a, const fs::path& b) {
                      return a.filename() < b.filename();
                  });

        decompressNextFile();
    } else {
        spdlog::warn("No files to decompress in directory: {}",
                     input_dir_.string());
    }
}

void DirectoryDecompressor::decompressNextFile() {
    if (files_to_decompress_.empty()) {
        return;
    }

    current_file_ = files_to_decompress_.back();
    files_to_decompress_.pop_back();

    // Create output path with preserved directory structure
    fs::path relative_path = fs::relative(current_file_, input_dir_);
    fs::path outputFilePath = output_folder_ / relative_path.parent_path() /
                              current_file_.filename().stem().concat(".out");

    // Create parent directories if needed
    if (!outputFilePath.parent_path().empty() &&
        !fs::exists(outputFilePath.parent_path())) {
        fs::create_directories(outputFilePath.parent_path());
    }

    gzFile inputHandle = gzopen(current_file_.string().c_str(), "rb");
    if (inputHandle == nullptr) {
        spdlog::error("Failed to open compressed file: {}",
                      current_file_.string());
        decompressNextFile();
        return;
    }

#ifdef _WIN32
    HANDLE fileHandle =
        CreateFile(outputFilePath.string().c_str(), GENERIC_WRITE, 0, NULL,
                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        gzclose(inputHandle);
        spdlog::error("Failed to create decompressed file: {}",
                      outputFilePath.string());
        decompressNextFile();
        return;
    }
    output_stream_.assign(fileHandle);
#else
    int file_descriptor = ::open(outputFilePath.string().c_str(),
                                 O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_descriptor == -1) {
        gzclose(inputHandle);
        spdlog::error("Failed to create decompressed file: {}",
                      outputFilePath.string());
        decompressNextFile();
        return;
    }
    output_stream_.assign(file_descriptor);
#endif

    decompress(inputHandle, output_stream_);
}

void DirectoryDecompressor::done() {
    output_stream_.close();
    decompressNextFile();
}

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

    io_context_.post([result = std::move(result)]() mutable {
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

    io_context_.post([result = std::move(result)]() mutable {
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

    io_context_.post([result = std::move(result)]() mutable {
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

    io_context_.post([result = std::move(result)]() mutable {
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
        auto file_size = static_cast<size_t>(inputFile.tellg());
        size_.store(file_size, std::memory_order_release);
    } catch (const std::exception& nested_e) {
        spdlog::error("Exception in fallback size retrieval: {}",
                      nested_e.what());
    }
}

}  // namespace atom::async::io
