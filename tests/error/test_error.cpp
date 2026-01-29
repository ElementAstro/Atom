/*
 * test_error.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Error Module
Tests error handling, stack traces, and exception management.

**************************************************/

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "atom/error/core/error_codes.hpp"
#include "atom/error/exception.hpp"
#include "atom/error/stacktrace.hpp"

namespace atom::error::test {

// ============================================================================
// Exception Tests
// ============================================================================

class ExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ExceptionTest, BasicExceptionHandling) {
    // Test basic exception creation and handling
    try {
        throw std::runtime_error("Test exception");
    } catch (const std::exception& e) {
        EXPECT_STREQ(e.what(), "Test exception");
    }

    // Test custom exception types
    try {
        throw std::invalid_argument("Invalid parameter");
    } catch (const std::invalid_argument& e) {
        EXPECT_STREQ(e.what(), "Invalid parameter");
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

TEST_F(ExceptionTest, AtomExceptionHandling) {
    // Test atom::error::Exception
    try {
        THROW_EXCEPTION("Test atom exception with value: ", 42);
    } catch (const atom::error::Exception& e) {
        std::string what_str = e.what();
        EXPECT_TRUE(what_str.find("Test atom exception with value: 42") !=
                    std::string::npos);
        EXPECT_TRUE(what_str.find("File:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Line:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Function:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Thread ID:") != std::string::npos);
        EXPECT_TRUE(what_str.find("Stack trace:") != std::string::npos);
    }
}

TEST_F(ExceptionTest, AtomSpecificExceptions) {
    // Test RuntimeError
    try {
        THROW_RUNTIME_ERROR("Runtime error test");
    } catch (const atom::error::RuntimeError& e) {
        std::string what_str = e.what();
        EXPECT_TRUE(what_str.find("Runtime error test") != std::string::npos);
    }

    // Test LogicError
    try {
        THROW_LOGIC_ERROR("Logic error test");
    } catch (const atom::error::LogicError& e) {
        std::string what_str = e.what();
        EXPECT_TRUE(what_str.find("Logic error test") != std::string::npos);
    }

    // Test NullPointer
    try {
        THROW_NULL_POINTER("Null pointer test");
    } catch (const atom::error::NullPointer& e) {
        std::string what_str = e.what();
        EXPECT_TRUE(what_str.find("Null pointer test") != std::string::npos);
    }

    // Test NotFound
    try {
        THROW_NOT_FOUND("Not found test");
    } catch (const atom::error::NotFound& e) {
        std::string what_str = e.what();
        EXPECT_TRUE(what_str.find("Not found test") != std::string::npos);
    }
}

TEST_F(ExceptionTest, NestedExceptions) {
    // Test nested exception handling
    try {
        try {
            throw std::runtime_error("Inner exception");
        } catch (...) {
            std::throw_with_nested(std::logic_error("Outer exception"));
        }
    } catch (const std::logic_error& e) {
        EXPECT_STREQ(e.what(), "Outer exception");

        // Check for nested exception
        try {
            std::rethrow_if_nested(e);
        } catch (const std::runtime_error& inner) {
            EXPECT_STREQ(inner.what(), "Inner exception");
        }
    }
}

// ============================================================================
// Stack Trace Tests
// ============================================================================

class StackTraceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper function to create a call stack
    void level3Function() { throw std::runtime_error("Exception at level 3"); }

    void level2Function() { level3Function(); }

    void level1Function() { level2Function(); }
};

TEST_F(StackTraceTest, BasicStackTrace) {
    // Test basic stack trace functionality
    try {
        level1Function();
    } catch (const std::exception& e) {
        EXPECT_STREQ(e.what(), "Exception at level 3");

        // Note: Actual stack trace testing depends on the implementation
        // This test verifies the exception propagates correctly through the
        // call stack
        SUCCEED();
    }
}

TEST_F(StackTraceTest, AtomStackTraceCapture) {
    // Test atom::error::StackTrace directly
    atom::error::StackTrace stackTrace;
    std::string traceStr = stackTrace.toString();

    // Verify that stack trace contains expected elements
    EXPECT_FALSE(traceStr.empty());
    EXPECT_TRUE(traceStr.find("Stack trace:") != std::string::npos);

    // Should contain at least one frame
    EXPECT_TRUE(traceStr.find("[0]") != std::string::npos);
}

TEST_F(StackTraceTest, StackTraceInException) {
    // Test stack trace integration with exceptions
    try {
        THROW_EXCEPTION("Exception with stack trace");
    } catch (const atom::error::Exception& e) {
        std::string what_str = e.what();
        EXPECT_TRUE(what_str.find("Stack trace:") != std::string::npos);
        EXPECT_TRUE(what_str.find("[0]") != std::string::npos);
    }
}

TEST_F(StackTraceTest, StackTraceDepth) {
    // Test stack trace with various depths
    std::vector<std::function<void()>> call_stack;

    // Create a deep call stack
    call_stack.push_back([&]() { call_stack[1](); });

    call_stack.push_back([&]() { call_stack[2](); });

    call_stack.push_back(
        [&]() { throw std::runtime_error("Deep stack exception"); });

    try {
        call_stack[0]();
    } catch (const std::exception& e) {
        EXPECT_STREQ(e.what(), "Deep stack exception");
        SUCCEED();
    }
}

// ============================================================================
// Error Code Tests
// ============================================================================

class ErrorCodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ErrorCodeTest, BasicErrorCodes) {
    // Test basic error code functionality

