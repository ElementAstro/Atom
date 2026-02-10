/*
 * fifo_platform.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Platform abstraction layer for FIFO operations

*************************************************/

#include "fifo_platform.hpp"

#include <filesystem>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/select.h>
#endif

namespace atom::connection {

auto FifoPlatform::createFifo(std::string_view path, int mode) -> FifoVoidResult {
#ifdef _WIN32
    (void)path;
    (void)mode;
    // Windows uses named pipes, not FIFO files
    return {};
#else
    std::string path_str(path);

    // Create parent directories if needed
    std::filesystem::path fs_path(path_str);
    if (auto parent = fs_path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return type::unexpected(make_error_code(FifoError::OpenFailed));
        }
    }

    if (mkfifo(path_str.c_str(), mode) == -1) {
        if (errno != EEXIST) {
            return type::unexpected(make_error_code(FifoError::OpenFailed));
        }
    }
    return {};
#endif
}

auto FifoPlatform::removeFifo(std::string_view path) -> FifoVoidResult {
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(std::string(path)), ec);
    if (ec) {
        return type::unexpected(make_error_code(FifoError::InvalidOperation));
    }
    return {};
}

auto FifoPlatform::openFifo(std::string_view path, OpenMode mode,
                            bool non_blocking) -> FifoResult<NativeHandle> {
    std::string path_str(path);

#ifdef _WIN32
    DWORD access = 0;
    switch (mode) {
        case OpenMode::ReadOnly:
            access = GENERIC_READ;
            break;
        case OpenMode::WriteOnly:
            access = GENERIC_WRITE;
            break;
        case OpenMode::ReadWrite:
            access = GENERIC_READ | GENERIC_WRITE;
            break;
    }

    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (non_blocking) {
        flags |= FILE_FLAG_OVERLAPPED;
    }

    HANDLE handle = CreateFileA(
        path_str.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, flags, nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        return type::unexpected(make_error_code(FifoError::OpenFailed));
    }

    return handle;
#else
    int flags = 0;
    switch (mode) {
        case OpenMode::ReadOnly:
            flags = O_RDONLY;
            break;
        case OpenMode::WriteOnly:
            flags = O_WRONLY;
            break;
        case OpenMode::ReadWrite:
            flags = O_RDWR;
            break;
    }

    if (non_blocking) {
        flags |= O_NONBLOCK;
    }

    int fd = ::open(path_str.c_str(), flags);
    if (fd == -1) {
        return type::unexpected(make_error_code(FifoError::OpenFailed));
    }

    return fd;
#endif
}

void FifoPlatform::closeFifo(NativeHandle handle) noexcept {
    if (!isValidHandle(handle)) {
        return;
    }

#ifdef _WIN32
    CloseHandle(handle);
#else
    ::close(handle);
#endif
}

auto FifoPlatform::isValidHandle(NativeHandle handle) noexcept -> bool {
#ifdef _WIN32
    return handle != INVALID_HANDLE_VALUE && handle != nullptr;
#else
    return handle >= 0;
#endif
}

auto FifoPlatform::write(NativeHandle handle, const char* data, size_t size,
                         std::optional<std::chrono::milliseconds> timeout)
    -> FifoResult<size_t> {
    if (!isValidHandle(handle)) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }

#ifdef _WIN32
    DWORD written = 0;

    if (timeout) {
        // For timeout support on Windows, we'd need overlapped I/O
        // For now, use synchronous write
        BOOL result =
            WriteFile(handle, data, static_cast<DWORD>(size), &written, nullptr);
        if (!result) {
            return type::unexpected(make_error_code(FifoError::WriteFailed));
        }
    } else {
        BOOL result =
            WriteFile(handle, data, static_cast<DWORD>(size), &written, nullptr);
        if (!result) {
            return type::unexpected(make_error_code(FifoError::WriteFailed));
        }
    }

    return static_cast<size_t>(written);
