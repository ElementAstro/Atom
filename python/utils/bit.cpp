#include "atom/utils/bit.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Template function to bind all bit manipulation functions for a specific
// numeric type
template <typename T>
void bind_bit_functions(py::module& m, const std::string& type_suffix) {
    // Bind createMask for this type
    m.def(("create_mask_" + type_suffix).c_str(), &atom::utils::createMask<T>,
          py::arg("bits"),
          R"(Creates a bitmask with the specified number of bits set to 1.

Args:
    bits: The number of bits to set to 1.

Returns:
    The bitmask with the specified number of bits set to 1.

Raises:
    RuntimeError: If number of bits is negative.

Examples:
    >>> from atom.utils import bit
    >>> bit.create_mask_u32(8)  # Creates 0x000000FF
)");

    // Bind countBytes for this type
    m.def(("count_bits_" + type_suffix).c_str(), &atom::utils::countBytes<T>,
          py::arg("value"),
          R"(Counts the number of set bits (1s) in the given value.

Args:
    value: The value whose set bits are to be counted.

Returns:
    The number of set bits in the value.

Examples:
    >>> from atom.utils import bit
    >>> bit.count_bits_u32(0x0000000F)  # Returns 4
)");

    // Bind reverseBits for this type
    m.def(("reverse_bits_" + type_suffix).c_str(), &atom::utils::reverseBits<T>,
          py::arg("value"),
          R"(Reverses the bits in the given value.

Args:
    value: The value whose bits are to be reversed.

Returns:
    The value with its bits reversed.

Examples:
    >>> from atom.utils import bit
    >>> bit.reverse_bits_u8(0x01)  # Returns 0x80
)");

    // Bind rotateLeft for this type
    m.def(("rotate_left_" + type_suffix).c_str(), &atom::utils::rotateLeft<T>,
          py::arg("value"), py::arg("shift"),
          R"(Performs a left rotation on the bits of the given value.

Args:
    value: The value to rotate.
    shift: The number of positions to rotate left.

Returns:
    The value after left rotation.

Raises:
    RuntimeError: If shift is negative.

Examples:
    >>> from atom.utils import bit
    >>> bit.rotate_left_u8(0x01, 1)  # Returns 0x02
)");

    // Bind rotateRight for this type
    m.def(("rotate_right_" + type_suffix).c_str(), &atom::utils::rotateRight<T>,
          py::arg("value"), py::arg("shift"),
          R"(Performs a right rotation on the bits of the given value.

Args:
    value: The value to rotate.
    shift: The number of positions to rotate right.

Returns:
    The value after right rotation.

Raises:
    RuntimeError: If shift is negative.

Examples:
    >>> from atom.utils import bit
    >>> bit.rotate_right_u8(0x80, 1)  # Returns 0x40
)");

    // Bind mergeMasks for this type
    m.def(("merge_masks_" + type_suffix).c_str(), &atom::utils::mergeMasks<T>,
          py::arg("mask1"), py::arg("mask2"),
          R"(Merges two bitmasks into one.

Args:
    mask1: The first bitmask.
    mask2: The second bitmask.

Returns:
    The merged bitmask.

Examples:
    >>> from atom.utils import bit
    >>> bit.merge_masks_u8(0x0F, 0xF0)  # Returns 0xFF
)");

    // Bind splitMask for this type
    m.def(("split_mask_" + type_suffix).c_str(), &atom::utils::splitMask<T>,
          py::arg("mask"), py::arg("position"),
          R"(Splits a bitmask into two parts.

Args:
    mask: The bitmask to split.
    position: The position to split the bitmask.

Returns:
    A tuple containing the two parts of the split bitmask.

Raises:
    RuntimeError: If position is negative or exceeds bit width.

Examples:
    >>> from atom.utils import bit
    >>> bit.split_mask_u8(0xFF, 4)  # Returns (0x0F, 0xF0)
)");

    // Bind isBitSet for this type
    m.def(("is_bit_set_" + type_suffix).c_str(), &atom::utils::isBitSet<T>,
          py::arg("value"), py::arg("position"),
          R"(Checks if a bit at the specified position is set.

Args:
    value: The value to check.
    position: The bit position to check.

Returns:
    True if the bit is set, False otherwise.

Raises:
    RuntimeError: If position is out of range.

Examples:
    >>> from atom.utils import bit
    >>> bit.is_bit_set_u8(0x08, 3)  # Returns True
)");

    // Bind setBit for this type
    m.def(("set_bit_" + type_suffix).c_str(), &atom::utils::setBit<T>,
          py::arg("value"), py::arg("position"),
          R"(Sets a bit at the specified position.

Args:
    value: The value to modify.
    position: The bit position to set.

Returns:
    The modified value with the bit set.

Raises:
    RuntimeError: If position is out of range.

Examples:
    >>> from atom.utils import bit
    >>> bit.set_bit_u8(0x00, 3)  # Returns 0x08
)");

    // Bind clearBit for this type
    m.def(("clear_bit_" + type_suffix).c_str(), &atom::utils::clearBit<T>,
          py::arg("value"), py::arg("position"),
          R"(Clears a bit at the specified position.

Args:
    value: The value to modify.
    position: The bit position to clear.

Returns:
    The modified value with the bit cleared.

Raises:
    RuntimeError: If position is out of range.

Examples:
    >>> from atom.utils import bit
    >>> bit.clear_bit_u8(0xFF, 3)  # Returns 0xF7
)");

    // Bind toggleBit for this type
    m.def(("toggle_bit_" + type_suffix).c_str(), &atom::utils::toggleBit<T>,
          py::arg("value"), py::arg("position"),
          R"(Toggles a bit at the specified position.

Args:
    value: The value to modify.
    position: The bit position to toggle.

Returns:
    The modified value with the bit toggled.

Raises:
    RuntimeError: If position is out of range.

Examples:
    >>> from atom.utils import bit
    >>> bit.toggle_bit_u8(0x00, 3)  # Returns 0x08
    >>> bit.toggle_bit_u8(0x08, 3)  # Returns 0x00
)");

    // Bind findFirstSetBit for this type
    m.def(("find_first_set_bit_" + type_suffix).c_str(),
          &atom::utils::findFirstSetBit<T>, py::arg("value"),
          R"(Finds the position of the first set bit.

Args:
    value: The value to check.

Returns:
    Position of the first set bit (0-indexed) or -1 if no bits are set.

Examples:
    >>> from atom.utils import bit
    >>> bit.find_first_set_bit_u8(0x08)  # Returns 3
    >>> bit.find_first_set_bit_u8(0x00)  # Returns -1
)");

    // Bind findLastSetBit for this type
    m.def(("find_last_set_bit_" + type_suffix).c_str(),
          &atom::utils::findLastSetBit<T>, py::arg("value"),
          R"(Finds the position of the last set bit.

Args:
    value: The value to check.

Returns:
    Position of the last set bit (0-indexed) or -1 if no bits are set.

Examples:
    >>> from atom.utils import bit
    >>> bit.find_last_set_bit_u8(0x88)  # Returns 7
    >>> bit.find_last_set_bit_u8(0x00)  # Returns -1
)");
}

