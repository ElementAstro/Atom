/**
 * @file simd_wrapper.hpp
 * @brief 跨平台SIMD操作封装库
 *
 * 提供统一的接口封装不同平台的SIMD指令集:
 * - x86/x64: SSE, AVX, AVX2, AVX-512
 * - ARM: NEON, SVE
 *
 * 支持多种数据类型和操作，实现代码可移植性与高性能。
 */

#ifndef SIMD_WRAPPER_HPP
#define SIMD_WRAPPER_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

// 检测架构和可用指令集
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
#define SIMD_ARCH_X86
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

// 检测各种x86指令集
#if defined(__AVX512F__)
#define SIMD_HAS_AVX512 1
#else
#define SIMD_HAS_AVX512 0
#endif

#if defined(__AVX2__)
#define SIMD_HAS_AVX2 1
#else
#define SIMD_HAS_AVX2 0
#endif

#if defined(__AVX__)
#define SIMD_HAS_AVX 1
#else
#define SIMD_HAS_AVX 0
#endif

#if defined(__SSE4_2__)
#define SIMD_HAS_SSE4_2 1
#else
#define SIMD_HAS_SSE4_2 0
#endif

#if defined(__SSE4_1__)
#define SIMD_HAS_SSE4_1 1
#else
#define SIMD_HAS_SSE4_1 0
#endif

#if defined(__SSE3__)
#define SIMD_HAS_SSE3 1
#else
#define SIMD_HAS_SSE3 0
#endif

#if defined(__SSE2__)
#define SIMD_HAS_SSE2 1
#else
#define SIMD_HAS_SSE2 0
#endif

#if defined(__SSE__)
#define SIMD_HAS_SSE 1
#else
#define SIMD_HAS_SSE 0
#endif

#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define SIMD_ARCH_ARM
#include <arm_neon.h>

#define SIMD_HAS_NEON 1

#if defined(__ARM_FEATURE_SVE)
#define SIMD_HAS_SVE 1
#include <arm_sve.h>
#else
#define SIMD_HAS_SVE 0
#endif
#else
// 标量实现回退
#define SIMD_ARCH_SCALAR
#endif

namespace simd {

// 前向声明
template <typename T, size_t N>
class Vec;
template <typename T, size_t N>
struct VecTraits;

//=====================================================================
// 默认标量实现 - 在任何平台上作为回退方案
//=====================================================================

template <typename T, size_t N>
struct VecTraits {
    using scalar_t = T;
    using vector_t = T[N];
    using mask_t = bool[N];
    static constexpr size_t width = N;

    // 初始化函数
    static vector_t zeros() {
        vector_t v;
        for (size_t i = 0; i < N; ++i)
            v[i] = 0;
        return v;
    }

    static vector_t set1(scalar_t value) {
        vector_t v;
        for (size_t i = 0; i < N; ++i)
            v[i] = value;
        return v;
    }

    // 加载/存储操作
    static vector_t load(const scalar_t* ptr) {
        vector_t v;
        for (size_t i = 0; i < N; ++i)
            v[i] = ptr[i];
        return v;
    }

    static vector_t loadu(const scalar_t* ptr) {
        return load(ptr);  // 标量模式下对齐与非对齐相同
    }

    static void store(scalar_t* ptr, const vector_t& v) {
        for (size_t i = 0; i < N; ++i)
            ptr[i] = v[i];
    }

    static void storeu(scalar_t* ptr, const vector_t& v) {
        store(ptr, v);  // 标量模式下对齐与非对齐相同
    }

    // 算术操作
    static vector_t add(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] + b[i];
        return result;
    }

