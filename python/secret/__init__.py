"""
Atom Secret Module - Python Bindings
=====================================

This module provides secure password management and cryptographic utilities
for Python applications.

Submodules:
----------
- core: Error codes and result types
- crypto: Encryption, hashing, and key derivation
- password: Password generation, validation, and breach checking
- otp: TOTP and HOTP one-time passwords
- storage: Secure storage backends
- manager: Password manager and session management
- serialization: JSON serialization

Example Usage:
-------------
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

Version: 2.0.0
License: GPL3
"""

try:
    from .secret import (  # noqa: I001; Core - Error codes and results; Core - Types and utilities; Crypto - Enums; Crypto - Params; Crypto - Classes; Password - Enums; Password - Classes; OTP; Storage; Manager; Serialization
        Argon2Params,
        AuditAction,
        AuditEntry,
        AuditLog,
        BackupConfig,
        BackupManager,
        Base32,
        BreachChecker,
        CustomField,
        EncryptedData,
        Encryption,
        EncryptionAlgorithm,
        EncryptionParams,
        ErrorCode,
        FileStorage,
        Hash,
        HashAlgorithm,
        Hmac,
        Hotp,
        HotpConfig,
        JsonSerializer,
        KeyDerivation,
        KeyDerivationAlgorithm,
        PasswordCategory,
        PasswordEntry,
        PasswordGenerator,
        PasswordGeneratorOptions,
        PasswordManager,
        PasswordManagerSettings,
        PasswordPolicy,
        PasswordStrength,
        PasswordValidator,
        Pbkdf2Params,
        ResultBool,
        ResultBytes,
        ResultInt,
        ResultString,
        ResultVoid,
        ScryptParams,
        SearchFilter,
        SecureBuffer,
        SecureMemory,
        SecureStorage,
        SecureString,
        SessionConfig,
        SessionManager,
        StorageBackend,
        Totp,
        TotpConfig,
        Utils,
        ValidationResult,
        audit_action_to_string,
        bytes_to_hex,
        category_to_string,
        constants,
        error_code_to_string,
        from_unix_timestamp,
        hex_to_bytes,
        is_error,
        is_success,
        now,
        strength_to_string,
        string_to_category,
        to_unix_timestamp,
    )
except ImportError as e:
    import warnings

    warnings.warn(f"Failed to import atom.secret C++ module: {e}", stacklevel=2)

__version__ = "2.0.0"
__all__ = [
    # Core - Error codes and results
    "ErrorCode",
    "ResultString",
    "ResultInt",
    "ResultBool",
    "ResultBytes",
    "ResultVoid",
    "error_code_to_string",
    "is_success",
    "is_error",
    # Core - Types and utilities
    "Utils",
    "bytes_to_hex",
    "hex_to_bytes",
    "now",
    "to_unix_timestamp",
    "from_unix_timestamp",
    "constants",
    # Crypto - Enums
    "EncryptionAlgorithm",
    "HashAlgorithm",
    "KeyDerivationAlgorithm",
    # Crypto - Params
    "EncryptionParams",
    "Pbkdf2Params",
    "Argon2Params",
    "ScryptParams",
    # Crypto - Classes
    "EncryptedData",
    "Encryption",
    "Hash",
    "Hmac",
    "KeyDerivation",
    "SecureMemory",
    "SecureBuffer",
    "SecureString",
    # Password - Enums
    "PasswordCategory",
    "PasswordStrength",
    # Password - Classes
    "CustomField",
    "PasswordEntry",
    "PasswordGeneratorOptions",
    "PasswordGenerator",
    "PasswordPolicy",
    "ValidationResult",
    "PasswordValidator",
    "BreachChecker",
    "category_to_string",
    "string_to_category",
    "strength_to_string",
    # OTP
    "TotpConfig",
    "Totp",
    "HotpConfig",
    "Hotp",
    "Base32",
    # Storage
    "StorageBackend",
    "SecureStorage",
    "FileStorage",
    "BackupConfig",
    "BackupManager",
    # Manager
    "SessionConfig",
    "SessionManager",
    "AuditAction",
    "AuditEntry",
    "AuditLog",
    "audit_action_to_string",
    "SearchFilter",
    "PasswordManagerSettings",
    "PasswordManager",
    # Serialization
    "JsonSerializer",
]
