/*
 * common_exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Common exception types

**************************************************/

#ifndef ATOM_ERROR_EXCEPTION_COMMON_HPP
#define ATOM_ERROR_EXCEPTION_COMMON_HPP

#include "exception_base.hpp"

namespace atom::error {

// Helper trait for printable types
namespace internal {
template <typename... Args>
struct are_all_printable;

template <>
struct are_all_printable<> {
    static constexpr bool value = true;
};

template <typename First, typename... Rest>
struct are_all_printable<First, Rest...> {
    static constexpr bool value =
        std::is_convertible<decltype(std::declval<std::ostream&>()
                                     << std::declval<First>()),
                            std::ostream&>::value &&
        are_all_printable<Rest...>::value;
};
}  // namespace internal

// Runtime error
class RuntimeError : public Exception {
public:
    using Exception::Exception;
};

#define THROW_RUNTIME_ERROR(...)                                    \
    throw atom::error::RuntimeError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                    ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_RUNTIME_ERROR(...)                                      \
    atom::error::RuntimeError::rethrowNested(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                             ATOM_FUNC_NAME, __VA_ARGS__)

// Logic error
class LogicError : public Exception {
public:
    using Exception::Exception;
};

#define THROW_LOGIC_ERROR(...)                                    \
    throw atom::error::LogicError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__)

// Unlawful operation
class UnlawfulOperation : public Exception {
public:
    using Exception::Exception;
};

#define THROW_UNLAWFUL_OPERATION(...)                                    \
    throw atom::error::UnlawfulOperation(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                         ATOM_FUNC_NAME, __VA_ARGS__)

// Out of range
class OutOfRange : public Exception {
public:
    using Exception::Exception;
};

#define THROW_OUT_OF_RANGE(...)                                   \
    throw atom::error::OutOfRange(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__);

// Overflow
class OverflowException : public Exception {
public:
    using Exception::Exception;
};

#define THROW_OVERFLOW(...)                                              \
    throw atom::error::OverflowException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                         ATOM_FUNC_NAME, __VA_ARGS__);

// Underflow
class UnderflowException : public Exception {
public:
    using Exception::Exception;
};

#define THROW_UNDERFLOW(...)                                              \
    throw atom::error::UnderflowException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                          ATOM_FUNC_NAME, __VA_ARGS__);

// Length error
class LengthException : public Exception {
public:
    using Exception::Exception;
};

#define THROW_LENGTH(...)                                              \
    throw atom::error::LengthException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__);

// Unknown error
class Unkown : public Exception {
public:
    using Exception::Exception;
};

#define THROW_UNKOWN(...)                                                     \
    throw atom::error::Unkown(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                              __VA_ARGS__);

}  // namespace atom::error

#endif  // ATOM_ERROR_EXCEPTION_COMMON_HPP
