#include "atom/algorithm/blowfish.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(blowfish, m) {
    m.doc() = R"pbdoc(
        Blowfish Encryption Algorithm
        ----------------------------
        
        This module provides a Python interface to the Blowfish encryption algorithm.
        Blowfish is a symmetric-key block cipher designed by Bruce Schneier in 1993.
        
        Example:
            >>> import atom.algorithm.blowfish as bf
            >>> # Generate a random key
            >>> key = bf.generate_key(16)
            >>> # Create a Blowfish cipher instance
            >>> cipher = bf.Blowfish(key)
            >>> # Encrypt some data
            >>> encrypted = cipher.encrypt_data(b"Hello, world!")
            >>> # Decrypt the data
            >>> decrypted = cipher.decrypt_data(encrypted)
            >>> assert decrypted == b"Hello, world!"
    )pbdoc";

    // Register exception translation
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define constants
    m.attr("BLOCK_SIZE") = 8;
    m.attr("MIN_KEY_SIZE") = 4;
    m.attr("MAX_KEY_SIZE") = 56;

    // Blowfish class
    py::class_<atom::algorithm::Blowfish>(m, "Blowfish", R"pbdoc(
        Blowfish cipher implementation.
        
        The Blowfish class implements the Blowfish encryption algorithm,
        a symmetric key block cipher that can be used for encrypting data.
        
        Args:
            key (bytes): The encryption key (4-56 bytes)
    )pbdoc")
        .def(py::init([](py::bytes key) {
                 // Convert Python bytes to std::span<const std::byte>
                 const char* data = PyBytes_AsString(key.ptr());
                 size_t size = PyBytes_Size(key.ptr());

                 // Key length check
                 if (size < 4 || size > 56) {
                     throw py::value_error(
                         "Key length must be between 4 and 56 bytes");
                 }

                 // Create a vector of bytes and copy the data
                 std::vector<std::byte> key_bytes(size);
                 for (size_t i = 0; i < size; ++i) {
                     key_bytes[i] = static_cast<std::byte>(data[i]);
                 }

                 return new atom::algorithm::Blowfish(
                     std::span<const std::byte>(key_bytes));
             }),
             py::arg("key"))
        .def(
            "encrypt_block",
            [](atom::algorithm::Blowfish& self, py::bytes block) {
                // Check if block is the right size
                if (PyBytes_Size(block.ptr()) != 8) {
                    throw py::value_error("Block must be exactly 8 bytes");
                }

                // Copy bytes to a mutable buffer
                std::array<std::byte, 8> buffer;
                const char* data = PyBytes_AsString(block.ptr());
                for (size_t i = 0; i < 8; ++i) {
                    buffer[i] = static_cast<std::byte>(data[i]);
                }

                // Encrypt the buffer
                self.encrypt(std::span<std::byte, 8>(buffer));

                // Return the encrypted bytes
                return py::bytes(reinterpret_cast<char*>(buffer.data()), 8);
            },
            py::arg("block"), R"pbdoc(
            Encrypt a single 8-byte block.
            
            Args:
                block (bytes): The 8-byte block to encrypt
                
            Returns:
                bytes: The encrypted 8-byte block
                
            Raises:
                ValueError: If the block is not exactly 8 bytes
        )pbdoc")
        .def(
            "decrypt_block",
            [](atom::algorithm::Blowfish& self, py::bytes block) {
                // Check if block is the right size
                if (PyBytes_Size(block.ptr()) != 8) {
                    throw py::value_error("Block must be exactly 8 bytes");
                }

                // Copy bytes to a mutable buffer
                std::array<std::byte, 8> buffer;
                const char* data = PyBytes_AsString(block.ptr());
                for (size_t i = 0; i < 8; ++i) {
                    buffer[i] = static_cast<std::byte>(data[i]);
                }

                // Decrypt the buffer
                self.decrypt(std::span<std::byte, 8>(buffer));

                // Return the decrypted bytes
                return py::bytes(reinterpret_cast<char*>(buffer.data()), 8);
            },
            py::arg("block"), R"pbdoc(
            Decrypt a single 8-byte block.
            
            Args:
                block (bytes): The 8-byte block to decrypt
                
            Returns:
                bytes: The decrypted 8-byte block
                
            Raises:
                ValueError: If the block is not exactly 8 bytes
        )pbdoc")
        .def(
            "encrypt_data",
            [](atom::algorithm::Blowfish& self, py::bytes data) {
                // Handle empty data
                if (PyBytes_Size(data.ptr()) == 0) {
                    throw py::value_error("Cannot encrypt empty data");
                }

                // Copy data to a mutable buffer
                size_t input_size = PyBytes_Size(data.ptr());
                std::vector<unsigned char> buffer(input_size);
                std::memcpy(buffer.data(), PyBytes_AsString(data.ptr()),
                            input_size);

                // Encrypt the data
                self.encrypt_data(std::span<unsigned char>(buffer));

                // Return the encrypted bytes
                return py::bytes(reinterpret_cast<char*>(buffer.data()),
                                 buffer.size());
            },
            py::arg("data"), R"pbdoc(
            Encrypt arbitrary data.
            
            This method encrypts arbitrary data using the Blowfish cipher.
            PKCS7 padding is automatically applied.
            
            Args:
                data (bytes): The data to encrypt
                
            Returns:
                bytes: The encrypted data
                
            Raises:
                ValueError: If the data is empty
        )pbdoc")
        .def(
            "decrypt_data",
            [](atom::algorithm::Blowfish& self, py::bytes data) {
                // Check if data is valid
                size_t data_size = PyBytes_Size(data.ptr());
                if (data_size == 0) {
                    throw py::value_error("Cannot decrypt empty data");
                }

                // Check if data is a multiple of the block size
                if (data_size % 8 != 0) {
                    throw py::value_error(
                        "Encrypted data must be a multiple of 8 bytes");
                }

                // Copy data to a mutable buffer
                std::vector<unsigned char> buffer(data_size);
                std::memcpy(buffer.data(), PyBytes_AsString(data.ptr()),
                            data_size);

                // Decrypt the data
                size_t output_size = data_size;
                self.decrypt_data(std::span<unsigned char>(buffer),
                                  output_size);

                // Return the decrypted bytes (without padding)
                return py::bytes(reinterpret_cast<char*>(buffer.data()),
                                 output_size);
            },
            py::arg("data"), R"pbdoc(
            Decrypt data.
            
            This method decrypts data that was encrypted with the encrypt_data method.
            PKCS7 padding is automatically removed.
            
            Args:
                data (bytes): The encrypted data
                
            Returns:
                bytes: The decrypted data
                
            Raises:
                ValueError: If the data is empty or not a multiple of 8 bytes
        )pbdoc")
        .def("encrypt_file", &atom::algorithm::Blowfish::encrypt_file,
             py::arg("input_file"), py::arg("output_file"),
             R"pbdoc(
                Encrypt a file.
                
                This method reads a file, encrypts its contents, and writes the
                encrypted data to another file.
                
                Args:
                    input_file (str): Path to the input file
                    output_file (str): Path to the output file
                
                Raises:
                    RuntimeError: If file operations fail
            )pbdoc")
        .def("decrypt_file", &atom::algorithm::Blowfish::decrypt_file,
             py::arg("input_file"), py::arg("output_file"),
             R"pbdoc(
                Decrypt a file.
                
                This method reads an encrypted file, decrypts its contents, and writes
                the decrypted data to another file.
                
                Args:
                    input_file (str): Path to the encrypted file
                    output_file (str): Path to the output file
                
                Raises:
                    RuntimeError: If file operations fail
            )pbdoc");

    // Utility functions
    m.def(
        "generate_key",
        [](size_t length) {
            // Check key length
            if (length < 4 || length > 56) {
                throw py::value_error(
                    "Key length must be between 4 and 56 bytes");
            }

            // Generate random bytes using Python's os.urandom
            py::object os = py::module::import("os");
            return os.attr("urandom")(length);
        },
        py::arg("length") = 16, R"pbdoc(
        Generate a cryptographically secure random key.
        
        Args:
            length (int, optional): The key length in bytes. Default is 16.
                Must be between 4 and 56 bytes.
                
        Returns:
            bytes: A random key of the specified length
            
        Raises:
            ValueError: If the length is not between 4 and 56 bytes
    )pbdoc");

    // Add helper for string encryption/decryption
    m.def(
        "encrypt_string",
        [](atom::algorithm::Blowfish& cipher, const std::string& text) {
            // Convert string to bytes
            std::vector<unsigned char> buffer(text.begin(), text.end());

            // Encrypt the data
            cipher.encrypt_data(std::span<unsigned char>(buffer));

            // Return the encrypted bytes
            return py::bytes(reinterpret_cast<char*>(buffer.data()),
                             buffer.size());
        },
        py::arg("cipher"), py::arg("text"), R"pbdoc(
        Encrypt a string using a Blowfish cipher.
        
        Args:
            cipher (Blowfish): The Blowfish cipher instance
            text (str): The string to encrypt
            
        Returns:
            bytes: The encrypted data
    )pbdoc");

    m.def(
        "decrypt_string",
        [](atom::algorithm::Blowfish& cipher, py::bytes data) {
            // Check if data is valid
            size_t data_size = PyBytes_Size(data.ptr());
            if (data_size == 0) {
                throw py::value_error("Cannot decrypt empty data");
            }

            // Check if data is a multiple of the block size
            if (data_size % 8 != 0) {
                throw py::value_error(
                    "Encrypted data must be a multiple of 8 bytes");
            }

            // Copy data to a mutable buffer
            std::vector<unsigned char> buffer(data_size);
            std::memcpy(buffer.data(), PyBytes_AsString(data.ptr()), data_size);

            // Decrypt the data
            size_t output_size = data_size;
            cipher.decrypt_data(std::span<unsigned char>(buffer), output_size);

            // Return the decrypted string
            return std::string(reinterpret_cast<char*>(buffer.data()),
                               output_size);
        },
        py::arg("cipher"), py::arg("data"), R"pbdoc(
        Decrypt data to a string using a Blowfish cipher.
        
        Args:
            cipher (Blowfish): The Blowfish cipher instance
            data (bytes): The encrypted data
            
        Returns:
            str: The decrypted string
            
        Raises:
            ValueError: If the data is empty or not a multiple of 8 bytes
            UnicodeDecodeError: If the decrypted data is not valid UTF-8
    )pbdoc");

    // Convenience function for encrypting with a password
    m.def(
        "encrypt_with_password",
        [](const std::string& password, py::bytes data) {
            // Create a key from the password (simple implementation)
            if (password.empty()) {
                throw py::value_error("Password cannot be empty");
            }

            // Use a simple key derivation (not secure for production)
            std::vector<std::byte> key;
            key.reserve(std::min(password.size(), size_t(56)));

            for (size_t i = 0; i < std::min(password.size(), size_t(56)); ++i) {
                key.push_back(static_cast<std::byte>(password[i]));
            }

            // Create cipher and encrypt
            atom::algorithm::Blowfish cipher(std::span<const std::byte>(key));

            // Handle empty data
            if (PyBytes_Size(data.ptr()) == 0) {
                throw py::value_error("Cannot encrypt empty data");
            }

            // Copy data to a mutable buffer
            size_t input_size = PyBytes_Size(data.ptr());
            std::vector<unsigned char> buffer(input_size);
            std::memcpy(buffer.data(), PyBytes_AsString(data.ptr()),
                        input_size);

            // Encrypt the data
            cipher.encrypt_data(std::span<unsigned char>(buffer));

            // Return the encrypted bytes
            return py::bytes(reinterpret_cast<char*>(buffer.data()),
                             buffer.size());
        },
        py::arg("password"), py::arg("data"), R"pbdoc(
        Encrypt data using a password.
        
        WARNING: This is a convenience function with a simple key derivation.
        For secure applications, use a proper key derivation function.
        
        Args:
            password (str): The password
            data (bytes): The data to encrypt
            
        Returns:
            bytes: The encrypted data
            
        Raises:
            ValueError: If the password is empty or data is empty
    )pbdoc");

    m.def(
        "decrypt_with_password",
        [](const std::string& password, py::bytes data) {
            // Create a key from the password (simple implementation)
            if (password.empty()) {
                throw py::value_error("Password cannot be empty");
            }

            // Use a simple key derivation (not secure for production)
            std::vector<std::byte> key;
            key.reserve(std::min(password.size(), size_t(56)));

            for (size_t i = 0; i < std::min(password.size(), size_t(56)); ++i) {
                key.push_back(static_cast<std::byte>(password[i]));
            }

            // Create cipher and decrypt
            atom::algorithm::Blowfish cipher(std::span<const std::byte>(key));

            // Check if data is valid
            size_t data_size = PyBytes_Size(data.ptr());
            if (data_size == 0) {
                throw py::value_error("Cannot decrypt empty data");
            }

            // Check if data is a multiple of the block size
            if (data_size % 8 != 0) {
                throw py::value_error(
                    "Encrypted data must be a multiple of 8 bytes");
            }

            // Copy data to a mutable buffer
            std::vector<unsigned char> buffer(data_size);
            std::memcpy(buffer.data(), PyBytes_AsString(data.ptr()), data_size);

            // Decrypt the data
            size_t output_size = data_size;
            cipher.decrypt_data(std::span<unsigned char>(buffer), output_size);

            // Return the decrypted bytes
            return py::bytes(reinterpret_cast<char*>(buffer.data()),
                             output_size);
        },
        py::arg("password"), py::arg("data"), R"pbdoc(
        Decrypt data using a password.
        
        WARNING: This is a convenience function with a simple key derivation.
        For secure applications, use a proper key derivation function.
        
        Args:
            password (str): The password
            data (bytes): The encrypted data
            
        Returns:
            bytes: The decrypted data
            
        Raises:
            ValueError: If the password is empty, data is empty, or data is not a multiple of 8 bytes
    )pbdoc");
}