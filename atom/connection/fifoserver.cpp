/*
 * fifoserver.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FIFO Server

*************************************************/

#include "fifoserver.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <format>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace atom::connection {

// Constants
constexpr size_t MAX_QUEUE_SIZE = 1000;  // Maximum number of queued messages
constexpr auto RETRY_TIMEOUT =
    std::chrono::milliseconds(100);  // Time between retries
constexpr int MAX_RETRIES = 3;       // Maximum number of write retries

class FIFOServer::Impl {
public:
    explicit Impl(std::string_view fifo_path)
        : fifo_path_(fifo_path), stop_server_(false) {
        if (fifo_path.empty()) {
            throw std::invalid_argument("FIFO path cannot be empty");
        }

        try {
            // Create directory path if it doesn't exist
            std::filesystem::path path(fifo_path_);
            if (auto parent = path.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent);
            }

// Create FIFO file with error handling
#ifdef _WIN32
            pipe_handle_ = CreateNamedPipeA(
                fifo_path_.c_str(), PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);

            if (pipe_handle_ == INVALID_HANDLE_VALUE) {
                throw std::runtime_error(std::format(
                    "Failed to create named pipe: {}", GetLastError()));
            }
#elif __APPLE__ || __linux__
            if (mkfifo(fifo_path_.c_str(), 0666) != 0 && errno != EEXIST) {
                throw std::runtime_error(
                    std::format("Failed to create FIFO: {}", strerror(errno)));
            }
#endif

            std::cout << "FIFO server initialized at: " << fifo_path_
                      << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error initializing FIFO server: " << e.what()
                      << std::endl;
            throw;  // Re-throw to notify client code
        }
    }

    ~Impl() {
        try {
            stop();

// Clean up resources
#ifdef _WIN32
            if (pipe_handle_ != INVALID_HANDLE_VALUE) {
                CloseHandle(pipe_handle_);
                pipe_handle_ = INVALID_HANDLE_VALUE;
            }
            // Attempt to delete the named pipe
            DeleteFileA(fifo_path_.c_str());
#elif __APPLE__ || __linux__
            // Remove the FIFO file if it exists
            std::filesystem::remove(fifo_path_);
#endif
        } catch (const std::exception& e) {
            std::cerr << "Error during FIFO server cleanup: " << e.what()
                      << std::endl;
        }
    }

    bool sendMessage(std::string message) {
        // Validate message
        if (message.empty()) {
            std::cerr << "Warning: Attempted to send empty message, ignoring"
                      << std::endl;
            return false;
        }

        if (!isRunning()) {
            std::cerr << "Warning: Attempted to send message while server is "
                         "not running"
                      << std::endl;
            return false;
        }

        try {
            // Use move semantics consistently
            {
                std::scoped_lock lock(queue_mutex_);
                // Limit queue size to prevent memory issues
                if (message_queue_.size() >= MAX_QUEUE_SIZE) {
                    std::cerr
                        << "Warning: Message queue overflow, dropping message"
                        << std::endl;
                    return false;
                }
                message_queue_.emplace(std::move(message));
            }
            message_cv_.notify_one();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error queueing message: " << e.what() << std::endl;
            return false;
        }
    }

    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    size_t sendMessages(R&& messages) {
        size_t count = 0;
        try {
            std::scoped_lock lock(queue_mutex_);

            for (auto&& msg : messages) {
                // Skip empty messages
                if (msg.empty()) {
                    continue;
                }

                // Check queue limit
                if (message_queue_.size() >= MAX_QUEUE_SIZE) {
                    std::cerr << "Warning: Message queue overflow, dropping "
                                 "remaining messages"
                              << std::endl;
                    break;
                }

                message_queue_.emplace(std::forward<decltype(msg)>(msg));
                count++;
            }

            if (count > 0) {
                message_cv_.notify_one();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error queueing messages: " << e.what() << std::endl;
        }
        return count;
    }

    void start() {
        try {
            if (!server_thread_.joinable()) {
                stop_server_ = false;
                server_thread_ = std::jthread([this] { serverLoop(); });
                std::cout << "FIFO server started" << std::endl;
            } else {
                std::cerr << "Server is already running" << std::endl;
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::format("Failed to start server: {}", e.what()));
        }
    }

    void stop() {
        try {
            if (server_thread_.joinable()) {
                stop_server_ = true;
                message_cv_.notify_all();
                server_thread_.join();
                std::cout << "FIFO server stopped" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error stopping server: " << e.what() << std::endl;
        }
    }

    [[nodiscard]] bool isRunning() const {
        return server_thread_.joinable() && !stop_server_;
    }

    [[nodiscard]] std::string getFifoPath() const { return fifo_path_; }

private:
    void serverLoop() {
        while (!stop_server_) {
            std::string message;
            {
                std::unique_lock lock(queue_mutex_);
                auto waitResult = message_cv_.wait_for(
                    lock, std::chrono::seconds(1),
                    [this] { return stop_server_ || !message_queue_.empty(); });

                if (stop_server_) {
                    // Process remaining messages before exiting if requested
                    if (message_queue_.empty()) {
                        break;
                    }
                }

                if (!waitResult) {
                    // Timeout occurred, loop back to check stop_server_ again
                    continue;
                }

                if (!message_queue_.empty()) {
                    message = std::move(message_queue_.front());
                    message_queue_.pop();
                }
            }

            if (!message.empty()) {
                writeMessage(message);
            }
        }

        std::cout << "FIFO server loop exited" << std::endl;
    }

    bool writeMessage(const std::string& message) {
        for (int retry = 0; retry < MAX_RETRIES; ++retry) {
            try {
#ifdef _WIN32
                HANDLE pipe = CreateFileA(fifo_path_.c_str(), GENERIC_WRITE, 0,
                                          NULL, OPEN_EXISTING, 0, NULL);
                if (pipe != INVALID_HANDLE_VALUE) {
                    DWORD bytes_written = 0;
                    BOOL success =
                        WriteFile(pipe, message.c_str(),
                                  static_cast<DWORD>(message.length()),
                                  &bytes_written, NULL);
                    CloseHandle(pipe);

                    if (!success) {
                        throw std::system_error(GetLastError(),
                                                std::system_category(),
                                                "Failed to write to pipe");
                    }

                    if (bytes_written != message.length()) {
                        std::cerr << "Warning: Partial write to pipe: "
                                  << bytes_written << " of " << message.length()
                                  << " bytes" << std::endl;
                    }

                    return true;
                } else {
                    throw std::system_error(GetLastError(),
                                            std::system_category(),
                                            "Failed to open pipe for writing");
                }
#elif __APPLE__ || __linux__
                // Try with non-blocking first, then blocking if needed
                int fd = open(fifo_path_.c_str(), O_WRONLY | O_NONBLOCK);
                if (fd == -1) {
                    // If no reader is available, non-blocking open might fail
                    fd = open(fifo_path_.c_str(), O_WRONLY);
                }

                if (fd != -1) {
                    ssize_t bytes_written =
                        write(fd, message.c_str(), message.length());
                    close(fd);

                    if (bytes_written == -1) {
                        throw std::system_error(errno, std::system_category(),
                                                "Failed to write to FIFO");
                    }

                    if (static_cast<size_t>(bytes_written) !=
                        message.length()) {
                        std::cerr << "Warning: Partial write to FIFO: "
                                  << bytes_written << " of " << message.length()
                                  << " bytes" << std::endl;
                    }

                    return true;
                } else {
                    throw std::system_error(errno, std::system_category(),
                                            "Failed to open FIFO for writing");
                }
#endif
            } catch (const std::exception& e) {
                std::cerr << "Error writing message (attempt " << (retry + 1)
                          << " of " << MAX_RETRIES << "): " << e.what()
                          << std::endl;

                if (retry < MAX_RETRIES - 1) {
                    // Wait before retrying
                    std::this_thread::sleep_for(RETRY_TIMEOUT);
                }
            }
        }

        return false;
    }

    std::string fifo_path_;
    std::jthread server_thread_;
    std::atomic_bool stop_server_;
    std::queue<std::string> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable message_cv_;

#ifdef _WIN32
    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
#endif
};

// FIFOServer implementation

FIFOServer::FIFOServer(std::string_view fifo_path)
    : impl_(std::make_unique<Impl>(fifo_path)) {}

FIFOServer::~FIFOServer() = default;

// Move operations
FIFOServer::FIFOServer(FIFOServer&&) noexcept = default;
FIFOServer& FIFOServer::operator=(FIFOServer&&) noexcept = default;

bool FIFOServer::sendMessage(std::string message) {
    return impl_->sendMessage(std::move(message));
}

template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
size_t FIFOServer::sendMessages(R&& messages) {
    return impl_->sendMessages(std::forward<R>(messages));
}

// Explicit instantiation of common template instances
template size_t FIFOServer::sendMessages(std::vector<std::string>&);
template size_t FIFOServer::sendMessages(const std::vector<std::string>&);
template size_t FIFOServer::sendMessages(std::vector<std::string>&&);

void FIFOServer::start() { impl_->start(); }

void FIFOServer::stop() { impl_->stop(); }

bool FIFOServer::isRunning() const { return impl_->isRunning(); }

std::string FIFOServer::getFifoPath() const { return impl_->getFifoPath(); }

}  // namespace atom::connection