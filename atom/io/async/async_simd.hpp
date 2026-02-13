#ifndef ATOM_IO_ASYNC_ASYNC_SIMD_HPP
#define ATOM_IO_ASYNC_ASYNC_SIMD_HPP

#include <cstddef>
#include <span>
#include <string>

// SIMD support detection
#ifdef __SSE4_2__
#include <immintrin.h>
#define ATOM_HAS_SSE42 1
#endif

#ifdef __AVX2__
#include <immintrin.h>
#define ATOM_HAS_AVX2 1
#endif

namespace atom::io::async {

/**
 * @brief SIMD-optimized buffer comparison
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
                       std::span<const char> buffer2) noexcept;

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
size_t simdFindByte(std::span<const char> buffer, char target) noexcept;

/**
 * @brief SIMD-optimized memory initialization
 *
 * Efficiently sets all bytes in a buffer to a specific value using vectorized
 * instructions. Processes 32 bytes at a time with AVX2 or 16 bytes with SSE4.2.
 *
 * @param buffer Buffer to initialize
 * @param value Byte value to set
 */
void simdMemorySet(std::span<char> buffer, char value) noexcept;

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_SIMD_HPP
