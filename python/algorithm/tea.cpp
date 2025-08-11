#include "atom/algorithm/tea.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <random>

namespace py = pybind11;

PYBIND11_MODULE(tea, m) {
    m.doc() =
        "TEA encryption algorithm implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::algorithm::TEAException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define XTEAKey type for Python
    py::class_<atom::algorithm::XTEAKey>(
        m, "XTEAKey",
        R"(A 128-bit key used for TEA and XTEA encryption algorithms.

Represented as an array of 4 unsigned 32-bit integers.

Examples:
    >>> from atom.algorithm.tea import XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
)")
        .def(py::init<>(), "Constructs an empty XTEAKey (all zeros).")
        .def(
            py::init([](const std::vector<uint32_t>& values) {
                if (values.size() != 4) {
                    throw py::value_error("XTEAKey must have exactly 4 values");
                }
                atom::algorithm::XTEAKey key;
                std::copy(values.begin(), values.end(), key.begin());
                return key;
            }),
            py::arg("values"),
            "Constructs an XTEAKey from a list of 4 unsigned 32-bit integers.")
        .def("__getitem__",
             [](const atom::algorithm::XTEAKey& key, size_t index) {
                 if (index >= key.size()) {
                     throw py::index_error("Index out of range");
                 }
                 return key[index];
             })
        .def("__setitem__",
             [](atom::algorithm::XTEAKey& key, size_t index, uint32_t value) {
                 if (index >= key.size()) {
                     throw py::index_error("Index out of range");
                 }
                 key[index] = value;
             })
        .def("__len__",
             [](const atom::algorithm::XTEAKey& key) { return key.size(); })
        .def("__repr__", [](const atom::algorithm::XTEAKey& key) {
            std::stringstream ss;
            ss << "XTEAKey([0x" << std::hex << key[0] << ", 0x" << key[1]
               << ", 0x" << key[2] << ", 0x" << key[3] << "])";
            return ss.str();
        });

    // TEA functions
    m.def(
        "tea_encrypt",
        [](uint32_t value0, uint32_t value1,
           const atom::algorithm::XTEAKey& key) {
            uint32_t v0_copy = value0;
            uint32_t v1_copy = value1;
            atom::algorithm::teaEncrypt(v0_copy, v1_copy, key);
            return py::make_tuple(v0_copy, v1_copy);
        },
        py::arg("value0"), py::arg("value1"), py::arg("key"),
        R"(Encrypts two 32-bit values using the TEA (Tiny Encryption Algorithm).

Args:
    value0: The first 32-bit value to be encrypted
    value1: The second 32-bit value to be encrypted
    key: A 128-bit key (XTEAKey object)

Returns:
    A tuple containing the two encrypted 32-bit values

Examples:
    >>> from atom.algorithm.tea import tea_encrypt, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> encrypted = tea_encrypt(0x12345678, 0x87654321, key)
    >>> print(encrypted)
)");

    m.def(
        "tea_decrypt",
        [](uint32_t value0, uint32_t value1,
           const atom::algorithm::XTEAKey& key) {
            uint32_t v0_copy = value0;
            uint32_t v1_copy = value1;
            atom::algorithm::teaDecrypt(v0_copy, v1_copy, key);
            return py::make_tuple(v0_copy, v1_copy);
        },
        py::arg("value0"), py::arg("value1"), py::arg("key"),
        R"(Decrypts two 32-bit values using the TEA (Tiny Encryption Algorithm).

Args:
    value0: The first 32-bit value to be decrypted
    value1: The second 32-bit value to be decrypted
    key: A 128-bit key (XTEAKey object)

Returns:
    A tuple containing the two decrypted 32-bit values

Examples:
    >>> from atom.algorithm.tea import tea_encrypt, tea_decrypt, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> encrypted = tea_encrypt(0x12345678, 0x87654321, key)
    >>> decrypted = tea_decrypt(encrypted[0], encrypted[1], key)
    >>> print(decrypted)  # Should be (0x12345678, 0x87654321)
)");

    // XTEA functions
    m.def(
        "xtea_encrypt",
        [](uint32_t value0, uint32_t value1,
           const atom::algorithm::XTEAKey& key) {
            uint32_t v0_copy = value0;
            uint32_t v1_copy = value1;
            atom::algorithm::xteaEncrypt(v0_copy, v1_copy, key);
            return py::make_tuple(v0_copy, v1_copy);
        },
        py::arg("value0"), py::arg("value1"), py::arg("key"),
        R"(Encrypts two 32-bit values using the XTEA (Extended TEA) algorithm.

Args:
    value0: The first 32-bit value to be encrypted
    value1: The second 32-bit value to be encrypted
    key: A 128-bit key (XTEAKey object)

Returns:
    A tuple containing the two encrypted 32-bit values

Examples:
    >>> from atom.algorithm.tea import xtea_encrypt, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> encrypted = xtea_encrypt(0x12345678, 0x87654321, key)
    >>> print(encrypted)
)");

    m.def(
        "xtea_decrypt",
        [](uint32_t value0, uint32_t value1,
           const atom::algorithm::XTEAKey& key) {
            uint32_t v0_copy = value0;
            uint32_t v1_copy = value1;
            atom::algorithm::xteaDecrypt(v0_copy, v1_copy, key);
            return py::make_tuple(v0_copy, v1_copy);
        },
        py::arg("value0"), py::arg("value1"), py::arg("key"),
        R"(Decrypts two 32-bit values using the XTEA (Extended TEA) algorithm.

Args:
    value0: The first 32-bit value to be decrypted
    value1: The second 32-bit value to be decrypted
    key: A 128-bit key (XTEAKey object)

Returns:
    A tuple containing the two decrypted 32-bit values

Examples:
    >>> from atom.algorithm.tea import xtea_encrypt, xtea_decrypt, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> encrypted = xtea_encrypt(0x12345678, 0x87654321, key)
    >>> decrypted = xtea_decrypt(encrypted[0], encrypted[1], key)
    >>> print(decrypted)  # Should be (0x12345678, 0x87654321)
)");

    // XXTEA functions
    m.def(
        "xxtea_encrypt",
        [](const std::vector<uint32_t>& data,
           const atom::algorithm::XTEAKey& key) {
            return atom::algorithm::xxteaEncrypt(data, key);
        },
        py::arg("data"), py::arg("key"),
        R"(Encrypts a vector of 32-bit values using the XXTEA algorithm.

Args:
    data: A list/vector of 32-bit values to be encrypted
    key: A 128-bit key (XTEAKey object)

Returns:
    A list of encrypted 32-bit values

Examples:
    >>> from atom.algorithm.tea import xxtea_encrypt, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> data = [0x12345678, 0x87654321, 0xAABBCCDD]
    >>> encrypted = xxtea_encrypt(data, key)
)");

    m.def(
        "xxtea_decrypt",
        [](const std::vector<uint32_t>& data,
           const atom::algorithm::XTEAKey& key) {
            return atom::algorithm::xxteaDecrypt(data, key);
        },
        py::arg("data"), py::arg("key"),
        R"(Decrypts a vector of 32-bit values using the XXTEA algorithm.

Args:
    data: A list/vector of 32-bit values to be decrypted
    key: A 128-bit key (XTEAKey object)

Returns:
    A list of decrypted 32-bit values

Examples:
    >>> from atom.algorithm.tea import xxtea_encrypt, xxtea_decrypt, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> data = [0x12345678, 0x87654321, 0xAABBCCDD]
    >>> encrypted = xxtea_encrypt(data, key)
    >>> decrypted = xxtea_decrypt(encrypted, key)
    >>> print(decrypted)  # Should be the original data
)");

    // Parallel XXTEA functions
    m.def(
        "xxtea_encrypt_parallel",
        [](const std::vector<uint32_t>& data,
           const atom::algorithm::XTEAKey& key, size_t num_threads) {
            return atom::algorithm::xxteaEncryptParallel(data, key,
                                                         num_threads);
        },
        py::arg("data"), py::arg("key"), py::arg("num_threads") = 0,
        R"(Encrypts a vector of 32-bit values using the XXTEA algorithm with parallel processing.

This function uses multiple threads to improve performance for large data sets.

Args:
    data: A list/vector of 32-bit values to be encrypted
    key: A 128-bit key (XTEAKey object)
    num_threads: Number of threads to use (0 = use system default)

Returns:
    A list of encrypted 32-bit values

Examples:
    >>> from atom.algorithm.tea import xxtea_encrypt_parallel, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> data = [0x12345678] * 1000  # Large data set
    >>> encrypted = xxtea_encrypt_parallel(data, key)
)");

    m.def(
        "xxtea_decrypt_parallel",
        [](const std::vector<uint32_t>& data,
           const atom::algorithm::XTEAKey& key, size_t num_threads) {
            return atom::algorithm::xxteaDecryptParallel(data, key,
                                                         num_threads);
        },
        py::arg("data"), py::arg("key"), py::arg("num_threads") = 0,
        R"(Decrypts a vector of 32-bit values using the XXTEA algorithm with parallel processing.

This function uses multiple threads to improve performance for large data sets.

Args:
    data: A list/vector of 32-bit values to be decrypted
    key: A 128-bit key (XTEAKey object)
    num_threads: Number of threads to use (0 = use system default)

Returns:
    A list of decrypted 32-bit values

Examples:
    >>> from atom.algorithm.tea import xxtea_encrypt_parallel, xxtea_decrypt_parallel, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> data = [0x12345678] * 1000  # Large data set
    >>> encrypted = xxtea_encrypt_parallel(data, key)
    >>> decrypted = xxtea_decrypt_parallel(encrypted, key)
)");

    // Byte array conversion utilities
    m.def(
        "to_uint32_vector",
        [](const py::bytes& data) {
            std::string str = data;
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(str.data());
            std::vector<uint8_t> bytes(ptr, ptr + str.size());
            return atom::algorithm::toUint32Vector(bytes);
        },
        py::arg("data"),
        R"(Converts a byte string to a vector of 32-bit unsigned integers.

This function is used to prepare byte data for encryption with XXTEA.

Args:
    data: A bytes object to be converted

Returns:
    A list of 32-bit unsigned integers

Examples:
    >>> from atom.algorithm.tea import to_uint32_vector, xxtea_encrypt, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> data = b"This is a test message for encryption"
    >>> uint32_data = to_uint32_vector(data)
    >>> encrypted = xxtea_encrypt(uint32_data, key)
)");

    m.def(
        "to_byte_array",
        [](const std::vector<uint32_t>& data) {
            std::vector<uint8_t> bytes = atom::algorithm::toByteArray(data);
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("data"),
        R"(Converts a vector of 32-bit unsigned integers to a byte string.

This function is used to convert XXTEA decryption results back to byte data.

Args:
    data: A list of 32-bit unsigned integers to be converted

Returns:
    A bytes object

Examples:
    >>> from atom.algorithm.tea import to_uint32_vector, to_byte_array, xxtea_encrypt, xxtea_decrypt, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> original = b"This is a test message for encryption"
    >>> uint32_data = to_uint32_vector(original)
    >>> encrypted = xxtea_encrypt(uint32_data, key)
    >>> decrypted = xxtea_decrypt(encrypted, key)
    >>> result = to_byte_array(decrypted)
    >>> print(result)  # Should be the original message
)");

    // Convenience functions
    m.def(
        "encrypt_bytes",
        [](const py::bytes& data, const atom::algorithm::XTEAKey& key,
           bool use_parallel, size_t num_threads) {
            std::string str = data;
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(str.data());
            std::vector<uint8_t> bytes(ptr, ptr + str.size());

            auto uint32_data = atom::algorithm::toUint32Vector(bytes);
            std::vector<uint32_t> encrypted;

            if (use_parallel) {
                encrypted = atom::algorithm::xxteaEncryptParallel(
                    uint32_data, key, num_threads);
            } else {
                encrypted = atom::algorithm::xxteaEncrypt(uint32_data, key);
            }

            std::vector<uint8_t> result_bytes =
                atom::algorithm::toByteArray(encrypted);
            return py::bytes(reinterpret_cast<const char*>(result_bytes.data()),
                             result_bytes.size());
        },
        py::arg("data"), py::arg("key"), py::arg("use_parallel") = false,
        py::arg("num_threads") = 0,
        R"(Encrypts a byte string using the XXTEA algorithm in a single step.

This is a convenience function that handles conversion between byte data and 32-bit values.

Args:
    data: A bytes object to be encrypted
    key: A 128-bit key (XTEAKey object)
    use_parallel: Whether to use parallel processing (default: False)
    num_threads: Number of threads to use if use_parallel is True (0 = use system default)

Returns:
    The encrypted byte string

Examples:
    >>> from atom.algorithm.tea import encrypt_bytes, decrypt_bytes, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> original = b"This is a test message for encryption"
    >>> encrypted = encrypt_bytes(original, key)
    >>> decrypted = decrypt_bytes(encrypted, key)
    >>> print(decrypted)  # Should be the original message
)");

    m.def(
        "decrypt_bytes",
        [](const py::bytes& data, const atom::algorithm::XTEAKey& key,
           bool use_parallel, size_t num_threads) {
            std::string str = data;
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(str.data());
            std::vector<uint8_t> bytes(ptr, ptr + str.size());

            auto uint32_data = atom::algorithm::toUint32Vector(bytes);
            std::vector<uint32_t> decrypted;

            if (use_parallel) {
                decrypted = atom::algorithm::xxteaDecryptParallel(
                    uint32_data, key, num_threads);
            } else {
                decrypted = atom::algorithm::xxteaDecrypt(uint32_data, key);
            }

            std::vector<uint8_t> result_bytes =
                atom::algorithm::toByteArray(decrypted);
            return py::bytes(reinterpret_cast<const char*>(result_bytes.data()),
                             result_bytes.size());
        },
        py::arg("data"), py::arg("key"), py::arg("use_parallel") = false,
        py::arg("num_threads") = 0,
        R"(Decrypts a byte string using the XXTEA algorithm in a single step.

This is a convenience function that handles conversion between byte data and 32-bit values.

Args:
    data: A bytes object to be decrypted
    key: A 128-bit key (XTEAKey object)
    use_parallel: Whether to use parallel processing (default: False)
    num_threads: Number of threads to use if use_parallel is True (0 = use system default)

Returns:
    The decrypted byte string

Examples:
    >>> from atom.algorithm.tea import encrypt_bytes, decrypt_bytes, XTEAKey
    >>> key = XTEAKey([0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210])
    >>> original = b"This is a test message for encryption"
    >>> encrypted = encrypt_bytes(original, key)
    >>> decrypted = decrypt_bytes(encrypted, key)
    >>> print(decrypted)  # Should be the original message
)");

    // Generate a random key
    m.def(
        "generate_random_key",
        []() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> distrib;

            atom::algorithm::XTEAKey key;
            for (size_t i = 0; i < key.size(); ++i) {
                key[i] = distrib(gen);
            }

            return key;
        },
        R"(Generates a random 128-bit key for TEA/XTEA/XXTEA encryption.

Returns:
    A randomly generated XTEAKey object

Examples:
    >>> from atom.algorithm.tea import generate_random_key, encrypt_bytes
    >>> key = generate_random_key()
    >>> print(key)
    >>> encrypted = encrypt_bytes(b"Secret message", key)
)");
}
