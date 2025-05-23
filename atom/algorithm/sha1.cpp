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

    // Check if CPU supports SIMD instructions
#ifdef __AVX2__
    useSIMD_ = true;
    spdlog::debug("SHA1: Using AVX2 SIMD acceleration");
#else
    spdlog::debug("SHA1: Using standard implementation (no SIMD)");
#endif
}

void SHA1::update(std::span<const u8> data) noexcept {
    update(data.data(), data.size());
}

void SHA1::update(const u8* data, usize length) {
    // Input validation
    if (!data && length > 0) {
        spdlog::error("SHA1: Null data pointer with non-zero length");
        throw std::invalid_argument("Null data pointer with non-zero length");
    }

    usize remaining = length;
    usize offset = 0;

    while (remaining > 0) {
        usize bufferOffset = (bitCount_ / 8) % BLOCK_SIZE;

        usize bytesToFill = BLOCK_SIZE - bufferOffset;
        usize bytesToCopy = std::min(remaining, bytesToFill);

        // Use std::memcpy for better performance
        std::memcpy(buffer_.data() + bufferOffset, data + offset, bytesToCopy);

        offset += bytesToCopy;
        remaining -= bytesToCopy;
        bitCount_ += bytesToCopy * BITS_PER_BYTE;

        if (bufferOffset + bytesToCopy == BLOCK_SIZE) {
            // Choose between SIMD or standard processing method
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

auto SHA1::digest() noexcept -> std::array<u8, SHA1::DIGEST_SIZE> {
    u64 bitLength = bitCount_;

    // Backup current state to ensure digest() operation doesn't affect object
    // state
    auto hashCopy = hash_;
    auto bufferCopy = buffer_;
    auto bitCountCopy = bitCount_;

    // Padding
    usize bufferOffset = (bitCountCopy / 8) % BLOCK_SIZE;
    bufferCopy[bufferOffset] = PADDING_BYTE;  // Append the bit '1'

    // Fill the rest of the buffer with zeros
    std::fill(bufferCopy.begin() + bufferOffset + 1,
              bufferCopy.begin() + BLOCK_SIZE, 0);

    if (bufferOffset >= BLOCK_SIZE - LENGTH_SIZE) {
        // Process current block, create new block for storing length
        processBlock(bufferCopy.data());
        std::fill(bufferCopy.begin(), bufferCopy.end(), 0);
    }

    // Use C++20 bit operations to handle byte order
    if constexpr (std::endian::native == std::endian::little) {
        // Convert on little endian systems
        bitLength = ((bitLength & 0xff00000000000000ULL) >> 56) |
                    ((bitLength & 0x00ff000000000000ULL) >> 40) |
                    ((bitLength & 0x0000ff0000000000ULL) >> 24) |
                    ((bitLength & 0x000000ff00000000ULL) >> 8) |
                    ((bitLength & 0x00000000ff000000ULL) << 8) |
                    ((bitLength & 0x0000000000ff0000ULL) << 24) |
                    ((bitLength & 0x000000000000ff00ULL) << 40) |
                    ((bitLength & 0x00000000000000ffULL) << 56);
    }

    // Append message length
    std::memcpy(bufferCopy.data() + BLOCK_SIZE - LENGTH_SIZE, &bitLength,
                LENGTH_SIZE);

    processBlock(bufferCopy.data());

    // Generate final hash value
    std::array<u8, DIGEST_SIZE> result;

    for (usize i = 0; i < HASH_SIZE; ++i) {
        u32 value = hashCopy[i];
        if constexpr (std::endian::native == std::endian::little) {
            // Byte order conversion needed on little endian systems
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

void SHA1::processBlock(const u8* block) noexcept {
    std::array<u32, SCHEDULE_SIZE> schedule{};

    // Use C++20 bit operations to handle byte order
    for (usize i = 0; i < 16; ++i) {
        if constexpr (std::endian::native == std::endian::little) {
            // Byte order conversion needed on little endian systems
            const u8* ptr = block + i * 4;
            schedule[i] = static_cast<u32>(ptr[0]) << 24 |
                          static_cast<u32>(ptr[1]) << 16 |
                          static_cast<u32>(ptr[2]) << 8 |
                          static_cast<u32>(ptr[3]);
        } else {
            // Direct copy on big endian systems
            std::memcpy(&schedule[i], block + i * 4, 4);
        }
    }

    // Calculate message schedule
    for (usize i = 16; i < SCHEDULE_SIZE; ++i) {
        schedule[i] = rotateLeft(schedule[i - 3] ^ schedule[i - 8] ^
                                     schedule[i - 14] ^ schedule[i - 16],
                                 1);
    }

    u32 a = hash_[0];
    u32 b = hash_[1];
    u32 c = hash_[2];
    u32 d = hash_[3];
    u32 e = hash_[4];

    // Optimized main loop - unroll first 20 iterations
    for (usize i = 0; i < 20; ++i) {
        u32 f = (b & c) | (~b & d);
        u32 k = 0x5A827999;
        u32 temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    // Next 20 iterations
    for (usize i = 20; i < 40; ++i) {
        u32 f = b ^ c ^ d;
        u32 k = 0x6ED9EBA1;
        u32 temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    // Next 20 iterations
    for (usize i = 40; i < 60; ++i) {
        u32 f = (b & c) | (b & d) | (c & d);
        u32 k = 0x8F1BBCDC;
        u32 temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    // Last 20 iterations
    for (usize i = 60; i < 80; ++i) {
        u32 f = b ^ c ^ d;
        u32 k = 0xCA62C1D6;
        u32 temp = rotateLeft(a, 5) + f + e + k + schedule[i];
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
void SHA1::processBlockSIMD(const u8* block) noexcept {
    // AVX2 optimized block processing
    std::array<u32, SCHEDULE_SIZE> schedule{};

    // Use SIMD to load data
    for (usize i = 0; i < 16; i += 4) {
        const u8* ptr = block + i * 4;
        __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));

        // Handle byte order
        if constexpr (std::endian::native == std::endian::little) {
            const __m128i mask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4,
                                              5, 6, 7, 0, 1, 2, 3);
            data = _mm_shuffle_epi8(data, mask);
        }

        _mm_storeu_si128(reinterpret_cast<__m128i*>(&schedule[i]), data);
    }

    // Use AVX2 instructions for parallel message schedule calculation
    for (usize i = 16; i < SCHEDULE_SIZE; i += 8) {
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

        // Rotate left by 1 bit
        const __m256i mask = _mm256_set1_epi32(0x01);
        __m256i shift_left = _mm256_slli_epi32(result, 1);
        __m256i shift_right = _mm256_srli_epi32(result, 31);
        result = _mm256_or_si256(shift_left, shift_right);

        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&schedule[i]), result);
    }

    // Start standard main loop from here
    u32 a = hash_[0];
    u32 b = hash_[1];
    u32 c = hash_[2];
    u32 d = hash_[3];
    u32 e = hash_[4];

    // Main loop same as in standard processBlock
    for (usize i = 0; i < 20; ++i) {
        u32 f = (b & c) | (~b & d);
        u32 k = 0x5A827999;
        u32 temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    for (usize i = 20; i < 40; ++i) {
        u32 f = b ^ c ^ d;
        u32 k = 0x6ED9EBA1;
        u32 temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    for (usize i = 40; i < 60; ++i) {
        u32 f = (b & c) | (b & d) | (c & d);
        u32 k = 0x8F1BBCDC;
        u32 temp = rotateLeft(a, 5) + f + e + k + schedule[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }

    for (usize i = 60; i < 80; ++i) {
        u32 f = b ^ c ^ d;
        u32 k = 0xCA62C1D6;
        u32 temp = rotateLeft(a, 5) + f + e + k + schedule[i];
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

template <usize N>
auto bytesToHex(const std::array<u8, N>& bytes) noexcept -> std::string {
    static constexpr char HEX_CHARS[] = "0123456789abcdef";
    std::string result(N * 2, ' ');

    for (usize i = 0; i < N; ++i) {
        result[i * 2] = HEX_CHARS[(bytes[i] >> 4) & 0xF];
        result[i * 2 + 1] = HEX_CHARS[bytes[i] & 0xF];
    }

    return result;
}

template <>
auto bytesToHex<SHA1::DIGEST_SIZE>(
    const std::array<u8, SHA1::DIGEST_SIZE>& bytes) noexcept -> std::string {
    static constexpr char HEX_CHARS[] = "0123456789abcdef";
    std::string result(SHA1::DIGEST_SIZE * 2, ' ');

    for (usize i = 0; i < SHA1::DIGEST_SIZE; ++i) {
        result[i * 2] = HEX_CHARS[(bytes[i] >> 4) & 0xF];
        result[i * 2 + 1] = HEX_CHARS[bytes[i] & 0xF];
    }

    return result;
}

template <ByteContainer... Containers>
auto computeHashesInParallel(const Containers&... containers)
    -> std::vector<std::array<u8, SHA1::DIGEST_SIZE>> {
    std::vector<std::array<u8, SHA1::DIGEST_SIZE>> results;
    results.reserve(sizeof...(Containers));

    auto hashComputation =
        [](const auto& container) -> std::array<u8, SHA1::DIGEST_SIZE> {
        SHA1 hasher;
        hasher.update(container);
        return hasher.digest();
    };

    std::vector<std::future<std::array<u8, SHA1::DIGEST_SIZE>>> futures;
    futures.reserve(sizeof...(Containers));

    spdlog::debug("Starting parallel hash computation for {} containers",
                  sizeof...(Containers));

    // Launch all computation tasks
    (futures.push_back(
         std::async(std::launch::async, hashComputation, containers)),
     ...);

    // Collect results
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    spdlog::debug("Completed parallel hash computation");
    return results;
}

}  // namespace atom::algorithm