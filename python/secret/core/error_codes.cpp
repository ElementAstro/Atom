/*
 * error_codes.cpp
 *
 * Python bindings for error codes, Result types, types, and utilities.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/secret/core/error_codes.hpp"
#include "atom/secret/core/result.hpp"
#include "atom/secret/core/types.hpp"
#include "atom/secret/core/utils.hpp"

namespace py = pybind11;
using namespace atom::secret;

void bind_error_codes(py::module& m) {
    // ErrorCode enum
    py::enum_<ErrorCode>(m, "ErrorCode", R"pbdoc(
        Error codes for the secret module.

        Provides detailed error information for various operations.
        )pbdoc")
        .value("Success", ErrorCode::Success, "Operation succeeded")
        .value("Unknown", ErrorCode::Unknown, "Unknown error")
        .value("InvalidArgument", ErrorCode::InvalidArgument,
               "Invalid argument")
        .value("OutOfMemory", ErrorCode::OutOfMemory, "Out of memory")
        .value("NotImplemented", ErrorCode::NotImplemented, "Not implemented")
        .value("OperationFailed", ErrorCode::OperationFailed,
               "Operation failed")
        // Crypto errors
        .value("EncryptionFailed", ErrorCode::EncryptionFailed,
               "Encryption failed")
        .value("DecryptionFailed", ErrorCode::DecryptionFailed,
               "Decryption failed")
        .value("KeyDerivationFailed", ErrorCode::KeyDerivationFailed,
               "Key derivation failed")
        .value("HashFailed", ErrorCode::HashFailed, "Hash operation failed")
        .value("InvalidKey", ErrorCode::InvalidKey, "Invalid key")
        .value("InvalidIv", ErrorCode::InvalidIv, "Invalid IV")
        .value("InvalidPlaintext", ErrorCode::InvalidPlaintext,
               "Invalid plaintext")
        .value("InvalidCiphertext", ErrorCode::InvalidCiphertext,
               "Invalid ciphertext")
        .value("AuthenticationFailed", ErrorCode::AuthenticationFailed,
               "Authentication failed")
        // Password errors
        .value("PasswordTooShort", ErrorCode::PasswordTooShort,
               "Password too short")
        .value("PasswordTooLong", ErrorCode::PasswordTooLong,
               "Password too long")
        .value("PasswordTooWeak", ErrorCode::PasswordTooWeak,
               "Password too weak")
        .value("PasswordEmpty", ErrorCode::PasswordEmpty, "Password empty")
        .value("PasswordInHistory", ErrorCode::PasswordInHistory,
               "Password in history")
        .value("PasswordBreached", ErrorCode::PasswordBreached,
               "Password breached")
        // Storage errors
        .value("StorageNotInitialized", ErrorCode::StorageNotInitialized,
               "Storage not initialized")
        .value("StorageAlreadyInitialized",
               ErrorCode::StorageAlreadyInitialized,
               "Storage already initialized")
        .value("StorageKeyNotFound", ErrorCode::StorageKeyNotFound,
               "Storage key not found")
        .value("StorageKeyExists", ErrorCode::StorageKeyExists,
               "Storage key exists")
        .value("StorageReadFailed", ErrorCode::StorageReadFailed,
               "Storage read failed")
        .value("StorageWriteFailed", ErrorCode::StorageWriteFailed,
               "Storage write failed")
        // Serialization errors
        .value("SerializationFailed", ErrorCode::SerializationFailed,
               "Serialization failed")
        .value("DeserializationFailed", ErrorCode::DeserializationFailed,
               "Deserialization failed")
        .value("InvalidJson", ErrorCode::InvalidJson, "Invalid JSON")
        // OTP errors
        .value("OtpGenerationFailed", ErrorCode::OtpGenerationFailed,
               "OTP generation failed")
        .value("OtpVerificationFailed", ErrorCode::OtpVerificationFailed,
               "OTP verification failed")
        .value("InvalidSecret", ErrorCode::InvalidSecret, "Invalid secret")
        // Manager errors
        .value("ManagerNotInitialized", ErrorCode::ManagerNotInitialized,
               "Manager not initialized")
        .value("ManagerLocked", ErrorCode::ManagerLocked, "Manager locked")
        .value("EntryNotFound", ErrorCode::EntryNotFound, "Entry not found")
        .value("EntryExists", ErrorCode::EntryExists, "Entry exists")
        .value("SessionExpired", ErrorCode::SessionExpired, "Session expired")
        .export_values();

    // Helper functions
    m.def("error_code_to_string", &errorCodeToString, py::arg("code"),
          R"pbdoc(
        Converts an error code to a human-readable string.

        Args:
            code: The error code to convert.

        Returns:
            str: Human-readable error message.
        )pbdoc");

    m.def("is_success", &isSuccess, py::arg("code"),
          R"pbdoc(
        Checks if an error code indicates success.

        Args:
            code: The error code to check.

        Returns:
            bool: True if the code indicates success.
        )pbdoc");

    m.def("is_error", &isError, py::arg("code"),
          R"pbdoc(
        Checks if an error code indicates an error.

        Args:
            code: The error code to check.

        Returns:
            bool: True if the code indicates an error.
        )pbdoc");
}

void bind_result_types(py::module& m) {
    // Result<std::string>
    py::class_<Result<std::string>>(m, "ResultString",
                                    R"pbdoc(
        Result type for string operations.

        Represents either a successful string value or an error.
        )pbdoc")
        .def("is_success", &Result<std::string>::isSuccess,
             "Returns True if the operation succeeded")
        .def("is_error", &Result<std::string>::isError,
             "Returns True if the operation failed")
        .def("value", &Result<std::string>::value,
             "Returns the string value (raises if error)")
        .def("value_or", &Result<std::string>::valueOr,
             py::arg("default_value"),
             "Returns the value or a default if error")
        .def("error_code", &Result<std::string>::errorCode,
             "Returns the error code")
        .def("error_message", &Result<std::string>::errorMessage,
             "Returns the error message");

    // Result<int>
    py::class_<Result<int>>(m, "ResultInt",
                            R"pbdoc(
        Result type for integer operations.
        )pbdoc")
        .def("is_success", &Result<int>::isSuccess)
        .def("is_error", &Result<int>::isError)
        .def("value", &Result<int>::value)
        .def("value_or", &Result<int>::valueOr, py::arg("default_value"))
        .def("error_code", &Result<int>::errorCode)
        .def("error_message", &Result<int>::errorMessage);

    // Result<bool>
    py::class_<Result<bool>>(m, "ResultBool",
                             R"pbdoc(
        Result type for boolean operations.
        )pbdoc")
        .def("is_success", &Result<bool>::isSuccess)
        .def("is_error", &Result<bool>::isError)
        .def("value", &Result<bool>::value)
        .def("value_or", &Result<bool>::valueOr, py::arg("default_value"))
        .def("error_code", &Result<bool>::errorCode)
        .def("error_message", &Result<bool>::errorMessage);

    // Result<std::vector<uint8_t>>
    py::class_<Result<std::vector<uint8_t>>>(m, "ResultBytes",
                                             R"pbdoc(
        Result type for byte array operations.
        )pbdoc")
        .def("is_success", &Result<std::vector<uint8_t>>::isSuccess)
        .def("is_error", &Result<std::vector<uint8_t>>::isError)
        .def("value", &Result<std::vector<uint8_t>>::value)
        .def("error_code", &Result<std::vector<uint8_t>>::errorCode)
        .def("error_message", &Result<std::vector<uint8_t>>::errorMessage);

    // Result<void>
    py::class_<Result<void>>(m, "ResultVoid",
                             R"pbdoc(
        Result type for void operations.
        )pbdoc")
        .def("is_success", &Result<void>::isSuccess)
        .def("is_error", &Result<void>::isError)
        .def("error_code", &Result<void>::errorCode)
        .def("error_message", &Result<void>::errorMessage);

    // ========================================================================
    // Types and Constants
    // ========================================================================

    // Constants submodule
    auto constants = m.def_submodule("constants", "Cryptographic constants");
    constants.attr("DEFAULT_PBKDF2_ITERATIONS") =
        atom::secret::constants::DEFAULT_PBKDF2_ITERATIONS;
    constants.attr("MIN_PBKDF2_ITERATIONS") =
        atom::secret::constants::MIN_PBKDF2_ITERATIONS;
    constants.attr("DEFAULT_ARGON2_MEMORY_COST") =
        atom::secret::constants::DEFAULT_ARGON2_MEMORY_COST;
    constants.attr("DEFAULT_ARGON2_TIME_COST") =
        atom::secret::constants::DEFAULT_ARGON2_TIME_COST;
    constants.attr("DEFAULT_ARGON2_PARALLELISM") =
        atom::secret::constants::DEFAULT_ARGON2_PARALLELISM;
    constants.attr("DEFAULT_SALT_LENGTH") =
        atom::secret::constants::DEFAULT_SALT_LENGTH;
    constants.attr("DEFAULT_KEY_LENGTH") =
        atom::secret::constants::DEFAULT_KEY_LENGTH;
    constants.attr("AES_BLOCK_SIZE") = atom::secret::constants::AES_BLOCK_SIZE;
    constants.attr("GCM_IV_SIZE") = atom::secret::constants::GCM_IV_SIZE;
    constants.attr("GCM_TAG_SIZE") = atom::secret::constants::GCM_TAG_SIZE;
    constants.attr("CBC_IV_SIZE") = atom::secret::constants::CBC_IV_SIZE;
    constants.attr("CHACHA20_NONCE_SIZE") =
        atom::secret::constants::CHACHA20_NONCE_SIZE;
    constants.attr("POLY1305_TAG_SIZE") =
        atom::secret::constants::POLY1305_TAG_SIZE;
    constants.attr("TOTP_DEFAULT_PERIOD") =
        atom::secret::constants::TOTP_DEFAULT_PERIOD;
    constants.attr("TOTP_DEFAULT_DIGITS") =
        atom::secret::constants::TOTP_DEFAULT_DIGITS;
    constants.attr("HOTP_DEFAULT_DIGITS") =
        atom::secret::constants::HOTP_DEFAULT_DIGITS;
    constants.attr("DEFAULT_AUTO_LOCK_TIMEOUT") =
        atom::secret::constants::DEFAULT_AUTO_LOCK_TIMEOUT;
    constants.attr("DEFAULT_PASSWORD_EXPIRY_DAYS") =
        atom::secret::constants::DEFAULT_PASSWORD_EXPIRY_DAYS;
    constants.attr("MIN_PASSWORD_LENGTH") =
        atom::secret::constants::MIN_PASSWORD_LENGTH;
    constants.attr("DEFAULT_PASSWORD_LENGTH") =
        atom::secret::constants::DEFAULT_PASSWORD_LENGTH;
    constants.attr("MAX_PASSWORD_HISTORY") =
        atom::secret::constants::MAX_PASSWORD_HISTORY;

    // Utility functions from types.hpp
    m.def("bytes_to_hex", &bytesToHex, py::arg("data"),
          py::arg("uppercase") = false, "Converts bytes to hexadecimal string");

    m.def("hex_to_bytes", &hexToBytes, py::arg("hex"),
          "Converts hexadecimal string to bytes");

    m.def("now", &now, "Gets the current time point");

    m.def("to_unix_timestamp", &toUnixTimestamp, py::arg("tp"),
          "Converts time point to Unix timestamp");

    m.def("from_unix_timestamp", &fromUnixTimestamp, py::arg("timestamp"),
          "Converts Unix timestamp to time point");

    // ========================================================================
    // Utils class
    // ========================================================================
    py::class_<Utils>(m, "Utils",
                      R"pbdoc(
        Utility functions for the secret module.
        )pbdoc")
        // Time utilities
        .def_static("format_time_iso8601", &Utils::formatTimeIso8601,
                    py::arg("tp"), "Formats a time point as ISO 8601 string")
        .def_static("parse_time_iso8601", &Utils::parseTimeIso8601,
                    py::arg("str"), "Parses an ISO 8601 string to time point")
        .def_static("format_duration", &Utils::formatDuration,
                    py::arg("seconds"),
                    "Formats a duration as human-readable string")
        // String utilities
        .def_static("trim", &Utils::trim, py::arg("str"),
                    "Trims whitespace from both ends of a string")
        .def_static("to_lower", &Utils::toLower, py::arg("str"),
                    "Converts string to lowercase")
        .def_static("to_upper", &Utils::toUpper, py::arg("str"),
                    "Converts string to uppercase")
        .def_static(
            "contains_ignore_case", &Utils::containsIgnoreCase,
            py::arg("haystack"), py::arg("needle"),
            "Checks if string contains another string (case-insensitive)")
        .def_static("mask", &Utils::mask, py::arg("str"),
                    py::arg("visible_start") = 0, py::arg("visible_end") = 0,
                    py::arg("mask_char") = '*', "Masks a string for display")
        // Validation utilities
        .def_static("is_valid_email", &Utils::isValidEmail, py::arg("email"),
                    "Validates an email address format")
        .def_static("is_valid_url", &Utils::isValidUrl, py::arg("url"),
                    "Validates a URL format")
        // Random utilities
        .def_static("random_int", &Utils::randomInt, py::arg("min"),
                    py::arg("max"),
                    "Generates a random integer in range [min, max]")
        .def_static("shuffle_string", &Utils::shuffleString, py::arg("str"),
                    "Shuffles a string randomly");
}
