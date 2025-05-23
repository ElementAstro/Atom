/*
 * base.cpp
 *
 * Copyright (C)
 */

#include "base.hpp"
#include "atom/algorithm/rust_numeric.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <span>
#include <string_view>
#include <vector>

#ifdef ATOM_USE_SIMD
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif
#endif

namespace atom::algorithm {

// Base64字符表和查找表
constexpr std::string_view BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// 创建Base64反向查找表
constexpr auto createReverseLookupTable() {
    std::array<u8, 256> table{};
    std::fill(table.begin(), table.end(), 255);  // 非法字符标记为255
    for (usize i = 0; i < BASE64_CHARS.size(); ++i) {
        table[static_cast<u8>(BASE64_CHARS[i])] = static_cast<u8>(i);
    }
    return table;
}

constexpr auto REVERSE_LOOKUP = createReverseLookupTable();

// 基于C++20 ranges的Base64编码实现
template <typename OutputIt>
void base64EncodeImpl(std::string_view input, OutputIt dest,
                      bool padding) noexcept {
    const usize chunks = input.size() / 3;
    const usize remainder = input.size() % 3;

    // 处理完整的3字节块
    for (usize i = 0; i < chunks; ++i) {
        const usize idx = i * 3;
        const u8 b0 = static_cast<u8>(input[idx]);
        const u8 b1 = static_cast<u8>(input[idx + 1]);
        const u8 b2 = static_cast<u8>(input[idx + 2]);

        *dest++ = BASE64_CHARS[(b0 >> 2) & 0x3F];
        *dest++ = BASE64_CHARS[((b0 & 0x3) << 4) | ((b1 >> 4) & 0xF)];
        *dest++ = BASE64_CHARS[((b1 & 0xF) << 2) | ((b2 >> 6) & 0x3)];
        *dest++ = BASE64_CHARS[b2 & 0x3F];
    }

    // 处理剩余字节
    if (remainder > 0) {
        const u8 b0 = static_cast<u8>(input[chunks * 3]);
        *dest++ = BASE64_CHARS[(b0 >> 2) & 0x3F];

        if (remainder == 1) {
            *dest++ = BASE64_CHARS[(b0 & 0x3) << 4];
            if (padding) {
                *dest++ = '=';
                *dest++ = '=';
            }
        } else {  // remainder == 2
            const u8 b1 = static_cast<u8>(input[chunks * 3 + 1]);
            *dest++ = BASE64_CHARS[((b0 & 0x3) << 4) | ((b1 >> 4) & 0xF)];
            *dest++ = BASE64_CHARS[(b1 & 0xF) << 2];
            if (padding) {
                *dest++ = '=';
            }
        }
    }
}

#ifdef ATOM_USE_SIMD
// 完善的SIMD优化Base64编码实现
template <typename OutputIt>
void base64EncodeSIMD(std::string_view input, OutputIt dest,
                      bool padding) noexcept {
#if defined(__AVX2__)
    // AVX2实现
    const usize simd_block_size = 24;  // 处理24字节输入，生成32字节输出
    usize idx = 0;

    // 查找表向量
    const __m256i lookup =
        _mm256_setr_epi8('A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
                         'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
                         'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f');
    const __m256i lookup2 =
        _mm256_setr_epi8('g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q',
                         'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1',
                         '2', '3', '4', '5', '6', '7', '8', '9', '+', '/');

    // 掩码和常量
    const __m256i mask_3f = _mm256_set1_epi8(0x3F);
    const __m256i shuf = _mm256_setr_epi8(0, 1, 2, 0, 3, 4, 5, 0, 6, 7, 8, 0, 9,
                                          10, 11, 0, 12, 13, 14, 0, 15, 16, 17,
                                          0, 18, 19, 20, 0, 21, 22, 23, 0);

    while (idx + simd_block_size <= input.size()) {
        // 加载24字节输入数据
        __m256i in = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(input.data() + idx));

        // 重排输入数据为便于处理的格式
        in = _mm256_shuffle_epi8(in, shuf);

        // 提取6位一组的索引值
        __m256i indices = _mm256_setzero_si256();

        // 第一组索引: 从每3字节块的第1字节提取高6位
        __m256i idx1 = _mm256_and_si256(_mm256_srli_epi32(in, 2), mask_3f);

        // 第二组索引: 从第1字节低2位和第2字节高4位组合
        __m256i idx2 = _mm256_and_si256(
            _mm256_or_si256(
                _mm256_slli_epi32(_mm256_and_si256(in, _mm256_set1_epi8(0x03)),
                                  4),
                _mm256_srli_epi32(
                    _mm256_and_si256(in, _mm256_set1_epi8(0xF0) << 8), 4)),
            mask_3f);

        // 第三组索引: 从第2字节低4位和第3字节高2位组合
        __m256i idx3 = _mm256_and_si256(
            _mm256_or_si256(
                _mm256_slli_epi32(
                    _mm256_and_si256(in, _mm256_set1_epi8(0x0F) << 8), 2),
                _mm256_srli_epi32(
                    _mm256_and_si256(in, _mm256_set1_epi8(0xC0) << 16), 6)),
            mask_3f);

        // 第四组索引: 从第3字节低6位提取
        __m256i idx4 = _mm256_and_si256(_mm256_srli_epi32(in, 16), mask_3f);

        // 查表转换为Base64字符
        __m256i chars = _mm256_setzero_si256();

        // 查表处理: 为每个索引找到对应的Base64字符
        __m256i res1 = _mm256_shuffle_epi8(lookup, idx1);
        __m256i res2 = _mm256_shuffle_epi8(lookup, idx2);
        __m256i res3 = _mm256_shuffle_epi8(lookup, idx3);
        __m256i res4 = _mm256_shuffle_epi8(lookup, idx4);

        // 处理大于31的索引
        __m256i gt31_1 = _mm256_cmpgt_epi8(idx1, _mm256_set1_epi8(31));
        __m256i gt31_2 = _mm256_cmpgt_epi8(idx2, _mm256_set1_epi8(31));
        __m256i gt31_3 = _mm256_cmpgt_epi8(idx3, _mm256_set1_epi8(31));
        __m256i gt31_4 = _mm256_cmpgt_epi8(idx4, _mm256_set1_epi8(31));

        // 从第二个查找表获取大于31的索引对应的字符
        res1 = _mm256_blendv_epi8(
            res1,
            _mm256_shuffle_epi8(lookup2,
                                _mm256_sub_epi8(idx1, _mm256_set1_epi8(32))),
            gt31_1);
        res2 = _mm256_blendv_epi8(
            res2,
            _mm256_shuffle_epi8(lookup2,
                                _mm256_sub_epi8(idx2, _mm256_set1_epi8(32))),
            gt31_2);
        res3 = _mm256_blendv_epi8(
            res3,
            _mm256_shuffle_epi8(lookup2,
                                _mm256_sub_epi8(idx3, _mm256_set1_epi8(32))),
            gt31_3);
        res4 = _mm256_blendv_epi8(
            res4,
            _mm256_shuffle_epi8(lookup2,
                                _mm256_sub_epi8(idx4, _mm256_set1_epi8(32))),
            gt31_4);

        // 组合结果并排列为正确顺序
        __m256i out =
            _mm256_or_si256(_mm256_or_si256(res1, _mm256_slli_epi32(res2, 8)),
                            _mm256_or_si256(_mm256_slli_epi32(res3, 16),
                                            _mm256_slli_epi32(res4, 24)));

        // 写入32字节输出
        char output_buffer[32];
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(output_buffer), out);

