/*
 * bit_ops.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Bit manipulation operations - implementations

**************************************************/

#include "bit_ops.hpp"

#include <bit>

// SIMD headers
#ifdef USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#endif

namespace atom::algorithm {

auto bitReverse64(u64 n) noexcept -> u64 {
    // Use efficient platform-specific intrinsics for bit reversal
    if constexpr (std::endian::native == std::endian::little) {
#ifdef USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
        return _byteswap_uint64(n);
#elif defined(__ARM_NEON)
        return vrev64_u8(vcreate_u8(n));
#endif
#endif
    }

    // Optimized implementation using lookup table and constexpr evaluation
    static constexpr u8 lookup[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
                                      0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};

    u64 result = 0;
    for (i32 i = 0; i < 16; ++i) {
        result = (result << 4) | lookup[(n >> (i * 4)) & 0xF];
    }
    return result;
}

}  // namespace atom::algorithm
