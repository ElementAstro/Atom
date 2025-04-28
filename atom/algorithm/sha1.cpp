#include "sha1.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <future>

#ifdef ATOM_USE_BOOST
#include <boost/endian/conversion.hpp>
#endif

namespace atom::algorithm {

SHA1::SHA1() noexcept {
    reset();

    // 检测CPU是否支持SIMD
#ifdef __AVX2__
    useSIMD_ = true;
#endif
}

void SHA1::update(std::span<const uint8_t> data) noexcept {
    update(data.data(), data.size());
}

void SHA1::update(const uint8_t* data, size_t length) {
    // 输入验证
    if (!data && length > 0) {
        throw std::invalid_argument("Null data pointer with non-zero length");
    }

    size_t remaining = length;
    size_t offset = 0;

    while (remaining > 0) {
        size_t bufferOffset = (bitCount_ / 8) % BLOCK_SIZE;

        size_t bytesToFill = BLOCK_SIZE - bufferOffset;
        size_t bytesToCopy = std::min(remaining, bytesToFill);

        // 直接使用std::memcpy提高性能
        std::memcpy(buffer_.data() + bufferOffset, data + offset, bytesToCopy);

        offset += bytesToCopy;
        remaining -= bytesToCopy;
        bitCount_ += bytesToCopy * BITS_PER_BYTE;

        if (bufferOffset + bytesToCopy == BLOCK_SIZE) {
            // 选择SIMD或标准处理方式
#ifdef __AVX2__
            if (useSIMD_) {
                processBlockSIMD(buffer_.data());
            } else {
                processBlock(buffer_.data());
            }
#else
            processBlock(buffer_.data());
#endif
        }
    }
}

auto SHA1::digest() noexcept -> std::array<uint8_t, SHA1::DIGEST_SIZE> {
    uint64_t bitLength = bitCount_;

    // 备份当前状态以保证digest()操作不影响对象状态
    auto hashCopy = hash_;
    auto bufferCopy = buffer_;
    auto bitCountCopy = bitCount_;

    // Padding
    size_t bufferOffset = (bitCountCopy / 8) % BLOCK_SIZE;
    bufferCopy[bufferOffset] = PADDING_BYTE;  // Append the bit '1'

    // 将其余buffer填0
    std::fill(bufferCopy.begin() + bufferOffset + 1,
              bufferCopy.begin() + BLOCK_SIZE, 0);

    if (bufferOffset >= BLOCK_SIZE - LENGTH_SIZE) {
        // 处理当前块，创建新块存储长度
        processBlock(bufferCopy.data());
        std::fill(bufferCopy.begin(), bufferCopy.end(), 0);
    }

    // 使用C++20的位操作来处理字节序
    if constexpr (std::endian::native == std::endian::little) {
        // 在小端系统上需要转换
        bitLength = ((bitLength & 0xff00000000000000ULL) >> 56) |
                    ((bitLength & 0x00ff000000000000ULL) >> 40) |
                    ((bitLength & 0x0000ff0000000000ULL) >> 24) |
                    ((bitLength & 0x000000ff00000000ULL) >> 8) |
                    ((bitLength & 0x00000000ff000000ULL) << 8) |
                    ((bitLength & 0x0000000000ff0000ULL) << 24) |
                    ((bitLength & 0x000000000000ff00ULL) << 40) |
                    ((bitLength & 0x00000000000000ffULL) << 56);
    }

    // 附加消息长度
    std::memcpy(bufferCopy.data() + BLOCK_SIZE - LENGTH_SIZE, &bitLength,
                LENGTH_SIZE);

    processBlock(bufferCopy.data());

    // 生成最终哈希值
    std::array<uint8_t, DIGEST_SIZE> result;

    for (size_t i = 0; i < HASH_SIZE; ++i) {
        uint32_t value = hashCopy[i];
        if constexpr (std::endian::native == std::endian::little) {
            // 在小端系统上需要转换字节序
            value = ((value & 0xff000000) >> 24) | ((value & 0x00ff0000) >> 8) |
                    ((value & 0x0000ff00) << 8) | ((value & 0x000000ff) << 24);
        }
        std::memcpy(&result[i * 4], &value, 4);
    }

    return result;
}

auto SHA1::digestAsString() noexcept -> std::string {
    return bytesToHex(digest());
}

void SHA1::reset() noexcept {
    bitCount_ = 0;
    hash_[0] = 0x67452301;
    hash_[1] = 0xEFCDAB89;
    hash_[2] = 0x98BADCFE;
    hash_[3] = 0x10325476;
    hash_[4] = 0xC3D2E1F0;
    buffer_.fill(0);
}

void SHA1::processBlock(const uint8_t* block) noexcept {
    std::array<uint32_t, SCHEDULE_SIZE> schedule{};

    // 使用C++20的位操作来处理字节序
    for (size_t i = 0; i < 16; ++i) {
        if constexpr (std::endian::native == std::endian::little) {
            // 小端系统需要字节序转换
            const uint8_t* ptr = block + i * 4;
            schedule[i] = static_cast<uint32_t>(ptr[0]) << 24 |
                          static_cast<uint32_t>(ptr[1]) << 16 |
                          static_cast<uint32_t>(ptr[2]) << 8 |
                          static_cast<uint32_t>(ptr[3]);
        } else {
            // 大端系统可以直接复制
            std::memcpy(&schedule[i], block + i * 4, 4);
        }
    }

    // 展开消息调度表计算
    for (size_t i = 16; i < SCHEDULE_SIZE; ++i) {
        schedule[i] = rotateLeft(schedule[i - 3] ^ schedule[i - 8] ^
                                     schedule[i - 14] ^ schedule[i - 16],
                                 1);
    }

    uint32_t a = hash_[0];
    uint32_t b = hash_[1];
    uint32_t c = hash_[2];
    uint32_t d = hash_[3];
    uint32_t e = hash_[4];

    // 主循环优化 - 展开前20个循环
    for (size_t i = 0; i < 20; ++i) {
        uint32_t f = (b & c) | (~b & d);
        uint32_t k = 0x5A827999;
        uint32_t temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    // 接下来20个循环
    for (size_t i = 20; i < 40; ++i) {
        uint32_t f = b ^ c ^ d;
        uint32_t k = 0x6ED9EBA1;
        uint32_t temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    // 接下来20个循环
    for (size_t i = 40; i < 60; ++i) {
        uint32_t f = (b & c) | (b & d) | (c & d);
        uint32_t k = 0x8F1BBCDC;
        uint32_t temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    // 最后20个循环
    for (size_t i = 60; i < 80; ++i) {
        uint32_t f = b ^ c ^ d;
        uint32_t k = 0xCA62C1D6;
        uint32_t temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    hash_[0] += a;
    hash_[1] += b;
    hash_[2] += c;
    hash_[3] += d;
    hash_[4] += e;
}

#ifdef __AVX2__
void SHA1::processBlockSIMD(const uint8_t* block) noexcept {
    // AVX2优化版本的块处理
    std::array<uint32_t, SCHEDULE_SIZE> schedule{};

    // 使用SIMD加载数据
    for (size_t i = 0; i < 16; i += 4) {
        const uint8_t* ptr = block + i * 4;
        __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));

        // 处理字节序
        if constexpr (std::endian::native == std::endian::little) {
            const __m128i mask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4,
                                              5, 6, 7, 0, 1, 2, 3);
            data = _mm_shuffle_epi8(data, mask);
        }

        _mm_storeu_si128(reinterpret_cast<__m128i*>(&schedule[i]), data);
    }

    // 使用AVX2指令并行计算消息调度表
    for (size_t i = 16; i < SCHEDULE_SIZE; i += 8) {
        __m256i w1 = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(&schedule[i - 3]));
        __m256i w2 = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(&schedule[i - 8]));
        __m256i w3 = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(&schedule[i - 14]));
        __m256i w4 = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(&schedule[i - 16]));

        __m256i result = _mm256_xor_si256(w1, w2);
        result = _mm256_xor_si256(result, w3);
        result = _mm256_xor_si256(result, w4);

        // 循环左移1位
        const __m256i mask = _mm256_set1_epi32(0x01);
        __m256i shift_left = _mm256_slli_epi32(result, 1);
        __m256i shift_right = _mm256_srli_epi32(result, 31);
        result = _mm256_or_si256(shift_left, shift_right);

        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&schedule[i]), result);
    }

    // 从这里开始执行标准主循环
    uint32_t a = hash_[0];
    uint32_t b = hash_[1];
    uint32_t c = hash_[2];
    uint32_t d = hash_[3];
    uint32_t e = hash_[4];

    // 主循环与普通版本相同
    // ...与普通processBlock中的主循环相同...
    for (size_t i = 0; i < 20; ++i) {
        uint32_t f = (b & c) | (~b & d);
        uint32_t k = 0x5A827999;
        uint32_t temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    for (size_t i = 20; i < 40; ++i) {
        uint32_t f = b ^ c ^ d;
        uint32_t k = 0x6ED9EBA1;
        uint32_t temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    for (size_t i = 40; i < 60; ++i) {
        uint32_t f = (b & c) | (b & d) | (c & d);
        uint32_t k = 0x8F1BBCDC;
        uint32_t temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    for (size_t i = 60; i < 80; ++i) {
        uint32_t f = b ^ c ^ d;
        uint32_t k = 0xCA62C1D6;
        uint32_t temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    hash_[0] += a;
    hash_[1] += b;
    hash_[2] += c;
    hash_[3] += d;
    hash_[4] += e;
}
#endif

template <size_t N>
auto bytesToHex(const std::array<uint8_t, N>& bytes) noexcept -> std::string {
    static constexpr char HEX_CHARS[] = "0123456789abcdef";
    std::string result(N * 2, ' ');

    for (size_t i = 0; i < N; ++i) {
        result[i * 2] = HEX_CHARS[(bytes[i] >> 4) & 0xF];
        result[i * 2 + 1] = HEX_CHARS[bytes[i] & 0xF];
    }

    return result;
}

template <>
auto bytesToHex<SHA1::DIGEST_SIZE>(
    const std::array<uint8_t, SHA1::DIGEST_SIZE>& bytes) noexcept
    -> std::string {
    static constexpr char HEX_CHARS[] = "0123456789abcdef";
    std::string result(SHA1::DIGEST_SIZE * 2, ' ');

    for (size_t i = 0; i < SHA1::DIGEST_SIZE; ++i) {
        result[i * 2] = HEX_CHARS[(bytes[i] >> 4) & 0xF];
        result[i * 2 + 1] = HEX_CHARS[bytes[i] & 0xF];
    }

    return result;
}

template <ByteContainer... Containers>
auto computeHashesInParallel(const Containers&... containers)
    -> std::vector<std::array<uint8_t, SHA1::DIGEST_SIZE>> {
    std::vector<std::array<uint8_t, SHA1::DIGEST_SIZE>> results;
    results.reserve(sizeof...(Containers));

    auto hashComputation =
        [](const auto& container) -> std::array<uint8_t, SHA1::DIGEST_SIZE> {
        SHA1 hasher;
        hasher.update(container);
        return hasher.digest();
    };

    std::vector<std::future<std::array<uint8_t, SHA1::DIGEST_SIZE>>> futures;
    futures.reserve(sizeof...(Containers));

    // 启动所有计算任务
    (futures.push_back(
         std::async(std::launch::async, hashComputation, containers)),
     ...);

    // 收集结果
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    return results;
}

}  // namespace atom::algorithm