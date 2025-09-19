/*
 * comprehensive_test.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive test for atom-error component
Tests all major functionality to ensure completeness.

**************************************************/

#include <gtest/gtest.h>
#include <iostream>
#include <string>

#include "atom/error/exception.hpp"
#include "atom/error/stacktrace.hpp"
#include "atom/error/error_code.hpp"

namespace atom::error::test {

class ComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ComprehensiveTest, AllExceptionTypes) {
    // Test all exception types defined in atom-error
    
    // Basic exceptions
    EXPECT_THROW(THROW_EXCEPTION("Basic exception"), Exception);
    EXPECT_THROW(THROW_RUNTIME_ERROR("Runtime error"), RuntimeError);
    EXPECT_THROW(THROW_LOGIC_ERROR("Logic error"), LogicError);
    EXPECT_THROW(THROW_UNLAWFUL_OPERATION("Unlawful operation"), UnlawfulOperation);
    
    // Range and overflow exceptions
    EXPECT_THROW(THROW_OUT_OF_RANGE("Out of range"), OutOfRange);
    EXPECT_THROW(THROW_OVERFLOW("Overflow"), OverflowException);
    EXPECT_THROW(THROW_UNDERFLOW("Underflow"), UnderflowException);
    EXPECT_THROW(THROW_LENGTH("Length error"), LengthException);
    
    // Object state exceptions
    EXPECT_THROW(THROW_OBJ_ALREADY_EXIST("Object exists"), ObjectAlreadyExist);
    EXPECT_THROW(THROW_OBJ_ALREADY_INITIALIZED("Object initialized"), ObjectAlreadyInitialized);
    EXPECT_THROW(THROW_OBJ_NOT_EXIST("Object not exist"), ObjectNotExist);
    EXPECT_THROW(THROW_OBJ_UNINITIALIZED("Object uninitialized"), ObjectUninitialized);
    
    // Pointer and search exceptions
    EXPECT_THROW(THROW_NULL_POINTER("Null pointer"), NullPointer);
    EXPECT_THROW(THROW_NOT_FOUND("Not found"), NotFound);
    EXPECT_THROW(THROW_UNKOWN("Unknown error"), Unkown);
    
    // Argument exceptions
    EXPECT_THROW(THROW_WRONG_ARGUMENT("Wrong argument"), WrongArgument);
    EXPECT_THROW(THROW_INVALID_ARGUMENT("Invalid argument"), InvalidArgument);
    EXPECT_THROW(THROW_MISSING_ARGUMENT("Missing argument"), MissingArgument);
}

TEST_F(ComprehensiveTest, AllFileExceptions) {
    // Test all file-related exceptions
    
    EXPECT_THROW(THROW_FILE_NOT_FOUND("file.txt"), FileNotFound);
    EXPECT_THROW(THROW_FILE_NOT_READABLE("file.txt"), FileNotReadable);
    EXPECT_THROW(THROW_FILE_NOT_WRITABLE("file.txt"), FileNotWritable);
    EXPECT_THROW(THROW_FAIL_TO_OPEN_FILE("file.txt"), FailToOpenFile);
    EXPECT_THROW(THROW_FAIL_TO_CLOSE_FILE("file.txt"), FailToCloseFile);
    EXPECT_THROW(THROW_FAIL_TO_CREATE_FILE("file.txt"), FailToCreateFile);
    EXPECT_THROW(THROW_FAIL_TO_DELETE_FILE("file.txt"), FailToDeleteFile);
    EXPECT_THROW(THROW_FAIL_TO_COPY_FILE("file.txt"), FailToCopyFile);
    EXPECT_THROW(THROW_FAIL_TO_MOVE_FILE("file.txt"), FailToMoveFile);
    EXPECT_THROW(THROW_FAIL_TO_READ_FILE("file.txt"), FailToReadFile);
    EXPECT_THROW(THROW_FAIL_TO_WRITE_FILE("file.txt"), FailToWriteFile);
}