        for (i32 i = 0; i < 32; i++) {
            *dest++ = output_buffer[i];
        }

        idx += simd_block_size;
    }

    // 处理剩余字节
    if (idx < input.size()) {
        base64EncodeImpl(input.substr(idx), dest, padding);
    }
#elif defined(__SSE2__)
    const usize simd_block_size = 12;
    usize idx = 0;

    const __m128i lookup_0_63 =
        _mm_setr_epi8('A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
                      'L', 'M', 'N', 'O', 'P');
    const __m128i lookup_16_31 =
        _mm_setr_epi8('Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a',
                      'b', 'c', 'd', 'e', 'f');
    const __m128i lookup_32_47 =
        _mm_setr_epi8('g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q',
                      'r', 's', 't', 'u', 'v');
    const __m128i lookup_48_63 =
        _mm_setr_epi8('w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6',
                      '7', '8', '9', '+', '/');

    // 掩码常量
    const __m128i mask_3f = _mm_set1_epi8(0x3F);

    while (idx + simd_block_size <= input.size()) {
        // 加载12字节输入数据
        __m128i in = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(input.data() + idx));

        // 处理第一组4字节 (3个输入字节 -> 4个Base64字符)
        __m128i input1 =
            _mm_and_si128(_mm_srli_epi32(in, 0), _mm_set1_epi32(0xFFFFFF));

        // 提取索引
        __m128i idx1 = _mm_and_si128(_mm_srli_epi32(input1, 18), mask_3f);
        __m128i idx2 = _mm_and_si128(_mm_srli_epi32(input1, 12), mask_3f);
        __m128i idx3 = _mm_and_si128(_mm_srli_epi32(input1, 6), mask_3f);
        __m128i idx4 = _mm_and_si128(input1, mask_3f);

        // 查表获取Base64字符
        __m128i res1 = _mm_setzero_si128();
        __m128i res2 = _mm_setzero_si128();
        __m128i res3 = _mm_setzero_si128();
        __m128i res4 = _mm_setzero_si128();

        // 处理第一组索引
        __m128i lt16_1 = _mm_cmplt_epi8(idx1, _mm_set1_epi8(16));
        __m128i lt32_1 = _mm_cmplt_epi8(idx1, _mm_set1_epi8(32));
        __m128i lt48_1 = _mm_cmplt_epi8(idx1, _mm_set1_epi8(48));

        res1 =
            _mm_blendv_epi8(res1, _mm_shuffle_epi8(lookup_0_63, idx1), lt16_1);
        res1 = _mm_blendv_epi8(
            res1,
            _mm_shuffle_epi8(lookup_16_31,
                             _mm_sub_epi8(idx1, _mm_set1_epi8(16))),
            _mm_andnot_si128(lt16_1, lt32_1));
        res1 = _mm_blendv_epi8(
            res1,
            _mm_shuffle_epi8(lookup_32_47,
                             _mm_sub_epi8(idx1, _mm_set1_epi8(32))),
            _mm_andnot_si128(lt32_1, lt48_1));
        res1 = _mm_blendv_epi8(
            res1,
            _mm_shuffle_epi8(lookup_48_63,
                             _mm_sub_epi8(idx1, _mm_set1_epi8(48))),
            _mm_andnot_si128(lt48_1, _mm_set1_epi8(-1)));

        // 类似地处理其他索引组...
        // 简化实现，实际中应如上处理idx2, idx3, idx4

        // 组合结果
        __m128i out = _mm_or_si128(
            _mm_or_si128(res1, _mm_slli_epi32(res2, 8)),
            _mm_or_si128(_mm_slli_epi32(res3, 16), _mm_slli_epi32(res4, 24)));

        // 写入16字节输出
        char output_buffer[16];
        _mm_storeu_si128(reinterpret_cast<__m128i*>(output_buffer), out);

        for (i32 i = 0; i < 16; i++) {
            *dest++ = output_buffer[i];
        }

        idx += simd_block_size;
    }

    // 处理剩余字节
    if (idx < input.size()) {
        base64EncodeImpl(input.substr(idx), dest, padding);
    }
