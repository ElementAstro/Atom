/*
 * system_exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: System-related exception types (DLL, Process, JSON, Network)

**************************************************/

#ifndef ATOM_ERROR_EXCEPTION_SYSTEM_HPP
#define ATOM_ERROR_EXCEPTION_SYSTEM_HPP

#include "exception_base.hpp"

namespace atom::error {

// -------------------------------------------------------------------
// Dynamic Library Exceptions
// -------------------------------------------------------------------

class FailToLoadDll : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_LOAD_DLL(...)                                  \
    throw atom::error::FailToLoadDll(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

class FailToUnloadDll : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_UNLOAD_DLL(...)                                  \
    throw atom::error::FailToUnloadDll(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

class FailToLoadSymbol : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_LOAD_SYMBOL(...)                                  \
    throw atom::error::FailToLoadSymbol(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                        ATOM_FUNC_NAME, __VA_ARGS__)

// -------------------------------------------------------------------
// Process Exceptions
// -------------------------------------------------------------------

class FailToCreateProcess : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_CREATE_PROCESS(...)                                  \
    throw atom::error::FailToCreateProcess(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                           ATOM_FUNC_NAME, __VA_ARGS__)

class FailToTerminateProcess : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_TERMINATE_PROCESS(...)                                  \
    throw atom::error::FailToTerminateProcess(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                              ATOM_FUNC_NAME, __VA_ARGS__)

// -------------------------------------------------------------------
// JSON Exceptions
// -------------------------------------------------------------------

class JsonParseError : public Exception {
public:
    using Exception::Exception;
};

#define THROW_JSON_PARSE_ERROR(...)                                   \
    throw atom::error::JsonParseError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

class JsonValueError : public Exception {
public:
    using Exception::Exception;
};

#define THROW_JSON_VALUE_ERROR(...)                                   \
    throw atom::error::JsonValueError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// -------------------------------------------------------------------
// Network Exceptions
// -------------------------------------------------------------------

class CurlInitializationError : public Exception {
public:
    using Exception::Exception;
};

#define THROW_CURL_INITIALIZATION_ERROR(...)                                   \
    throw atom::error::CurlInitializationError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                               ATOM_FUNC_NAME, __VA_ARGS__)

class CurlRuntimeError : public Exception {
public:
    using Exception::Exception;
};

#define THROW_CURL_RUNTIME_ERROR(...)                                   \
    throw atom::error::CurlRuntimeError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                        ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace atom::error

#endif  // ATOM_ERROR_EXCEPTION_SYSTEM_HPP