    static vector_t sub(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] - b[i];
        return result;
    }

    static vector_t mul(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] * b[i];
        return result;
    }

    static vector_t div(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] / b[i];
        return result;
    }

    // 融合乘加: a * b + c
    static vector_t fmadd(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] * b[i] + c[i];
        return result;
    }

    // 融合乘减: a * b - c
    static vector_t fmsub(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] * b[i] - c[i];
        return result;
    }

    // 数学函数
    static vector_t sqrt(const vector_t& a) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = std::sqrt(a[i]);
        return result;
    }

    static vector_t abs(const vector_t& a) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = std::abs(a[i]);
        return result;
    }

    static vector_t min(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = std::min(a[i], b[i]);
        return result;
    }

    static vector_t max(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = std::max(a[i], b[i]);
        return result;
    }

    // 更多数学函数
    static vector_t sin(const vector_t& a) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = std::sin(a[i]);
        return result;
    }

    static vector_t cos(const vector_t& a) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = std::cos(a[i]);
        return result;
    }

    static vector_t log(const vector_t& a) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = std::log(a[i]);
        return result;
    }

    static vector_t exp(const vector_t& a) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = std::exp(a[i]);
        return result;
    }

    // 比较操作
    static mask_t cmpeq(const vector_t& a, const vector_t& b) {
        mask_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = (a[i] == b[i]);
        return result;
    }

    static mask_t cmpne(const vector_t& a, const vector_t& b) {
        mask_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = (a[i] != b[i]);
        return result;
    }

    static mask_t cmplt(const vector_t& a, const vector_t& b) {
        mask_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = (a[i] < b[i]);
        return result;
    }

    static mask_t cmple(const vector_t& a, const vector_t& b) {
        mask_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = (a[i] <= b[i]);
        return result;
    }

    static mask_t cmpgt(const vector_t& a, const vector_t& b) {
        mask_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = (a[i] > b[i]);
        return result;
    }

    static mask_t cmpge(const vector_t& a, const vector_t& b) {
        mask_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = (a[i] >= b[i]);
        return result;
    }

    // 位操作 (对整数类型)
    template <typename U = T>
    static typename std::enable_if<std::is_integral<U>::value, vector_t>::type
    bitwise_and(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] & b[i];
        return result;
    }

    template <typename U = T>
    static typename std::enable_if<std::is_integral<U>::value, vector_t>::type
    bitwise_or(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] | b[i];
        return result;
    }

    template <typename U = T>
    static typename std::enable_if<std::is_integral<U>::value, vector_t>::type
    bitwise_xor(const vector_t& a, const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] ^ b[i];
        return result;
    }

    template <typename U = T>
    static typename std::enable_if<std::is_integral<U>::value, vector_t>::type
    bitwise_not(const vector_t& a) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = ~a[i];
        return result;
    }

    // 移位操作 (对整数类型)
    template <typename U = T>
    static typename std::enable_if<std::is_integral<U>::value, vector_t>::type
    shift_left(const vector_t& a, int count) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] << count;
        return result;
    }

    template <typename U = T>
    static typename std::enable_if<std::is_integral<U>::value, vector_t>::type
    shift_right(const vector_t& a, int count) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = a[i] >> count;
        return result;
    }

    // 混合操作 (根据掩码选择)
    static vector_t blend(const mask_t& mask, const vector_t& a,
                          const vector_t& b) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = mask[i] ? a[i] : b[i];
        return result;
    }

    // 掩码转换为向量
    static vector_t mask_to_vector(const mask_t& mask) {
        vector_t result;
        for (size_t i = 0; i < N; ++i)
            result[i] = mask[i] ? ~static_cast<T>(0) : 0;
        return result;
    }

    // 元素提取/插入
    static scalar_t extract(const vector_t& v, size_t index) {
        return v[index];
    }

    static vector_t insert(const vector_t& v, size_t index, scalar_t value) {
        vector_t result = v;
        result[index] = value;
        return result;
    }

    // 元素重排
    template <size_t... Indices>
    static vector_t shuffle(const vector_t& v) {
        static_assert(sizeof...(Indices) == N, "Shuffle must have N indices");
        vector_t result;
        size_t indices[] = {Indices...};
        for (size_t i = 0; i < N; ++i) {
            result[i] = v[indices[i]];
        }
        return result;
    }

    // 水平求和
    static scalar_t horizontal_sum(const vector_t& v) {
        scalar_t sum = 0;
        for (size_t i = 0; i < N; ++i)
            sum += v[i];
        return sum;
    }

    // 水平最大值
    static scalar_t horizontal_max(const vector_t& v) {
        scalar_t max_val = v[0];
        for (size_t i = 1; i < N; ++i)
            if (v[i] > max_val)
                max_val = v[i];
        return max_val;
    }

    // 水平最小值
    static scalar_t horizontal_min(const vector_t& v) {
        scalar_t min_val = v[0];
        for (size_t i = 1; i < N; ++i)
            if (v[i] < min_val)
                min_val = v[i];
        return min_val;
    }
};

