#include "async_decompressor.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <execution>
#include <stdexcept>
#include <system_error>

namespace atom::io::async {

BaseDecompressor::BaseDecompressor(asio::io_context& io_context) noexcept
    : io_context_(io_context) {}

void BaseDecompressor::setCompletionHandler(CompletionHandler handler) {
    completion_handler_ = std::move(handler);
    completion_notified_.store(false);
}

void BaseDecompressor::notifyCompletion(const std::error_code& ec,
                                        std::size_t bytes) {
    bool expected = false;
    if (completion_handler_ &&
        completion_notified_.compare_exchange_strong(expected, true)) {
        auto handler = completion_handler_;
        asio::post(io_context_, [handler = std::move(handler), ec,
                                 bytes]() mutable { handler(ec, bytes); });
    }
}

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
                    notifyCompletion(ec);
                    done();
                }
            });
    } else {
        if (read_result < 0) {
            spdlog::error("Error during file read");
            notifyCompletion(std::make_error_code(std::errc::io_error));
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
    completion_notified_.store(false);
    if (!fs::exists(input_file_)) {
        spdlog::error("Input file does not exist: {}", input_file_.string());
        notifyCompletion(
            std::make_error_code(std::errc::no_such_file_or_directory));
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
        notifyCompletion(std::make_error_code(std::errc::io_error));
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
        notifyCompletion(std::make_error_code(std::errc::io_error));
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
        notifyCompletion(std::make_error_code(std::errc::io_error));
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
    notifyCompletion({});
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
    completion_notified_.store(false);
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
        notifyCompletion({});
    }
}

void DirectoryDecompressor::decompressNextFile() {
    if (files_to_decompress_.empty()) {
        notifyCompletion({});
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

}  // namespace atom::io::async
