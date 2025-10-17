#ifndef ATOM_ALGORITHM_CORE_SIMD_UTILS_HPP
#define ATOM_ALGORITHM_CORE_SIMD_UTILS_HPP

#include <cstddef>
#include <cstring>
#include <type_traits>

#include "rust_numeric.hpp"

// SIMD capability detection
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
#define ATOM_SIMD_X86 1
#if defined(__AVX512F__)
#define ATOM_SIMD_AVX512 1
#include <immintrin.h>
#elif defined(__AVX2__)
#define ATOM_SIMD_AVX2 1
#include <immintrin.h>
#elif defined(__AVX__)
#define ATOM_SIMD_AVX 1
#include <immintrin.h>
#elif defined(__SSE4_2__)
#define ATOM_SIMD_SSE42 1
#include <nmmintrin.h>
#elif defined(__SSE4_1__)
#define ATOM_SIMD_SSE41 1
#include <smmintrin.h>
#elif defined(__SSE2__)
#define ATOM_SIMD_SSE2 1
#include <emmintrin.h>
#endif
#elif defined(__ARM_NEON) || defined(__aarch64__)
#define ATOM_SIMD_ARM 1
#define ATOM_SIMD_NEON 1
#include <arm_neon.h>
#endif

namespace atom::algorithm::simd {

/**
 * @brief SIMD vector width constants for different instruction sets
 */
struct VectorWidth {
    static constexpr usize AVX512_F32 = 16;  // 512 bits / 32 bits = 16 floats
    static constexpr usize AVX512_F64 = 8;   // 512 bits / 64 bits = 8 doubles
    static constexpr usize AVX2_F32 = 8;     // 256 bits / 32 bits = 8 floats
    static constexpr usize AVX2_F64 = 4;     // 256 bits / 64 bits = 4 doubles
    static constexpr usize SSE_F32 = 4;      // 128 bits / 32 bits = 4 floats
    static constexpr usize SSE_F64 = 2;      // 128 bits / 64 bits = 2 doubles
    static constexpr usize NEON_F32 = 4;     // 128 bits / 32 bits = 4 floats
    static constexpr usize NEON_F64 = 2;     // 128 bits / 64 bits = 2 doubles
};

/**
 * @brief Get optimal vector width for the current platform and data type
 */
template <typename T>
constexpr usize getOptimalVectorWidth() {
    if constexpr (std::is_same_v<T, f32>) {
#ifdef ATOM_SIMD_AVX512
        return VectorWidth::AVX512_F32;
#elif defined(ATOM_SIMD_AVX2)
        return VectorWidth::AVX2_F32;
#elif defined(ATOM_SIMD_SSE2)
        return VectorWidth::SSE_F32;
#elif defined(ATOM_SIMD_NEON)
        return VectorWidth::NEON_F32;
#else
        return 1;
#endif
    } else if constexpr (std::is_same_v<T, f64>) {
#ifdef ATOM_SIMD_AVX512
        return VectorWidth::AVX512_F64;
#elif defined(ATOM_SIMD_AVX2)
        return VectorWidth::AVX2_F64;
#elif defined(ATOM_SIMD_SSE2)
        return VectorWidth::SSE_F64;
#elif defined(ATOM_SIMD_NEON)
        return VectorWidth::NEON_F64;
#else
        return 1;
#endif
    } else {
        return 1;
    }
}

/**
 * @brief SIMD-optimized memory operations
 */
class MemoryOps {
public:
    /**
     * @brief SIMD-optimized memory copy
     * @param dest Destination pointer
     * @param src Source pointer
     * @param size Number of bytes to copy
     */
    static void copy(void* dest, const void* src, usize size) noexcept {
#ifdef ATOM_SIMD_AVX2
        if (size >= 32 && reinterpret_cast<uintptr_t>(dest) % 32 == 0 &&
            reinterpret_cast<uintptr_t>(src) % 32 == 0) {
            copyAVX2(dest, src, size);
            return;
        }
#endif
#ifdef ATOM_SIMD_SSE2
        if (size >= 16 && reinterpret_cast<uintptr_t>(dest) % 16 == 0 &&
            reinterpret_cast<uintptr_t>(src) % 16 == 0) {
            copySSE2(dest, src, size);
            return;
        }
#endif
        std::memcpy(dest, src, size);
    }