    // Simulate different error conditions
    enum class TestErrorCode {
        Success = 0,
        InvalidInput = 1,
        FileNotFound = 2,
        NetworkError = 3,
        OutOfMemory = 4
    };

    auto testFunction = [](int input) -> TestErrorCode {
        if (input < 0)
            return TestErrorCode::InvalidInput;
        if (input == 404)
            return TestErrorCode::FileNotFound;
        if (input == 500)
            return TestErrorCode::NetworkError;
        if (input > 1000000)
            return TestErrorCode::OutOfMemory;
        return TestErrorCode::Success;
    };

    // Test various error conditions
    EXPECT_EQ(testFunction(42), TestErrorCode::Success);
    EXPECT_EQ(testFunction(-1), TestErrorCode::InvalidInput);
    EXPECT_EQ(testFunction(404), TestErrorCode::FileNotFound);
    EXPECT_EQ(testFunction(500), TestErrorCode::NetworkError);
    EXPECT_EQ(testFunction(2000000), TestErrorCode::OutOfMemory);
}

TEST_F(ErrorCodeTest, AtomErrorCodes) {
    // Test atom error code enums
    using namespace atom::error;

    // Test FileError enum
    EXPECT_EQ(static_cast<int>(FileError::None), 0);
    EXPECT_EQ(static_cast<int>(FileError::NotFound), 100);
    EXPECT_EQ(static_cast<int>(FileError::OpenError), 101);
    EXPECT_EQ(static_cast<int>(FileError::AccessDenied), 102);

    // Test DeviceError enum
    EXPECT_EQ(static_cast<int>(DeviceError::None), 0);
    EXPECT_EQ(static_cast<int>(DeviceError::NotFound), 201);
    EXPECT_EQ(static_cast<int>(DeviceError::NotSupported), 202);

    // Test MemoryError enum
    EXPECT_EQ(static_cast<int>(MemoryError::None), 0);
    EXPECT_EQ(static_cast<int>(MemoryError::AllocationFailed), 600);
    EXPECT_EQ(static_cast<int>(MemoryError::OutOfMemory), 601);
}

TEST_F(ErrorCodeTest, ErrorCodeMapping) {
    // Test error code to string mapping

    auto errorCodeToString = [](int code) -> std::string {
        switch (code) {
            case 0:
                return "Success";
            case 1:
                return "Invalid Input";
            case 2:
                return "File Not Found";
            case 3:
                return "Network Error";
            case 4:
                return "Out of Memory";
            default:
                return "Unknown Error";
        }
    };

    EXPECT_EQ(errorCodeToString(0), "Success");
    EXPECT_EQ(errorCodeToString(1), "Invalid Input");
    EXPECT_EQ(errorCodeToString(2), "File Not Found");
    EXPECT_EQ(errorCodeToString(3), "Network Error");
    EXPECT_EQ(errorCodeToString(4), "Out of Memory");
    EXPECT_EQ(errorCodeToString(999), "Unknown Error");
}

