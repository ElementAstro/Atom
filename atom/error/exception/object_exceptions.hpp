/*
 * object_exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Object-related exception types

**************************************************/

#ifndef ATOM_ERROR_EXCEPTION_OBJECT_HPP
#define ATOM_ERROR_EXCEPTION_OBJECT_HPP

#include "exception_base.hpp"

namespace atom::error {

// Object already exists
class ObjectAlreadyExist : public Exception {
public:
    using Exception::Exception;
};

#define THROW_OBJ_ALREADY_EXIST(...)                                      \
    throw atom::error::ObjectAlreadyExist(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                          ATOM_FUNC_NAME, __VA_ARGS__)

// Object already initialized
class ObjectAlreadyInitialized : public Exception {
public:
    using Exception::Exception;
};

#define THROW_OBJ_ALREADY_INITIALIZED(...)       \
    throw atom::error::ObjectAlreadyInitialized( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

// Object not exist
class ObjectNotExist : public Exception {
public:
    using Exception::Exception;
};

#define THROW_OBJ_NOT_EXIST(...)                                      \
    throw atom::error::ObjectNotExist(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// Object uninitialized
class ObjectUninitialized : public Exception {
public:
    using Exception::Exception;
};

#define THROW_OBJ_UNINITIALIZED(...)                                       \
    throw atom::error::ObjectUninitialized(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                           ATOM_FUNC_NAME, __VA_ARGS__)

// System collapse
class SystemCollapse : public Exception {
public:
    using Exception::Exception;
};

#define THROW_SYSTEM_COLLAPSE(...)                                    \
    throw atom::error::SystemCollapse(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// Null pointer
class NullPointer : public Exception {
public:
    using Exception::Exception;
};

#define THROW_NULL_POINTER(...)                                    \
    throw atom::error::NullPointer(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                   ATOM_FUNC_NAME, __VA_ARGS__)

// Not found
class NotFound : public Exception {
public:
    using Exception::Exception;
};

#define THROW_NOT_FOUND(...)                                    \
    throw atom::error::NotFound(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace atom::error

#endif  // ATOM_ERROR_EXCEPTION_OBJECT_HPP
