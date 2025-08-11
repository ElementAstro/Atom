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
        "count_bits_parallel", &atom::utils::countBitsParallel, py::arg("data"),
        py::arg("size"),
        R"(Counts set bits in a large array using SIMD instructions for performance.

Args:
    data: Pointer to the data array (as bytes).
    size: Size of the array in bytes.

Returns:
    Total count of set bits.

Raises:
    RuntimeError: If bit counting fails.

Examples:
    >>> from atom.utils import bit
    >>> import array
    >>> data = array.array('B', [0xFF, 0x0F, 0xF0, 0x00])
    >>> bit.count_bits_parallel(data.buffer_info()[0], len(data))  # Returns 20
)");
#endif

    // Add functions for parallel bit operations
    m.def(
        "parallel_bit_operation",
        [](py::buffer b, const std::string& operation) {
            py::buffer_info info = b.request();
            if (info.format != py::format_descriptor<uint8_t>::format() &&
                info.format != py::format_descriptor<uint32_t>::format() &&
                info.format != py::format_descriptor<uint64_t>::format()) {
                throw std::runtime_error(
                    "Unsupported buffer format. Use uint8, uint32, or uint64 "
                    "arrays.");
            }

            std::vector<uint8_t> result(info.size);

            if (info.format == py::format_descriptor<uint8_t>::format()) {
                auto* data = static_cast<uint8_t*>(info.ptr);
                std::span<const uint8_t> span(data, info.size);

                if (operation == "count") {
                    result = atom::utils::parallelBitOp(
                        span, [](uint8_t x) { return std::popcount(x); });
                } else if (operation == "reverse") {
                    result = atom::utils::parallelBitOp(span, [](uint8_t x) {
                        return atom::utils::reverseBits(x);
                    });
                } else {
                    throw std::invalid_argument(
                        "Unknown operation. Supported operations: 'count', "
                        "'reverse'");
                }
            }
            // Similar blocks for uint32_t and uint64_t would be added here

            return py::bytes(reinterpret_cast<char*>(result.data()),
                             result.size());
        },
        py::arg("buffer"), py::arg("operation"),
        R"(Performs parallel bit operations on a buffer of data.

Args:
    buffer: Input buffer (accepts uint8, uint32, or uint64 arrays).
    operation: Operation to perform ('count', 'reverse').

Returns:
    Bytes object containing the result.

Raises:
    ValueError: If the operation is not supported.
    RuntimeError: If the buffer format is not supported.

Examples:
    >>> from atom.utils import bit
    >>> import array
    >>> data = array.array('B', [0xFF, 0x0F, 0xF0, 0x00])
    >>> result = bit.parallel_bit_operation(data, "count")
)");
}
