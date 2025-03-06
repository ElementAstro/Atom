/*
 * fifoclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FIFO Client

*************************************************/

#include "fifoclient.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace atom::connection {

// Create a custom error category for FIFO operations
class FifoErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override {
        return "fifo_error";
    }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<FifoError>(ev)) {
            case FifoError::OpenFailed:
                return "Failed to open FIFO pipe";
            case FifoError::ReadFailed:
                return "Failed to read from FIFO pipe";
            case FifoError::WriteFailed:
                return "Failed to write to FIFO pipe";
            case FifoError::Timeout:
                return "Operation timed out";
            case FifoError::InvalidOperation:
                return "Invalid operation on FIFO pipe";
            case FifoError::NotOpen:
                return "FIFO pipe is not open";
            default:
                return "Unknown FIFO error";
        }
    }
};

// Global instance of the FIFO error category
const FifoErrorCategory theFifoErrorCategory{};

// Helper function to create an error code from a FifoError
[[nodiscard]] std::error_code make_error_code(FifoError e) {
    return {static_cast<int>(e), theFifoErrorCategory};
}

struct FifoClient::Impl {
#ifdef _WIN32
    HANDLE fifoHandle{INVALID_HANDLE_VALUE};
#else
    int fifoFd{-1};
#endif
    std::string fifoPath;
    std::mutex operationMutex;  // Mutex for thread-safe operations

    // Default buffer size for read operations
    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 4096;

    explicit Impl(std::string_view path) : fifoPath(path) {
        try {
#ifdef _WIN32
            fifoHandle = CreateFileA(
                fifoPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                nullptr);

            if (fifoHandle == INVALID_HANDLE_VALUE) {
                throw std::system_error(
                    GetLastError(), std::system_category(),
                    "Failed to open FIFO pipe: " + fifoPath);
            }
#else
            // Try to create the FIFO if it doesn't exist
            struct stat st;
            if (stat(fifoPath.c_str(), &st) == -1) {
                if (mkfifo(fifoPath.c_str(), 0666) == -1 && errno != EEXIST) {
                    throw std::system_error(
                        errno, std::system_category(),
                        "Failed to create FIFO pipe: " + fifoPath);
                }
            } else if (!S_ISFIFO(st.st_mode)) {
                throw std::system_error(
                    ENOTSUP, std::system_category(),
                    "Path exists but is not a FIFO: " + fifoPath);
            }

            fifoFd = open(fifoPath.c_str(), O_RDWR | O_NONBLOCK);
            if (fifoFd == -1) {
                throw std::system_error(
                    errno, std::system_category(),
                    "Failed to open FIFO pipe: " + fifoPath);
            }
#endif
        } catch (const std::exception& e) {
            // Close any resources that might have been opened
            close();
            throw;  // Re-throw the exception
        }
    }

    ~Impl() { close(); }

    [[nodiscard]] bool isOpen() const noexcept {
#ifdef _WIN32
        return fifoHandle != INVALID_HANDLE_VALUE;
#else
        return fifoFd != -1;
#endif
    }

    void close() noexcept {
        std::lock_guard<std::mutex> lock(operationMutex);

#ifdef _WIN32
        if (fifoHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(fifoHandle);
            fifoHandle = INVALID_HANDLE_VALUE;
        }
#else
        if (fifoFd != -1) {
            ::close(fifoFd);
            fifoFd = -1;
        }
#endif
    }