//=====================================================================
// SSE 实现 (128位向量)
//=====================================================================

#if defined(SIMD_ARCH_X86) && SIMD_HAS_SSE

// float 特化 (4 x float)
template <>
struct VecTraits<float, 4> {
    using scalar_t = float;
    using vector_t = __m128;
    using mask_t = __m128;
    static constexpr size_t width = 4;

    // 基本操作
    static vector_t zeros() { return _mm_setzero_ps(); }
    static vector_t set1(scalar_t value) { return _mm_set1_ps(value); }

    // 加载/存储
    static vector_t load(const scalar_t* ptr) { return _mm_load_ps(ptr); }
    static vector_t loadu(const scalar_t* ptr) { return _mm_loadu_ps(ptr); }
    static void store(scalar_t* ptr, const vector_t& v) {
        _mm_store_ps(ptr, v);
    }
    static void storeu(scalar_t* ptr, const vector_t& v) {
        _mm_storeu_ps(ptr, v);
    }

    // 算术操作
    static vector_t add(const vector_t& a, const vector_t& b) {
        return _mm_add_ps(a, b);
    }
    static vector_t sub(const vector_t& a, const vector_t& b) {
        return _mm_sub_ps(a, b);
    }
    static vector_t mul(const vector_t& a, const vector_t& b) {
        return _mm_mul_ps(a, b);
    }
    static vector_t div(const vector_t& a, const vector_t& b) {
        return _mm_div_ps(a, b);
    }

// 高级算术操作
#if SIMD_HAS_SSE3
    static vector_t hadd(const vector_t& a, const vector_t& b) {
        return _mm_hadd_ps(a, b);
    }
    static vector_t hsub(const vector_t& a, const vector_t& b) {
        return _mm_hsub_ps(a, b);
    }
#endif

#if SIMD_HAS_AVX2 || SIMD_HAS_FMA
    static vector_t fmadd(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        return _mm_fmadd_ps(a, b, c);  // a * b + c
    }

    static vector_t fmsub(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        return _mm_fmsub_ps(a, b, c);  // a * b - c
    }
#else
    static vector_t fmadd(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        return _mm_add_ps(_mm_mul_ps(a, b), c);  // a * b + c
    }

    static vector_t fmsub(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        return _mm_sub_ps(_mm_mul_ps(a, b), c);  // a * b - c
    }
#endif

    // 数学函数
    static vector_t sqrt(const vector_t& a) { return _mm_sqrt_ps(a); }
    static vector_t rcp(const vector_t& a) {
        return _mm_rcp_ps(a);
    }  // 倒数近似
    static vector_t rsqrt(const vector_t& a) {
        return _mm_rsqrt_ps(a);
    }  // 平方根倒数近似

    static vector_t abs(const vector_t& a) {
        return _mm_andnot_ps(_mm_set1_ps(-0.0f), a);  // 清除符号位
    }

    static vector_t min(const vector_t& a, const vector_t& b) {
        return _mm_min_ps(a, b);
    }
    static vector_t max(const vector_t& a, const vector_t& b) {
        return _mm_max_ps(a, b);
    }

