/*
 * file_exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: File-related exception types

**************************************************/

#ifndef ATOM_ERROR_EXCEPTION_FILE_HPP
#define ATOM_ERROR_EXCEPTION_FILE_HPP

#include "exception_base.hpp"

namespace atom::error {

// File not found
class FileNotFound : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FILE_NOT_FOUND(...)                                   \
    throw atom::error::FileNotFound(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                    ATOM_FUNC_NAME, __VA_ARGS__)

// File not readable
class FileNotReadable : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FILE_NOT_READABLE(...)                                   \
    throw atom::error::FileNotReadable(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

// File not writable
class FileNotWritable : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FILE_NOT_WRITABLE(...)                                   \
    throw atom::error::FileNotWritable(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

// Fail to open file
class FailToOpenFile : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_OPEN_FILE(...)                                  \
    throw atom::error::FailToOpenFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// Fail to close file
class FailToCloseFile : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_CLOSE_FILE(...)                                  \
    throw atom::error::FailToCloseFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

// Fail to create file
class FailToCreateFile : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_CREATE_FILE(...)                                  \
    throw atom::error::FailToCreateFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                        ATOM_FUNC_NAME, __VA_ARGS__)

// Fail to delete file
class FailToDeleteFile : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_DELETE_FILE(...)                                  \
    throw atom::error::FailToDeleteFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                        ATOM_FUNC_NAME, __VA_ARGS__)

// Fail to copy file
class FailToCopyFile : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_COPY_FILE(...)                                  \
    throw atom::error::FailToCopyFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// Fail to move file
class FailToMoveFile : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_MOVE_FILE(...)                                  \
    throw atom::error::FailToMoveFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// Fail to read file
class FailToReadFile : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_READ_FILE(...)                                  \
    throw atom::error::FailToReadFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// Fail to write file
class FailToWriteFile : public Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_WRITE_FILE(...)                                  \
    throw atom::error::FailToWriteFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace atom::error

#endif  // ATOM_ERROR_EXCEPTION_FILE_HPP