// ============================================================================
// Integration Tests
// ============================================================================

class ErrorIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ErrorIntegrationTest, CompleteErrorHandling) {
    // Test complete error handling workflow

    auto riskyOperation = [](int mode) -> void {
        switch (mode) {
            case 1:
                throw std::invalid_argument("Invalid argument provided");
            case 2:
                throw std::runtime_error("Runtime error occurred");
            case 3:
                throw std::logic_error("Logic error detected");
            case 4:
                throw std::out_of_range("Index out of range");
            default:
                // Success case
                break;
        }
    };

    // Test successful operation
    EXPECT_NO_THROW(riskyOperation(0));

    // Test various exception types
    EXPECT_THROW(riskyOperation(1), std::invalid_argument);
    EXPECT_THROW(riskyOperation(2), std::runtime_error);
    EXPECT_THROW(riskyOperation(3), std::logic_error);
    EXPECT_THROW(riskyOperation(4), std::out_of_range);

    // Test exception message content
    try {
        riskyOperation(1);
    } catch (const std::invalid_argument& e) {
        EXPECT_STREQ(e.what(), "Invalid argument provided");
    }
}

TEST_F(ErrorIntegrationTest, AtomFileExceptions) {
    // Test file-related exceptions
    EXPECT_THROW(THROW_FILE_NOT_FOUND("test.txt"), atom::error::FileNotFound);
    EXPECT_THROW(THROW_FILE_NOT_READABLE("test.txt"),
                 atom::error::FileNotReadable);
    EXPECT_THROW(THROW_FILE_NOT_WRITABLE("test.txt"),
                 atom::error::FileNotWritable);
    EXPECT_THROW(THROW_FAIL_TO_OPEN_FILE("test.txt"),
                 atom::error::FailToOpenFile);
    EXPECT_THROW(THROW_FAIL_TO_CLOSE_FILE("test.txt"),
                 atom::error::FailToCloseFile);
    EXPECT_THROW(THROW_FAIL_TO_CREATE_FILE("test.txt"),
                 atom::error::FailToCreateFile);
    EXPECT_THROW(THROW_FAIL_TO_DELETE_FILE("test.txt"),
                 atom::error::FailToDeleteFile);
    EXPECT_THROW(THROW_FAIL_TO_COPY_FILE("test.txt"),
                 atom::error::FailToCopyFile);
    EXPECT_THROW(THROW_FAIL_TO_MOVE_FILE("test.txt"),
                 atom::error::FailToMoveFile);
    EXPECT_THROW(THROW_FAIL_TO_READ_FILE("test.txt"),
                 atom::error::FailToReadFile);
    EXPECT_THROW(THROW_FAIL_TO_WRITE_FILE("test.txt"),
                 atom::error::FailToWriteFile);
}

TEST_F(ErrorIntegrationTest, AtomSystemExceptions) {
    // Test system-related exceptions
    EXPECT_THROW(THROW_SYSTEM_ERROR(1, "System error"),
                 atom::error::SystemErrorException);
    EXPECT_THROW(THROW_SYSTEM_COLLAPSE("System collapse"),
                 atom::error::SystemCollapse);
    EXPECT_THROW(THROW_FAIL_TO_LOAD_DLL("test.dll"),
                 atom::error::FailToLoadDll);
    EXPECT_THROW(THROW_FAIL_TO_UNLOAD_DLL("test.dll"),
                 atom::error::FailToUnloadDll);
    EXPECT_THROW(THROW_FAIL_TO_LOAD_SYMBOL("symbol"),
                 atom::error::FailToLoadSymbol);
    EXPECT_THROW(THROW_FAIL_TO_CREATE_PROCESS("process"),
                 atom::error::FailToCreateProcess);
    EXPECT_THROW(THROW_FAIL_TO_TERMINATE_PROCESS("process"),
                 atom::error::FailToTerminateProcess);
}

