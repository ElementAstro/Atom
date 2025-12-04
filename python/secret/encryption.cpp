#include "atom/secret/encryption.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_encryption(py::module& m) {
    // SecureMemory class
    py::class_<atom::secret::SecureMemory>(m, "SecureMemory",
                                           R"pbdoc(
        Secure memory management utilities for sensitive data.

        Provides static methods for securely clearing memory, locking pages
        to prevent swapping, and allocating secure memory.

        Example:
            >>> # Securely clear a string
            >>> password = "MyPassword123!"
            >>> SecureMemory.secure_clear(password)
        )pbdoc")
        .def_static("secure_clear",
                    py::overload_cast<std::string&>(
                        &atom::secret::SecureMemory::secureClear),
                    py::arg("str"),
                    R"pbdoc(
            Securely clears a string's contents.

            Args:
                str: String to clear.
            )pbdoc");

    // KeyDerivation class
    py::class_<atom::secret::KeyDerivation>(m, "KeyDerivation",
                                            R"pbdoc(
        Cryptographic key derivation and management.

        Provides static methods for deriving keys from passwords using PBKDF2
        and generating cryptographically secure random data.

        Example:
            >>> # Generate a salt
            >>> salt_result = KeyDerivation.generate_salt(32)
            >>> if salt_result.is_success():
            ...     salt = salt_result.value()
            >>>
            >>> # Derive a key from a password
            >>> key_result = KeyDerivation.derive_key("password", salt, 100000, 32)
            >>> if key_result.is_success():
            ...     key = key_result.value()
        )pbdoc")
        .def_static("derive_key", &atom::secret::KeyDerivation::deriveKey,
                    py::arg("password"), py::arg("salt"), py::arg("iterations"),
                    py::arg("key_length"),
                    R"pbdoc(
            Derives a key from a password using PBKDF2.

            Args:
                password: The password to derive from.
                salt: The salt for key derivation.
                iterations: Number of PBKDF2 iterations.
                key_length: Desired key length in bytes.

            Returns:
                Result[bytes]: Derived key or error message.
            )pbdoc")
        .def_static("generate_salt", &atom::secret::KeyDerivation::generateSalt,
                    py::arg("length") = 32,
                    R"pbdoc(
            Generates a cryptographically secure random salt.

            Args:
                length: Length of salt in bytes (default: 32).

            Returns:
                Result[bytes]: Salt or error message.
            )pbdoc")
        .def_static("generate_key", &atom::secret::KeyDerivation::generateKey,
                    py::arg("length") = 32,
                    R"pbdoc(
            Generates a cryptographically secure random key.

            Args:
                length: Length of key in bytes (default: 32).

            Returns:
                Result[bytes]: Key or error message.
            )pbdoc");

    // EncryptedData struct
    py::class_<atom::secret::EncryptedData>(m, "EncryptedData",
                                            R"pbdoc(
        Encrypted data container with metadata.

        Contains encrypted data along with all necessary metadata for
        decryption including IV, salt, authentication tag, and encryption
        method information.

        Attributes:
            ciphertext (bytes): The encrypted data
            iv (bytes): Initialization vector
            salt (bytes): Salt used for key derivation
            tag (bytes): Authentication tag (for AEAD modes)
            method (EncryptionMethod): Encryption method used
            key_iterations (int): PBKDF2 iterations used
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("ciphertext", &atom::secret::EncryptedData::ciphertext,
                       "The encrypted data")
        .def_readwrite("iv", &atom::secret::EncryptedData::iv,
                       "Initialization vector")
        .def_readwrite("salt", &atom::secret::EncryptedData::salt,
                       "Salt used for key derivation")
        .def_readwrite("tag", &atom::secret::EncryptedData::tag,
                       "Authentication tag (for AEAD modes)")
        .def_readwrite("method", &atom::secret::EncryptedData::method,
                       "Encryption method used")
        .def_readwrite("key_iterations",
                       &atom::secret::EncryptedData::keyIterations,
                       "PBKDF2 iterations used")
        .def("serialize", &atom::secret::EncryptedData::serialize,
             R"pbdoc(
            Serializes the encrypted data to a binary format.

            Returns:
                bytes: Serialized data.
            )pbdoc")
        .def_static("deserialize", &atom::secret::EncryptedData::deserialize,
                    py::arg("data"),
                    R"pbdoc(
            Deserializes encrypted data from binary format.

            Args:
                data: Serialized data.

            Returns:
                Result[EncryptedData]: EncryptedData or error message.
            )pbdoc");

    // Encryption class
    py::class_<atom::secret::Encryption>(m, "Encryption",
                                         R"pbdoc(
        High-level encryption and decryption utilities.

        Provides static methods for encrypting and decrypting data using
        various encryption methods with password-based or key-based encryption.

        Example:
            >>> # Encrypt data with a password
            >>> plaintext = "Secret message"
            >>> password = "MySecurePassword123!"
            >>> options = EncryptionOptions()
            >>> result = Encryption.encrypt(plaintext, password, options)
            >>> if result.is_success():
            ...     encrypted_data = result.value()
            >>>
            >>> # Decrypt data
            >>> result = Encryption.decrypt(encrypted_data, password)
            >>> if result.is_success():
            ...     decrypted = result.value()
            ...     print(f"Decrypted: {decrypted}")
        )pbdoc")
        .def_static("encrypt", &atom::secret::Encryption::encrypt,
                    py::arg("plaintext"), py::arg("password"),
                    py::arg("options") = atom::secret::EncryptionOptions{},
                    R"pbdoc(
            Encrypts data using the specified options.

            Args:
                plaintext: The data to encrypt.
                password: The password for encryption.
                options: Encryption options (default: AES-GCM).

            Returns:
                Result[EncryptedData]: Encrypted data or error message.
            )pbdoc")
        .def_static("decrypt", &atom::secret::Encryption::decrypt,
                    py::arg("encrypted_data"), py::arg("password"),
                    R"pbdoc(
            Decrypts data using the provided password.

            Args:
                encrypted_data: The encrypted data to decrypt.
                password: The password for decryption.

            Returns:
                Result[str]: Decrypted plaintext or error message.
            )pbdoc")
        .def_static("encrypt_with_key",
                    &atom::secret::Encryption::encryptWithKey,
                    py::arg("plaintext"), py::arg("key"),
                    py::arg("options") = atom::secret::EncryptionOptions{},
                    R"pbdoc(
            Encrypts data with a pre-derived key.

            Args:
                plaintext: The data to encrypt.
                key: The encryption key.
                options: Encryption options (default: AES-GCM).

            Returns:
                Result[EncryptedData]: Encrypted data or error message.
            )pbdoc")
        .def_static("decrypt_with_key",
                    &atom::secret::Encryption::decryptWithKey,
                    py::arg("encrypted_data"), py::arg("key"),
                    R"pbdoc(
            Decrypts data with a pre-derived key.

            Args:
                encrypted_data: The encrypted data to decrypt.
                key: The decryption key.

            Returns:
                Result[str]: Decrypted plaintext or error message.
            )pbdoc");
}
