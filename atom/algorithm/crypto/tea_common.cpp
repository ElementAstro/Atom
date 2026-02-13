#include "tea_common.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include "atom/algorithm/common/endian.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/endian/conversion.hpp>
#endif

namespace atom::algorithm {

// Optimized byte conversion function using compile-time conditional branches
static inline u32 byteToNative(u8 byte, i32 position) noexcept {
    u32 value = static_cast<u32>(byte) << (position * BYTE_SHIFT);
#ifdef ATOM_USE_BOOST
    if constexpr (std::endian::native != std::endian::little) {
        return boost::endian::little_to_native(value);
    }
#endif
    return value;
}

static inline u8 nativeToByte(u32 value, i32 position) noexcept {
#ifdef ATOM_USE_BOOST
    if constexpr (std::endian::native != std::endian::little) {
        value = boost::endian::native_to_little(value);
    }
#endif
    return static_cast<u8>(value >> (position * BYTE_SHIFT));
}

// Helper function to validate key
bool isValidKey(const std::array<u32, 4>& key) noexcept {
    // Check if the key is all zeros, which is generally insecure
    if (key[0] == 0 && key[1] == 0 && key[2] == 0 && key[3] == 0) {
        return false;
    }

    // Check for low entropy (all same values)
    if (key[0] == key[1] && key[1] == key[2] && key[2] == key[3]) {
        spdlog::warn("TEA key has low entropy: all values are identical");
    }

    // Count unique bytes for basic entropy check
    std::array<u8, 16> key_bytes{};
    for (usize i = 0; i < 4; ++i) {
        key_bytes[i * 4] = static_cast<u8>(key[i] >> 24);
        key_bytes[i * 4 + 1] = static_cast<u8>(key[i] >> 16);
        key_bytes[i * 4 + 2] = static_cast<u8>(key[i] >> 8);
        key_bytes[i * 4 + 3] = static_cast<u8>(key[i]);
    }

    std::array<bool, 256> seen{};
    usize unique_count = 0;
    for (auto byte : key_bytes) {
        if (!seen[byte]) {
            seen[byte] = true;
            ++unique_count;
        }
    }

    if (unique_count < 4) {
        spdlog::warn("TEA key has very low entropy: only {} unique bytes",
                     unique_count);
    }

    return true;
}

// Implementation of non-template versions of toUint32Vector and toByteArray for
// internal use
auto toUint32VectorImpl(std::span<const u8> data) -> std::vector<u32> {
    usize numElements = (data.size() + 3) / 4;
    std::vector<u32> result(numElements, 0);

    for (usize index = 0; index < data.size(); ++index) {
        result[index / 4] |= byteToNative(data[index], index % 4);
    }

    return result;
}

auto toByteArrayImpl(std::span<const u32> data) -> std::vector<u8> {
    std::vector<u8> result(data.size() * 4);

    for (usize index = 0; index < data.size(); ++index) {
        for (i32 bytePos = 0; bytePos < 4; ++bytePos) {
            result[index * 4 + bytePos] = nativeToByte(data[index], bytePos);
        }
    }

    return result;
}

// Explicit template instantiations for common cases
template auto toUint32Vector<std::vector<u8>>(const std::vector<u8>& data)
    -> std::vector<u32>;

template auto toByteArray<std::vector<u32>>(const std::vector<u32>& data)
    -> std::vector<u8>;

}  // namespace atom::algorithm
