/*
 * argument_exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Argument-related exception types

**************************************************/

#ifndef ATOM_ERROR_EXCEPTION_ARGUMENT_HPP
#define ATOM_ERROR_EXCEPTION_ARGUMENT_HPP

#include "exception_base.hpp"

namespace atom::error {

// Wrong argument
class WrongArgument : public Exception {
public:
    using Exception::Exception;
};

#define THROW_WRONG_ARGUMENT(...)                                    \
    throw atom::error::WrongArgument(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

// Invalid argument
class InvalidArgument : public Exception {
public:
    using Exception::Exception;
};

#define THROW_INVALID_ARGUMENT(...)                                    \
    throw atom::error::InvalidArgument(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

// Missing argument
class MissingArgument : public Exception {
public:
    using Exception::Exception;
};

#define THROW_MISSING_ARGUMENT(...)                                    \
    throw atom::error::MissingArgument(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace atom::error

#endif  // ATOM_ERROR_EXCEPTION_ARGUMENT_HPP