TEST_F(ErrorIntegrationTest, ErrorRecovery) {
    // Test error recovery mechanisms

    int attempt_count = 0;
    const int max_attempts = 3;
    bool operation_succeeded = false;

    auto unreliableOperation = [&]() -> bool {
        attempt_count++;
        if (attempt_count < max_attempts) {
            throw std::runtime_error("Operation failed, attempt " +
                                     std::to_string(attempt_count));
        }
        return true;
    };

    // Retry mechanism
    for (int i = 0; i < max_attempts; ++i) {
        try {
            operation_succeeded = unreliableOperation();
            break;
        } catch (const std::exception& e) {
            // Log error and continue (in real code, you might add delays)
            EXPECT_TRUE(std::string(e.what()).find("Operation failed") !=
                        std::string::npos);
        }
    }

    EXPECT_TRUE(operation_succeeded);
    EXPECT_EQ(attempt_count, max_attempts);
}

TEST_F(ErrorIntegrationTest, AtomNetworkExceptions) {
    // Test network-related exceptions
    EXPECT_THROW(THROW_JSON_PARSE_ERROR("JSON parse error"),
                 atom::error::JsonParseError);
    EXPECT_THROW(THROW_JSON_VALUE_ERROR("JSON value error"),
                 atom::error::JsonValueError);
    EXPECT_THROW(THROW_CURL_INITIALIZATION_ERROR("CURL init error"),
                 atom::error::CurlInitializationError);
    EXPECT_THROW(THROW_CURL_RUNTIME_ERROR("CURL runtime error"),
                 atom::error::CurlRuntimeError);
}

TEST_F(ErrorIntegrationTest, AllExceptionTypesCoverage) {
    // Comprehensive test for all exception types

    // Basic exceptions
    EXPECT_THROW(THROW_EXCEPTION("Basic exception"), atom::error::Exception);
    EXPECT_THROW(THROW_RUNTIME_ERROR("Runtime error"),
                 atom::error::RuntimeError);
    EXPECT_THROW(THROW_LOGIC_ERROR("Logic error"), atom::error::LogicError);
    EXPECT_THROW(THROW_UNLAWFUL_OPERATION("Unlawful operation"),
                 atom::error::UnlawfulOperation);

    // Range and overflow exceptions
    EXPECT_THROW(THROW_OUT_OF_RANGE("Out of range"), atom::error::OutOfRange);
    EXPECT_THROW(THROW_OVERFLOW("Overflow"), atom::error::OverflowException);
    EXPECT_THROW(THROW_UNDERFLOW("Underflow"), atom::error::UnderflowException);
    EXPECT_THROW(THROW_LENGTH("Length error"), atom::error::LengthException);

    // Object state exceptions
    EXPECT_THROW(THROW_OBJ_ALREADY_EXIST("Object exists"),
                 atom::error::ObjectAlreadyExist);
    EXPECT_THROW(THROW_OBJ_ALREADY_INITIALIZED("Object initialized"),
                 atom::error::ObjectAlreadyInitialized);
    EXPECT_THROW(THROW_OBJ_NOT_EXIST("Object not exist"),
                 atom::error::ObjectNotExist);
    EXPECT_THROW(THROW_OBJ_UNINITIALIZED("Object uninitialized"),
                 atom::error::ObjectUninitialized);

    // Pointer and search exceptions
    EXPECT_THROW(THROW_NULL_POINTER("Null pointer"), atom::error::NullPointer);
    EXPECT_THROW(THROW_NOT_FOUND("Not found"), atom::error::NotFound);
    EXPECT_THROW(THROW_UNKOWN("Unknown error"), atom::error::Unkown);

    // Argument exceptions
    EXPECT_THROW(THROW_WRONG_ARGUMENT("Wrong argument"),
                 atom::error::WrongArgument);
    EXPECT_THROW(THROW_INVALID_ARGUMENT("Invalid argument"),
                 atom::error::InvalidArgument);
    EXPECT_THROW(THROW_MISSING_ARGUMENT("Missing argument"),
                 atom::error::MissingArgument);
}

}  // namespace atom::error::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