    // 三角函数和指数函数 (使用SVML或者模拟)
    static vector_t sin(const vector_t& a) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
        return _mm_sin_ps(a);  // Intel编译器或使用Intel SVML的MSVC
#else
        alignas(16) scalar_t a_array[4], result[4];
        _mm_store_ps(a_array, a);
        for (int i = 0; i < 4; ++i)
            result[i] = std::sin(a_array[i]);
        return _mm_load_ps(result);
#endif
    }

    static vector_t cos(const vector_t& a) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
        return _mm_cos_ps(a);
#else
        alignas(16) scalar_t a_array[4], result[4];
        _mm_store_ps(a_array, a);
        for (int i = 0; i < 4; ++i)
            result[i] = std::cos(a_array[i]);
        return _mm_load_ps(result);
#endif
    }

    static vector_t log(const vector_t& a) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
        return _mm_log_ps(a);
#else
        alignas(16) scalar_t a_array[4], result[4];
        _mm_store_ps(a_array, a);
        for (int i = 0; i < 4; ++i)
            result[i] = std::log(a_array[i]);
        return _mm_load_ps(result);
#endif
    }

    static vector_t exp(const vector_t& a) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
        return _mm_exp_ps(a);
#else
        alignas(16) scalar_t a_array[4], result[4];
        _mm_store_ps(a_array, a);
        for (int i = 0; i < 4; ++i)
            result[i] = std::exp(a_array[i]);
        return _mm_load_ps(result);
#endif
    }

    // 比较操作
    static mask_t cmpeq(const vector_t& a, const vector_t& b) {
        return _mm_cmpeq_ps(a, b);
    }
    static mask_t cmpne(const vector_t& a, const vector_t& b) {
        return _mm_cmpneq_ps(a, b);
    }
    static mask_t cmplt(const vector_t& a, const vector_t& b) {
        return _mm_cmplt_ps(a, b);
    }
    static mask_t cmple(const vector_t& a, const vector_t& b) {
        return _mm_cmple_ps(a, b);
    }
    static mask_t cmpgt(const vector_t& a, const vector_t& b) {
        return _mm_cmpgt_ps(a, b);
    }
    static mask_t cmpge(const vector_t& a, const vector_t& b) {
        return _mm_cmpge_ps(a, b);
    }

    // 位操作
    static vector_t bitwise_and(const vector_t& a, const vector_t& b) {
        return _mm_and_ps(a, b);
    }
    static vector_t bitwise_or(const vector_t& a, const vector_t& b) {
        return _mm_or_ps(a, b);
    }
    static vector_t bitwise_xor(const vector_t& a, const vector_t& b) {
        return _mm_xor_ps(a, b);
    }
    static vector_t bitwise_not(const vector_t& a) {
        return _mm_xor_ps(a, _mm_castsi128_ps(_mm_set1_epi32(-1)));
    }

    // 混合操作
    static vector_t blend(const mask_t& mask, const vector_t& a,
                          const vector_t& b) {
#if SIMD_HAS_SSE4_1
        return _mm_blendv_ps(b, a, mask);
#else
        return _mm_or_ps(_mm_and_ps(mask, a), _mm_andnot_ps(mask, b));
#endif
    }

    // 元素访问
    // 元素访问
    static scalar_t extract(const vector_t& v, size_t index) {
#if SIMD_HAS_SSE4_1
        switch (index) {
            case 0:
                return _mm_cvtss_f32(v);
            case 1:
                return _mm_cvtss_f32(
                    _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));
            case 2:
                return _mm_cvtss_f32(
                    _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
            case 3:
                return _mm_cvtss_f32(
                    _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3)));
            default:
                return 0;
        }
#else
        alignas(16) scalar_t tmp[4];
        _mm_store_ps(tmp, v);
        return tmp[index];
