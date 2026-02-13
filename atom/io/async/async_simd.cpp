#include "async_simd.hpp"

#include <algorithm>

namespace atom::io::async {

/**
 * @brief High-performance SIMD-optimized buffer comparison
 *
 * Uses vectorized instructions (AVX2/SSE4.2) when available for optimal
 * performance. Falls back to standard library implementation on unsupported
 * platforms.
 *
 * @param buffer1 First buffer to compare
 * @param buffer2 Second buffer to compare
 * @return true if buffers are identical, false otherwise
 */
bool simdBufferCompare(std::span<const char> buffer1,
                       std::span<const char> buffer2) noexcept {
    if (buffer1.size() != buffer2.size()) {
        return false;
    }

    if (buffer1.empty()) {
        return true;
    }

    const char* data1 [[maybe_unused]] = buffer1.data();
    const char* data2 [[maybe_unused]] = buffer2.data();
    size_t size [[maybe_unused]] = buffer1.size();

#ifdef ATOM_HAS_AVX2
    // Process 32 bytes at a time with AVX2
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk1 =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data1 + i));
        __m256i chunk2 =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data2 + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk1, chunk2);
        int mask = _mm256_movemask_epi8(cmp);
        if (mask != 0xFFFFFFFF) {
            return false;
        }
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        if (data1[i] != data2[i]) {
            return false;
        }
    }
    return true;
#elif defined(ATOM_HAS_SSE42)
    // Process 16 bytes at a time with SSE4.2
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk1 =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(data1 + i));
        __m128i chunk2 =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(data2 + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk1, chunk2);
        int mask = _mm_movemask_epi8(cmp);
        if (mask != 0xFFFF) {
            return false;
        }
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        if (data1[i] != data2[i]) {
            return false;
        }
    }
    return true;
#else
    // Fallback to standard library
    return std::equal(buffer1.begin(), buffer1.end(), buffer2.begin());
#endif
}

/**
 * @brief SIMD-optimized byte search in buffer
 *
 * Efficiently searches for a specific byte using vectorized instructions.
 * Processes 32 bytes at a time with AVX2 or 16 bytes with SSE4.2.
 *
 * @param buffer Buffer to search in
 * @param target Byte value to find
 * @return Position of first occurrence or std::string::npos if not found
 */
size_t simdFindByte(std::span<const char> buffer, char target) noexcept {
    if (buffer.empty()) {
        return std::string::npos;
    }

    const char* data [[maybe_unused]] = buffer.data();
    size_t size [[maybe_unused]] = buffer.size();

#ifdef ATOM_HAS_AVX2
    const __m256i target_vec = _mm256_set1_epi8(target);
    size_t i = 0;

    // Process 32 bytes at a time
    for (; i + 32 <= size; i += 32) {
        __m256i chunk =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, target_vec);
        int mask = _mm256_movemask_epi8(cmp);

        if (mask != 0) {
            return i + __builtin_ctz(mask);
        }
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        if (data[i] == target) {
            return i;
        }
    }
    return std::string::npos;
#elif defined(ATOM_HAS_SSE42)
    const __m128i target_vec = _mm_set1_epi8(target);
    size_t i = 0;

    // Process 16 bytes at a time
    for (; i + 16 <= size; i += 16) {
        __m128i chunk =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk, target_vec);
        int mask = _mm_movemask_epi8(cmp);

        if (mask != 0) {
            return i + __builtin_ctz(mask);
        }
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        if (data[i] == target) {
            return i;
        }
    }
    return std::string::npos;
#else
    // Fallback to standard library
    auto it = std::find(buffer.begin(), buffer.end(), target);
    return it != buffer.end() ? std::distance(buffer.begin(), it)
                              : std::string::npos;
#endif
}

/**
 * @brief SIMD-optimized memory initialization
 *
 * Efficiently sets all bytes in a buffer to a specific value using vectorized
 * instructions. Processes 32 bytes at a time with AVX2 or 16 bytes with SSE4.2.
 *
 * @param buffer Buffer to initialize
 * @param value Byte value to set
 */
void simdMemorySet(std::span<char> buffer, char value) noexcept {
    if (buffer.empty()) {
        return;
    }

    char* data [[maybe_unused]] = buffer.data();
    size_t size [[maybe_unused]] = buffer.size();

#ifdef ATOM_HAS_AVX2
    const __m256i value_vec = _mm256_set1_epi8(value);
    size_t i = 0;

    // Process 32 bytes at a time
    for (; i + 32 <= size; i += 32) {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), value_vec);
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        data[i] = value;
    }
#elif defined(ATOM_HAS_SSE42)
    const __m128i value_vec = _mm_set1_epi8(value);
    size_t i = 0;

    // Process 16 bytes at a time
    for (; i + 16 <= size; i += 16) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), value_vec);
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        data[i] = value;
    }
#else
    // Fallback to standard library
    std::fill(buffer.begin(), buffer.end(), value);
#endif
}

}  // namespace atom::io::async
