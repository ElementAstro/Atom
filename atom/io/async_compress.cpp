#include "async_compress.hpp"

#include "atom/log/loguru.hpp"

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

// BaseCompressor implementation
BaseCompressor::BaseCompressor(asio::io_context& io_context,
                               const fs::path& output_file)
    : io_context_(io_context), output_stream_(io_context) {
    LOG_F(INFO, "BaseCompressor constructor called with output_file: {}",
          output_file.string());

    try {
        // Validate output path
        if (output_file.empty()) {
            throw std::invalid_argument("Output file path cannot be empty");
        }

        // Create parent directories if they don't exist
        if (!output_file.parent_path().empty() &&
            !fs::exists(output_file.parent_path())) {
            fs::create_directories(output_file.parent_path());
        }

        openOutputFile(output_file);

        zlib_stream_.zalloc = Z_NULL;
        zlib_stream_.zfree = Z_NULL;
        zlib_stream_.opaque = Z_NULL;

        int result = deflateInit2(&zlib_stream_, Z_DEFAULT_COMPRESSION,
                                  Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
        if (result != Z_OK) {
            LOG_F(ERROR, "Failed to initialize zlib: error code {}", result);
            throw std::runtime_error("Failed to initialize zlib");
        }

        is_initialized_ = true;
        LOG_F(INFO, "BaseCompressor initialized successfully");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "BaseCompressor initialization failed: {}", e.what());
        throw;
    }
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
        LOG_F(ERROR, "Exception during BaseCompressor destruction");
    }
}

void BaseCompressor::openOutputFile(const fs::path& output_file) {
    LOG_F(INFO, "Opening output file: {}", output_file.string());

    try {
#ifdef _WIN32
        HANDLE fileHandle =
            CreateFile(output_file.string().c_str(), GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fileHandle == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            LOG_F(ERROR, "Failed to open output file: {} (Error code: {})",
                  output_file.string(), error);
            throw std::runtime_error("Failed to open output file");
        }
        output_stream_.assign(fileHandle);
#else
        int file_descriptor = ::open(output_file.string().c_str(),
                                     O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_descriptor == -1) {
            LOG_F(ERROR, "Failed to open output file: {} (Error: {})",
                  output_file.string(), strerror(errno));
            throw std::runtime_error("Failed to open output file");
        }
        output_stream_.assign(file_descriptor);
#endif
        LOG_F(INFO, "Output file opened successfully: {}",
              output_file.string());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception opening output file: {}", e.what());
        throw;
    }
}

