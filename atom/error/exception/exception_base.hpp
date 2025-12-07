/*
 * exception_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Base exception class with detailed information

**************************************************/

#ifndef ATOM_ERROR_EXCEPTION_BASE_HPP
#define ATOM_ERROR_EXCEPTION_BASE_HPP

#include <exception>
#include <ostream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

#include "../../macro.hpp"
#include "../stacktrace/stacktrace.hpp"

namespace atom::error {

/**
 * @brief Custom exception class with detailed information about the error.
 */
class Exception : public std::exception {
public:
    /**
     * @brief Constructs an Exception object.
     * @param file The file where the exception occurred.
     * @param line The line number in the file where the exception occurred.
     * @param func The function where the exception occurred.
     * @param args Additional arguments to provide context for the exception.
     */
    template <typename... Args>
    Exception(const char* file, int line, const char* func, Args&&... args)
        : file_(file), line_(line), func_(func) {
        std::ostringstream oss;
        (print_one(oss, std::forward<Args>(args)), ...);
        message_ = oss.str();
    }

private:
    template <typename T>
    static void print_one(std::ostream& os, T&& arg) {
        if constexpr (requires(std::ostream& s, T a) { s << a; }) {
            os << std::forward<T>(arg);
        } else {
            os << "[unprintable]";
        }
    }

public:
    template <typename... Args>
    static void rethrowNested(Args&&... args) {
        try {
            throw;  // Capture current exception
        } catch (...) {
            std::throw_with_nested(Exception(std::forward<Args>(args)...));
        }
    }

    /**
     * @brief Returns a C-style string describing the exception.
     * @return A pointer to a string describing the exception.
     */
    auto what() const ATOM_NOEXCEPT -> const char* override;

    /**
     * @brief Gets the file where the exception occurred.
     * @return The file where the exception occurred.
     */
    auto getFile() const -> std::string;

    /**
     * @brief Gets the line number where the exception occurred.
     * @return The line number where the exception occurred.
     */
    auto getLine() const -> int;

    /**
     * @brief Gets the function where the exception occurred.
     * @return The function where the exception occurred.
     */
    auto getFunction() const -> std::string;

    /**
     * @brief Gets the message associated with the exception.
     * @return The message associated with the exception.
     */
    auto getMessage() const -> std::string;

    /**
     * @brief Gets the ID of the thread where the exception occurred.
     * @return The ID of the thread where the exception occurred.
     */
    auto getThreadId() const -> std::thread::id;

private:
    std::string file_;
    int line_;
    std::string func_;
    std::string message_;
    mutable std::string full_message_;
    std::thread::id thread_id_ = std::this_thread::get_id();
    StackTrace stack_trace_;
};

/**
 * @brief System error exception class
 */
class SystemErrorException : public Exception {
public:
    SystemErrorException(const char* file, int line, const char* func,
                         int err_code, std::string msg)
        : Exception(file, line, func, msg),
          error_code_(err_code),
          error_message_(
              std::error_code(err_code, std::generic_category()).message()) {}

    const char* what() const noexcept override {
        if (what_message_.empty()) {
            what_message_ = "System error [" + std::to_string(error_code_) +
                            "]: " + error_message_ + "\n" + Exception::what();
        }
        return what_message_.c_str();
    }

    int getErrorCode() const { return error_code_; }
    const std::string& getErrorMessage() const { return error_message_; }

private:
    int error_code_;
    std::string error_message_;
    mutable std::string what_message_;
};

// Exception throwing macros
#define THROW_EXCEPTION(...)                                     \
    throw atom::error::Exception(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_EXCEPTION(...)                                       \
    atom::error::Exception::rethrowNested(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                          ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_SYSTEM_ERROR(error_code, ...)                                 \
    static_assert(std::is_integral<decltype(error_code)>::value,            \
                  "Error code must be an integral type");                   \
    static_assert(error_code != 0, "Error code must be non-zero");          \
    throw atom::error::SystemErrorException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                            ATOM_FUNC_NAME, error_code,     \
                                            __VA_ARGS__)

}  // namespace atom::error

#endif  // ATOM_ERROR_EXCEPTION_BASE_HPP
