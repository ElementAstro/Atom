// exception.h
#pragma once

#include <format>
#include <source_location>
#include <stdexcept>
#include <string>


namespace serastro {

// Base exception class for all SER-related errors
class SERException : public std::runtime_error {
public:
    explicit SERException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : std::runtime_error(std::format("{}:{} in {}: {}",
                                         location.file_name(), location.line(),
                                         location.function_name(), message)) {}
};

// File IO related errors
class SERIOException : public SERException {
public:
    explicit SERIOException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : SERException(message, location) {}
};

// Format-related errors
class SERFormatException : public SERException {
public:
    explicit SERFormatException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : SERException(message, location) {}
};

// Processing errors
class ProcessingException : public SERException {
public:
    explicit ProcessingException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : SERException(message, location) {}
};

// Invalid parameter errors
class InvalidParameterException : public SERException {
public:
    explicit InvalidParameterException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : SERException(message, location) {}

    static InvalidParameterException outOfRange(
        const std::string& paramName, double value, double min, double max,
        const std::source_location& location =
            std::source_location::current()) {
        return InvalidParameterException(
            std::format("Parameter '{}' value {} is out of range [{}, {}]",
                        paramName, value, min, max),
            location);
    }
};

// Resource errors (memory, GPU, etc.)
class ResourceException : public SERException {
public:
    explicit ResourceException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : SERException(message, location) {}
};

}  // namespace serastro
