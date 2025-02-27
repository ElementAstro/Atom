#ifndef ATOM_ALGORITHM_SHA1_HPP
#define ATOM_ALGORITHM_SHA1_HPP

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#ifdef __AVX2__
#include <immintrin.h>  // AVX2指令集
#endif

namespace atom::algorithm {

// 定义字节容器概念
template <typename T>
concept ByteContainer = requires(T t) {
    { std::data(t) } -> std::convertible_to<const uint8_t*>;
    { std::size(t) } -> std::convertible_to<size_t>;
};

class SHA1 {
public:
    SHA1() noexcept;

    // 增加span版本的update方法
    void update(std::span<const uint8_t> data) noexcept;
    void update(const uint8_t* data, size_t length);

    // 支持任何符合ByteContainer概念的容器
    template <ByteContainer Container>
    void update(const Container& container) noexcept {
        update(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(std::data(container)),
            std::size(container)));
    }

    [[nodiscard]] auto digest() noexcept -> std::array<uint8_t, 20>;
    [[nodiscard]] auto digestAsString() noexcept -> std::string;
    void reset() noexcept;

    static constexpr size_t DIGEST_SIZE = 20;

private:
    void processBlock(const uint8_t* block) noexcept;

    // 让编译器更容易内联和优化
    [[nodiscard]] static constexpr auto rotateLeft(
        uint32_t value, size_t bits) noexcept -> uint32_t {
        return (value << bits) | (value >> (WORD_SIZE - bits));
    }

#ifdef __AVX2__
    // SIMD优化版本的块处理
    void processBlockSIMD(const uint8_t* block) noexcept;
#endif

    static constexpr size_t BLOCK_SIZE = 64;
    static constexpr size_t HASH_SIZE = 5;
    static constexpr size_t SCHEDULE_SIZE = 80;
    static constexpr size_t LENGTH_SIZE = 8;
    static constexpr size_t BITS_PER_BYTE = 8;
    static constexpr uint8_t PADDING_BYTE = 0x80;
    static constexpr uint8_t BYTE_MASK = 0xFF;
    static constexpr size_t WORD_SIZE = 32;

    std::array<uint32_t, HASH_SIZE> hash_;
    std::array<uint8_t, BLOCK_SIZE> buffer_;
    uint64_t bitCount_;
    bool useSIMD_ = false;
};

// 辅助函数升级为泛型，支持任意长度的字节数组
template <size_t N>
[[nodiscard]] auto bytesToHex(const std::array<uint8_t, N>& bytes) noexcept
    -> std::string;

// 特例化最常用的20字节版本
template <>
[[nodiscard]] auto bytesToHex<SHA1::DIGEST_SIZE>(
    const std::array<uint8_t, SHA1::DIGEST_SIZE>& bytes) noexcept
    -> std::string;

// 并发计算多个哈希
template <ByteContainer... Containers>
[[nodiscard]] auto computeHashesInParallel(const Containers&... containers)
    -> std::vector<std::array<uint8_t, SHA1::DIGEST_SIZE>>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SHA1_HPP