#endif
    }

    static vector_t insert(const vector_t& v, size_t index, scalar_t value) {
#if SIMD_HAS_SSE4_1
        switch (index) {
            case 0:
                return _mm_insert_ps(v, _mm_set_ss(value), 0x00);
            case 1:
                return _mm_insert_ps(v, _mm_set_ss(value), 0x10);
            case 2:
                return _mm_insert_ps(v, _mm_set_ss(value), 0x20);
            case 3:
                return _mm_insert_ps(v, _mm_set_ss(value), 0x30);
            default:
                return v;
        }
#else
        alignas(16) scalar_t tmp[4];
        _mm_store_ps(tmp, v);
        tmp[index] = value;
        return _mm_load_ps(tmp);
#endif
    }

    // 洗牌操作
    template <int I0, int I1, int I2, int I3>
    static vector_t shuffle(const vector_t& v) {
        return _mm_shuffle_ps(v, v, _MM_SHUFFLE(I3, I2, I1, I0));
    }

    template <int I0, int I1, int I2, int I3>
    static vector_t shuffle(const vector_t& a, const vector_t& b) {
        return _mm_shuffle_ps(a, b, _MM_SHUFFLE(I3, I2, I1, I0));
    }

    // 水平操作
    static scalar_t horizontal_sum(const vector_t& v) {
#if SIMD_HAS_SSE3
        __m128 temp = _mm_hadd_ps(v, v);
        temp = _mm_hadd_ps(temp, temp);
        return _mm_cvtss_f32(temp);
#else
        __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
        __m128 sums = _mm_add_ps(v, shuf);
        shuf = _mm_movehl_ps(shuf, sums);
        sums = _mm_add_ss(sums, shuf);
        return _mm_cvtss_f32(sums);
#endif
    }

    static scalar_t horizontal_max(const vector_t& v) {
        __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
        __m128 maxs = _mm_max_ps(v, shuf);
        shuf = _mm_movehl_ps(shuf, maxs);
        maxs = _mm_max_ss(maxs, shuf);
        return _mm_cvtss_f32(maxs);
    }

    static scalar_t horizontal_min(const vector_t& v) {
        __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
        __m128 mins = _mm_min_ps(v, shuf);
        shuf = _mm_movehl_ps(shuf, mins);
        mins = _mm_min_ss(mins, shuf);
        return _mm_cvtss_f32(mins);
    }
};

// int32_t 特化 (4 x int32_t)
template <>
struct VecTraits<int32_t, 4> {
    using scalar_t = int32_t;
    using vector_t = __m128i;
    using mask_t = __m128i;
    static constexpr size_t width = 4;

    // 基本操作
    static vector_t zeros() { return _mm_setzero_si128(); }
    static vector_t set1(scalar_t value) { return _mm_set1_epi32(value); }

    // 加载/存储
    static vector_t load(const scalar_t* ptr) {
        return _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
    }
    static vector_t loadu(const scalar_t* ptr) {
        return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
    }
    static void store(scalar_t* ptr, const vector_t& v) {
        _mm_store_si128(reinterpret_cast<__m128i*>(ptr), v);
    }
    static void storeu(scalar_t* ptr, const vector_t& v) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), v);
    }

    // 算术操作
    static vector_t add(const vector_t& a, const vector_t& b) {
        return _mm_add_epi32(a, b);
    }
    static vector_t sub(const vector_t& a, const vector_t& b) {
        return _mm_sub_epi32(a, b);
    }

#if SIMD_HAS_SSE4_1
    static vector_t mul(const vector_t& a, const vector_t& b) {
        return _mm_mullo_epi32(a, b);
    }
#else
    static vector_t mul(const vector_t& a, const vector_t& b) {
        __m128i tmp1 = _mm_mul_epu32(a, b);
        __m128i tmp2 =
            _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4));
        return _mm_unpacklo_epi32(
            _mm_shuffle_epi32(tmp1, _MM_SHUFFLE(0, 0, 2, 0)),
            _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(0, 0, 2, 0)));
    }
