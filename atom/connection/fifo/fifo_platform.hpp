/*
 * fifo_platform.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Platform abstraction layer for FIFO operations

*************************************************/

#ifndef ATOM_CONNECTION_FIFO_PLATFORM_HPP
#define ATOM_CONNECTION_FIFO_PLATFORM_HPP

#include <chrono>
#include <string>
#include <string_view>

#include "fifo_common.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

namespace atom::connection {

/**
 * @brief Platform-specific handle type
 */
#ifdef _WIN32
using NativeHandle = HANDLE;
// INVALID_HANDLE_VALUE cannot be constexpr on Windows due to reinterpret_cast
inline NativeHandle getInvalidNativeHandle() noexcept {
    return INVALID_HANDLE_VALUE;
}
#define INVALID_NATIVE_HANDLE (::atom::connection::getInvalidNativeHandle())
#else
using NativeHandle = int;
constexpr NativeHandle INVALID_NATIVE_HANDLE = -1;
#endif

/**
 * @brief Platform abstraction for FIFO operations
 *
 * This class provides a unified interface for FIFO operations across
 * different platforms (Windows named pipes and POSIX FIFOs).
 */
class FifoPlatform {
public:
    /**
     * @brief Open mode for FIFO
     */
    enum class OpenMode { ReadOnly, WriteOnly, ReadWrite };

    /**
     * @brief Create a FIFO file (POSIX only, no-op on Windows)
     *
     * @param path Path to the FIFO
     * @param mode Permission mode (default: 0666)
     * @return FifoVoidResult Success or error
     */
    static auto createFifo(std::string_view path, int mode = 0666)
        -> FifoVoidResult;

    /**
     * @brief Remove a FIFO file
     *
     * @param path Path to the FIFO
     * @return FifoVoidResult Success or error
     */
    static auto removeFifo(std::string_view path) -> FifoVoidResult;

    /**
     * @brief Open a FIFO for reading and/or writing
     *
     * @param path Path to the FIFO
     * @param mode Open mode
     * @param non_blocking Whether to open in non-blocking mode
     * @return FifoResult<NativeHandle> Handle or error
     */
    static auto openFifo(std::string_view path,
                         OpenMode mode = OpenMode::ReadWrite,
                         bool non_blocking = true) -> FifoResult<NativeHandle>;

    /**
     * @brief Close a FIFO handle
     *
     * @param handle Handle to close
     */
    static void closeFifo(NativeHandle handle) noexcept;

    /**
     * @brief Check if a handle is valid
     *
     * @param handle Handle to check
     * @return bool True if valid
     */
    static auto isValidHandle(NativeHandle handle) noexcept -> bool;

    /**
     * @brief Write data to a FIFO
     *
     * @param handle FIFO handle
     * @param data Data to write
     * @param size Size of data
     * @param timeout Optional timeout
     * @return FifoResult<size_t> Bytes written or error
     */
    static auto write(NativeHandle handle, const char* data, size_t size,
                      std::optional<std::chrono::milliseconds> timeout =
                          std::nullopt) -> FifoResult<size_t>;

    /**
     * @brief Read data from a FIFO
     *
     * @param handle FIFO handle
     * @param buffer Buffer to read into
     * @param size Maximum bytes to read
     * @param timeout Optional timeout
     * @return FifoResult<size_t> Bytes read or error
     */
    static auto read(NativeHandle handle, char* buffer, size_t size,
                     std::optional<std::chrono::milliseconds> timeout =
                         std::nullopt) -> FifoResult<size_t>;

    /**
     * @brief Wait for FIFO to be readable
     *
     * @param handle FIFO handle
     * @param timeout Timeout duration
     * @return FifoVoidResult Success (readable), or Timeout/error
     */
    static auto waitReadable(NativeHandle handle,
                             std::chrono::milliseconds timeout)
        -> FifoVoidResult;

    /**
     * @brief Wait for FIFO to be writable
     *
     * @param handle FIFO handle
     * @param timeout Timeout duration
     * @return FifoVoidResult Success (writable), or Timeout/error
     */
    static auto waitWritable(NativeHandle handle,
                             std::chrono::milliseconds timeout)
        -> FifoVoidResult;

    /**
     * @brief Create a Windows named pipe (Windows only)
     *
     * @param path Pipe name (e.g., "\\\\.\\pipe\\mypipe")
     * @return FifoResult<NativeHandle> Handle or error
     */
    static auto createNamedPipe(std::string_view path)
        -> FifoResult<NativeHandle>;

    /**
     * @brief Get the last platform error as a FifoError
     *
     * @return std::error_code Platform error code
     */
    static auto getLastError() -> std::error_code;

    /**
     * @brief Get platform error message
     *
     * @return std::string Error message
     */
    static auto getLastErrorMessage() -> std::string;

private:
    FifoPlatform() = delete;
};

/**
 * @brief RAII wrapper for FIFO handles
 */
class FifoHandle {
public:
    FifoHandle() noexcept : handle_(INVALID_NATIVE_HANDLE) {}

    explicit FifoHandle(NativeHandle handle) noexcept : handle_(handle) {}

    ~FifoHandle() { close(); }

    // Non-copyable
    FifoHandle(const FifoHandle&) = delete;
    FifoHandle& operator=(const FifoHandle&) = delete;

    // Movable
    FifoHandle(FifoHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = INVALID_NATIVE_HANDLE;
    }

    FifoHandle& operator=(FifoHandle&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = INVALID_NATIVE_HANDLE;
        }
        return *this;
    }

    [[nodiscard]] NativeHandle get() const noexcept { return handle_; }

    [[nodiscard]] bool isValid() const noexcept {
        return FifoPlatform::isValidHandle(handle_);
    }

    [[nodiscard]] explicit operator bool() const noexcept { return isValid(); }

    NativeHandle release() noexcept {
        NativeHandle h = handle_;
        handle_ = INVALID_NATIVE_HANDLE;
        return h;
    }

    void reset(NativeHandle handle = INVALID_NATIVE_HANDLE) noexcept {
        close();
        handle_ = handle;
    }

    void close() noexcept {
        if (isValid()) {
            FifoPlatform::closeFifo(handle_);
            handle_ = INVALID_NATIVE_HANDLE;
        }
    }

private:
    NativeHandle handle_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_FIFO_PLATFORM_HPP