    type::expected<std::size_t, std::error_code> write(
        std::string_view data,
        std::optional<std::chrono::milliseconds> timeout) {
        if (data.empty()) {
            return 0;  // Nothing to write
        }

        if (!isOpen()) {
            return type::unexpected(make_error_code(FifoError::NotOpen));
        }

        std::lock_guard<std::mutex> lock(operationMutex);

        try {
#ifdef _WIN32
            OVERLAPPED overlapped = {0};
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (overlapped.hEvent == NULL) {
                return type::unexpected(
                    std::error_code(GetLastError(), std::system_category()));
            }

            DWORD bytesWritten = 0;
            bool success = WriteFile(fifoHandle, data.data(),
                                     static_cast<DWORD>(data.size()),
                                     &bytesWritten, &overlapped);

            // Handle asynchronous operation
            if (!success && GetLastError() == ERROR_IO_PENDING) {
                DWORD waitTime =
                    timeout ? static_cast<DWORD>(timeout->count()) : INFINITE;
                DWORD waitResult =
                    WaitForSingleObject(overlapped.hEvent, waitTime);

                if (waitResult == WAIT_TIMEOUT) {
                    CancelIo(fifoHandle);
                    CloseHandle(overlapped.hEvent);
                    return type::unexpected(
                        make_error_code(FifoError::Timeout));
                } else if (waitResult != WAIT_OBJECT_0) {
                    CloseHandle(overlapped.hEvent);
                    return type::unexpected(std::error_code(
                        GetLastError(), std::system_category()));
                }

                // Get the result of the operation
                if (!GetOverlappedResult(fifoHandle, &overlapped, &bytesWritten,
                                         FALSE)) {
                    CloseHandle(overlapped.hEvent);
                    return type::unexpected(std::error_code(
                        GetLastError(), std::system_category()));
                }
            } else if (!success) {
                CloseHandle(overlapped.hEvent);
                return type::unexpected(
                    std::error_code(GetLastError(), std::system_category()));
            }

            CloseHandle(overlapped.hEvent);
            return static_cast<std::size_t>(bytesWritten);
#else
            if (timeout) {
                pollfd pfd{};
                pfd.fd = fifoFd;
                pfd.events = POLLOUT;

                int pollResult =
                    poll(&pfd, 1, static_cast<int>(timeout->count()));

                if (pollResult == 0) {
                    return type::unexpected(
                        make_error_code(FifoError::Timeout));
                } else if (pollResult < 0) {
                    return type::unexpected(
                        std::error_code(errno, std::system_category()));
                }

                if (!(pfd.revents & POLLOUT)) {
                    return type::unexpected(
                        make_error_code(FifoError::WriteFailed));
                }
            }

            ssize_t bytesWritten = ::write(fifoFd, data.data(), data.size());

            if (bytesWritten == -1) {
                return type::unexpected(
                    std::error_code(errno, std::system_category()));
            }

            return static_cast<std::size_t>(bytesWritten);
#endif
        } catch (const std::exception& e) {
            return type::unexpected(make_error_code(FifoError::WriteFailed));
        }
    }

