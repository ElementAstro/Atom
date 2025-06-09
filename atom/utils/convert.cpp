/*
 * convert.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifdef _WIN32

#include "convert.hpp"

#include <memory>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

namespace atom::utils {

namespace {
// Thread-local buffers to avoid frequent memory allocations
thread_local std::vector<std::unique_ptr<wchar_t[]>> wide_buffers;
thread_local std::vector<std::unique_ptr<char[]>> char_buffers;

constexpr size_t MAX_CACHED_BUFFERS = 10;

// Clean up excess buffers to prevent memory growth
void cleanup_buffers() {
    if (wide_buffers.size() > MAX_CACHED_BUFFERS) {
        wide_buffers.erase(
            wide_buffers.begin(),
            wide_buffers.begin() + (wide_buffers.size() - MAX_CACHED_BUFFERS));
    }
    if (char_buffers.size() > MAX_CACHED_BUFFERS) {
        char_buffers.erase(
            char_buffers.begin(),
            char_buffers.begin() + (char_buffers.size() - MAX_CACHED_BUFFERS));
    }
}

// Get formatted error message from Windows API
auto get_last_error_message() -> std::string {
    DWORD error = GetLastError();
    if (error == 0)
        return "No error";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer), 0, nullptr);

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}
}  // namespace

LPWSTR CharToLPWSTR(std::string_view str) {
    spdlog::debug("Converting string_view to LPWSTR, length: {}", str.size());

    // Fast path for empty strings
    if (str.empty()) {
        auto buffer = std::make_unique<wchar_t[]>(1);
        buffer[0] = L'\0';
        auto* result = buffer.get();
        wide_buffers.emplace_back(std::move(buffer));
        cleanup_buffers();
        return result;
    }

    // Get required buffer size
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (size == 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error determining buffer size for conversion: {}",
                      error_msg);
        THROW_RUNTIME_ERROR("Error determining buffer size for conversion: " +
                            error_msg);
    }

    // Perform conversion
    auto buffer = std::make_unique<wchar_t[]>(size + 1);
    const int result =
        MultiByteToWideChar(CP_UTF8, 0, str.data(),
                            static_cast<int>(str.size()), buffer.get(), size);
    if (result == 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error in MultiByteToWideChar conversion: {}", error_msg);
        THROW_RUNTIME_ERROR("Error in MultiByteToWideChar conversion: " +
                            error_msg);
    }

    buffer[size] = L'\0';
    auto* ptr = buffer.get();
    wide_buffers.emplace_back(std::move(buffer));
    cleanup_buffers();

    spdlog::debug("Conversion to LPWSTR successful");
    return ptr;
}

std::string WCharArrayToString(const WCHAR* wCharArray) {
    spdlog::debug("Converting WCHAR array to std::string");

    // Handle empty input
    if (!wCharArray || *wCharArray == L'\0') {
        spdlog::debug("Empty or null WCHAR array");
        return {};
    }

    // Get required buffer size
    const int size_needed = WideCharToMultiByte(CP_UTF8, 0, wCharArray, -1,
                                                nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error getting buffer size for WCHAR conversion: {}",
                      error_msg);
        THROW_RUNTIME_ERROR("Error getting buffer size for WCHAR conversion: " +
                            error_msg);
    }

    // Perform conversion
    std::string str(size_needed - 1, '\0');
    const int result = WideCharToMultiByte(
        CP_UTF8, 0, wCharArray, -1, str.data(), size_needed, nullptr, nullptr);
    if (result == 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error in WideCharToMultiByte conversion: {}", error_msg);
        THROW_RUNTIME_ERROR("Error in WideCharToMultiByte conversion: " +
                            error_msg);
    }

    return str;
}

LPSTR StringToLPSTR(const std::string& str) {
    spdlog::debug("Converting std::string to LPSTR, length: {}", str.length());

    auto buffer = std::make_unique<char[]>(str.length() + 1);
    std::copy(str.begin(), str.end(), buffer.get());
    buffer[str.length()] = '\0';

    auto* result = buffer.get();
    char_buffers.emplace_back(std::move(buffer));
    cleanup_buffers();

    return result;
}

LPSTR WStringToLPSTR(const std::wstring& wstr) {
    spdlog::debug("Converting std::wstring to LPSTR, length: {}",
                  wstr.length());

    // Fast path for empty strings
    if (wstr.empty()) {
        auto buffer = std::make_unique<char[]>(1);
        buffer[0] = '\0';
        auto* result = buffer.get();
        char_buffers.emplace_back(std::move(buffer));
        cleanup_buffers();
        return result;
    }

    // Get required buffer size
    const int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1,
                                               nullptr, 0, nullptr, nullptr);
    if (bufferSize <= 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error(
            "Error getting buffer size for wstring to LPSTR conversion: {}",
            error_msg);
        THROW_RUNTIME_ERROR(
            "Error getting buffer size for wstring to LPSTR conversion: " +
            error_msg);
    }

    // Perform conversion
    auto buffer = std::make_unique<char[]>(bufferSize);
    const int result =
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer.get(),
                            bufferSize, nullptr, nullptr);
    if (result == 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error in WideCharToMultiByte conversion: {}", error_msg);
        THROW_RUNTIME_ERROR("Error in WideCharToMultiByte conversion: " +
                            error_msg);
    }

    auto* ptr = buffer.get();
    char_buffers.emplace_back(std::move(buffer));
    cleanup_buffers();

    return ptr;
}

LPWSTR StringToLPWSTR(const std::string& str) {
    spdlog::debug("Converting std::string to LPWSTR");
    return CharToLPWSTR(str);
}

std::string LPWSTRToString(LPWSTR lpwstr) {
    spdlog::debug("Converting LPWSTR to std::string");

    // Handle empty input
    if (!lpwstr || *lpwstr == L'\0') {
        spdlog::debug("Empty or null LPWSTR");
        return {};
    }

    // Get required buffer size
    const int size = WideCharToMultiByte(CP_UTF8, 0, lpwstr, -1, nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error getting buffer size for LPWSTR conversion: {}",
                      error_msg);
        THROW_RUNTIME_ERROR(
            "Error getting buffer size for LPWSTR conversion: " + error_msg);
    }

    // Perform conversion
    std::string str(size - 1, '\0');
    const int result = WideCharToMultiByte(CP_UTF8, 0, lpwstr, -1, str.data(),
                                           size, nullptr, nullptr);
    if (result == 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error in WideCharToMultiByte conversion: {}", error_msg);
        THROW_RUNTIME_ERROR("Error in WideCharToMultiByte conversion: " +
                            error_msg);
    }

    return str;
}

std::string LPCWSTRToString(LPCWSTR lpcwstr) {
    spdlog::debug("Converting LPCWSTR to std::string");

    // Handle empty input
    if (!lpcwstr || *lpcwstr == L'\0') {
        spdlog::debug("Empty or null LPCWSTR");
        return {};
    }

    // Get required buffer size
    const int size = WideCharToMultiByte(CP_UTF8, 0, lpcwstr, -1, nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error getting buffer size for LPCWSTR conversion: {}",
                      error_msg);
        THROW_RUNTIME_ERROR(
            "Error getting buffer size for LPCWSTR conversion: " + error_msg);
    }

    // Perform conversion
    std::string str(size - 1, '\0');
    const int result = WideCharToMultiByte(CP_UTF8, 0, lpcwstr, -1, str.data(),
                                           size, nullptr, nullptr);
    if (result == 0) {
        const auto error_msg = get_last_error_message();
        spdlog::error("Error in WideCharToMultiByte conversion: {}", error_msg);
        THROW_RUNTIME_ERROR("Error in WideCharToMultiByte conversion: " +
                            error_msg);
    }

    return str;
}

LPWSTR WStringToLPWSTR(const std::wstring& wstr) {
    spdlog::debug("Converting std::wstring to LPWSTR, length: {}",
                  wstr.length());

    auto buffer = std::make_unique<wchar_t[]>(wstr.size() + 1);
    std::copy(wstr.begin(), wstr.end(), buffer.get());
    buffer[wstr.size()] = L'\0';

    auto* result = buffer.get();
    wide_buffers.emplace_back(std::move(buffer));
    cleanup_buffers();

    return result;
}

std::wstring LPWSTRToWString(LPWSTR lpwstr) {
    spdlog::debug("Converting LPWSTR to std::wstring");

    if (!lpwstr) {
        spdlog::debug("Null LPWSTR");
        return {};
    }

    return std::wstring(lpwstr);
}

std::wstring LPCWSTRToWString(LPCWSTR lpcwstr) {
    spdlog::debug("Converting LPCWSTR to std::wstring");

    if (!lpcwstr) {
        spdlog::debug("Null LPCWSTR");
        return {};
    }

    return std::wstring(lpcwstr);
}

}  // namespace atom::utils

#endif