#endif

    // 没有直接的整数除法指令，使用标量模拟
    static vector_t div(const vector_t& a, const vector_t& b) {
        alignas(16) scalar_t a_array[4], b_array[4], result[4];
        _mm_store_si128(reinterpret_cast<__m128i*>(a_array), a);
        _mm_store_si128(reinterpret_cast<__m128i*>(b_array), b);
        for (int i = 0; i < 4; ++i)
            result[i] = a_array[i] / b_array[i];
        return _mm_load_si128(reinterpret_cast<const __m128i*>(result));
    }

    // 数学函数
    static vector_t abs(const vector_t& a) {
#if SIMD_HAS_SSE4_1
        return _mm_abs_epi32(a);
#else
        // 创建符号掩码
        __m128i sign_mask = _mm_srai_epi32(a, 31);
        // XOR并添加来执行2的补码转换
        return _mm_add_epi32(_mm_xor_si128(a, sign_mask),
                             _mm_srli_epi32(sign_mask, 31));
#endif
    }

    static vector_t min(const vector_t& a, const vector_t& b) {
#if SIMD_HAS_SSE4_1
        return _mm_min_epi32(a, b);
#else
        // 使用比较和混合
        __m128i mask = _mm_cmplt_epi32(a, b);
        return _mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b));
#endif
    }

    static vector_t max(const vector_t& a, const vector_t& b) {
#if SIMD_HAS_SSE4_1
        return _mm_max_epi32(a, b);
#else
        // 使用比较和混合
        __m128i mask = _mm_cmpgt_epi32(a, b);
        return _mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b));
#endif
    }

    // 比较操作
    static mask_t cmpeq(const vector_t& a, const vector_t& b) {
        return _mm_cmpeq_epi32(a, b);
    }
    static mask_t cmplt(const vector_t& a, const vector_t& b) {
        return _mm_cmplt_epi32(a, b);
    }
    static mask_t cmpgt(const vector_t& a, const vector_t& b) {
        return _mm_cmpgt_epi32(a, b);
    }

    static mask_t cmpne(const vector_t& a, const vector_t& b) {
        return _mm_xor_si128(_mm_cmpeq_epi32(a, b), _mm_set1_epi32(-1));
    }

    static mask_t cmple(const vector_t& a, const vector_t& b) {
        return _mm_or_si128(_mm_cmplt_epi32(a, b), _mm_cmpeq_epi32(a, b));
    }

    static mask_t cmpge(const vector_t& a, const vector_t& b) {
        return _mm_or_si128(_mm_cmpgt_epi32(a, b), _mm_cmpeq_epi32(a, b));
    }

    // 位操作
    static vector_t bitwise_and(const vector_t& a, const vector_t& b) {
        return _mm_and_si128(a, b);
    }
    static vector_t bitwise_or(const vector_t& a, const vector_t& b) {
        return _mm_or_si128(a, b);
    }
    static vector_t bitwise_xor(const vector_t& a, const vector_t& b) {
        return _mm_xor_si128(a, b);
    }
    static vector_t bitwise_not(const vector_t& a) {
        return _mm_xor_si128(a, _mm_set1_epi32(-1));
    }

    // 移位操作
    static vector_t shift_left(const vector_t& a, int count) {
        return _mm_slli_epi32(a, count);
    }
    static vector_t shift_right(const vector_t& a, int count) {
        return _mm_srli_epi32(a, count);
    }
    static vector_t shift_right_arithmetic(const vector_t& a, int count) {
        return _mm_srai_epi32(a, count);
    }

    // 混合操作
    static vector_t blend(const mask_t& mask, const vector_t& a,
                          const vector_t& b) {
#if SIMD_HAS_SSE4_1
        return _mm_blendv_epi8(b, a, mask);
#else
        return _mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b));