    type::expected<std::string, std::error_code> read(
        std::size_t maxSize, std::optional<std::chrono::milliseconds> timeout) {
        if (!isOpen()) {
            return type::unexpected(make_error_code(FifoError::NotOpen));
        }

        if (maxSize == 0 ||
            maxSize > 1024 * 1024) {  // Limit to reasonable size
            maxSize = DEFAULT_BUFFER_SIZE;
        }

        std::lock_guard<std::mutex> lock(operationMutex);

        try {
            std::string result;
            result.reserve(maxSize);  // Pre-allocate memory

            // Use an array for small reads (stack allocation), vector for
            // larger reads
            constexpr std::size_t STACK_BUFFER_SIZE = 4096;

            if (maxSize <= STACK_BUFFER_SIZE) {
                std::array<char, STACK_BUFFER_SIZE> buffer;

#ifdef _WIN32
                OVERLAPPED overlapped = {0};
                overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                if (overlapped.hEvent == NULL) {
                    return type::unexpected(std::error_code(
                        GetLastError(), std::system_category()));
                }

                DWORD bytesRead = 0;
                bool success = ReadFile(
                    fifoHandle, buffer.data(),
                    static_cast<DWORD>(std::min(maxSize, buffer.size())),
                    &bytesRead, &overlapped);

                // Handle asynchronous operation
                if (!success && GetLastError() == ERROR_IO_PENDING) {
                    DWORD waitTime = timeout
                                         ? static_cast<DWORD>(timeout->count())
                                         : INFINITE;
                    DWORD waitResult =
                        WaitForSingleObject(overlapped.hEvent, waitTime);

                    if (waitResult == WAIT_TIMEOUT) {
                        CancelIo(fifoHandle);
                        CloseHandle(overlapped.hEvent);
                        return type::unexpected(
                            make_error_code(FifoError::Timeout));
                    } else if (waitResult != WAIT_OBJECT_0) {
                        CloseHandle(overlapped.hEvent);
                        return type::unexpected(std::error_code(
                            GetLastError(), std::system_category()));
                    }

                    // Get the result of the operation
                    if (!GetOverlappedResult(fifoHandle, &overlapped,
                                             &bytesRead, FALSE)) {
                        CloseHandle(overlapped.hEvent);
                        return type::unexpected(std::error_code(
                            GetLastError(), std::system_category()));
                    }
                } else if (!success) {
                    CloseHandle(overlapped.hEvent);
                    return type::unexpected(std::error_code(
                        GetLastError(), std::system_category()));
                }

                CloseHandle(overlapped.hEvent);

                if (bytesRead > 0) {
                    result.append(buffer.data(), bytesRead);
                }
#else
                if (timeout) {
                    pollfd pfd{};
                    pfd.fd = fifoFd;
                    pfd.events = POLLIN;

                    int pollResult =
                        poll(&pfd, 1, static_cast<int>(timeout->count()));

                    if (pollResult == 0) {
                        return type::unexpected(
                            make_error_code(FifoError::Timeout));
                    } else if (pollResult < 0) {
                        return type::unexpected(
                            std::error_code(errno, std::system_category()));
                    }

                    if (!(pfd.revents & POLLIN)) {
                        return type::unexpected(
                            make_error_code(FifoError::ReadFailed));
                    }
                }

                ssize_t bytesRead = ::read(fifoFd, buffer.data(),
                                           std::min(maxSize, buffer.size()));

                if (bytesRead == -1) {
                    return type::unexpected(
                        std::error_code(errno, std::system_category()));
                }

                if (bytesRead > 0) {
                    result.append(buffer.data(), bytesRead);
                }
#endif
            } else {
                // Use a vector for larger reads
                std::vector<char> buffer(maxSize);

#ifdef _WIN32
                OVERLAPPED overlapped = {0};
                overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                if (overlapped.hEvent == NULL) {
                    return type::unexpected(std::error_code(
                        GetLastError(), std::system_category()));
                }

                DWORD bytesRead = 0;
                bool success = ReadFile(fifoHandle, buffer.data(),
                                        static_cast<DWORD>(buffer.size()),
                                        &bytesRead, &overlapped);

                // Handle asynchronous operation
                if (!success && GetLastError() == ERROR_IO_PENDING) {
                    DWORD waitTime = timeout
                                         ? static_cast<DWORD>(timeout->count())
                                         : INFINITE;
                    DWORD waitResult =
                        WaitForSingleObject(overlapped.hEvent, waitTime);

                    if (waitResult == WAIT_TIMEOUT) {
                        CancelIo(fifoHandle);
                        CloseHandle(overlapped.hEvent);
                        return type::unexpected(
                            make_error_code(FifoError::Timeout));
                    } else if (waitResult != WAIT_OBJECT_0) {
                        CloseHandle(overlapped.hEvent);
                        return type::unexpected(std::error_code(
                            GetLastError(), std::system_category()));
                    }

                    // Get the result of the operation
                    if (!GetOverlappedResult(fifoHandle, &overlapped,
                                             &bytesRead, FALSE)) {
                        CloseHandle(overlapped.hEvent);
                        return type::unexpected(std::error_code(
                            GetLastError(), std::system_category()));
                    }
                } else if (!success) {
                    CloseHandle(overlapped.hEvent);
                    return type::unexpected(std::error_code(
                        GetLastError(), std::system_category()));
                }

                CloseHandle(overlapped.hEvent);

                if (bytesRead > 0) {
                    result.append(buffer.data(), bytesRead);
                }
#else
                if (timeout) {
                    pollfd pfd{};
                    pfd.fd = fifoFd;
                    pfd.events = POLLIN;

                    int pollResult =
                        poll(&pfd, 1, static_cast<int>(timeout->count()));

                    if (pollResult == 0) {
                        return type::unexpected(
                            make_error_code(FifoError::Timeout));
                    } else if (pollResult < 0) {
                        return type::unexpected(
                            std::error_code(errno, std::system_category()));
                    }

                    if (!(pfd.revents & POLLIN)) {
                        return type::unexpected(
                            make_error_code(FifoError::ReadFailed));
                    }
                }

                ssize_t bytesRead =
                    ::read(fifoFd, buffer.data(), buffer.size());

                if (bytesRead == -1) {
                    return type::unexpected(
                        std::error_code(errno, std::system_category()));
                }

                if (bytesRead > 0) {
                    result.append(buffer.data(), bytesRead);
                }
#endif
            }

            return result;
        } catch (const std::exception& e) {
            return type::unexpected(make_error_code(FifoError::ReadFailed));
        }
    }
};

// FifoClient implementation

FifoClient::FifoClient(std::string_view fifoPath)
    : m_impl(std::make_unique<Impl>(fifoPath)) {}

FifoClient::FifoClient(FifoClient&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}

FifoClient& FifoClient::operator=(FifoClient&& other) noexcept {
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

FifoClient::~FifoClient() = default;

auto FifoClient::write(std::string_view data,
                       std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<std::size_t, std::error_code> {
    if (!m_impl) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    return m_impl->write(data, timeout);
}

auto FifoClient::read(std::size_t maxSize,
                      std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<std::string, std::error_code> {
    if (!m_impl) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    return m_impl->read(maxSize, timeout);
}

bool FifoClient::isOpen() const noexcept { return m_impl && m_impl->isOpen(); }

std::string_view FifoClient::getPath() const noexcept {
    if (!m_impl) {
        static const std::string empty;
        return empty;
    }
    return m_impl->fifoPath;
}

void FifoClient::close() noexcept {
    if (m_impl) {
        m_impl->close();
    }
}

}  // namespace atom::connection