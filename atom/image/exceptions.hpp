/*
 * exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-24

Description: Unified exception types for atom::image module

**************************************************/

#ifndef ATOM_IMAGE_EXCEPTIONS_HPP
#define ATOM_IMAGE_EXCEPTIONS_HPP

#include "atom/error/exception.hpp"

namespace atom::image {

// ============================================================================
// Base Image Exception
// ============================================================================

/**
 * @brief Base exception class for all image-related errors
 */
class ImageException : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_IMAGE_EXCEPTION(...)                                    \
    throw atom::image::ImageException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// ============================================================================
// Format Exceptions
// ============================================================================

/**
 * @brief Exception for image format errors
 */
class FormatException : public ImageException {
public:
    using ImageException::ImageException;
    explicit FormatException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit FormatException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_FORMAT_EXCEPTION(...)                                    \
    throw atom::image::FormatException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for unsupported format errors
 */
class UnsupportedFormatException : public FormatException {
public:
    using FormatException::FormatException;
    explicit UnsupportedFormatException(const std::string& message)
        : FormatException("", 0, "", message) {}
    explicit UnsupportedFormatException(const char* message)
        : FormatException("", 0, "", message) {}
};

#define THROW_UNSUPPORTED_FORMAT(...)              \
    throw atom::image::UnsupportedFormatException( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

// ============================================================================
// I/O Exceptions
// ============================================================================

/**
 * @brief Exception for image loading errors
 */
class LoadException : public ImageException {
public:
    using ImageException::ImageException;
    explicit LoadException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit LoadException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_LOAD_EXCEPTION(...)                                    \
    throw atom::image::LoadException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for image saving errors
 */
class SaveException : public ImageException {
public:
    using ImageException::ImageException;
    explicit SaveException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit SaveException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_SAVE_EXCEPTION(...)                                    \
    throw atom::image::SaveException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for general I/O errors
 */
class IOException : public ImageException {
public:
    using ImageException::ImageException;
    explicit IOException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit IOException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_IO_EXCEPTION(...)                                    \
    throw atom::image::IOException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                   ATOM_FUNC_NAME, __VA_ARGS__)

// ============================================================================
// Processing Exceptions
// ============================================================================

/**
 * @brief Exception for image processing errors
 */
class ProcessingException : public ImageException {
public:
    using ImageException::ImageException;
    explicit ProcessingException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit ProcessingException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_PROCESSING_EXCEPTION(...)                                    \
    throw atom::image::ProcessingException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                           ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for codec errors
 */
class CodecException : public ProcessingException {
public:
    using ProcessingException::ProcessingException;
    explicit CodecException(const std::string& message)
        : ProcessingException("", 0, "", message) {}
    explicit CodecException(const char* message)
        : ProcessingException("", 0, "", message) {}
};

#define THROW_CODEC_EXCEPTION(...)                                    \
    throw atom::image::CodecException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for unsupported operations
 */
class UnsupportedOperationException : public ProcessingException {
public:
    using ProcessingException::ProcessingException;
    explicit UnsupportedOperationException(const std::string& message)
        : ProcessingException("", 0, "", message) {}
    explicit UnsupportedOperationException(const char* message)
        : ProcessingException("", 0, "", message) {}
};

#define THROW_UNSUPPORTED_OPERATION(...)              \
    throw atom::image::UnsupportedOperationException( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

// ============================================================================
// Data Exceptions
// ============================================================================

/**
 * @brief Exception for validation errors
 */
class ValidationException : public ImageException {
public:
    using ImageException::ImageException;
    explicit ValidationException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit ValidationException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_VALIDATION_EXCEPTION(...)                                    \
    throw atom::image::ValidationException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                           ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for out of bounds errors
 */
class OutOfBoundsException : public ImageException {
public:
    using ImageException::ImageException;
    explicit OutOfBoundsException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit OutOfBoundsException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_OUT_OF_BOUNDS(...)                                            \
    throw atom::image::OutOfBoundsException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                            ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for memory allocation errors
 */
class MemoryException : public ImageException {
public:
    using ImageException::ImageException;
    explicit MemoryException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit MemoryException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_MEMORY_EXCEPTION(...)                                    \
    throw atom::image::MemoryException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

// ============================================================================
// Configuration Exceptions
// ============================================================================

/**
 * @brief Exception for configuration errors
 */
class ConfigurationException : public ImageException {
public:
    using ImageException::ImageException;
};

#define THROW_CONFIGURATION_EXCEPTION(...)                                    \
    throw atom::image::ConfigurationException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                              ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for timeout errors
 */
class TimeoutException : public ImageException {
public:
    using ImageException::ImageException;
};

#define THROW_TIMEOUT_EXCEPTION(...)                                    \
    throw atom::image::TimeoutException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                        ATOM_FUNC_NAME, __VA_ARGS__)

// ============================================================================
// FITS-specific Exceptions
// ============================================================================

/**
 * @brief Exception for FITS data errors
 */
class FITSException : public FormatException {
public:
    using FormatException::FormatException;

    /**
     * @brief Construct with simple message (for backward compatibility)
     */
    explicit FITSException(const std::string& message)
        : FormatException("", 0, "", message) {}
    explicit FITSException(const char* message)
        : FormatException("", 0, "", message) {}

    /**
     * @brief Construct with error code and message (for backward compatibility)
     */
    template <typename ErrorCode>
    FITSException(ErrorCode /*code*/, const std::string& message)
        : FormatException("", 0, "", message) {}
    template <typename ErrorCode>
    FITSException(ErrorCode /*code*/, const char* message)
        : FormatException("", 0, "", message) {}
};

#define THROW_FITS_EXCEPTION(...)                                    \
    throw atom::image::FITSException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for HDU errors
 */
class HDUException : public FITSException {
public:
    using FITSException::FITSException;
};

#define THROW_HDU_EXCEPTION(...)                                    \
    throw atom::image::HDUException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                    ATOM_FUNC_NAME, __VA_ARGS__)

// ============================================================================
// Metadata Exceptions
// ============================================================================

/**
 * @brief Exception for EXIF parsing errors
 */
class ExifException : public ImageException {
public:
    using ImageException::ImageException;
};

#define THROW_EXIF_EXCEPTION(...)                                    \
    throw atom::image::ExifException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for buffer read errors
 */
class BufferReadException : public ImageException {
public:
    using ImageException::ImageException;

    /**
     * @brief Construct with simple message (for backward compatibility)
     */
    explicit BufferReadException(const std::string& message)
        : ImageException("", 0, "", message) {}
    explicit BufferReadException(const char* message)
        : ImageException("", 0, "", message) {}
};

#define THROW_BUFFER_READ_EXCEPTION(...)                                   \
    throw atom::image::BufferReadException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                           ATOM_FUNC_NAME, __VA_ARGS__)

// ============================================================================
// Backward Compatibility Aliases (deprecated, will be removed)
// ============================================================================

namespace core {
// Aliases to maintain backward compatibility during transition
using ImageException = atom::image::ImageException;
using FormatException = atom::image::FormatException;
using LoadException = atom::image::LoadException;
using SaveException = atom::image::SaveException;
using ProcessingException = atom::image::ProcessingException;
using MemoryException = atom::image::MemoryException;
using ValidationException = atom::image::ValidationException;
using IOException = atom::image::IOException;
using CodecException = atom::image::CodecException;
using OutOfBoundsException = atom::image::OutOfBoundsException;
using UnsupportedOperationException =
    atom::image::UnsupportedOperationException;
using TimeoutException = atom::image::TimeoutException;
using ConfigurationException = atom::image::ConfigurationException;
}  // namespace core

}  // namespace atom::image

#endif  // ATOM_IMAGE_EXCEPTIONS_HPP
