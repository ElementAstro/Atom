#pragma once

#include <stdexcept>
#include <string>

namespace dotenv {

/**
 * @brief Base exception class for all dotenv-related errors
 */
class DotenvException : public std::runtime_error {
public:
    explicit DotenvException(const std::string& message)
        : std::runtime_error("Dotenv Error: " + message) {}
};

/**
 * @brief Exception thrown when file operations fail
 */
class FileException : public DotenvException {
public:
    explicit FileException(const std::string& message)
        : DotenvException("File Error: " + message) {}
};

/**
 * @brief Exception thrown when parsing fails
 */
class ParseException : public DotenvException {
public:
    explicit ParseException(const std::string& message, size_t line_number = 0)
        : DotenvException("Parse Error at line " + std::to_string(line_number) +
                          ": " + message) {}
};

/**
 * @brief Exception thrown when validation fails
 */
class ValidationException : public DotenvException {
public:
    explicit ValidationException(const std::string& message)
        : DotenvException("Validation Error: " + message) {}
};

}  // namespace dotenv