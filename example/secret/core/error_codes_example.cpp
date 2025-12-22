/*
 * error_codes_example.cpp
 *
 * Demonstrates the usage of error codes and Result type in the Atom Secret
 * module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>
#include <string>

#include "atom/secret/core/error_codes.hpp"
#include "atom/secret/core/result.hpp"

using namespace atom::secret;

// Example function that returns a ResultResult<int> divide(int a, int b) {
if (b == 0) {
    return Result<int>::error(ErrorCode::InvalidArgument, "Division by zero");
}
return Result<int>::success(a / b);
}

// Example function that returns a Result with stringResult<std::string>
// processData(const std::string& input) {
if (input.empty()) {
    return Result<std::string>::error(ErrorCode::InvalidArgument,
                                      "Input cannot be empty");
}
if (input.length() < 3) {
    return Result<std::string>::error(ErrorCode::InvalidArgument,
                                      "Input too short");
}
return Result<std::string>::success("Processed: " + input);
}

// Example function that returns void ResultResult<void> validatePassword(const
// std::string& password) {
if (password.empty()) {
    return Result<void>::error(ErrorCode::PasswordEmpty,
                               "Password cannot be empty");
}
if (password.length() < 8) {
    return Result<void>::error(ErrorCode::PasswordTooShort,
                               "Password must be at least 8 characters");
}
return Result<void>::success();
}

int main() {
    std::cout << "=== Atom Secret Error Codes Example ===" << std::endl
              << std::endl;

    // ========================================================================
    // Error Code to String Conversion
    // ========================================================================
    std::cout << "--- Error Code Strings ---" << std::endl;
    std::cout << "Success: " << errorCodeToString(ErrorCode::Success)
              << std::endl;
    std::cout << "InvalidArgument: "
              << errorCodeToString(ErrorCode::InvalidArgument) << std::endl;
    std::cout << "EncryptionFailed: "
              << errorCodeToString(ErrorCode::EncryptionFailed) << std::endl;
    std::cout << "PasswordTooShort: "
              << errorCodeToString(ErrorCode::PasswordTooShort) << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Using isSuccess and isError
    // ========================================================================
    std::cout << "--- Error Code Checks ---" << std::endl;
    std::cout << "isSuccess(Success): "
              << (isSuccess(ErrorCode::Success) ? "true" : "false")
              << std::endl;
    std::cout << "isError(InvalidArgument): "
              << (isError(ErrorCode::InvalidArgument) ? "true" : "false")
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Result Type Usage
    // ========================================================================
    std::cout << "--- Result Type Examples ---" << std::endl;

    // Successful division
    auto result1 = divide(10, 2);
    if (result1.isSuccess()) {
        std::cout << "10 / 2 = " << result1.value() << std::endl;
    }

    // Failed division
    auto result2 = divide(10, 0);
    if (result2.isError()) {
        std::cout << "Division error: " << result2.errorMessage() << std::endl;
        std::cout << "Error code: " << errorCodeToString(result2.errorCode())
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // String Result
    // ========================================================================
    std::cout << "--- String Result Examples ---" << std::endl;

    auto result3 = processData("Hello World");
    if (result3.isSuccess()) {
        std::cout << result3.value() << std::endl;
    }

    auto result4 = processData("");
    if (result4.isError()) {
        std::cout << "Process error: " << result4.errorMessage() << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Void Result
    // ========================================================================
    std::cout << "--- Void Result Examples ---" << std::endl;

    auto result5 = validatePassword("SecurePassword123!");
    if (result5.isSuccess()) {
        std::cout << "Password is valid!" << std::endl;
    }

    auto result6 = validatePassword("short");
    if (result6.isError()) {
        std::cout << "Validation error: " << result6.errorMessage()
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Using valueOr
    // ========================================================================
    std::cout << "--- Using valueOr ---" << std::endl;

    auto successResult = divide(20, 4);
    auto errorResult = divide(20, 0);

    std::cout << "Success valueOr(0): " << successResult.valueOr(0)
              << std::endl;
    std::cout << "Error valueOr(0): " << errorResult.valueOr(0) << std::endl;

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