PYBIND11_MODULE(bit, m) {
    m.doc() = "Bit manipulation utilities module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::utils::BitManipulationException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Register BitManipulationException
    py::register_exception<atom::utils::BitManipulationException>(
        m, "BitManipulationError", PyExc_RuntimeError);

    // Bind functions for different unsigned integer types
    bind_bit_functions<uint8_t>(m, "u8");
    bind_bit_functions<uint16_t>(m, "u16");
    bind_bit_functions<uint32_t>(m, "u32");
    bind_bit_functions<uint64_t>(m, "u64");

    // Add SIMD-optimized functions when available
#ifdef ATOM_SIMD_SUPPORT
    m.def(
        "count_bits_parallel",
        [](py::buffer b) -> uint64_t {
            py::buffer_info info = b.request();
            if (info.format != py::format_descriptor<uint8_t>::format()) {
                throw std::runtime_error("Buffer must contain uint8 data");
            }
            auto* data = static_cast<const uint8_t*>(info.ptr);
            return atom::utils::countBitsParallel(data, info.size);
        },
        py::arg("buffer"),
        R"(Counts set bits in a large array using SIMD instructions for performance.

Args:
    buffer: Buffer containing uint8 data (bytes, bytearray, or numpy array).

Returns:
    Total count of set bits.

Raises:
    RuntimeError: If bit counting fails or buffer format is invalid.

Examples:
    >>> from atom.utils import bit
    >>> data = bytearray([0xFF, 0x0F, 0xF0, 0x00])
    >>> bit.count_bits_parallel(data)  # Returns 20
    >>>
    >>> # With numpy array
    >>> import numpy as np
    >>> arr = np.array([255, 15, 240, 0], dtype=np.uint8)
    >>> bit.count_bits_parallel(arr)  # Returns 20
)");
#else
    m.def(
        "count_bits_parallel",
        [](py::buffer b) -> uint64_t {
            py::buffer_info info = b.request();
            if (info.format != py::format_descriptor<uint8_t>::format()) {
                throw std::runtime_error("Buffer must contain uint8 data");
            }
            auto* data = static_cast<const uint8_t*>(info.ptr);
            uint64_t count = 0;
            for (size_t i = 0; i < info.size; ++i) {
                count += std::popcount(data[i]);
            }
            return count;
        },
        py::arg("buffer"),
        R"(Counts set bits in a large array (fallback implementation without SIMD).

Args:
    buffer: Buffer containing uint8 data (bytes, bytearray, or numpy array).

Returns:
    Total count of set bits.

Raises:
    RuntimeError: If buffer format is invalid.

Examples:
    >>> from atom.utils import bit
    >>> data = bytearray([0xFF, 0x0F, 0xF0, 0x00])
    >>> bit.count_bits_parallel(data)  # Returns 20
)");
#endif

    // Add convenience functions for common operations
    m.def("count_set_bits",
          [](uint64_t value) -> uint32_t {
              return atom::utils::countBytes(value);
          },
          py::arg("value"),
          R"(Convenience function to count set bits in a 64-bit value.

          Args:
              value: The value whose set bits are to be counted.

          Returns:
              The number of set bits in the value.

          Examples:
              >>> from atom.utils import bit
              >>> bit.count_set_bits(15)  # Returns 4
              >>> bit.count_set_bits(0xFF)  # Returns 8
          )");

    m.def("create_bitmask",
          [](int bits) -> uint64_t {
              return atom::utils::createMask<uint64_t>(bits);
          },
          py::arg("bits"),
          R"(Convenience function to create a 64-bit bitmask.

          Args:
              bits: The number of bits to set to 1.

          Returns:
              The bitmask with the specified number of bits set to 1.

          Examples:
              >>> from atom.utils import bit
              >>> hex(bit.create_bitmask(8))  # Returns '0xff'
              >>> hex(bit.create_bitmask(16))  # Returns '0xffff'
          )");

    m.def("reverse_byte",
          [](uint8_t value) -> uint8_t {
              return atom::utils::reverseBits(value);
          },
          py::arg("value"),
          R"(Convenience function to reverse bits in a byte.

          Args:
              value: The byte value whose bits are to be reversed.

          Returns:
              The value with its bits reversed.

          Examples:
              >>> from atom.utils import bit
              >>> hex(bit.reverse_byte(0x01))  # Returns '0x80'
              >>> hex(bit.reverse_byte(0x0F))  # Returns '0xf0'
          )");

    m.def("hamming_distance",
          [](uint64_t a, uint64_t b) -> uint32_t {
              return atom::utils::countBytes(a ^ b);
          },
          py::arg("a"), py::arg("b"),
          R"(Calculate the Hamming distance between two values.

          The Hamming distance is the number of positions at which
          the corresponding bits are different.

          Args:
              a: First value.
              b: Second value.

          Returns:
              The Hamming distance between the two values.

          Examples:
              >>> from atom.utils import bit
              >>> bit.hamming_distance(0b1010, 0b1100)  # Returns 2
              >>> bit.hamming_distance(0xFF, 0x00)  # Returns 8
          )");

    m.def("is_power_of_two",
          [](uint64_t value) -> bool {
              return value != 0 && (value & (value - 1)) == 0;
          },
          py::arg("value"),
          R"(Check if a value is a power of two.

          Args:
              value: The value to check.

          Returns:
              True if the value is a power of two, False otherwise.

          Examples:
              >>> from atom.utils import bit
              >>> bit.is_power_of_two(8)   # Returns True
              >>> bit.is_power_of_two(10)  # Returns False
              >>> bit.is_power_of_two(0)   # Returns False
          )");

    m.def("next_power_of_two",
          [](uint64_t value) -> uint64_t {
              if (value == 0) return 1;
              if ((value & (value - 1)) == 0) return value;  // Already power of 2

              value--;
              value |= value >> 1;
              value |= value >> 2;
              value |= value >> 4;
              value |= value >> 8;
              value |= value >> 16;
              value |= value >> 32;
              return value + 1;
          },
          py::arg("value"),
          R"(Find the next power of two greater than or equal to the given value.

          Args:
              value: The input value.

          Returns:
              The next power of two >= value.

          Examples:
              >>> from atom.utils import bit
              >>> bit.next_power_of_two(10)  # Returns 16
              >>> bit.next_power_of_two(8)   # Returns 8
              >>> bit.next_power_of_two(0)   # Returns 1
          )");

    // Add functions for parallel bit operations
    m.def(
        "parallel_bit_operation",
        [](py::buffer b, const std::string& operation) -> py::list {
            py::buffer_info info = b.request();
            if (info.format != py::format_descriptor<uint8_t>::format()) {
                throw std::runtime_error("Buffer must contain uint8 data");
            }

            auto* data = static_cast<const uint8_t*>(info.ptr);
            std::span<const uint8_t> span(data, info.size);
            py::list result;

            if (operation == "count") {
                auto counts = atom::utils::parallelBitOp(
                    span, [](uint8_t x) -> uint8_t { return std::popcount(x); });
                for (auto count : counts) {
                    result.append(count);
                }
            } else if (operation == "reverse") {
                auto reversed = atom::utils::parallelBitOp(span, [](uint8_t x) {
                    return atom::utils::reverseBits(x);
                });
                for (auto rev : reversed) {
                    result.append(rev);
                }
            } else {
                throw std::invalid_argument(
                    "Unknown operation. Supported operations: 'count', 'reverse'");
            }

            return result;
        },
        py::arg("buffer"), py::arg("operation"),
        R"(Performs parallel bit operations on a buffer of data.

Args:
    buffer: Input buffer containing uint8 data.
    operation: Operation to perform ('count', 'reverse').

Returns:
    List containing the results for each byte.

Raises:
    ValueError: If the operation is not supported.
    RuntimeError: If the buffer format is not supported.

Examples:
    >>> from atom.utils import bit
    >>> data = bytearray([0xFF, 0x0F, 0xF0, 0x00])
    >>> bit.parallel_bit_operation(data, "count")  # [8, 4, 4, 0]
    >>> bit.parallel_bit_operation(data, "reverse")  # [255, 240, 15, 0]
)");
}
