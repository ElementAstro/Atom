/*
 * md5.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Self implemented MD5 algorithm.

**************************************************/

#ifndef ATOM_UTILS_MD5_HPP
#define ATOM_UTILS_MD5_HPP

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace atom::algorithm {

// 自定义异常类
class MD5Exception : public std::runtime_error {
public:
    explicit MD5Exception(const std::string& message)
        : std::runtime_error(message) {}
};

// 定义字符串类型的concept
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

/**
 * @class MD5
 * @brief A class that implements the MD5 hashing algorithm.
 */
class MD5 {
public:
    /**
     * @brief Default constructor initializes the MD5 context
     */
    MD5() noexcept;

    /**
     * @brief Encrypts the input string using the MD5 algorithm.
     * @param input The input string to be hashed.
     * @return The MD5 hash of the input string.
     * @throws MD5Exception If input validation fails or internal error occurs.
     */
    template <StringLike T>
    static auto encrypt(const T& input) -> std::string;

    /**
     * @brief Computes MD5 hash for binary data
     * @param data Pointer to data
     * @param length Length of data in bytes
     * @return The MD5 hash as string
     * @throws MD5Exception If input validation fails or internal error occurs.
     */
    static auto encryptBinary(std::span<const std::byte> data) -> std::string;

    /**
     * @brief Verify if a string matches a given MD5 hash
     * @param input Input string to check
     * @param hash Expected MD5 hash
     * @return True if the hash of input matches the expected hash
     */
    template <StringLike T>
    static auto verify(const T& input,
                       const std::string& hash) noexcept -> bool;

private:
    /**
     * @brief Initializes the MD5 context.
     */
    void init() noexcept;

    /**
     * @brief Updates the MD5 context with a new input data.
     * @param input The input data to update the context with.
     * @throws MD5Exception If processing fails.
     */
    void update(std::span<const std::byte> input);

    /**
     * @brief Finalizes the MD5 hash and returns the result.
     * @return The finalized MD5 hash as a string.
     * @throws MD5Exception If finalization fails.
     */
    auto finalize() -> std::string;

    /**
     * @brief Processes a 512-bit block of the input.
     * @param block A span representing the 512-bit block.
     */
    void processBlock(std::span<const std::byte, 64> block) noexcept;

    // 声明MD5算法使用的四个辅助函数为constexpr以支持编译时计算
    static constexpr auto F(uint32_t x, uint32_t y,
                            uint32_t z) noexcept -> uint32_t;
    static constexpr auto G(uint32_t x, uint32_t y,
                            uint32_t z) noexcept -> uint32_t;
    static constexpr auto H(uint32_t x, uint32_t y,
                            uint32_t z) noexcept -> uint32_t;
    static constexpr auto I(uint32_t x, uint32_t y,
                            uint32_t z) noexcept -> uint32_t;
    static constexpr auto leftRotate(uint32_t x,
                                     uint32_t n) noexcept -> uint32_t;

    uint32_t a_, b_, c_, d_;         ///< MD5 state variables.
    uint64_t count_;                 ///< Number of bits processed.
    std::vector<std::byte> buffer_;  ///< Input buffer.

    // 常量表，使用constexpr定义
    static constexpr std::array<uint32_t, 64> T{
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
        0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
        0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
        0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

    static constexpr std::array<uint32_t, 64> s{
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
};

// Template implementation
template <StringLike T>
auto MD5::encrypt(const T& input) -> std::string {
    try {
        std::string_view sv(input);
        if (sv.empty()) {
            return encryptBinary({});
        }

        const auto* data_ptr = reinterpret_cast<const std::byte*>(sv.data());
        return encryptBinary(std::span<const std::byte>(data_ptr, sv.size()));
    } catch (const std::exception& e) {
        throw MD5Exception(std::string("MD5 encryption failed: ") + e.what());
    }
}

template <StringLike T>
auto MD5::verify(const T& input, const std::string& hash) noexcept -> bool {
    try {
        return encrypt(input) == hash;
    } catch (...) {
        return false;
    }
}

}  // namespace atom::algorithm

#endif  // ATOM_UTILS_MD5_HPP