#else
    ssize_t result = ::write(handle, data, size);

    if (result == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (timeout) {
                auto wait_result = waitWritable(handle, *timeout);
                if (!wait_result) {
                    return type::unexpected(wait_result.error());
                }
                result = ::write(handle, data, size);
            }
        }

        if (result == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return type::unexpected(make_error_code(FifoError::Timeout));
            }
            return type::unexpected(make_error_code(FifoError::WriteFailed));
        }
    }

    return static_cast<size_t>(result);
#endif
}

auto FifoPlatform::read(NativeHandle handle, char* buffer, size_t size,
                        std::optional<std::chrono::milliseconds> timeout)
    -> FifoResult<size_t> {
    if (!isValidHandle(handle)) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }

#ifdef _WIN32
    DWORD bytes_read = 0;

    BOOL result =
        ReadFile(handle, buffer, static_cast<DWORD>(size), &bytes_read, nullptr);
    if (!result) {
        DWORD error = GetLastError();
        if (error == ERROR_NO_DATA) {
            return 0;
        }
        return type::unexpected(make_error_code(FifoError::ReadFailed));
    }

    return static_cast<size_t>(bytes_read);
#else
    ssize_t result = ::read(handle, buffer, size);

    if (result == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (timeout) {
                auto wait_result = waitReadable(handle, *timeout);
                if (!wait_result) {
                    return type::unexpected(wait_result.error());
                }
                result = ::read(handle, buffer, size);
            }
        }

        if (result == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return type::unexpected(make_error_code(FifoError::Timeout));
            }
            return type::unexpected(make_error_code(FifoError::ReadFailed));
        }
    }

    return static_cast<size_t>(result);
#endif
}

auto FifoPlatform::waitReadable(NativeHandle handle,
                                std::chrono::milliseconds timeout)
    -> FifoVoidResult {
#ifdef _WIN32
    // Windows doesn't have direct poll equivalent for handles
    // For overlapped I/O, we'd use WaitForSingleObject
    (void)handle;
    (void)timeout;
    return {};
#else
    pollfd pfd{handle, POLLIN, 0};
    int result = poll(&pfd, 1, static_cast<int>(timeout.count()));

    if (result == 0) {
        return type::unexpected(make_error_code(FifoError::Timeout));
    } else if (result == -1) {
        return type::unexpected(make_error_code(FifoError::ReadFailed));
    }

    return {};
#endif
}

auto FifoPlatform::waitWritable(NativeHandle handle,
                                std::chrono::milliseconds timeout)
    -> FifoVoidResult {
#ifdef _WIN32
    (void)handle;
    (void)timeout;
    return {};
#else
    pollfd pfd{handle, POLLOUT, 0};
    int result = poll(&pfd, 1, static_cast<int>(timeout.count()));

    if (result == 0) {
        return type::unexpected(make_error_code(FifoError::Timeout));
    } else if (result == -1) {
        return type::unexpected(make_error_code(FifoError::WriteFailed));
    }

    return {};
#endif
}

auto FifoPlatform::createNamedPipe(std::string_view path)
    -> FifoResult<NativeHandle> {
#ifdef _WIN32
    std::string path_str(path);

    HANDLE handle = CreateNamedPipeA(
        path_str.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        return type::unexpected(make_error_code(FifoError::OpenFailed));
    }

    return handle;
#else
    (void)path;
    return type::unexpected(make_error_code(FifoError::InvalidOperation));
#endif
}

auto FifoPlatform::getLastError() -> std::error_code {
#ifdef _WIN32
    DWORD error = GetLastError();
    return std::error_code(static_cast<int>(error), std::system_category());
#else
    return std::error_code(errno, std::generic_category());
#endif
}

auto FifoPlatform::getLastErrorMessage() -> std::string {
#ifdef _WIN32
    DWORD error = GetLastError();
    if (error == 0) {
        return "No error";
    }

    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer), 0, nullptr);

    std::string message(buffer, size);
    LocalFree(buffer);

    // Remove trailing newlines
    while (!message.empty() &&
           (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }

    return message;
#else
    return std::strerror(errno);
#endif
}

}  // namespace atom::connection