#endif
    }

    // 元素访问
    static scalar_t extract(const vector_t& v, size_t index) {
#if SIMD_HAS_SSE4_1
        switch (index) {
            case 0:
                return _mm_extract_epi32(v, 0);
            case 1:
                return _mm_extract_epi32(v, 1);
            case 2:
                return _mm_extract_epi32(v, 2);
            case 3:
                return _mm_extract_epi32(v, 3);
            default:
                return 0;
        }
#else
        alignas(16) scalar_t tmp[4];
        _mm_store_si128(reinterpret_cast<__m128i*>(tmp), v);
        return tmp[index];
#endif
    }

    static vector_t insert(const vector_t& v, size_t index, scalar_t value) {
#if SIMD_HAS_SSE4_1
        switch (index) {
            case 0:
                return _mm_insert_epi32(v, value, 0);
            case 1:
                return _mm_insert_epi32(v, value, 1);
            case 2:
                return _mm_insert_epi32(v, value, 2);
            case 3:
                return _mm_insert_epi32(v, value, 3);
            default:
                return v;
        }
#else
        alignas(16) scalar_t tmp[4];
        _mm_store_si128(reinterpret_cast<__m128i*>(tmp), v);
        tmp[index] = value;
        return _mm_load_si128(reinterpret_cast<const __m128i*>(tmp));
#endif
    }

    // 洗牌操作
    template <int I0, int I1, int I2, int I3>
    static vector_t shuffle(const vector_t& v) {
        return _mm_shuffle_epi32(v, _MM_SHUFFLE(I3, I2, I1, I0));
    }

    // 水平操作
    static scalar_t horizontal_sum(const vector_t& v) {
#if SIMD_HAS_SSE3
        __m128i temp = _mm_hadd_epi32(v, v);
        temp = _mm_hadd_epi32(temp, temp);
        return _mm_cvtsi128_si32(temp);
#else
        __m128i t1 =
            _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(1, 0, 3, 2)));
        __m128i t2 =
            _mm_add_epi32(t1, _mm_shuffle_epi32(t1, _MM_SHUFFLE(2, 3, 0, 1)));
        return _mm_cvtsi128_si32(t2);
#endif
    }
};

// double 特化 (2 x double)
template <>
struct VecTraits<double, 2> {
    using scalar_t = double;
    using vector_t = __m128d;
    using mask_t = __m128d;
    static constexpr size_t width = 2;

    // 基本操作
    static vector_t zeros() { return _mm_setzero_pd(); }
    static vector_t set1(scalar_t value) { return _mm_set1_pd(value); }

    // 加载/存储
    static vector_t load(const scalar_t* ptr) { return _mm_load_pd(ptr); }
    static vector_t loadu(const scalar_t* ptr) { return _mm_loadu_pd(ptr); }
    static void store(scalar_t* ptr, const vector_t& v) {
        _mm_store_pd(ptr, v);
    }
    static void storeu(scalar_t* ptr, const vector_t& v) {
        _mm_storeu_pd(ptr, v);
    }

    // 算术操作
    static vector_t add(const vector_t& a, const vector_t& b) {
        return _mm_add_pd(a, b);
    }
    static vector_t sub(const vector_t& a, const vector_t& b) {
        return _mm_sub_pd(a, b);
    }
    static vector_t mul(const vector_t& a, const vector_t& b) {
        return _mm_mul_pd(a, b);
    }
    static vector_t div(const vector_t& a, const vector_t& b) {
        return _mm_div_pd(a, b);
    }

// 高级算术操作
#if SIMD_HAS_AVX2 || SIMD_HAS_FMA
    static vector_t fmadd(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        return _mm_fmadd_pd(a, b, c);
    }

    static vector_t fmsub(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        return _mm_fmsub_pd(a, b, c);
    }
#else
    static vector_t fmadd(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        return _mm_add_pd(_mm_mul_pd(a, b), c);
    }

    static vector_t fmsub(const vector_t& a, const vector_t& b,
                          const vector_t& c) {
        return _mm_sub_pd(_mm_mul_pd(a, b), c);
    }
#endif

    // 数学函数
    static vector_t sqrt(const vector_t& a) { return _mm_sqrt_pd(a); }

