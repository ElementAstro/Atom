#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declarations of binding functions
void bind_common(py::module& m);
void bind_result_types(py::module& m);
void bind_password_entry(py::module& m);
void bind_password_utils(py::module& m);
void bind_encryption(py::module& m);
void bind_storage(py::module& m);
void bind_password_manager(py::module& m);
void bind_serialization(py::module& m);

PYBIND11_MODULE(secret, m) {
    m.doc() = R"pbdoc(
        Atom Secret Module - Secure Password Management
        ===============================================

        This module provides comprehensive password management and cryptographic
        utilities for Python applications. It includes secure password storage,
        generation, validation, encryption, and cross-platform secure storage.

        Key Features:
        ------------
        - **Password Management**: Complete password manager with master password
          protection, auto-lock, and session management
        - **Password Generation**: Secure password generation with customizable
          options and memorable password support
        - **Password Validation**: Comprehensive password strength analysis with
          entropy calculation and common password detection
        - **Encryption**: Multiple encryption methods (AES-GCM, AES-CBC,
          ChaCha20-Poly1305) with PBKDF2 key derivation
        - **Secure Storage**: Cross-platform secure storage backends (Windows
          Credential Manager, macOS Keychain, Linux Secret Service)
        - **Import/Export**: JSON-based import/export functionality
        - **Search & Filter**: Advanced search and filtering capabilities

        Main Components:
        ---------------
        - PasswordManager: Main password management interface
        - PasswordGenerator: Secure password generation utilities
        - PasswordValidator: Password strength analysis and validation
        - Encryption: High-level encryption/decryption utilities
        - KeyDerivation: Cryptographic key derivation (PBKDF2)
        - SecureMemory: Secure memory management for sensitive data
        - SecureStorage: Platform-specific secure storage interface
        - JsonSerializer: JSON serialization/deserialization utilities

        Quick Start:
        -----------
        >>> from atom.secret import (
        ...     PasswordManager, PasswordGenerator, PasswordValidator,
        ...     PasswordEntry, PasswordCategory, PasswordStrength
        ... )
        >>>
        >>> # Create and initialize a password manager
        >>> manager = PasswordManager()
        >>> manager.initialize("MyMasterPassword123!")
        >>>
        >>> # Generate a secure password
        >>> password = PasswordGenerator.generate_password()
        >>> if password.is_success():
        ...     print(f"Generated: {password.value()}")
        >>>
        >>> # Analyze password strength
        >>> analysis = PasswordValidator.analyze_password(password.value())
        >>> print(f"Strength: {analysis.strength}")
        >>> print(f"Score: {analysis.score}/100")
        >>>
        >>> # Create and store a password entry
        >>> entry = PasswordEntry()
        >>> entry.password = password.value()
        >>> entry.username = "user@example.com"
        >>> entry.url = "https://example.com"
        >>> entry.category = PasswordCategory.Personal
        >>> manager.store_password("example.com", entry)
        >>>
        >>> # Search passwords
        >>> results = manager.search_passwords("example")
        >>> for key, entry in results:
        ...     print(f"{key}: {entry.username}")
        >>>
        >>> # Export to JSON
        >>> json_result = manager.export_to_json()
        >>> if json_result.is_success():
        ...     print("Exported successfully")
        >>>
        >>> # Lock when done
        >>> manager.lock()

        Security Considerations:
        -----------------------
        - Always use strong master passwords (12+ characters, mixed case,
          numbers, special characters)
        - Enable auto-lock timeout for sensitive environments
        - Use hardware acceleration when available for better performance
        - Regularly update passwords and check for weak/expired entries
        - Keep the master password secure and never store it in plain text
        - Use the secure memory utilities for handling sensitive data

        Platform Support:
        ----------------
        - **Windows**: Uses Windows Credential Manager for secure storage
        - **macOS**: Uses macOS Keychain for secure storage
        - **Linux**: Uses Secret Service API (libsecret) for secure storage
        - **Fallback**: File-based encrypted storage for unsupported platforms

        Thread Safety:
        -------------
        The PasswordManager class is thread-safe and can be used from multiple
        threads. All operations are protected by internal mutexes.

        Examples:
        --------
        See the individual class documentation for more detailed examples.

        Version: 1.0.0
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
    bind_common(m);
    bind_result_types(m);
    bind_password_entry(m);
    bind_password_utils(m);
    bind_encryption(m);
    bind_storage(m);
    bind_password_manager(m);
    bind_serialization(m);

    // Add version information
    m.attr("__version__") = "1.0.0";
}
