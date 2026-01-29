#include "atom/utils/aligned.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(aligned, m) {
    m.doc() =
        "Memory alignment validation utilities module for the atom package";

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

    // Utility functions for alignment validation
    m.def(
        "is_valid_alignment",
        [](std::size_t impl_size, std::size_t impl_align,
           std::size_t storage_size, std::size_t storage_align) -> bool {
            return (storage_size >= impl_size) &&
                   (storage_align % impl_align == 0);
        },
        py::arg("impl_size"), py::arg("impl_align"), py::arg("storage_size"),
        py::arg("storage_align"),
        R"(Validates that storage meets alignment requirements for an implementation.

Args:
    impl_size: The size of the implementation in bytes.
    impl_align: The alignment requirement of the implementation in bytes.
    storage_size: The size of the storage in bytes.
    storage_align: The alignment of the storage in bytes.

Returns:
    True if the storage meets the alignment requirements, False otherwise.

Raises:
    ValueError: If any parameter is invalid.

Examples:
    >>> from atom.utils import aligned
    >>> aligned.is_valid_alignment(16, 8, 32, 16)  # Valid alignment
    True
    >>> aligned.is_valid_alignment(32, 8, 16, 16)  # Storage too small
    False
)");

    m.def(
        "validate_alignment",
        [](std::size_t impl_size, std::size_t impl_align,
           std::size_t storage_size, std::size_t storage_align) -> void {
            if (storage_size < impl_size) {
                throw std::invalid_argument(
                    "StorageSize must be greater than or equal to ImplSize");
            }
            if (storage_align % impl_align != 0) {
                throw std::invalid_argument(
                    "StorageAlign must be a multiple of ImplAlign");
            }
        },
        py::arg("impl_size"), py::arg("impl_align"), py::arg("storage_size"),
        py::arg("storage_align"),
        R"(Validates alignment requirements and throws an exception if invalid.

Args:
    impl_size: The size of the implementation in bytes.
    impl_align: The alignment requirement of the implementation in bytes.
    storage_size: The size of the storage in bytes.
    storage_align: The alignment of the storage in bytes.

Raises:
    ValueError: If storage size is insufficient or alignment is invalid.

Examples:
    >>> from atom.utils import aligned
    >>> aligned.validate_alignment(16, 8, 32, 16)  # No exception
    >>> aligned.validate_alignment(32, 8, 16, 16)  # Raises ValueError
)");

    m.def(
        "calculate_aligned_size",
        [](std::size_t size, std::size_t alignment) -> std::size_t {
            if (alignment == 0) {
                throw std::invalid_argument("Alignment cannot be zero");
            }
            if ((alignment & (alignment - 1)) != 0) {
                throw std::invalid_argument("Alignment must be a power of 2");
            }
            return (size + alignment - 1) & ~(alignment - 1);
        },
        py::arg("size"), py::arg("alignment"),
        R"(Calculates the aligned size for a given size and alignment requirement.

Args:
    size: The original size in bytes.
    alignment: The alignment requirement in bytes (must be a power of 2).

Returns:
    The aligned size that meets the alignment requirement.

Raises:
    ValueError: If alignment is zero or not a power of 2.

Examples:
    >>> from atom.utils import aligned
    >>> aligned.calculate_aligned_size(13, 8)  # Returns 16
    16
    >>> aligned.calculate_aligned_size(16, 8)  # Returns 16
    16
)");

    m.def(
        "is_power_of_two",
        [](std::size_t value) -> bool {
            return value != 0 && (value & (value - 1)) == 0;
        },
        py::arg("value"),
        R"(Checks if a value is a power of 2.

Args:
    value: The value to check.

Returns:
    True if the value is a power of 2, False otherwise.

Examples:
    >>> from atom.utils import aligned
    >>> aligned.is_power_of_two(8)   # True
    True
    >>> aligned.is_power_of_two(10)  # False
    False
)");

    m.def(
        "get_alignment_offset",
        [](std::size_t address, std::size_t alignment) -> std::size_t {
            if (alignment == 0) {
                throw std::invalid_argument("Alignment cannot be zero");
            }
            if ((alignment & (alignment - 1)) != 0) {
                throw std::invalid_argument("Alignment must be a power of 2");
            }
            return (alignment - (address % alignment)) % alignment;
        },
        py::arg("address"), py::arg("alignment"),
        R"(Calculates the offset needed to align an address.

Args:
    address: The memory address to align.
    alignment: The alignment requirement in bytes (must be a power of 2).

Returns:
    The number of bytes to add to the address to achieve alignment.

Raises:
    ValueError: If alignment is zero or not a power of 2.

Examples:
    >>> from atom.utils import aligned
    >>> aligned.get_alignment_offset(13, 8)  # Returns 3
    3
    >>> aligned.get_alignment_offset(16, 8)  # Returns 0
    0
)");

    // Common alignment constants
    m.attr("BYTE_ALIGNMENT") = 1;
    m.attr("WORD_ALIGNMENT") = 2;
    m.attr("DWORD_ALIGNMENT") = 4;
    m.attr("QWORD_ALIGNMENT") = 8;
    m.attr("CACHE_LINE_ALIGNMENT") = 64;
    m.attr("PAGE_ALIGNMENT") = 4096;
}
