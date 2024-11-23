/*
 * base.cpp
 *
 * Copyright (C)
 */

#include "base.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

#ifdef ATOM_USE_SIMD
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif
#endif

namespace atom::algorithm {

// Base64字符表
constexpr std::string_view BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// 创建Base64反向查找表
constexpr auto createReverseLookupTable() {
    std::array<unsigned char, 256> table{};
    for (size_t i = 0; i < BASE64_CHARS.size(); ++i) {
        table[static_cast<unsigned char>(BASE64_CHARS[i])] =
            static_cast<unsigned char>(i);
    }
    return table;
}

constexpr auto REVERSE_LOOKUP = createReverseLookupTable();

// 通用的Base64编码实现
template <typename InputIt, typename OutputIt>
void base64EncodeImpl(InputIt begin, InputIt end, OutputIt dest) {
    std::array<unsigned char, 3> input{};
    std::array<unsigned char, 4> output{};
    size_t inputLen = 0;

    while (begin != end) {
        inputLen = 0;
        for (size_t i = 0; i < 3 && begin != end; ++i, ++begin) {
            input[i] = static_cast<unsigned char>(*begin);
            ++inputLen;
        }

        output[0] = (input[0] & 0xfc) >> 2;
        output[1] = ((input[0] & 0x03) << 4) |
                    ((inputLen > 1 ? input[1] : 0) & 0xf0) >> 4;
        output[2] = ((inputLen > 1 ? input[1] : 0) & 0x0f) << 2 |
                    ((inputLen > 2 ? input[2] : 0) & 0xc0) >> 6;
        output[3] = (inputLen > 2 ? input[2] : 0) & 0x3f;

        for (size_t i = 0; i < (inputLen + 1); ++i) {
            *dest++ = BASE64_CHARS[output[i]];
        }

        while (inputLen++ < 3) {
            *dest++ = '=';
        }
    }
}

#ifdef ATOM_USE_SIMD
// SIMD优化的Base64编码实现（使用SSE2）
template <typename InputIt, typename OutputIt>
void base64EncodeSIMD(InputIt begin, InputIt end, OutputIt dest) {
#if defined(__SSE2__)
    const size_t simd_block_size = 12;  // 12字节输入 -> 16字节输出
    while (std::distance(begin, end) >= simd_block_size) {
        // 加载12字节数据
        __m128i data =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(&(*begin)));

        // 分别提取每3字节块并编码为4个Base64字符
        // 这里需要实现具体的Base64编码逻辑，包含位操作和查表

        // 示例实现：仅复制输入到输出（需要替换为实际编码逻辑）
        // 请根据Base64算法实现具体的SIMD编码步骤

        // 假设每12字节转换为16字节Base64
        // 这里简化为将数据直接复制
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&(*dest)), data);

        begin += simd_block_size;
        dest += 16;
    }
#endif
    // 处理剩余部分
    base64EncodeImpl(begin, end, dest);
}
#endif

// 通用的Base64解码实现
template <typename InputIt, typename OutputIt>
void base64DecodeImpl(InputIt begin, InputIt end, OutputIt dest) {
    std::array<unsigned char, 4> input{};
    std::array<unsigned char, 3> output{};
    size_t inputLen = 0;

    while (begin != end && *begin != '=') {
        inputLen = 0;
        for (size_t i = 0; i < 4 && begin != end && *begin != '=';
             ++i, ++begin) {
            input[i] = REVERSE_LOOKUP[static_cast<unsigned char>(*begin)];
            ++inputLen;
        }

        if (inputLen >= 2) {
            output[0] = (input[0] << 2) | (input[1] >> 4);
            *dest++ = output[0];
        }
        if (inputLen >= 3) {
            output[1] = ((input[1] & 0x0f) << 4) | (input[2] >> 2);
            *dest++ = output[1];
        }
        if (inputLen == 4) {
            output[2] = ((input[2] & 0x03) << 6) | input[3];
            *dest++ = output[2];
        }
    }
}

