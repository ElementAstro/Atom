"""
Atom Secret Module - Python Bindings
=====================================

This module provides secure password management and cryptographic utilities
for Python applications. It includes:

- Password generation with customizable options
- Password strength analysis and validation
- Secure encryption and decryption (AES-GCM, AES-CBC, ChaCha20-Poly1305)
- Password manager with master password protection
- Cross-platform secure storage backends
- Import/export functionality
- Search and filtering capabilities

Key Components:
--------------
- PasswordManager: Main password management interface
- PasswordGenerator: Secure password generation utilities
- PasswordValidator: Password strength analysis and validation
- Encryption: High-level encryption/decryption utilities
- KeyDerivation: Cryptographic key derivation (PBKDF2)
- SecureMemory: Secure memory management for sensitive data
- SecureStorage: Platform-specific secure storage interface

Enums:
------
- PasswordStrength: Password strength levels (VeryWeak to VeryStrong)
- PasswordCategory: Password categories (General, Finance, Work, etc.)
- EncryptionMethod: Encryption methods (AES_GCM, AES_CBC, CHACHA20_POLY1305)

Data Structures:
---------------
- PasswordEntry: Structure representing a password entry
- EncryptionOptions: Configuration for encryption operations
- PasswordManagerSettings: Settings for the password manager
- EncryptedData: Container for encrypted data with metadata

Example Usage:
-------------
    >>> from atom.secret import (
    ...     PasswordManager, PasswordGenerator, PasswordValidator,
    ...     PasswordStrength, PasswordCategory
    ... )
    >>>
    >>> # Create and initialize a password manager
    >>> manager = PasswordManager()
    >>> manager.initialize("MyMasterPassword123!")
    >>>
    >>> # Generate a secure password
    >>> password = PasswordGenerator.generate_password()
    >>> print(f"Generated password: {password}")
    >>>
    >>> # Analyze password strength
    >>> analysis = PasswordValidator.analyze_password(password)
    >>> print(f"Strength: {analysis.strength}")
    >>> print(f"Score: {analysis.score}/100")
    >>> print(f"Entropy: {analysis.entropy} bits")
    >>>
    >>> # Create and store a password entry
    >>> from atom.secret import PasswordEntry
    >>> entry = PasswordEntry()
    >>> entry.password = password
    >>> entry.username = "user@example.com"
    >>> entry.url = "https://example.com"
    >>> entry.category = PasswordCategory.Personal
    >>> manager.store_password("example.com", entry)
    >>>
    >>> # Retrieve a password
    >>> retrieved = manager.retrieve_password("example.com")
    >>> print(f"Username: {retrieved.username}")
    >>>
    >>> # Search passwords
    >>> results = manager.search_passwords("example")
    >>> for key, entry in results:
    ...     print(f"{key}: {entry.username}")
    >>>
    >>> # Lock the manager when done
    >>> manager.lock()

Version: 1.0.0
License: GPL3
"""

try:
    from .secret import (  # noqa: I001
        AnalysisResult,
        EncryptedData,
        Encryption,
        EncryptionMethod,
        EncryptionOptions,
        Error,
        GenerationOptions,
        JsonSerializer,
        KeyDerivation,
        PasswordCategory,
        PasswordEntry,
        PasswordGenerator,
        PasswordManager,
        PasswordManagerSettings,
        PasswordStrength,
        PasswordValidator,
        ResultBool,
        ResultBytes,
        ResultEncryptedData,
        ResultInt,
        ResultPasswordEntry,
        ResultString,
        SecureComparison,
        SecureMemory,
        SecureStorage,
        SimpleJsonParser,
        Statistics,
    )
except ImportError as e:
    import warnings

    warnings.warn(f"Failed to import atom.secret C++ module: {e}", stacklevel=2)

__version__ = "1.0.0"
__all__ = [
    # Enums
    "PasswordStrength",
    "PasswordCategory",
    "EncryptionMethod",
    # Data structures
    "PasswordEntry",
    "EncryptionOptions",
    "PasswordManagerSettings",
    "EncryptedData",
    "Error",
    # Result types
    "ResultString",
    "ResultBool",
    "ResultInt",
    "ResultPasswordEntry",
    "ResultEncryptedData",
    "ResultBytes",
    # Password utilities
    "PasswordGenerator",
    "GenerationOptions",
    "PasswordValidator",
    "AnalysisResult",
    "SecureComparison",
    # Encryption utilities
    "KeyDerivation",
    "Encryption",
    "SecureMemory",
    # Password manager
    "PasswordManager",
    "Statistics",
    # Storage
    "SecureStorage",
    # Serialization
    "JsonSerializer",
    "SimpleJsonParser",
]
