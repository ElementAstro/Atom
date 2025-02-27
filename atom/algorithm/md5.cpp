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
#include <cstring>
#include <format>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>

// SIMD和并行支持
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
    buffer_.reserve(64);  // 预分配空间提高性能
}

void MD5::update(std::span<const std::byte> input) {
    try {
        auto update_length = [this](size_t length) {
            if (std::numeric_limits<uint64_t>::max() - count_ < length * 8) {
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
        throw MD5Exception(std::format("Update failed: {}", e.what()));
    }
}

auto MD5::finalize() -> std::string {
    try {
        // Padding
        buffer_.push_back(static_cast<std::byte>(0x80));

        // 调整buffer至最终大小
        const size_t padding_needed =
            (56 <= buffer_.size() && buffer_.size() < 64)
                ? (64 + 56 - buffer_.size())
                : (56 - buffer_.size());

        buffer_.resize(buffer_.size() + padding_needed,
                       static_cast<std::byte>(0));

        // 添加消息长度作为64位整数
        for (int i = 0; i < 8; ++i) {
            buffer_.push_back(
                static_cast<std::byte>((count_ >> (i * 8)) & 0xff));
        }

        // 处理最终块
        if (buffer_.size() == 64) {
            processBlock(std::span<const std::byte, 64>(buffer_.data(), 64));
        } else {
            throw MD5Exception("Buffer size incorrect during finalization");
        }

        // 格式化结果
        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        // 使用std::byteswap实现小端序转换 (C++20)
        ss << std::setw(8) << std::byteswap(a_);
        ss << std::setw(8) << std::byteswap(b_);
        ss << std::setw(8) << std::byteswap(c_);
        ss << std::setw(8) << std::byteswap(d_);

        return ss.str();
    } catch (const std::exception& e) {
        throw MD5Exception(std::format("Finalization failed: {}", e.what()));
    }
}

void MD5::processBlock(std::span<const std::byte, 64> block) noexcept {
    // 将输入块转换为16个32位字
    std::array<uint32_t, 16> M;

#ifdef USE_SIMD
    // 使用AVX2指令集加速数据加载和处理
    for (size_t i = 0; i < 16; i += 4) {
        __m128i chunk =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(&block[i * 4]));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&M[i]), chunk);
    }
#else
    // 标准实现
    for (size_t i = 0; i < 16; ++i) {
        uint32_t value = 0;
        for (size_t j = 0; j < 4; ++j) {
            value |= static_cast<uint32_t>(
                         std::to_integer<uint8_t>(block[i * 4 + j]))
                     << (j * 8);
        }
        M[i] = value;
    }
#endif

    uint32_t a = a_;
    uint32_t b = b_;
    uint32_t c = c_;
    uint32_t d = d_;

#ifdef USE_OPENMP
    // 划分为四个独立阶段，每个阶段可并行处理
    constexpr int rounds[] = {16, 32, 48, 64};
    for (int round = 0; round < 4; ++round) {
        const int start = (round > 0) ? rounds[round - 1] : 0;
        const int end = rounds[round];

#pragma omp parallel for
        for (int i = start; i < end; ++i) {
            uint32_t f, g;

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

            uint32_t temp = d;
            d = c;
            c = b;
            b = b + leftRotate(a + f + T[i] + M[g], s[i]);
            a = temp;
        }
    }
#else
    // 标准串行实现
    for (uint32_t i = 0; i < 64; ++i) {
        uint32_t f, g;
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

        uint32_t temp = d;
        d = c;
        c = b;
        b = b + leftRotate(a + f + T[i] + M[g], s[i]);
        a = temp;
    }
#endif

    // 更新状态变量
    a_ += a;
    b_ += b;
    c_ += c;
    d_ += d;
}

constexpr auto MD5::F(uint32_t x, uint32_t y, uint32_t z) noexcept -> uint32_t {
    return (x & y) | (~x & z);
}

constexpr auto MD5::G(uint32_t x, uint32_t y, uint32_t z) noexcept -> uint32_t {
    return (x & z) | (y & ~z);
}

constexpr auto MD5::H(uint32_t x, uint32_t y, uint32_t z) noexcept -> uint32_t {
    return x ^ y ^ z;
}

constexpr auto MD5::I(uint32_t x, uint32_t y, uint32_t z) noexcept -> uint32_t {
    return y ^ (x | ~z);
}

constexpr auto MD5::leftRotate(uint32_t x, uint32_t n) noexcept -> uint32_t {
    return std::rotl(x, n);  // C++20的std::rotl
}

auto MD5::encryptBinary(std::span<const std::byte> data) -> std::string {
    try {
        MD5 md5;
        md5.init();
        md5.update(data);
        return md5.finalize();
    } catch (const std::exception& e) {
        throw MD5Exception(
            std::format("Binary encryption failed: {}", e.what()));
    }
}

}  // namespace atom::algorithm