TEST_F(ComprehensiveTest, AllSystemExceptions) {
    // Test all system-related exceptions
    
    EXPECT_THROW(THROW_SYSTEM_ERROR(1, "System error"), SystemErrorException);
    EXPECT_THROW(THROW_SYSTEM_COLLAPSE("System collapse"), SystemCollapse);
    EXPECT_THROW(THROW_FAIL_TO_LOAD_DLL("library.dll"), FailToLoadDll);
    EXPECT_THROW(THROW_FAIL_TO_UNLOAD_DLL("library.dll"), FailToUnloadDll);
    EXPECT_THROW(THROW_FAIL_TO_LOAD_SYMBOL("symbol"), FailToLoadSymbol);
    EXPECT_THROW(THROW_FAIL_TO_CREATE_PROCESS("process"), FailToCreateProcess);
    EXPECT_THROW(THROW_FAIL_TO_TERMINATE_PROCESS("process"), FailToTerminateProcess);
}

TEST_F(ComprehensiveTest, AllNetworkExceptions) {
    // Test all network-related exceptions
    
    EXPECT_THROW(THROW_JSON_PARSE_ERROR("JSON parse error"), JsonParseError);
    EXPECT_THROW(THROW_JSON_VALUE_ERROR("JSON value error"), JsonValueError);
    EXPECT_THROW(THROW_CURL_INITIALIZATION_ERROR("CURL init error"), CurlInitializationError);
    EXPECT_THROW(THROW_CURL_RUNTIME_ERROR("CURL runtime error"), CurlRuntimeError);
}

TEST_F(ComprehensiveTest, StackTraceIntegration) {
    // Test stack trace integration with all exception types
    
    try {
        THROW_EXCEPTION("Test exception with stack trace");
        FAIL() << "Exception should have been thrown";
    } catch (const Exception& e) {
        std::string what_str = e.what();
        
        // Verify all expected components are present
        EXPECT_TRUE(what_str.find("Exception occurred:") != std::string::npos);
        EXPECT_TRUE(what_str.find("File:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Line:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Function:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Thread ID:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Message:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Stack trace:") != std::string::npos);
        
        // Should contain at least one stack frame
        EXPECT_TRUE(what_str.find("[0]") != std::string::npos);
    }
}

TEST_F(ComprehensiveTest, ErrorCodeEnums) {
    // Test all error code enums are properly defined
    
    // FileError
    EXPECT_EQ(static_cast<int>(FileError::None), 0);
    EXPECT_NE(static_cast<int>(FileError::NotFound), 0);
    EXPECT_NE(static_cast<int>(FileError::OpenError), 0);
    
    // DeviceError
    EXPECT_EQ(static_cast<int>(DeviceError::None), 0);
    EXPECT_NE(static_cast<int>(DeviceError::NotFound), 0);
    EXPECT_NE(static_cast<int>(DeviceError::NotSupported), 0);
    
    // MemoryError
    EXPECT_EQ(static_cast<int>(MemoryError::None), 0);
    EXPECT_NE(static_cast<int>(MemoryError::AllocationFailed), 0);
    EXPECT_NE(static_cast<int>(MemoryError::OutOfMemory), 0);
    
    // UserInputError
    EXPECT_EQ(static_cast<int>(UserInputError::None), 0);
    EXPECT_NE(static_cast<int>(UserInputError::InvalidInput), 0);
    EXPECT_NE(static_cast<int>(UserInputError::OutOfRange), 0);
    
    // ServerError
    EXPECT_EQ(static_cast<int>(ServerError::None), 0);
    EXPECT_NE(static_cast<int>(ServerError::InvalidParameters), 0);
    EXPECT_NE(static_cast<int>(ServerError::NetworkError), 0);
}

TEST_F(ComprehensiveTest, NestedExceptionHandling) {
    // Test nested exception functionality
    
    try {
        try {
            THROW_EXCEPTION("Inner exception");
        } catch (...) {
            THROW_NESTED_EXCEPTION("Outer exception");
        }
        FAIL() << "Exception should have been thrown";
    } catch (const Exception& e) {
        std::string what_str = e.what();
        EXPECT_TRUE(what_str.find("Outer exception") != std::string::npos);
        // Nested exception handling is implementation-specific
        SUCCEED();
    }
}

TEST_F(ComprehensiveTest, SystemErrorException) {
    // Test system error exception with error codes
    
    try {
        THROW_SYSTEM_ERROR(ENOENT, "File not found");
        FAIL() << "Exception should have been thrown";
    } catch (const SystemErrorException& e) {
        std::string what_str = e.what();
        EXPECT_TRUE(what_str.find("System error") != std::string::npos);
        EXPECT_TRUE(what_str.find("File not found") != std::string::npos);
    }
}

} // namespace atom::error::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
