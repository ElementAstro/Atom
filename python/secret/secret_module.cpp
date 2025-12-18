/*
 * secret_module.cpp
 *
 * Main Python module for Atom Secret.
 * Combines all submodule bindings.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declarations of binding functions
void bind_error_codes(py::module& m);
void bind_result_types(py::module& m);
void bind_crypto(py::module& m);
void bind_password(py::module& m);
void bind_otp(py::module& m);
void bind_storage(py::module& m);
void bind_manager(py::module& m);
void bind_serialization(py::module& m);

PYBIND11_MODULE(secret, m) {
    m.doc() = R"pbdoc(
        Atom Secret Module - Secure Password Management
        ===============================================

        This module provides comprehensive password management and cryptographic
        utilities for Python applications.

        Submodules:
        ----------
        - core: Error codes and result types
        - crypto: Encryption, hashing, and key derivation
        - password: Password generation, validation, and breach checking
        - otp: TOTP and HOTP one-time passwords
        - storage: Secure storage backends
        - manager: Password manager and session management
        - serialization: JSON serialization

        Quick Start:
        -----------
        >>> from atom.secret import (
        ...     PasswordManager, PasswordGenerator, PasswordValidator,
        ...     PasswordEntry, PasswordCategory, Encryption
        ... )
        >>>
        >>> # Generate a secure password
        >>> result = PasswordGenerator.generate()
        >>> if result.is_success():
        ...     password = result.value()
        ...     print(f"Generated: {password}")
        >>>
        >>> # Validate password strength
        >>> validation = PasswordValidator.validate(password)
        >>> if validation.is_success():
        ...     print(f"Score: {validation.value().score}/100")
        >>>
        >>> # Encrypt data
        >>> encrypted = Encryption.encrypt("secret data", "password123")
        >>> if encrypted.is_success():
        ...     print("Encrypted successfully")

        Version: 2.0.0
        License: GPL3
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            throw py::value_error(e.what());
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(e.what());
        }
    });

    // Bind all components
    bind_error_codes(m);
    bind_result_types(m);
    bind_crypto(m);
    bind_password(m);
    bind_otp(m);
    bind_storage(m);
    bind_manager(m);
    bind_serialization(m);

    // Add version information
    m.attr("__version__") = "2.0.0";
}
