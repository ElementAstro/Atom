#include "async_compressor.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <execution>
#include <future>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <system_error>

namespace atom::io::async {

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

void BaseCompressor::setCompletionHandler(CompletionHandler handler) {
    completion_handler_ = std::move(handler);
    completion_notified_.store(false);
}

void BaseCompressor::notifyCompletion(const std::error_code& ec,
                                      std::size_t bytes) {
    bool expected = false;
    if (completion_handler_ &&
        completion_notified_.compare_exchange_strong(expected, true)) {
        auto handler = completion_handler_;
        asio::post(io_context_, [handler = std::move(handler), ec,
                                 bytes]() mutable { handler(ec, bytes); });
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
                    notifyCompletion(ec);
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
                    notifyCompletion({});
                } else if (ec) {
                    spdlog::error("Error during file write: {}", ec.message());
                    notifyCompletion(ec);
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

void SingleFileCompressor::start() {
    completion_notified_.store(false);
    doRead();
}

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
                    notifyCompletion(ec);
                    return;
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
    completion_notified_.store(false);
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
        notifyCompletion({});
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

}  // namespace atom::io::async
