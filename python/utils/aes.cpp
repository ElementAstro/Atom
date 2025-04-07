#include "atom/utils/aes.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(aes, m) {
    m.doc() = "AES encryption and hashing utility module for the atom package";

    // Register exception translations
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

    // Expose the AES encryption/decryption functions
    m.def(
        "encrypt_aes",
        [](const std::string& plaintext, const std::string& key) {
            std::vector<unsigned char> iv, tag;
            std::string ciphertext =
                atom::utils::encryptAES(plaintext, key, iv, tag);
            return py::make_tuple(ciphertext, iv, tag);
        },
        py::arg("plaintext"), py::arg("key"),
        R"(Encrypts the input plaintext using the AES algorithm.

Args:
    plaintext: The plaintext data to be encrypted
    key: The encryption key

Returns:
    A tuple containing (ciphertext, initialization vector, authentication tag)

Raises:
    ValueError: If inputs are invalid
    RuntimeError: If encryption fails

Examples:
    >>> from atom.utils import aes
    >>> ciphertext, iv, tag = aes.encrypt_aes("hello world", "my-secret-key")
)");

    m.def(
        "decrypt_aes",
        [](const std::string& ciphertext, const std::string& key,
           const std::vector<unsigned char>& iv,
           const std::vector<unsigned char>& tag) {
            return atom::utils::decryptAES(ciphertext, key, iv, tag);
        },
        py::arg("ciphertext"), py::arg("key"), py::arg("iv"), py::arg("tag"),
        R"(Decrypts the input ciphertext using the AES algorithm.

Args:
    ciphertext: The ciphertext data to be decrypted
    key: The decryption key
    iv: Initialization vector used during encryption
    tag: Authentication tag from encryption

Returns:
    The decrypted plaintext

Raises:
    ValueError: If inputs are invalid
    RuntimeError: If decryption fails

Examples:
    >>> from atom.utils import aes
    >>> ciphertext, iv, tag = aes.encrypt_aes("hello world", "my-secret-key")
    >>> plaintext = aes.decrypt_aes(ciphertext, "my-secret-key", iv, tag)
    >>> plaintext
    'hello world'
)");

    // Expose the compression functions
    m.def("compress", &atom::utils::compress<const std::string&>,
          py::arg("data"),
          R"(Compresses the input data using the Zlib library.

Args:
    data: The data to be compressed

Returns:
    The compressed data

Raises:
    ValueError: If input is empty
    RuntimeError: If compression fails

Examples:
    >>> from atom.utils import aes
    >>> compressed = aes.compress("hello world repeated many times")
)");

    m.def("decompress", &atom::utils::decompress<const std::string&>,
          py::arg("data"),
          R"(Decompresses the input data using the Zlib library.

Args:
    data: The data to be decompressed

Returns:
    The decompressed data

Raises:
    ValueError: If input is empty
    RuntimeError: If decompression fails

Examples:
    >>> from atom.utils import aes
    >>> compressed = aes.compress("hello world repeated many times")
    >>> decompressed = aes.decompress(compressed)
)");

    // Expose the hash functions
    m.def("calculate_sha256", &atom::utils::calculateSha256<const std::string&>,
          py::arg("filename"),
          R"(Calculates the SHA-256 hash of a file.

Args:
    filename: The name of the file

Returns:
    The SHA-256 hash of the file, empty string if file doesn't exist

Raises:
    RuntimeError: If hash calculation fails

Examples:
    >>> from atom.utils import aes
    >>> hash_value = aes.calculate_sha256("myfile.txt")
)");

    m.def("calculate_sha224", &atom::utils::calculateSha224, py::arg("data"),
          R"(Calculates the SHA-224 hash of a string.

Args:
    data: The string to be hashed

Returns:
    The SHA-224 hash of the string

Raises:
    ValueError: If input is empty
    RuntimeError: If hash calculation fails

Examples:
    >>> from atom.utils import aes
    >>> hash_value = aes.calculate_sha224("hello world")
)");

    m.def("calculate_sha384", &atom::utils::calculateSha384, py::arg("data"),
          R"(Calculates the SHA-384 hash of a string.

Args:
    data: The string to be hashed

Returns:
    The SHA-384 hash of the string

Raises:
    ValueError: If input is empty
    RuntimeError: If hash calculation fails

Examples:
    >>> from atom.utils import aes
    >>> hash_value = aes.calculate_sha384("hello world")
)");

    m.def("calculate_sha512", &atom::utils::calculateSha512, py::arg("data"),
          R"(Calculates the SHA-512 hash of a string.

Args:
    data: The string to be hashed

Returns:
    The SHA-512 hash of the string

Raises:
    ValueError: If input is empty
    RuntimeError: If hash calculation fails

Examples:
    >>> from atom.utils import aes
    >>> hash_value = aes.calculate_sha512("hello world")
)");
}