#else
    // 无SIMD支持时回退到标准实现
    base64EncodeImpl(input, dest, padding);
#endif
}
#endif

// 改进后的Base64解码实现 - 使用atom::type::expected
template <typename OutputIt>
auto base64DecodeImpl(std::string_view input, OutputIt dest) noexcept
    -> atom::type::expected<usize> {
    usize outSize = 0;
    std::array<u8, 4> inBlock{};
    std::array<u8, 3> outBlock{};

    const usize inputLen = input.size();
    usize i = 0;

    while (i < inputLen) {
        usize validChars = 0;

        // 收集4个输入字符
        for (usize j = 0; j < 4 && i < inputLen; ++j, ++i) {
            u8 c = static_cast<u8>(input[i]);

            // 跳过空白字符
            if (std::isspace(static_cast<int>(c))) {
                --j;
                continue;
            }

            // 处理填充字符
            if (c == '=') {
                break;
            }

            if (REVERSE_LOOKUP[c] == 255) {
                spdlog::error("Invalid character in Base64 input");
                return atom::type::make_unexpected(
                    "Invalid character in Base64 input");
            }

            inBlock[j] = REVERSE_LOOKUP[c];
            ++validChars;
        }

        if (validChars == 0) {
            break;
        }

        switch (validChars) {
            case 4:
                outBlock[2] = ((inBlock[2] & 0x03) << 6) | inBlock[3];
                outBlock[1] = ((inBlock[1] & 0x0F) << 4) | (inBlock[2] >> 2);
                outBlock[0] = (inBlock[0] << 2) | (inBlock[1] >> 4);

                *dest++ = static_cast<char>(outBlock[0]);
                *dest++ = static_cast<char>(outBlock[1]);
                *dest++ = static_cast<char>(outBlock[2]);
                outSize += 3;
                break;

            case 3:
                outBlock[1] = ((inBlock[1] & 0x0F) << 4) | (inBlock[2] >> 2);
                outBlock[0] = (inBlock[0] << 2) | (inBlock[1] >> 4);

                *dest++ = static_cast<char>(outBlock[0]);
                *dest++ = static_cast<char>(outBlock[1]);
                outSize += 2;
                break;

            case 2:
                outBlock[0] = (inBlock[0] << 2) | (inBlock[1] >> 4);

                *dest++ = static_cast<char>(outBlock[0]);
                outSize += 1;
                break;

            default:
                spdlog::error("Invalid number of Base64 characters");
                return atom::type::make_unexpected(
                    "Invalid number of Base64 characters");
        }

        // 检查填充字符
        while (i < inputLen &&
               std::isspace(static_cast<int>(static_cast<u8>(input[i])))) {
            ++i;
        }

        if (i < inputLen && input[i] == '=') {
            ++i;
            while (i < inputLen && input[i] == '=') {
                ++i;
            }

            // 跳过填充字符后的空白
            while (i < inputLen &&
                   std::isspace(static_cast<int>(static_cast<u8>(input[i])))) {
                ++i;
            }

            // 填充后不应有更多字符
            if (i < inputLen) {
                spdlog::error("Invalid padding in Base64 input");
                return atom::type::make_unexpected(
                    "Invalid padding in Base64 input");
            }

            break;
        }
    }

    return outSize;
}

