/*
 * encryption.cpp
 *
 * Python bindings for encryption module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/secret/crypto/encryption.hpp"
#include "atom/secret/crypto/hash.hpp"
#include "atom/secret/crypto/hmac.hpp"
#include "atom/secret/crypto/key_derivation.hpp"
#include "atom/secret/crypto/secure_memory.hpp"

namespace py = pybind11;
using namespace atom::secret;

void bind_crypto(py::module& m) {
    // EncryptionAlgorithm enum
    py::enum_<EncryptionAlgorithm>(m, "EncryptionAlgorithm",
                                   R"pbdoc(
        Encryption algorithms supported by the module.
        )pbdoc")
        .value("AES_128_GCM", EncryptionAlgorithm::AES_128_GCM)
        .value("AES_256_GCM", EncryptionAlgorithm::AES_256_GCM)
        .value("AES_128_CBC", EncryptionAlgorithm::AES_128_CBC)
        .value("AES_256_CBC", EncryptionAlgorithm::AES_256_CBC)
        .value("ChaCha20_Poly1305", EncryptionAlgorithm::ChaCha20_Poly1305)
        .export_values();

    // HashAlgorithm enum
    py::enum_<HashAlgorithm>(m, "HashAlgorithm",
                             R"pbdoc(
        Hash algorithms supported by the module.
        )pbdoc")
        .value("SHA256", HashAlgorithm::SHA256)
        .value("SHA384", HashAlgorithm::SHA384)
        .value("SHA512", HashAlgorithm::SHA512)
        .value("SHA3_256", HashAlgorithm::SHA3_256)
        .value("SHA3_512", HashAlgorithm::SHA3_512)
        .value("BLAKE2b256", HashAlgorithm::BLAKE2b256)
        .value("BLAKE2b512", HashAlgorithm::BLAKE2b512)
        .value("BLAKE2s256", HashAlgorithm::BLAKE2s256)
        .value("MD5", HashAlgorithm::MD5)
        .export_values();

    // KeyDerivationAlgorithm enum
    py::enum_<KeyDerivationAlgorithm>(m, "KeyDerivationAlgorithm",
                                      R"pbdoc(
        Key derivation algorithms.
        )pbdoc")
        .value("PBKDF2_SHA256", KeyDerivationAlgorithm::PBKDF2_SHA256)
        .value("PBKDF2_SHA512", KeyDerivationAlgorithm::PBKDF2_SHA512)
        .value("Argon2id", KeyDerivationAlgorithm::Argon2id)
        .value("Argon2i", KeyDerivationAlgorithm::Argon2i)
        .value("Argon2d", KeyDerivationAlgorithm::Argon2d)
        .value("Scrypt", KeyDerivationAlgorithm::Scrypt)
        .export_values();

    // EncryptionParams struct
    py::class_<EncryptionParams>(m, "EncryptionParams",
                                 R"pbdoc(
        Parameters for encryption operations.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("algorithm", &EncryptionParams::algorithm)
        .def_readwrite("key_iterations", &EncryptionParams::keyIterations);

    // EncryptedData struct
    py::class_<EncryptedData>(m, "EncryptedData",
                              R"pbdoc(
        Container for encrypted data with metadata.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("ciphertext", &EncryptedData::ciphertext)
        .def_readwrite("iv", &EncryptedData::iv)
        .def_readwrite("salt", &EncryptedData::salt)
        .def_readwrite("tag", &EncryptedData::tag)
        .def_readwrite("algorithm", &EncryptedData::algorithm)
        .def("serialize", &EncryptedData::serialize,
             "Serializes the encrypted data to a string")
        .def_static("deserialize", &EncryptedData::deserialize, py::arg("data"),
                    "Deserializes encrypted data from a string");

    // Encryption class
    py::class_<Encryption>(m, "Encryption",
                           R"pbdoc(
        High-level encryption and decryption utilities.
        )pbdoc")
        .def_static(
            "encrypt",
            py::overload_cast<const std::string&, const std::string&,
                              const EncryptionParams&>(&Encryption::encrypt),
            py::arg("plaintext"), py::arg("password"),
            py::arg("params") = EncryptionParams(),
            "Encrypts a string with a password")
        .def_static("decrypt",
                    py::overload_cast<const EncryptedData&, const std::string&>(
                        &Encryption::decrypt),
                    py::arg("encrypted_data"), py::arg("password"),
                    "Decrypts encrypted data with a password")
        .def_static("get_key_size", &Encryption::getKeySize,
                    py::arg("algorithm"),
                    "Returns the key size for an algorithm")
        .def_static("get_iv_size", &Encryption::getIvSize, py::arg("algorithm"),
                    "Returns the IV size for an algorithm")
        .def_static("is_aead", &Encryption::isAead, py::arg("algorithm"),
                    "Returns whether an algorithm is AEAD")
        .def_static("get_algorithm_name", &Encryption::getAlgorithmName,
                    py::arg("algorithm"), "Returns the name of an algorithm")
        .def_static("is_available", &Encryption::isAvailable,
                    py::arg("algorithm"),
                    "Returns whether an algorithm is available");

    // Hash class
    py::class_<Hash>(m, "Hash",
                     R"pbdoc(
        Cryptographic hash functions.
        )pbdoc")
        .def_static("sha256",
                    py::overload_cast<const std::string&>(&Hash::sha256),
                    py::arg("data"), "Computes SHA-256 hash of a string")
        .def_static("sha384",
                    py::overload_cast<const std::string&>(&Hash::sha384),
                    py::arg("data"), "Computes SHA-384 hash of a string")
        .def_static("sha512",
                    py::overload_cast<const std::string&>(&Hash::sha512),
                    py::arg("data"), "Computes SHA-512 hash of a string")
        .def_static("sha3_256",
                    py::overload_cast<const std::string&>(&Hash::sha3_256),
                    py::arg("data"), "Computes SHA3-256 hash of a string")
        .def_static("blake2b256",
                    py::overload_cast<const std::string&>(&Hash::blake2b256),
                    py::arg("data"), "Computes BLAKE2b-256 hash of a string")
        .def_static("get_digest_size", &Hash::getDigestSize,
                    py::arg("algorithm"),
                    "Returns the digest size for an algorithm");

    // Hmac class
    py::class_<Hmac>(m, "Hmac",
                     R"pbdoc(
        HMAC (Hash-based Message Authentication Code) functions.
        )pbdoc")
        .def_static("sha256",
                    py::overload_cast<const std::string&, const std::string&>(
                        &Hmac::sha256),
                    py::arg("data"), py::arg("key"), "Computes HMAC-SHA256")
        .def_static("sha512",
                    py::overload_cast<const std::string&, const std::string&>(
                        &Hmac::sha512),
                    py::arg("data"), py::arg("key"), "Computes HMAC-SHA512")
        .def_static("verify", &Hmac::verify, py::arg("data"), py::arg("key"),
                    py::arg("expected_hmac"),
                    py::arg("algorithm") = HashAlgorithm::SHA256,
                    "Verifies an HMAC");

    // Pbkdf2Params struct
    py::class_<Pbkdf2Params>(m, "Pbkdf2Params",
                             R"pbdoc(
        Parameters for PBKDF2 key derivation.
        )pbdoc")
        .def(py::init<>())
        .def_static("defaults", &Pbkdf2Params::defaults)
        .def_static("high_security", &Pbkdf2Params::highSecurity)
        .def_readwrite("iterations", &Pbkdf2Params::iterations);

    // Argon2Params struct
    py::class_<Argon2Params>(m, "Argon2Params",
                             R"pbdoc(
        Parameters for Argon2 key derivation.
        )pbdoc")
        .def(py::init<>())
        .def_static("defaults", &Argon2Params::defaults)
        .def_static("low_memory", &Argon2Params::lowMemory)
        .def_static("high_security", &Argon2Params::highSecurity)
        .def_readwrite("memory_cost", &Argon2Params::memoryCost)
        .def_readwrite("time_cost", &Argon2Params::timeCost)
        .def_readwrite("parallelism", &Argon2Params::parallelism);

    // ScryptParams struct
    py::class_<ScryptParams>(m, "ScryptParams",
                             R"pbdoc(
        Parameters for scrypt key derivation.
        )pbdoc")
        .def(py::init<>())
        .def_static("defaults", &ScryptParams::defaults)
        .def_static("high_security", &ScryptParams::highSecurity)
        .def_readwrite("n", &ScryptParams::n)
        .def_readwrite("r", &ScryptParams::r)
        .def_readwrite("p", &ScryptParams::p);

    // KeyDerivation class
    py::class_<KeyDerivation>(m, "KeyDerivation",
                              R"pbdoc(
        Key derivation functions.
        )pbdoc")
        .def_static("generate_salt", &KeyDerivation::generateSalt,
                    py::arg("length") = 32, "Generates a random salt")
        .def_static("generate_key", &KeyDerivation::generateKey,
                    py::arg("length") = 32, "Generates a random key")
        .def_static("generate_random_bytes",
                    &KeyDerivation::generateRandomBytes, py::arg("length"),
                    "Generates random bytes")
        .def_static("pbkdf2_sha256", &KeyDerivation::pbkdf2Sha256,
                    py::arg("password"), py::arg("salt"), py::arg("iterations"),
                    py::arg("key_length"), "Derives a key using PBKDF2-SHA256")
        .def_static("pbkdf2_sha512", &KeyDerivation::pbkdf2Sha512,
                    py::arg("password"), py::arg("salt"), py::arg("iterations"),
                    py::arg("key_length"), "Derives a key using PBKDF2-SHA512")
        .def_static("argon2id", &KeyDerivation::argon2id, py::arg("password"),
                    py::arg("salt"), py::arg("params"), py::arg("key_length"),
                    "Derives a key using Argon2id")
        .def_static("argon2i", &KeyDerivation::argon2i, py::arg("password"),
                    py::arg("salt"), py::arg("params"), py::arg("key_length"),
                    "Derives a key using Argon2i")
        .def_static("scrypt", &KeyDerivation::scrypt, py::arg("password"),
                    py::arg("salt"), py::arg("params"), py::arg("key_length"),
                    "Derives a key using scrypt")
        .def_static("hkdf", &KeyDerivation::hkdf, py::arg("input_key"),
                    py::arg("salt"), py::arg("info"), py::arg("key_length"),
                    "Derives a key using HKDF")
        .def_static("is_argon2_available", &KeyDerivation::isArgon2Available,
                    "Returns whether Argon2 is available")
        .def_static("is_scrypt_available", &KeyDerivation::isScryptAvailable,
                    "Returns whether scrypt is available")
        .def_static(
            "benchmark_iterations", &KeyDerivation::benchmarkIterations,
            py::arg("target_milliseconds"),
            py::arg("algorithm") = KeyDerivationAlgorithm::PBKDF2_SHA256,
            "Benchmarks key derivation to determine appropriate parameters");

    // SecureMemory class
    py::class_<SecureMemory>(m, "SecureMemory",
                             R"pbdoc(
        Secure memory management utilities.
        )pbdoc")
        .def_static("secure_clear",
                    py::overload_cast<std::string&>(&SecureMemory::secureClear),
                    py::arg("str"), "Securely clears a string")
        .def_static("lock_memory", &SecureMemory::lockMemory, py::arg("ptr"),
                    py::arg("size"), "Locks memory to prevent swapping")
        .def_static("unlock_memory", &SecureMemory::unlockMemory,
                    py::arg("ptr"), py::arg("size"),
                    "Unlocks previously locked memory")
        .def_static("is_memory_locking_available",
                    &SecureMemory::isMemoryLockingAvailable,
                    "Returns whether memory locking is available")
        .def_static("get_page_size", &SecureMemory::getPageSize,
                    "Gets the system page size")
        .def_static("align_to_page", &SecureMemory::alignToPage,
                    py::arg("size"),
                    "Aligns a size to the system page boundary");

    // SecureBuffer<uint8_t> class
    py::class_<SecureBuffer<uint8_t>>(m, "SecureBuffer",
                                      R"pbdoc(
        Secure buffer for sensitive data.
        )pbdoc")
        .def(py::init<size_t>(), py::arg("size"))
        .def("size", &SecureBuffer<uint8_t>::size)
        .def("size_bytes", &SecureBuffer<uint8_t>::sizeBytes)
        .def("is_valid", &SecureBuffer<uint8_t>::isValid)
        .def("fill", &SecureBuffer<uint8_t>::fill, py::arg("value"))
        .def("zero", &SecureBuffer<uint8_t>::zero);

    // SecureString class
    py::class_<SecureString>(m, "SecureString",
                             R"pbdoc(
        Secure string that automatically clears on destruction.
        )pbdoc")
        .def(py::init<>())
        .def(py::init<const std::string&>(), py::arg("str"))
        .def(py::init<const char*>(), py::arg("str"))
        .def("str", &SecureString::str)
        .def("c_str", &SecureString::c_str)
        .def("size", &SecureString::size)
        .def("length", &SecureString::length)
        .def("empty", &SecureString::empty)
        .def("clear", &SecureString::clear)
        .def("append",
             py::overload_cast<const std::string&>(&SecureString::append),
             py::arg("str"))
        .def("__len__", &SecureString::size)
        .def("__str__", &SecureString::str);
}