#ifdef ATOM_USE_SIMD
// SIMD优化的Base64解码实现（使用SSE2）
template <typename InputIt, typename OutputIt>
void base64DecodeSIMD(InputIt begin, InputIt end, OutputIt dest) {
#if defined(__SSE2__)
    const size_t simd_block_size = 16;  // 16字符输入 -> 12字节输出
    while (std::distance(begin, end) >= simd_block_size) {
        // 加载16字符数据
        __m128i data =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(&(*begin)));

        // 将Base64字符转换为6-bit值
        // 需要实现字符到值的映射，可以使用查找表或位操作
        // 这里简化为直接复制（需替换为实际解码逻辑）
        __m128i decoded = data;  // 替换为实际解码结果

        // 存储解码后的12字节数据
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&(*dest)), decoded);

        begin += simd_block_size;
        dest += 12;
    }
#endif
    // 处理剩余部分
    base64DecodeImpl(begin, end, dest);
}
#endif

// Base64编码接口
auto base64Encode(std::string_view input) -> std::string {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);
#ifdef ATOM_USE_SIMD
    base64EncodeSIMD(input.begin(), input.end(), std::back_inserter(output));
#else
    base64EncodeImpl(input.begin(), input.end(), std::back_inserter(output));
#endif
    return output;
}

// Base64解码接口
auto base64Decode(std::string_view input) -> std::string {
    std::string output;
    output.reserve((input.size() / 4) * 3);
#ifdef ATOM_USE_SIMD
    base64DecodeSIMD(input.begin(), input.end(), std::back_inserter(output));
#else
    base64DecodeImpl(input.begin(), input.end(), std::back_inserter(output));
#endif
    return output;
}

// 检查是否为有效的Base64字符串
auto isBase64(const std::string& str) -> bool {
    if (str.empty() || str.length() % 4 != 0) {
        return false;
    }
    for (char c : str) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' &&
            c != '/' && c != '=') {
            return false;
        }
    }
    return true;
}

// XOR加密/解密
auto xorEncryptDecrypt(std::string_view text, uint8_t key) -> std::string {
    std::string result;
    result.reserve(text.size());
    std::transform(text.begin(), text.end(), std::back_inserter(result),
                   [key](char c) { return c ^ key; });
    return result;
}

auto xorEncrypt(std::string_view plaintext, uint8_t key) -> std::string {
    return xorEncryptDecrypt(plaintext, key);
}

auto xorDecrypt(std::string_view ciphertext, uint8_t key) -> std::string {
    return xorEncryptDecrypt(ciphertext, key);
}

constexpr std::string_view BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

auto encodeBase32(const std::vector<uint8_t>& data) -> std::string {
    std::string encoded;
    encoded.reserve(((data.size() * 8) + 4) / 5);
    uint32_t buffer = 0;
    int bitsLeft = 0;

    for (auto byte : data) {
        buffer <<= 8;
        buffer |= byte & 0xFF;
        bitsLeft += 8;
        while (bitsLeft >= 5) {
            encoded += BASE32_ALPHABET[(buffer >> (bitsLeft - 5)) & 0x1F];
            bitsLeft -= 5;
        }
    }

    if (bitsLeft > 0) {
        buffer <<= (5 - bitsLeft);
        encoded += BASE32_ALPHABET[buffer & 0x1F];
    }

    while (encoded.size() % 8 != 0) {
        encoded += '=';
    }

    return encoded;
}

auto decodeBase32(const std::string& encoded) -> std::vector<uint8_t> {
    std::vector<uint8_t> decoded;
    decoded.reserve((encoded.size() * 5) / 8);
    uint32_t buffer = 0;
    int bitsLeft = 0;

    for (char c : encoded) {
        if (c == '=') {
            break;
        }
        auto pos = BASE32_ALPHABET.find(c);
        if (pos == std::string_view::npos) {
            throw std::invalid_argument("Invalid character in Base32 string");
        }
        buffer <<= 5;
        buffer |= pos & 0x1F;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            decoded.push_back(
                static_cast<uint8_t>((buffer >> (bitsLeft - 8)) & 0xFF));
            bitsLeft -= 8;
        }
    }

    return decoded;
}

}  // namespace atom::algorithm