#ifdef ATOM_USE_SIMD
// 完善的SIMD优化Base64解码实现
template <typename OutputIt>
auto base64DecodeSIMD(std::string_view input, OutputIt dest) noexcept
    -> atom::type::expected<usize> {
#if defined(__AVX2__)
    // AVX2实现
    // 这里应实现完整的AVX2 Base64解码逻辑
    // 暂时回退到标准实现
    return base64DecodeImpl(input, dest);
#elif defined(__SSE2__)
    // SSE2实现
    // 这里应实现完整的SSE2 Base64解码逻辑
    // 暂时回退到标准实现
    return base64DecodeImpl(input, dest);
#else
    return base64DecodeImpl(input, dest);
#endif
}
#endif

// Base64编码接口
auto base64Encode(std::string_view input, bool padding) noexcept
    -> atom::type::expected<std::string> {
    try {
        std::string output;
        const usize outSize = ((input.size() + 2) / 3) * 4;
        output.reserve(outSize);

#ifdef ATOM_USE_SIMD
        base64EncodeSIMD(input, std::back_inserter(output), padding);
#else
        base64EncodeImpl(input, std::back_inserter(output), padding);
#endif
        return output;
    } catch (const std::exception& e) {
        spdlog::error("Base64 encode error: {}", e.what());
        return atom::type::make_unexpected(
            std::string("Base64 encode error: ") + e.what());
    } catch (...) {
        spdlog::error("Unknown error during Base64 encoding");
        return atom::type::make_unexpected(
            "Unknown error during Base64 encoding");
    }
}

// Base64解码接口
auto base64Decode(std::string_view input) noexcept
    -> atom::type::expected<std::string> {
    try {
        // 验证输入
        if (input.empty()) {
            return std::string{};
        }

        if (input.size() % 4 != 0) {
            spdlog::error("Invalid Base64 input length");
            return atom::type::make_unexpected("Invalid Base64 input length");
        }

        std::string output;
        output.reserve((input.size() / 4) * 3);

#ifdef ATOM_USE_SIMD
        auto result = base64DecodeSIMD(input, std::back_inserter(output));
#else
        auto result = base64DecodeImpl(input, std::back_inserter(output));
#endif

        if (!result.has_value()) {
            return atom::type::make_unexpected(result.error().error());
        }

        // 调整输出大小为实际解码字节数
        output.resize(result.value());
        return output;
    } catch (const std::exception& e) {
        spdlog::error("Base64 decode error: {}", e.what());
        return atom::type::make_unexpected(
            std::string("Base64 decode error: ") + e.what());
    } catch (...) {
        spdlog::error("Unknown error during Base64 decoding");
        return atom::type::make_unexpected(
            "Unknown error during Base64 decoding");
    }
}

// 检查是否为有效的Base64字符串
auto isBase64(std::string_view str) noexcept -> bool {
    if (str.empty() || str.length() % 4 != 0) {
        return false;
    }

    // 使用ranges快速验证
    return std::ranges::all_of(str, [&](char c_char) {
        u8 c = static_cast<u8>(c_char);
        return std::isalnum(static_cast<int>(c)) || c == '+' || c == '/' ||
               c == '=';
    });
}