    static vector_t abs(const vector_t& a) {
        return _mm_andnot_pd(_mm_set1_pd(-0.0), a);  // 清除符号位
    }

    static vector_t min(const vector_t& a, const vector_t& b) {
        return _mm_min_pd(a, b);
    }
    static vector_t max(const vector_t& a, const vector_t& b) {
        return _mm_max_pd(a, b);
    }

    // 高级数学函数（SVML或标量模拟）
    static vector_t sin(const vector_t& a) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
        return _mm_sin_pd(a);
#else
        alignas(16) scalar_t a_array[2], result[2];
        _mm_store_pd(a_array, a);
        for (int i = 0; i < 2; ++i)
            result[i] = std::sin(a_array[i]);
        return _mm_load_pd(result);
#endif
    }

    static vector_t cos(const vector_t& a) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
        return _mm_cos_pd(a);
#else
        alignas(16) scalar_t a_array[2], result[2];
        _mm_store_pd(a_array, a);
        for (int i = 0; i < 2; ++i)
            result[i] = std::cos(a_array[i]);
        return _mm_load_pd(result);
#endif
    }

    static vector_t log(const vector_t& a) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
        return _mm_log_pd(a);
#else
        alignas(16) scalar_t a_array[2], result[2];
        _mm_store_pd(a_array, a);
        for (int i = 0; i < 2; ++i)
            result[i] = std::log(a_array[i]);
        return _mm_load_pd(result);
#endif
    }

    static vector_t exp(const vector_t& a) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
        return _mm_exp_pd(a);
#else
        alignas(16) scalar_t a_array[2], result[2];
        _mm_store_pd(a_array, a);
        for (int i = 0; i < 2; ++i)
            result[i] = std::exp(a_array[i]);
        return _mm_load_pd(result);
#endif
    }

    // 比较操作
    static mask_t cmpeq(const vector_t& a, const vector_t& b) {
        return _mm_cmpeq_pd(a, b);
    }
    static mask_t cmpne(const vector_t& a, const vector_t& b) {
        return _mm_cmpneq_pd(a, b);
    }
    static mask_t cmplt(const vector_t& a, const vector_t& b) {
        return _mm_cmplt_pd(a, b);
    }
    static mask_t cmple(const vector_t& a, const vector_t& b) {
        return _mm_cmple_pd(a, b);
    }
    static mask_t cmpgt(const vector_t& a, const vector_t& b) {
        return _mm_cmpgt_pd(a, b);
    }
    static mask_t cmpge(const vector_t& a, const vector_t& b) {
        return _mm_cmpge_pd(a, b);
    }

    // 位操作
    static vector_t bitwise_and(const vector_t& a, const vector_t& b) {
        return _mm_and_pd(a, b);
    }
    static vector_t bitwise_or(const vector_t& a, const vector_t& b) {
        return _mm_or_pd(a, b);
    }
    static vector_t bitwise_xor(const vector_t& a, const vector_t& b) {
        return _mm_xor_pd(a, b);
    }
    static vector_t bitwise_not(const vector_t& a) {
        return _mm_xor_pd(a, _mm_castsi128_pd(_mm_set1_epi64x(-1)));
    }

    // 混合操作
    static vector_t blend(const mask_t& mask, const vector_t& a,
                          const vector_t& b) {
#if SIMD_HAS_SSE4_1
        return _mm_blendv_pd(b, a, mask);
#else
        return _mm_or_pd(_mm_and_pd(mask, a), _mm_andnot_pd(mask, b));
#endif
    }

    // 元素访问
    static scalar_t extract(const vector_t& v, size_t index) {
#if SIMD_HAS_SSE4_1
        switch (index) {
            case 0:
                return _mm_cvtsd_f64(v);
            case 1:
                return _mm_cvtsd_f64(_mm_shuffle_pd(v, v, 1));
            default:
                return 0;
        }
#else
        alignas(16) scalar_t tmp[2];
        _mm_store_pd(tmp, v);
        return tmp[index];