void BaseCompressor::doCompress() {
    LOG_F(INFO, "Starting compression");

    try {
        zlib_stream_.avail_out = out_buffer_.size();
        zlib_stream_.next_out = reinterpret_cast<Bytef*>(out_buffer_.data());

        int ret = deflate(&zlib_stream_, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            LOG_F(ERROR, "Zlib stream error during compression");
            throw std::runtime_error("Zlib stream error");
        }

        std::size_t bytesToWrite = out_buffer_.size() - zlib_stream_.avail_out;
        LOG_F(INFO, "Writing {} bytes to output file", bytesToWrite);

        asio::async_write(
            output_stream_, asio::buffer(out_buffer_, bytesToWrite),
            [this](std::error_code ec, std::size_t /*bytes_written*/) {
                try {
                    if (!ec) {
                        LOG_F(INFO, "Write to output file successful");
                        if (zlib_stream_.avail_in > 0) {
                            doCompress();
                        } else {
                            onAfterWrite();
                        }
                    } else {
                        LOG_F(ERROR, "Error during file write: {}",
                              ec.message());
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in write callback: {}", e.what());
                }
            });
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during compression: {}", e.what());
    }
}

void BaseCompressor::finishCompression() {
    LOG_F(INFO, "Finishing compression");

    try {
        zlib_stream_.avail_in = 0;
        zlib_stream_.next_in = Z_NULL;

        int ret;
        do {
            zlib_stream_.avail_out = out_buffer_.size();
            zlib_stream_.next_out =
                reinterpret_cast<Bytef*>(out_buffer_.data());
            ret = deflate(&zlib_stream_, Z_FINISH);

            if (ret == Z_STREAM_ERROR) {
                LOG_F(ERROR, "Zlib stream error during finish compression");
                throw std::runtime_error("Zlib stream error");
            }

            std::size_t bytesToWrite =
                out_buffer_.size() - zlib_stream_.avail_out;
            if (bytesToWrite == 0)
                continue;

            LOG_F(INFO, "Writing {} bytes to output file during finish",
                  bytesToWrite);

            // Use std::shared_ptr for self-reference to prevent premature
            // destruction
            auto self =
                std::shared_ptr<BaseCompressor>(this, [](BaseCompressor*) {});

            asio::async_write(
                output_stream_, asio::buffer(out_buffer_, bytesToWrite),
                [this, ret, self](std::error_code ec,
                                  std::size_t /*bytes_written*/) {
                    try {
                        if (!ec && ret == Z_FINISH) {
                            deflateEnd(&zlib_stream_);
                            is_initialized_ = false;
                            LOG_F(INFO, "Compression finished successfully");
                        } else if (ec) {
                            LOG_F(ERROR, "Error during file write: {}",
                                  ec.message());
                        }
                    } catch (const std::exception& e) {
                        LOG_F(ERROR,
                              "Exception in finish compression callback: {}",
                              e.what());
                    }
                });

        } while (ret != Z_STREAM_END);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during finish compression: {}", e.what());
    }
}

// SingleFileCompressor implementation
SingleFileCompressor::SingleFileCompressor(asio::io_context& io_context,
                                           const fs::path& input_file,
                                           const fs::path& output_file)
    : BaseCompressor(io_context, output_file), input_stream_(io_context) {
    LOG_F(INFO,
          "SingleFileCompressor constructor called with input_file: {}, "
          "output_file: {}",
          input_file.string(), output_file.string());

    try {
        // Validate input file
        if (!fs::exists(input_file)) {
            throw std::invalid_argument("Input file does not exist: " +
                                        input_file.string());
        }

        if (!fs::is_regular_file(input_file)) {
            throw std::invalid_argument("Input is not a regular file: " +
                                        input_file.string());
        }

        openInputFile(input_file);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "SingleFileCompressor initialization failed: {}",
              e.what());
        throw;
    }
}

void SingleFileCompressor::start() {
    LOG_F(INFO, "Starting SingleFileCompressor");
    try {
        doRead();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in SingleFileCompressor start: {}", e.what());
    }
}

void SingleFileCompressor::openInputFile(const fs::path& input_file) {
    LOG_F(INFO, "Opening input file: {}", input_file.string());

    try {
#ifdef _WIN32
        HANDLE fileHandle =
            CreateFile(input_file.string().c_str(), GENERIC_READ, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fileHandle == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            LOG_F(ERROR, "Failed to open input file: {} (Error code: {})",
                  input_file.string(), error);
            throw std::runtime_error("Failed to open input file");
        }
        input_stream_.assign(fileHandle);
#else
        int file_descriptor = ::open(input_file.string().c_str(), O_RDONLY);
        if (file_descriptor == -1) {
            LOG_F(ERROR, "Failed to open input file: {} (Error: {})",
                  input_file.string(), strerror(errno));
            throw std::runtime_error("Failed to open input file");
        }
        input_stream_.assign(file_descriptor);
#endif
        LOG_F(INFO, "Input file opened successfully: {}", input_file.string());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception opening input file: {}", e.what());
        throw;
    }
}

