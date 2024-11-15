#ifndef ATOM_TYPE_UINT_HPP
#define ATOM_TYPE_UINT_HPP

#include <cstdint>

#include "atom/error/exception.hpp"

/// Maximum value for uint8_t
constexpr uint8_t MAX_UINT8 = 0xFF;

/// Maximum value for uint16_t
constexpr uint16_t MAX_UINT16 = 0xFFFF;

/// Maximum value for uint32_t
constexpr uint32_t MAX_UINT32 = 0xFFFFFFFF;

/**
 * @brief User-defined literal for uint8_t type.
 *
 * This literal allows you to create uint8_t values using the _u8 suffix.
 *
 * @param value The value to be converted to uint8_t.
 * @return uint8_t The converted value.
 * @throws std::out_of_range if the value exceeds the range of uint8_t.
 */
constexpr auto operator"" _u8(unsigned long long value) -> uint8_t {
    if (value > MAX_UINT8) {  // uint8_t maximum value is 0xFF
        THROW_OUT_OF_RANGE("Value exceeds uint8_t range");
    }
    return static_cast<uint8_t>(value);
}

/**
 * @brief User-defined literal for uint16_t type.
 *
 * This literal allows you to create uint16_t values using the _u16 suffix.
 *
 * @param value The value to be converted to uint16_t.
 * @return uint16_t The converted value.
 * @throws std::out_of_range if the value exceeds the range of uint16_t.
 */
constexpr auto operator"" _u16(unsigned long long value) -> uint16_t {
    if (value > MAX_UINT16) {  // uint16_t maximum value is 0xFFFF
        THROW_OUT_OF_RANGE("Value exceeds uint16_t range");
    }
    return static_cast<uint16_t>(value);
}

/**
 * @brief User-defined literal for uint32_t type.
 *
 * This literal allows you to create uint32_t values using the _u32 suffix.
 *
 * @param value The value to be converted to uint32_t.
 * @return uint32_t The converted value.
 * @throws std::out_of_range if the value exceeds the range of uint32_t.
 */
constexpr auto operator"" _u32(unsigned long long value) -> uint32_t {
    if (value > MAX_UINT32) {  // uint32_t maximum value is 0xFFFFFFFF
        THROW_OUT_OF_RANGE("Value exceeds uint32_t range");
    }
    return static_cast<uint32_t>(value);
}

/**
 * @brief User-defined literal for uint64_t type.
 *
 * This literal allows you to create uint64_t values using the _u64 suffix.
 *
 * @param value The value to be converted to uint64_t.
 * @return uint64_t The converted value.
 */
constexpr auto operator"" _u64(unsigned long long value) -> uint64_t {
    return static_cast<uint64_t>(value);
}

#endif  // ATOM_TYPE_UINT_HPP