    /**
     * @brief SIMD-optimized memory set
     * @param dest Destination pointer
     * @param value Value to set
     * @param size Number of bytes to set
     */
    static void set(void* dest, u8 value, usize size) noexcept {
#ifdef ATOM_SIMD_AVX2
        if (size >= 32 && reinterpret_cast<uintptr_t>(dest) % 32 == 0) {
            setAVX2(dest, value, size);
            return;
        }
#endif
#ifdef ATOM_SIMD_SSE2
        if (size >= 16 && reinterpret_cast<uintptr_t>(dest) % 16 == 0) {
            setSSE2(dest, value, size);
            return;
        }
#endif
        std::memset(dest, value, size);
    }

private:
#ifdef ATOM_SIMD_AVX2
    static void copyAVX2(void* dest, const void* src, usize size) noexcept {
        auto* d = static_cast<u8*>(dest);
        const auto* s = static_cast<const u8*>(src);

        usize simd_size = size - (size % 32);
        for (usize i = 0; i < simd_size; i += 32) {
            __m256i data =
                _mm256_load_si256(reinterpret_cast<const __m256i*>(s + i));
            _mm256_store_si256(reinterpret_cast<__m256i*>(d + i), data);
        }

        // Handle remaining bytes
        if (size % 32 != 0) {
            std::memcpy(d + simd_size, s + simd_size, size % 32);
        }
    }

    static void setAVX2(void* dest, u8 value, usize size) noexcept {
        auto* d = static_cast<u8*>(dest);
        __m256i val = _mm256_set1_epi8(static_cast<char>(value));

        usize simd_size = size - (size % 32);
        for (usize i = 0; i < simd_size; i += 32) {
            _mm256_store_si256(reinterpret_cast<__m256i*>(d + i), val);
        }

        // Handle remaining bytes
        if (size % 32 != 0) {
            std::memset(d + simd_size, value, size % 32);
        }
    }
#endif

#ifdef ATOM_SIMD_SSE2
    static void copySSE2(void* dest, const void* src, usize size) noexcept {
        auto* d = static_cast<u8*>(dest);
        const auto* s = static_cast<const u8*>(src);

        usize simd_size = size - (size % 16);
        for (usize i = 0; i < simd_size; i += 16) {
            __m128i data =
                _mm_load_si128(reinterpret_cast<const __m128i*>(s + i));
            _mm_store_si128(reinterpret_cast<__m128i*>(d + i), data);
        }

        // Handle remaining bytes
        if (size % 16 != 0) {
            std::memcpy(d + simd_size, s + simd_size, size % 16);
        }
    }

    static void setSSE2(void* dest, u8 value, usize size) noexcept {
        auto* d = static_cast<u8*>(dest);
        __m128i val = _mm_set1_epi8(static_cast<char>(value));

        usize simd_size = size - (size % 16);
        for (usize i = 0; i < simd_size; i += 16) {
            _mm_store_si128(reinterpret_cast<__m128i*>(d + i), val);
        }

        // Handle remaining bytes
        if (size % 16 != 0) {
            std::memset(d + simd_size, value, size % 16);
        }
    }
#endif
};

/**
 * @brief SIMD-optimized mathematical operations
 */
class MathOps {
public:
    /**
     * @brief SIMD-optimized vector addition
     * @param a First vector
     * @param b Second vector
     * @param result Result vector
     * @param size Number of elements
     */
    template <typename T>
    static void vectorAdd(const T* a, const T* b, T* result,
                          usize size) noexcept {
        static_assert(std::is_floating_point_v<T>,
                      "Only floating point types supported");

        if constexpr (std::is_same_v<T, f32>) {
#ifdef ATOM_SIMD_AVX2
            vectorAddAVX2(a, b, result, size);
#elif defined(ATOM_SIMD_SSE2)
            vectorAddSSE2(a, b, result, size);
#elif defined(ATOM_SIMD_NEON)
            vectorAddNEON(a, b, result, size);
#else
            vectorAddScalar(a, b, result, size);
#endif
        } else if constexpr (std::is_same_v<T, f64>) {
#ifdef ATOM_SIMD_AVX2
            vectorAddAVX2_f64(a, b, result, size);
#elif defined(ATOM_SIMD_SSE2)
            vectorAddSSE2_f64(a, b, result, size);
#else
            vectorAddScalar(a, b, result, size);
#endif
        }
    }

