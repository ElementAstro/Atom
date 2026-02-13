// rust_error.hpp
#pragma once

#include <string>

namespace atom::algorithm {

enum class ErrorKind {
    ParseIntError,
    ParseFloatError,
    DivideByZero,
    NumericOverflow,
    NumericUnderflow,
    InvalidOperation,
};

class Error {
private:
    ErrorKind m_kind;
    std::string m_message;

public:
    Error(ErrorKind kind, const std::string& message)
        : m_kind(kind), m_message(message) {}

    ErrorKind kind() const { return m_kind; }
    const std::string& message() const { return m_message; }

    std::string to_string() const {
        std::string kind_str;
        switch (m_kind) {
            case ErrorKind::ParseIntError:
                kind_str = "ParseIntError";
                break;
            case ErrorKind::ParseFloatError:
                kind_str = "ParseFloatError";
                break;
            case ErrorKind::DivideByZero:
                kind_str = "DivideByZero";
                break;
            case ErrorKind::NumericOverflow:
                kind_str = "NumericOverflow";
                break;
            case ErrorKind::NumericUnderflow:
                kind_str = "NumericUnderflow";
                break;
            case ErrorKind::InvalidOperation:
                kind_str = "InvalidOperation";
                break;
        }
        return kind_str + ": " + m_message;
    }
};

}  // namespace atom::algorithm