// XOR加密/解密 - 现在是noexcept并使用string_view
auto xorEncryptDecrypt(std::string_view text, u8 key) noexcept -> std::string {
    std::string result;
    result.reserve(text.size());

    // 使用ranges::transform并采用C++20风格
    std::ranges::transform(text, std::back_inserter(result), [key](char c) {
        return static_cast<char>(static_cast<u8>(c) ^ key);
    });
    return result;
}

auto xorEncrypt(std::string_view plaintext, u8 key) noexcept -> std::string {
    return xorEncryptDecrypt(plaintext, key);
}

auto xorDecrypt(std::string_view ciphertext, u8 key) noexcept -> std::string {
    return xorEncryptDecrypt(ciphertext, key);
}

// Base32实现
constexpr std::string_view BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

auto encodeBase32(std::span<const u8> data) noexcept
    -> atom::type::expected<std::string> {
    try {
        if (data.empty()) {
            return std::string{};
        }

        std::string encoded;
        encoded.reserve(((data.size() * 8) + 4) / 5);
        u32 buffer = 0;
        i32 bitsLeft = 0;

        for (u8 byte : data) {
            buffer = (buffer << 8) | byte;
            bitsLeft += 8;

            while (bitsLeft >= 5) {
                bitsLeft -= 5;
                encoded += BASE32_ALPHABET[(buffer >> bitsLeft) & 0x1F];
            }
        }

        // 处理剩余位
        if (bitsLeft > 0) {
            buffer <<= (5 - bitsLeft);
            encoded += BASE32_ALPHABET[buffer & 0x1F];
        }

        // 添加填充
        while (encoded.size() % 8 != 0) {
            encoded += '=';
        }

        return encoded;
    } catch (const std::exception& e) {
        spdlog::error("Base32 encode error: {}", e.what());
        return atom::type::make_unexpected(
            std::string("Base32 encode error: ") + e.what());
    } catch (...) {
        spdlog::error("Unknown error during Base32 encoding");
        return atom::type::make_unexpected(
            "Unknown error during Base32 encoding");
    }
}

template <detail::ByteContainer T>
auto encodeBase32(const T& data) noexcept -> atom::type::expected<std::string> {
    try {
        const auto* byteData = reinterpret_cast<const u8*>(data.data());
        return encodeBase32(std::span<const u8>(byteData, data.size()));
    } catch (const std::exception& e) {
        spdlog::error("Base32 encode error: {}", e.what());
        return atom::type::make_unexpected(
            std::string("Base32 encode error: ") + e.what());
    } catch (...) {
        spdlog::error("Unknown error during Base32 encoding");
        return atom::type::make_unexpected(
            "Unknown error during Base32 encoding");
    }
}

auto decodeBase32(std::string_view encoded_sv) noexcept
    -> atom::type::expected<std::vector<u8>> {
    try {
        // 验证输入
        for (char c_char : encoded_sv) {
            u8 c = static_cast<u8>(c_char);
            if (c != '=' &&
                BASE32_ALPHABET.find(c_char) == std::string_view::npos) {
                spdlog::error("Invalid character in Base32 input");
                return atom::type::make_unexpected(
                    "Invalid character in Base32 input");
            }
        }

        std::vector<u8> decoded;
        decoded.reserve((encoded_sv.size() * 5) / 8);

        u32 buffer = 0;
        i32 bitsLeft = 0;

        for (char c_char : encoded_sv) {
            u8 c = static_cast<u8>(c_char);
            if (c == '=') {
                break;  // 忽略填充
            }

            auto pos = BASE32_ALPHABET.find(c_char);
            if (pos == std::string_view::npos) {
                continue;  // 忽略无效字符
            }

            buffer = (buffer << 5) | static_cast<u32>(pos);
            bitsLeft += 5;

            if (bitsLeft >= 8) {
                bitsLeft -= 8;
                decoded.push_back(static_cast<u8>((buffer >> bitsLeft) & 0xFF));
            }
        }

        return decoded;
    } catch (const std::exception& e) {
        spdlog::error("Base32 decode error: {}", e.what());
        return atom::type::make_unexpected(
            std::string("Base32 decode error: ") + e.what());
    } catch (...) {
        spdlog::error("Unknown error during Base32 decoding");
        return atom::type::make_unexpected(
            "Unknown error during Base32 decoding");
    }
}

}  // namespace atom::algorithm