/*
 * md5.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Self implemented MD5 algorithm.

**************************************************/

#include "md5.hpp"

#include <bit>
#include <format>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>

// SIMD and parallel support
#ifdef __AVX2__
#include <immintrin.h>
#define USE_SIMD
#endif

#ifdef USE_OPENMP
#include <omp.h>
#endif

namespace atom::algorithm {

MD5::MD5() noexcept { init(); }

void MD5::init() noexcept {
    a_ = 0x67452301;
    b_ = 0xefcdab89;
    c_ = 0x98badcfe;
    d_ = 0x10325476;
    count_ = 0;
    buffer_.clear();
    buffer_.reserve(64);  // Preallocate space for better performance
}

void MD5::update(std::span<const std::byte> input) {
    try {
        auto update_length = [this](usize length) {
            if (std::numeric_limits<u64>::max() - count_ < length * 8) {
                spdlog::error(
                    "MD5: Input too large, would cause counter overflow");
                throw MD5Exception(
                    "Input too large, would cause counter overflow");
            }
            count_ += length * 8;
        };

        update_length(input.size());

        for (const auto& byte : input) {
            buffer_.push_back(byte);
            if (buffer_.size() == 64) {
                processBlock(
                    std::span<const std::byte, 64>(buffer_.data(), 64));
                buffer_.clear();
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("MD5: Update failed - {}", e.what());
        throw MD5Exception(std::format("Update failed: {}", e.what()));
    }
}

auto MD5::finalize() -> std::string {
    try {
        // Padding
        buffer_.push_back(static_cast<std::byte>(0x80));

        // Adjust buffer to final size
        const usize padding_needed =
            (56 <= buffer_.size() && buffer_.size() < 64)
                ? (64 + 56 - buffer_.size())
                : (56 - buffer_.size());

        buffer_.resize(buffer_.size() + padding_needed,
                       static_cast<std::byte>(0));

        // Append message length as 64-bit integer
        for (i32 i = 0; i < 8; ++i) {
            buffer_.push_back(
                static_cast<std::byte>((count_ >> (i * 8)) & 0xff));
        }

        // Process final block
        if (buffer_.size() == 64) {
            processBlock(std::span<const std::byte, 64>(buffer_.data(), 64));
        } else {
            spdlog::error("MD5: Buffer size incorrect during finalization");
            throw MD5Exception("Buffer size incorrect during finalization");
        }

        // Format result
        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        // Use std::byteswap for little-endian conversion (C++20)
        ss << std::setw(8) << std::byteswap(a_);
        ss << std::setw(8) << std::byteswap(b_);
        ss << std::setw(8) << std::byteswap(c_);
        ss << std::setw(8) << std::byteswap(d_);

        return ss.str();
    } catch (const std::exception& e) {
        spdlog::error("MD5: Finalization failed - {}", e.what());
        throw MD5Exception(std::format("Finalization failed: {}", e.what()));
    }
}

void MD5::processBlock(std::span<const std::byte, 64> block) noexcept {
    // Convert input block to 16 32-bit words
    std::array<u32, 16> M;

#ifdef USE_SIMD
    // Use AVX2 instruction set to accelerate data loading and processing
    for (usize i = 0; i < 16; i += 4) {
        __m128i chunk =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(&block[i * 4]));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&M[i]), chunk);
    }
#else
    // Standard implementation
    for (usize i = 0; i < 16; ++i) {
        u32 value = 0;
        for (usize j = 0; j < 4; ++j) {
            value |= static_cast<u32>(std::to_integer<u8>(block[i * 4 + j]))
                     << (j * 8);
        }
        M[i] = value;
    }
#endif

    u32 a = a_;
    u32 b = b_;
    u32 c = c_;
    u32 d = d_;

#ifdef USE_OPENMP
    // Divide into four independent stages, each stage can be processed in
    // parallel
    constexpr i32 rounds[] = {16, 32, 48, 64};
    for (i32 round = 0; round < 4; ++round) {
        const i32 start = (round > 0) ? rounds[round - 1] : 0;
        const i32 end = rounds[round];

#pragma omp parallel for
        for (i32 i = start; i < end; ++i) {
            u32 f, g;

            if (i < 16) {
                f = F(b, c, d);
                g = i;
            } else if (i < 32) {
                f = G(b, c, d);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = H(b, c, d);
                g = (3 * i + 5) % 16;
            } else {
                f = I(b, c, d);
                g = (7 * i) % 16;
            }

            u32 temp = d;
            d = c;
            c = b;
            b = b + leftRotate(a + f + T_Constants[i] + M[g], s[i]);
            a = temp;
        }
    }
#else
    // Standard serial implementation
    for (u32 i = 0; i < 64; ++i) {
        u32 f, g;
        if (i < 16) {
            f = F(b, c, d);
            g = i;
        } else if (i < 32) {
            f = G(b, c, d);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = H(b, c, d);
            g = (3 * i + 5) % 16;
        } else {
            f = I(b, c, d);
            g = (7 * i) % 16;
        }

        u32 temp = d;
        d = c;
        c = b;
        b = b + leftRotate(a + f + T_Constants[i] + M[g], s[i]);
        a = temp;
    }
#endif

    // Update state variables
    a_ += a;
    b_ += b;
    c_ += c;
    d_ += d;
}

constexpr auto MD5::F(u32 x, u32 y, u32 z) noexcept -> u32 {
    return (x & y) | (~x & z);
}

constexpr auto MD5::G(u32 x, u32 y, u32 z) noexcept -> u32 {
    return (x & z) | (y & ~z);
}

constexpr auto MD5::H(u32 x, u32 y, u32 z) noexcept -> u32 { return x ^ y ^ z; }

constexpr auto MD5::I(u32 x, u32 y, u32 z) noexcept -> u32 {
    return y ^ (x | ~z);
}

constexpr auto MD5::leftRotate(u32 x, u32 n) noexcept -> u32 {
    return std::rotl(x, n);  // C++20's std::rotl
}

auto MD5::encryptBinary(std::span<const std::byte> data) -> std::string {
    try {
        spdlog::debug("MD5: Processing binary data of size {}", data.size());
        MD5 md5;
        md5.init();
        md5.update(data);
        return md5.finalize();
    } catch (const std::exception& e) {
        spdlog::error("MD5: Binary encryption failed - {}", e.what());
        throw MD5Exception(
            std::format("Binary encryption failed: {}", e.what()));
    }
}

}  // namespace atom::algorithm