void SingleFileCompressor::doRead() {
    LOG_F(INFO, "Starting to read from input file");

    try {
        input_stream_.async_read_some(
            asio::buffer(in_buffer_),
            [this](std::error_code ec, std::size_t bytes_transferred) {
                try {
                    if (!ec) {
                        LOG_F(INFO, "Read {} bytes from input file",
                              bytes_transferred);
                        zlib_stream_.avail_in = bytes_transferred;
                        zlib_stream_.next_in =
                            reinterpret_cast<Bytef*>(in_buffer_.data());
                        doCompress();
                    } else {
                        if (ec != asio::error::eof) {
                            LOG_F(ERROR, "Error during file read: {}",
                                  ec.message());
                        }
                        finishCompression();
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in read callback: {}", e.what());
                }
            });
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during read: {}", e.what());
    }
}

void SingleFileCompressor::onAfterWrite() {
    LOG_F(INFO, "SingleFileCompressor onAfterWrite called");
    try {
        doRead();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in onAfterWrite: {}", e.what());
    }
}

// DirectoryCompressor implementation
DirectoryCompressor::DirectoryCompressor(asio::io_context& io_context,
                                         fs::path input_dir,
                                         const fs::path& output_file)
    : BaseCompressor(io_context, output_file),
      input_dir_(std::move(input_dir)) {
    LOG_F(INFO,
          "DirectoryCompressor constructor called with input_dir: {}, "
          "output_file: {}",
          input_dir_.string(), output_file.string());

    try {
        // Validate input directory
        if (!fs::exists(input_dir_)) {
            throw std::invalid_argument("Input directory does not exist: " +
                                        input_dir_.string());
        }

        if (!fs::is_directory(input_dir_)) {
            throw std::invalid_argument("Input is not a directory: " +
                                        input_dir_.string());
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "DirectoryCompressor initialization failed: {}", e.what());
        throw;
    }
}

void DirectoryCompressor::start() {
    LOG_F(INFO, "Starting DirectoryCompressor");

    try {
        // Use C++20 ranges for file collection
        files_to_compress_.clear();
        total_bytes_processed_ = 0;

        // First collect all entries to avoid recursion issues with parallelism
        std::vector<fs::path> all_entries;

        // Use C++20 ranges view for directory traversal
        if (fs::exists(input_dir_) && fs::is_directory(input_dir_)) {
            for (const auto& entry :
                 fs::recursive_directory_iterator(input_dir_)) {
                all_entries.push_back(entry.path());
            }
        } else {
            LOG_F(ERROR,
                  "Input directory does not exist or is not a directory: {}",
                  input_dir_.string());
            return;
        }

        // Use C++20 parallel algorithms to process entries
        std::mutex file_list_mutex;
        std::for_each(std::execution::par, all_entries.begin(),
                      all_entries.end(), [&](const fs::path& path) {
                          if (fs::is_regular_file(path)) {
                              std::lock_guard<std::mutex> lock(file_list_mutex);
                              files_to_compress_.push_back(path);
                              LOG_F(INFO, "Added file to compress: {}",
                                    path.string());
                          }
                      });

        if (!files_to_compress_.empty()) {
            // Sort files by size for better compression performance (smaller
            // files first)
            std::sort(std::execution::par, files_to_compress_.begin(),
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
            LOG_F(WARNING, "No files to compress in directory: {}",
                  input_dir_.string());
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in DirectoryCompressor start: {}", e.what());
    }
}

void DirectoryCompressor::doCompressNextFile() {
    LOG_F(INFO, "Starting compression of next file");

    try {
        if (files_to_compress_.empty()) {
            LOG_F(INFO, "No more files to compress, finishing compression");
            LOG_F(INFO, "Total bytes processed: {}", total_bytes_processed_);
            finishCompression();
            return;
        }

        current_file_ = files_to_compress_.back();
        files_to_compress_.pop_back();
        LOG_F(INFO, "Compressing file: {}", current_file_.string());

        if (!fs::exists(current_file_) || !fs::is_regular_file(current_file_)) {
            LOG_F(ERROR, "File does not exist or is not a regular file: {}",
                  current_file_.string());
            doCompressNextFile();
            return;
        }

        input_stream_.open(current_file_, std::ios::binary);
        if (!input_stream_) {
            LOG_F(ERROR, "Failed to open file: {}", current_file_.string());
            doCompressNextFile();
            return;
        }

        doRead();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in doCompressNextFile: {}", e.what());
        // Continue with next file on error
        if (input_stream_.is_open()) {
            input_stream_.close();
        }
        doCompressNextFile();
    }
}

void DirectoryCompressor::doRead() {
    LOG_F(INFO, "Starting to read from file: {}", current_file_.string());

    try {
        input_stream_.read(in_buffer_.data(), in_buffer_.size());
        auto bytesRead = input_stream_.gcount();
        if (bytesRead > 0) {
            total_bytes_processed_ += bytesRead;
            LOG_F(INFO, "Read {} bytes from file: {}", bytesRead,
                  current_file_.string());
            zlib_stream_.avail_in = bytesRead;
            zlib_stream_.next_in = reinterpret_cast<Bytef*>(in_buffer_.data());
            doCompress();
        } else {
            LOG_F(INFO, "Finished reading file: {}", current_file_.string());
            input_stream_.close();
            doCompressNextFile();
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in doRead: {}", e.what());
        if (input_stream_.is_open()) {
            input_stream_.close();
        }
        doCompressNextFile();
    }
}

void DirectoryCompressor::onAfterWrite() {
    LOG_F(INFO, "DirectoryCompressor onAfterWrite called");
    try {
        doRead();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in onAfterWrite: {}", e.what());
    }
}

// BaseDecompressor implementation
BaseDecompressor::BaseDecompressor(asio::io_context& io_context) noexcept
    : io_context_(io_context) {
    LOG_F(INFO, "BaseDecompressor constructor called");
}

void BaseDecompressor::decompress(gzFile source, StreamHandle& output_stream) {
    LOG_F(INFO, "BaseDecompressor::decompress called");

    try {
        if (!source) {
            LOG_F(ERROR, "Invalid source gzFile");
            throw std::invalid_argument("Invalid source gzFile");
        }

        in_file_ = source;
        out_stream_ = &output_stream;
        doRead();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in decompress: {}", e.what());
        throw;
    }
}

void BaseDecompressor::doRead() {
    LOG_F(INFO, "BaseDecompressor::doRead called");

    try {
        int read_result =
            gzread(in_file_, in_buffer_.data(), in_buffer_.size());
        if (read_result > 0) {
            std::size_t bytesTransferred =
                static_cast<std::size_t>(read_result);
            LOG_F(INFO, "Read {} bytes from compressed file", bytesTransferred);

            // Use std::shared_ptr for self-reference to prevent premature
            // destruction
            auto self = std::shared_ptr<BaseDecompressor>(
                this, [](BaseDecompressor*) {});

            asio::async_write(
                *out_stream_, asio::buffer(in_buffer_, bytesTransferred),
                [this, self](std::error_code ec,
                             std::size_t /*bytes_written*/) {
                    try {
                        if (!ec) {
                            LOG_F(INFO, "Write to output stream successful");
                            doRead();
                        } else {
                            LOG_F(ERROR, "Error during file write: {}",
                                  ec.message());
                            done();
                        }
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Exception in write callback: {}",
                              e.what());
                        done();
                    }
                });
        } else {
            if (read_result < 0) {
                LOG_F(ERROR, "Error during file read");
            }
            gzclose(in_file_);
            done();
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in doRead: {}", e.what());
        gzclose(in_file_);
        done();
    }
}

// SingleFileDecompressor implementation
SingleFileDecompressor::SingleFileDecompressor(asio::io_context& io_context,
                                               fs::path input_file,
                                               fs::path output_folder)
    : BaseDecompressor(io_context),
      input_file_(std::move(input_file)),
      output_folder_(std::move(output_folder)),
      output_stream_(io_context) {
    LOG_F(INFO,
          "SingleFileDecompressor constructor called with input_file: {}, "
          "output_folder: {}",
          input_file_.string(), output_folder_.string());

    try {
        // Validate paths
        if (input_file_.empty()) {
            throw std::invalid_argument("Input file path cannot be empty");
        }

        if (output_folder_.empty()) {
            throw std::invalid_argument("Output folder path cannot be empty");
        }

        // Create output directory if it doesn't exist
        if (!fs::exists(output_folder_)) {
            fs::create_directories(output_folder_);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "SingleFileDecompressor initialization failed: {}",
              e.what());
        throw;
    }
}

void SingleFileDecompressor::start() {
    LOG_F(INFO, "SingleFileDecompressor::start called");

    try {
        if (!fs::exists(input_file_)) {
            LOG_F(ERROR, "Input file does not exist: {}", input_file_.string());
            return;
        }

        fs::path outputFilePath =
            output_folder_ / input_file_.filename().stem().concat(".out");

        // Create parent directories if needed
        if (!outputFilePath.parent_path().empty() &&
            !fs::exists(outputFilePath.parent_path())) {
            fs::create_directories(outputFilePath.parent_path());
        }

        gzFile inputHandle = gzopen(input_file_.string().c_str(), "rb");
        if (inputHandle == nullptr) {
            LOG_F(ERROR, "Failed to open compressed file: {}",
                  input_file_.string());
            return;
        }

#ifdef _WIN32
        HANDLE file_handle =
            CreateFile(outputFilePath.string().c_str(), GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file_handle == INVALID_HANDLE_VALUE) {
            gzclose(inputHandle);
            LOG_F(ERROR, "Failed to create decompressed file: {}",
                  outputFilePath.string());
            return;
        }
        output_stream_.assign(file_handle);
#else
        int file_descriptor = ::open(outputFilePath.string().c_str(),
                                     O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_descriptor == -1) {
            gzclose(inputHandle);
            LOG_F(ERROR, "Failed to create decompressed file: {}",
                  outputFilePath.string());
            return;
        }
        output_stream_.assign(file_descriptor);
#endif

        decompress(inputHandle, output_stream_);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in SingleFileDecompressor::start: {}",
              e.what());
    }
}

void SingleFileDecompressor::done() {
    LOG_F(INFO, "SingleFileDecompressor::done called");

    try {
        if (output_stream_.is_open()) {
            output_stream_.close();
        }
        LOG_F(INFO, "Decompressed file successfully: {}", input_file_.string());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in SingleFileDecompressor::done: {}", e.what());
    }
}

// DirectoryDecompressor implementation
DirectoryDecompressor::DirectoryDecompressor(asio::io_context& io_context,
                                             const fs::path& input_dir,
                                             const fs::path& output_folder)
    : BaseDecompressor(io_context),
      input_dir_(input_dir),
      output_folder_(output_folder),
      output_stream_(io_context) {
    LOG_F(INFO,
          "DirectoryDecompressor constructor called with input_dir: {}, "
          "output_folder: {}",
          input_dir_.string(), output_folder_.string());

    try {
        // Validate paths
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

        // Create output directory if it doesn't exist
        if (!fs::exists(output_folder_)) {
            fs::create_directories(output_folder_);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "DirectoryDecompressor initialization failed: {}",
              e.what());
        throw;
    }
}

void DirectoryDecompressor::start() {
    LOG_F(INFO, "DirectoryDecompressor::start called");

    try {
        // Use C++20 ranges to collect files to decompress
        files_to_decompress_.clear();

        auto is_regular_file = [](const fs::path& p) {
            return fs::is_regular_file(p);
        };

        // Use std::ranges to filter and collect paths
        for (const auto& entry : fs::recursive_directory_iterator(input_dir_)) {
            // Check if it's a regular file using ranges
            if (is_regular_file(entry.path())) {
                files_to_decompress_.push_back(entry.path());
                LOG_F(INFO, "Added file to decompress: {}",
                      entry.path().string());
            }
        }

        // Use parallel sorting for optimized processing order
        if (!files_to_decompress_.empty()) {
            std::sort(std::execution::par, files_to_decompress_.begin(),
                      files_to_decompress_.end(),
                      [](const fs::path& a, const fs::path& b) {
                          return a.filename() < b.filename();
                      });

            decompressNextFile();
        } else {
            LOG_F(WARNING, "No files to decompress in directory: {}",
                  input_dir_.string());
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in DirectoryDecompressor::start: {}", e.what());
    }
}

void DirectoryDecompressor::decompressNextFile() {
    LOG_F(INFO, "DirectoryDecompressor::decompressNextFile called");

    try {
        if (files_to_decompress_.empty()) {
            LOG_F(INFO, "All files decompressed successfully.");
            return;
        }

        current_file_ = files_to_decompress_.back();
        files_to_decompress_.pop_back();
        LOG_F(INFO, "Decompressing file: {}", current_file_.string());

        // Create output path with preserved directory structure
        fs::path relative_path = fs::relative(current_file_, input_dir_);
        fs::path outputFilePath =
            output_folder_ / relative_path.parent_path() /
            current_file_.filename().stem().concat(".out");

        // Create parent directories if needed
        if (!outputFilePath.parent_path().empty() &&
            !fs::exists(outputFilePath.parent_path())) {
            fs::create_directories(outputFilePath.parent_path());
        }

        gzFile inputHandle = gzopen(current_file_.string().c_str(), "rb");
        if (inputHandle == nullptr) {
            LOG_F(ERROR, "Failed to open compressed file: {}",
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
            LOG_F(ERROR, "Failed to create decompressed file: {}",
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
            LOG_F(ERROR, "Failed to create decompressed file: {}",
                  outputFilePath.string());
            decompressNextFile();
            return;
        }
        output_stream_.assign(file_descriptor);
#endif

        decompress(inputHandle, output_stream_);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in decompressNextFile: {}", e.what());
        decompressNextFile();  // Continue with next file
    }
}

void DirectoryDecompressor::done() {
    LOG_F(INFO, "DirectoryDecompressor::done called");

    try {
        output_stream_.close();
        LOG_F(INFO, "Decompressed file successfully: {}",
              current_file_.string());
        decompressNextFile();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in DirectoryDecompressor::done: {}", e.what());
        decompressNextFile();  // Continue with next file
    }
}

// ListFilesInZip implementation
ListFilesInZip::ListFilesInZip(asio::io_context& io_context,
                               std::string_view zip_file)
    : io_context_(io_context), zip_file_(zip_file) {
    LOG_F(INFO, "ListFilesInZip constructor called with zip_file: {}",
          zip_file);

    // Validate input
    if (zip_file.empty()) {
        LOG_F(ERROR, "ZIP file path cannot be empty");
        throw std::invalid_argument("ZIP file path cannot be empty");
    }
}

void ListFilesInZip::start() {
    LOG_F(INFO, "ListFilesInZip::start called");

    try {
        // Use std::make_shared for better memory safety
        auto result = std::make_shared<std::future<void>>(
            std::async(std::launch::async, &ListFilesInZip::listFiles, this));

        io_context_.post([result = std::move(result)]() mutable {
            try {
                result->get();
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception during ZIP file listing: {}", e.what());
            }
        });
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in ListFilesInZip::start: {}", e.what());
    }
}

std::vector<std::string> ListFilesInZip::getFileList() const noexcept {
    LOG_F(INFO, "ListFilesInZip::getFileList called");
    // Thread-safe access to fileList_
    std::lock_guard lock(fileListMutex_);
    return fileList_;
}

void ListFilesInZip::listFiles() {
    LOG_F(INFO, "ListFilesInZip::listFiles called");

    try {
        if (!fs::exists(zip_file_)) {
            LOG_F(ERROR, "ZIP file does not exist: {}", zip_file_);
            return;
        }

        unzFile zipReader = unzOpen(zip_file_.data());
        if (zipReader == nullptr) {
            LOG_F(ERROR, "Failed to open ZIP file: {}", zip_file_);
            return;
        }

        // Use RAII to ensure zipReader is closed
        auto zipCloser = [](unzFile z) { unzClose(z); };
        std::unique_ptr<void, decltype(zipCloser)> zipReaderGuard(zipReader,
                                                                  zipCloser);

        if (unzGoToFirstFile(zipReader) != UNZ_OK) {
            LOG_F(ERROR, "Failed to read first file in ZIP: {}", zip_file_);
            return;
        }

        std::vector<std::string> tempFileList;

        do {
            std::array<char, 256> filename;
            unz_file_info fileInfo;
            if (unzGetCurrentFileInfo(zipReader, &fileInfo, filename.data(),
                                      filename.size(), nullptr, 0, nullptr,
                                      0) != UNZ_OK) {
                LOG_F(ERROR, "Failed to get file info in ZIP: {}", zip_file_);
                return;
            }
            tempFileList.emplace_back(filename.data());
            LOG_F(INFO, "Found file in ZIP: {}", filename.data());
        } while (unzGoToNextFile(zipReader) != UNZ_END_OF_LIST_OF_FILE);

        // Update the file list in a thread-safe manner
        {
            std::lock_guard<std::mutex> lock(fileListMutex_);
            fileList_ = std::move(tempFileList);
        }

        LOG_F(INFO, "ListFilesInZip::listFiles completed");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in listFiles: {}", e.what());
    }
}

// FileExistsInZip implementation
FileExistsInZip::FileExistsInZip(asio::io_context& io_context,
                                 std::string_view zip_file,
                                 std::string_view file_name)
    : io_context_(io_context), zip_file_(zip_file), file_name_(file_name) {
    LOG_F(INFO,
          "FileExistsInZip constructor called with zip_file: {}, file_name: {}",
          zip_file, file_name);

    // Validate input
    if (zip_file.empty()) {
        LOG_F(ERROR, "ZIP file path cannot be empty");
        throw std::invalid_argument("ZIP file path cannot be empty");
    }

    if (file_name.empty()) {
        LOG_F(ERROR, "File name cannot be empty");
        throw std::invalid_argument("File name cannot be empty");
    }
}

void FileExistsInZip::start() {
    LOG_F(INFO, "FileExistsInZip::start called");

    try {
        // Use std::make_shared for better memory safety
        auto result = std::make_shared<std::future<void>>(std::async(
            std::launch::async, &FileExistsInZip::checkFileExists, this));

        io_context_.post([result = std::move(result)]() mutable {
            try {
                result->get();
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception during file existence check: {}",
                      e.what());
            }
        });
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in FileExistsInZip::start: {}", e.what());
    }
}

bool FileExistsInZip::found() const noexcept {
    LOG_F(INFO, "FileExistsInZip::found called, returning: {}",
          fileExists_.load());
    return fileExists_.load(std::memory_order_acquire);
}

void FileExistsInZip::checkFileExists() {
    LOG_F(INFO, "FileExistsInZip::checkFileExists called");

    try {
        if (!fs::exists(zip_file_)) {
            LOG_F(ERROR, "ZIP file does not exist: {}", zip_file_);
            return;
        }

        unzFile zipReader = unzOpen(zip_file_.data());
        if (zipReader == nullptr) {
            LOG_F(ERROR, "Failed to open ZIP file: {}", zip_file_);
            return;
        }

        // Use RAII to ensure zipReader is closed
        auto zipCloser = [](unzFile z) { unzClose(z); };
        std::unique_ptr<void, decltype(zipCloser)> zipReaderGuard(zipReader,
                                                                  zipCloser);

        bool exists =
            (unzLocateFile(zipReader, file_name_.data(), 0) == UNZ_OK);
        fileExists_.store(exists, std::memory_order_release);

        if (exists) {
            LOG_F(INFO, "File found in ZIP: {}", file_name_);
        } else {
            LOG_F(WARNING, "File not found in ZIP: {}", file_name_);
        }

        LOG_F(INFO, "FileExistsInZip::checkFileExists completed");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in checkFileExists: {}", e.what());
    }
}

// RemoveFileFromZip implementation
RemoveFileFromZip::RemoveFileFromZip(asio::io_context& io_context,
                                     std::string_view zip_file,
                                     std::string_view file_name)
    : io_context_(io_context), zip_file_(zip_file), file_name_(file_name) {
    LOG_F(
        INFO,
        "RemoveFileFromZip constructor called with zip_file: {}, file_name: {}",
        zip_file, file_name);

    // Validate input
    if (zip_file.empty()) {
        LOG_F(ERROR, "ZIP file path cannot be empty");
        throw std::invalid_argument("ZIP file path cannot be empty");
    }

    if (file_name.empty()) {
        LOG_F(ERROR, "File name cannot be empty");
        throw std::invalid_argument("File name cannot be empty");
    }
}

void RemoveFileFromZip::start() {
    LOG_F(INFO, "RemoveFileFromZip::start called");

    try {
        // Use std::make_shared for better memory safety
        auto result = std::make_shared<std::future<void>>(std::async(
            std::launch::async, &RemoveFileFromZip::removeFile, this));

        io_context_.post([result = std::move(result)]() mutable {
            try {
                result->get();
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception during file removal from ZIP: {}",
                      e.what());
            }
        });
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in RemoveFileFromZip::start: {}", e.what());
    }
}

bool RemoveFileFromZip::isSuccessful() const noexcept {
    LOG_F(INFO, "RemoveFileFromZip::isSuccessful called, returning: {}",
          success_.load());
    return success_.load(std::memory_order_acquire);
}

void RemoveFileFromZip::removeFile() {
    LOG_F(INFO, "RemoveFileFromZip::removeFile called");

    try {
        if (!fs::exists(zip_file_)) {
            LOG_F(ERROR, "ZIP file does not exist: {}", zip_file_);
            return;
        }

        unzFile zipReader = unzOpen(zip_file_.data());
        if (zipReader == nullptr) {
            LOG_F(ERROR, "Failed to open ZIP file: {}", zip_file_);
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
            LOG_F(ERROR, "File not found in ZIP: {}", file_name_);
            return;
        }

        std::string tempZipFile = std::string(zip_file_) + ".tmp";
        zipFile zipWriter = zipOpen(tempZipFile.c_str(), APPEND_STATUS_CREATE);
        if (zipWriter == nullptr) {
            LOG_F(ERROR, "Failed to create temporary ZIP file: {}",
                  tempZipFile);
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
            LOG_F(ERROR, "Failed to read first file in ZIP: {}", zip_file_);
            return;
        }

        // Process files using C++20 features
        std::vector<std::pair<std::string, std::vector<char>>> files_data;

        do {
            std::array<char, 256> filename{};
            unz_file_info fileInfo;
            if (unzGetCurrentFileInfo(zipReader, &fileInfo, filename.data(),
                                      filename.size(), nullptr, 0, nullptr,
                                      0) != UNZ_OK) {
                LOG_F(ERROR, "Failed to get file info in ZIP: {}", zip_file_);
                return;
            }

            std::string current_filename = filename.data();
            if (file_name_ == current_filename) {
                LOG_F(INFO, "Skipping file: {} for removal", current_filename);
                continue;
            }

            if (unzOpenCurrentFile(zipReader) != UNZ_OK) {
                LOG_F(ERROR, "Failed to open file in ZIP: {}",
                      current_filename);
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
                LOG_F(ERROR, "Failed to add file to temporary ZIP: {}",
                      filename);
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
            LOG_F(INFO, "RemoveFileFromZip::removeFile completed successfully");
        } catch (const fs::filesystem_error& e) {
            LOG_F(ERROR, "Filesystem error during file replacement: {}",
                  e.what());
            if (fs::exists(tempZipFile)) {
                fs::remove(tempZipFile);
            }
            return;
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in removeFile: {}", e.what());
    }
}

// GetZipFileSize implementation
GetZipFileSize::GetZipFileSize(asio::io_context& io_context,
                               std::string_view zip_file)
    : io_context_(io_context), zip_file_(zip_file) {
    LOG_F(INFO, "GetZipFileSize constructor called with zip_file: {}",
          zip_file);

    // Validate input
    if (zip_file.empty()) {
        LOG_F(ERROR, "ZIP file path cannot be empty");
        throw std::invalid_argument("ZIP file path cannot be empty");
    }
}

void GetZipFileSize::start() {
    LOG_F(INFO, "GetZipFileSize::start called");

    try {
        // Use std::make_shared for better memory safety
        auto result = std::make_shared<std::future<void>>(
            std::async(std::launch::async, &GetZipFileSize::getSize, this));

        io_context_.post([result = std::move(result)]() mutable {
            try {
                result->get();
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception during ZIP file size retrieval: {}",
                      e.what());
            }
        });
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in GetZipFileSize::start: {}", e.what());
    }
}

size_t GetZipFileSize::getSizeValue() const noexcept {
    LOG_F(INFO, "GetZipFileSize::getSizeValue called, returning: {}",
          size_.load());
    return size_.load(std::memory_order_acquire);
}

void GetZipFileSize::getSize() {
    LOG_F(INFO, "GetZipFileSize::getSize called");

    try {
        if (!fs::exists(zip_file_)) {
            LOG_F(ERROR, "ZIP file does not exist: {}", zip_file_);
            return;
        }

        // Use C++17 filesystem for efficient file size retrieval
        auto file_size = fs::file_size(zip_file_);
        size_.store(file_size, std::memory_order_release);
        LOG_F(INFO, "GetZipFileSize::getSize completed, size: {}", file_size);
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Filesystem error during size retrieval: {}", e.what());

        // Fall back to traditional method if filesystem API fails
        try {
            std::ifstream inputFile(zip_file_.data(),
                                    std::ifstream::ate | std::ifstream::binary);
            if (!inputFile) {
                LOG_F(ERROR, "Failed to open ZIP file to get size: {}",
                      zip_file_);
                return;
            }
            auto file_size = static_cast<size_t>(inputFile.tellg());
            size_.store(file_size, std::memory_order_release);
            LOG_F(
                INFO,
                "GetZipFileSize::getSize completed (fallback method), size: {}",
                file_size);
        } catch (const std::exception& nested_e) {
            LOG_F(ERROR, "Exception in fallback size retrieval: {}",
                  nested_e.what());
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getSize: {}", e.what());
    }
}

}  // namespace atom::async::io
