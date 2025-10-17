#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

namespace atom::image::core {

/**
 * @brief Base exception class for image processing errors
 */
class ImageException : public std::runtime_error {
public:
    explicit ImageException(const std::string& message)
        : std::runtime_error("Image Error: " + message) {}

    explicit ImageException(const std::string& message, std::error_code ec)
        : std::runtime_error("Image Error: " + message + " (" + ec.message() + ")"),
          errorCode_(ec) {}

    std::error_code errorCode() const { return errorCode_; }

private:
    std::error_code errorCode_;
};

/**
 * @brief Exception thrown for image format errors
 */
class FormatException : public ImageException {
public:
    explicit FormatException(const std::string& message)
        : ImageException("Format error: " + message) {}
};

/**
 * @brief Exception thrown for image loading errors
 */
class LoadException : public ImageException {
public:
    explicit LoadException(const std::string& message)
        : ImageException("Load error: " + message) {}
};

/**
 * @brief Exception thrown for image saving errors
 */
class SaveException : public ImageException {
public:
    explicit SaveException(const std::string& message)
        : ImageException("Save error: " + message) {}
};

/**
 * @brief Exception thrown for image processing errors
 */
class ProcessingException : public ImageException {
public:
    explicit ProcessingException(const std::string& message)
        : ImageException("Processing error: " + message) {}
};

/**
 * @brief Exception thrown for memory allocation errors
 */
class MemoryException : public ImageException {
public:
    explicit MemoryException(const std::string& message)
        : ImageException("Memory error: " + message) {}
};

/**
 * @brief Exception thrown for validation errors
 */
class ValidationException : public ImageException {
public:
    explicit ValidationException(const std::string& message)
        : ImageException("Validation error: " + message) {}
};

/**
 * @brief Exception thrown for I/O errors
 */
class IOException : public ImageException {
public:
    explicit IOException(const std::string& message)
        : ImageException("I/O error: " + message) {}
};

/**
 * @brief Exception thrown for codec errors
 */
class CodecException : public ImageException {
public:
    explicit CodecException(const std::string& message)
        : ImageException("Codec error: " + message) {}
};

/**
 * @brief Exception thrown for out of bounds errors
 */
class OutOfBoundsException : public ImageException {
public:
    explicit OutOfBoundsException(const std::string& message)
        : ImageException("Out of bounds: " + message) {}
};

/**
 * @brief Exception thrown for unsupported operation errors
 */
class UnsupportedOperationException : public ImageException {
public:
    explicit UnsupportedOperationException(const std::string& message)
        : ImageException("Unsupported operation: " + message) {}
};

/**
 * @brief Exception thrown for timeout errors
 */
class TimeoutException : public ImageException {
public:
    explicit TimeoutException(const std::string& message)
        : ImageException("Timeout: " + message) {}
};

/**
 * @brief Exception thrown for configuration errors
 */
class ConfigurationException : public ImageException {
public:
    explicit ConfigurationException(const std::string& message)
        : ImageException("Configuration error: " + message) {}
};

} // namespace atom::image::core