    /**
     * @brief SIMD-optimized dot product
     * @param a First vector
     * @param b Second vector
     * @param size Number of elements
     * @return Dot product result
     */
    template <typename T>
    static T dotProduct(const T* a, const T* b, usize size) noexcept {
        static_assert(std::is_floating_point_v<T>,
                      "Only floating point types supported");

        if constexpr (std::is_same_v<T, f32>) {
#ifdef ATOM_SIMD_AVX2
            return dotProductAVX2(a, b, size);
#elif defined(ATOM_SIMD_SSE2)
            return dotProductSSE2(a, b, size);
#elif defined(ATOM_SIMD_NEON)
            return dotProductNEON(a, b, size);
#else
            return dotProductScalar(a, b, size);
#endif
        } else if constexpr (std::is_same_v<T, f64>) {
#ifdef ATOM_SIMD_AVX2
            return dotProductAVX2_f64(a, b, size);
#elif defined(ATOM_SIMD_SSE2)
            return dotProductSSE2_f64(a, b, size);
#else
            return dotProductScalar(a, b, size);
#endif
        }
    }

private:
    template <typename T>
    static void vectorAddScalar(const T* a, const T* b, T* result,
                                usize size) noexcept {
        for (usize i = 0; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    template <typename T>
    static T dotProductScalar(const T* a, const T* b, usize size) noexcept {
        T sum = T{0};
        for (usize i = 0; i < size; ++i) {
            sum += a[i] * b[i];
        }
        return sum;
    }

#ifdef ATOM_SIMD_AVX2
    static void vectorAddAVX2(const f32* a, const f32* b, f32* result,
                              usize size) noexcept {
        usize simd_size = size - (size % 8);
        for (usize i = 0; i < simd_size; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 vr = _mm256_add_ps(va, vb);
            _mm256_storeu_ps(result + i, vr);
        }

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static f32 dotProductAVX2(const f32* a, const f32* b, usize size) noexcept {
        __m256 sum = _mm256_setzero_ps();
        usize simd_size = size - (size % 8);

        for (usize i = 0; i < simd_size; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 mul = _mm256_mul_ps(va, vb);
            sum = _mm256_add_ps(sum, mul);
        }

        // Horizontal sum
        __m128 hi = _mm256_extractf128_ps(sum, 1);
        __m128 lo = _mm256_castps256_ps128(sum);
        __m128 sum128 = _mm_add_ps(hi, lo);
        sum128 = _mm_hadd_ps(sum128, sum128);
        sum128 = _mm_hadd_ps(sum128, sum128);
        f32 result = _mm_cvtss_f32(sum128);

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }

    static void vectorAddAVX2_f64(const f64* a, const f64* b, f64* result,
                                  usize size) noexcept {
        usize simd_size = size - (size % 4);
        for (usize i = 0; i < simd_size; i += 4) {
            __m256d va = _mm256_loadu_pd(a + i);
            __m256d vb = _mm256_loadu_pd(b + i);
            __m256d vr = _mm256_add_pd(va, vb);
            _mm256_storeu_pd(result + i, vr);
        }

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static f64 dotProductAVX2_f64(const f64* a, const f64* b,
                                  usize size) noexcept {
        __m256d sum = _mm256_setzero_pd();
        usize simd_size = size - (size % 4);

        for (usize i = 0; i < simd_size; i += 4) {
            __m256d va = _mm256_loadu_pd(a + i);
            __m256d vb = _mm256_loadu_pd(b + i);
            __m256d mul = _mm256_mul_pd(va, vb);
            sum = _mm256_add_pd(sum, mul);
        }

        // Horizontal sum
        __m128d hi = _mm256_extractf128_pd(sum, 1);
        __m128d lo = _mm256_castpd256_pd128(sum);
        __m128d sum128 = _mm_add_pd(hi, lo);
        sum128 = _mm_hadd_pd(sum128, sum128);
        f64 result = _mm_cvtsd_f64(sum128);

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif

#ifdef ATOM_SIMD_SSE2
    static void vectorAddSSE2(const f32* a, const f32* b, f32* result,
                              usize size) noexcept {
        usize simd_size = size - (size % 4);
        for (usize i = 0; i < simd_size; i += 4) {
            __m128 va = _mm_loadu_ps(a + i);
            __m128 vb = _mm_loadu_ps(b + i);
            __m128 vr = _mm_add_ps(va, vb);
            _mm_storeu_ps(result + i, vr);
        }

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static f32 dotProductSSE2(const f32* a, const f32* b, usize size) noexcept {
        __m128 sum = _mm_setzero_ps();
        usize simd_size = size - (size % 4);

        for (usize i = 0; i < simd_size; i += 4) {
            __m128 va = _mm_loadu_ps(a + i);
            __m128 vb = _mm_loadu_ps(b + i);
            __m128 mul = _mm_mul_ps(va, vb);
            sum = _mm_add_ps(sum, mul);
        }

        // Horizontal sum (manual implementation for SSE2 compatibility)
        __m128 shuf = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(2, 3, 0, 1));
        sum = _mm_add_ps(sum, shuf);
        shuf = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(1, 0, 3, 2));
        sum = _mm_add_ps(sum, shuf);
        f32 result = _mm_cvtss_f32(sum);

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }

    static void vectorAddSSE2_f64(const f64* a, const f64* b, f64* result,
                                  usize size) noexcept {
        usize simd_size = size - (size % 2);
        for (usize i = 0; i < simd_size; i += 2) {
            __m128d va = _mm_loadu_pd(a + i);
            __m128d vb = _mm_loadu_pd(b + i);
            __m128d vr = _mm_add_pd(va, vb);
            _mm_storeu_pd(result + i, vr);
        }

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static f64 dotProductSSE2_f64(const f64* a, const f64* b,
                                  usize size) noexcept {
        __m128d sum = _mm_setzero_pd();
        usize simd_size = size - (size % 2);

        for (usize i = 0; i < simd_size; i += 2) {
            __m128d va = _mm_loadu_pd(a + i);
            __m128d vb = _mm_loadu_pd(b + i);
            __m128d mul = _mm_mul_pd(va, vb);
            sum = _mm_add_pd(sum, mul);
        }

        // Horizontal sum (manual implementation for SSE2 compatibility)
        __m128d shuf = _mm_shuffle_pd(sum, sum, 1);
        sum = _mm_add_pd(sum, shuf);
        f64 result = _mm_cvtsd_f64(sum);

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif

#ifdef ATOM_SIMD_NEON
    static void vectorAddNEON(const f32* a, const f32* b, f32* result,
                              usize size) noexcept {
        usize simd_size = size - (size % 4);
        for (usize i = 0; i < simd_size; i += 4) {
            float32x4_t va = vld1q_f32(a + i);
            float32x4_t vb = vld1q_f32(b + i);
            float32x4_t vr = vaddq_f32(va, vb);
            vst1q_f32(result + i, vr);
        }

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static f32 dotProductNEON(const f32* a, const f32* b, usize size) noexcept {
        float32x4_t sum = vdupq_n_f32(0.0f);
        usize simd_size = size - (size % 4);

        for (usize i = 0; i < simd_size; i += 4) {
            float32x4_t va = vld1q_f32(a + i);
            float32x4_t vb = vld1q_f32(b + i);
            sum = vmlaq_f32(sum, va, vb);
        }

        // Horizontal sum
        float32x2_t sum_pair = vadd_f32(vget_high_f32(sum), vget_low_f32(sum));
        f32 result = vget_lane_f32(vpadd_f32(sum_pair, sum_pair), 0);

        // Handle remaining elements
        for (usize i = simd_size; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif
};

/**
 * @brief Check if SIMD is available at runtime
 */
class SIMDCapabilities {
public:
    static bool hasSSE2() noexcept {
#ifdef ATOM_SIMD_SSE2
        return true;
#else
        return false;
#endif
    }

    static bool hasAVX2() noexcept {
#ifdef ATOM_SIMD_AVX2
        return true;
#else
        return false;
#endif
    }

    static bool hasAVX512() noexcept {
#ifdef ATOM_SIMD_AVX512
        return true;
#else
        return false;
#endif
    }

    static bool hasNEON() noexcept {
#ifdef ATOM_SIMD_NEON
        return true;
#else
        return false;
#endif
    }
};

}  // namespace atom::algorithm::simd

#endif  // ATOM_ALGORITHM_CORE_SIMD_UTILS_HPP
