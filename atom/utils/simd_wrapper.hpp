/**
 * @file simd_wrapper.hpp
 * @brief Cross-platform SIMD operations wrapper library
 *
 * Provides a unified interface encapsulating different platforms' SIMD
 * instruction sets:
 * - x86/x64: SSE, AVX, AVX2, AVX-512
 * - ARM: NEON, SVE
 *
 * Supports various data types and operations, enabling code portability with
 * high performance.
 */

#ifndef SIMD_WRAPPER_HPP
#define SIMD_WRAPPER_HPP

#include <algorithm>
#include <array>
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
    using vector_t = std::array<T, N>;
    using mask_t = std::array<bool, N>;
    static constexpr size_t width = N;

    // 初始化函数
    static vector_t zeros() {
        vector_t v;
        v.fill(0);
        return v;
    }

    static vector_t set1(scalar_t value) {
        vector_t v;
        v.fill(value);
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
// Vec类 - 向量类通用实现
//=====================================================================

template <typename T, size_t N>
class Vec {
public:
    using Traits = VecTraits<T, N>;
    using scalar_t = typename Traits::scalar_t;
    using vector_t = typename Traits::vector_t;
    using mask_t = typename Traits::mask_t;
    static constexpr size_t width = N;

private:
    vector_t data;

public:
    // 构造函数
    Vec() = default;

    // 从单一值构造(广播)
    explicit Vec(scalar_t value) : data(Traits::set1(value)) {}

    // 从数组构造
    explicit Vec(const vector_t& v) : data(v) {}

    // 从原始指针构造(对齐)
    static Vec load(const scalar_t* ptr) { return Vec(Traits::load(ptr)); }

    // 从原始指针构造(非对齐)
    static Vec loadu(const scalar_t* ptr) { return Vec(Traits::loadu(ptr)); }

    // 存储方法(对齐)
    void store(scalar_t* ptr) const { Traits::store(ptr, data); }

    // 存储方法(非对齐)
    void storeu(scalar_t* ptr) const { Traits::storeu(ptr, data); }

    // 获取原始数据
    const vector_t& raw() const { return data; }
    vector_t& raw() { return data; }

    // 访问运算符
    scalar_t operator[](size_t i) const { return data[i]; }
    scalar_t& operator[](size_t i) { return data[i]; }

    // 算术运算符
    Vec operator+(const Vec& rhs) const {
        return Vec(Traits::add(data, rhs.data));
    }

    Vec operator-(const Vec& rhs) const {
        return Vec(Traits::sub(data, rhs.data));
    }

    Vec operator*(const Vec& rhs) const {
        return Vec(Traits::mul(data, rhs.data));
    }

    Vec operator/(const Vec& rhs) const {
        return Vec(Traits::div(data, rhs.data));
    }

    Vec& operator+=(const Vec& rhs) {
        data = Traits::add(data, rhs.data);
        return *this;
    }

    Vec& operator-=(const Vec& rhs) {
        data = Traits::sub(data, rhs.data);
        return *this;
    }

    Vec& operator*=(const Vec& rhs) {
        data = Traits::mul(data, rhs.data);
        return *this;
    }

    Vec& operator/=(const Vec& rhs) {
        data = Traits::div(data, rhs.data);
        return *this;
    }

    // 数学方法
    Vec sqrt() const { return Vec(Traits::sqrt(data)); }

    Vec abs() const { return Vec(Traits::abs(data)); }

    Vec sin() const { return Vec(Traits::sin(data)); }

    Vec cos() const { return Vec(Traits::cos(data)); }

    Vec log() const { return Vec(Traits::log(data)); }

    Vec exp() const { return Vec(Traits::exp(data)); }

    // 静态数学方法
    static Vec min(const Vec& a, const Vec& b) {
        return Vec(Traits::min(a.data, b.data));
    }

    static Vec max(const Vec& a, const Vec& b) {
        return Vec(Traits::max(a.data, b.data));
    }

    // 融合乘加
    static Vec fmadd(const Vec& a, const Vec& b, const Vec& c) {
        return Vec(Traits::fmadd(a.data, b.data, c.data));
    }

    // 融合乘减
    static Vec fmsub(const Vec& a, const Vec& b, const Vec& c) {
        return Vec(Traits::fmsub(a.data, b.data, c.data));
    }

    // 比较操作
    mask_t operator==(const Vec& rhs) const {
        return Traits::cmpeq(data, rhs.data);
    }

    mask_t operator!=(const Vec& rhs) const {
        return Traits::cmpne(data, rhs.data);
    }

    mask_t operator<(const Vec& rhs) const {
        return Traits::cmplt(data, rhs.data);
    }

    mask_t operator<=(const Vec& rhs) const {
        return Traits::cmple(data, rhs.data);
    }

    mask_t operator>(const Vec& rhs) const {
        return Traits::cmpgt(data, rhs.data);
    }

    mask_t operator>=(const Vec& rhs) const {
        return Traits::cmpge(data, rhs.data);
    }

    // 位操作 (对整数类型)
    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value, Vec>::type operator&(
        const Vec& rhs) const {
        return Vec(Traits::bitwise_and(data, rhs.data));
    }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value, Vec>::type operator|(
        const Vec& rhs) const {
        return Vec(Traits::bitwise_or(data, rhs.data));
    }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value, Vec>::type operator^(
        const Vec& rhs) const {
        return Vec(Traits::bitwise_xor(data, rhs.data));
    }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value, Vec>::type operator~()
        const {
        return Vec(Traits::bitwise_not(data));
    }

    // 移位操作 (对整数类型)
    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value, Vec>::type operator<<(
        int count) const {
        return Vec(Traits::shift_left(data, count));
    }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value, Vec>::type operator>>(
        int count) const {
        return Vec(Traits::shift_right(data, count));
    }

    // 混合操作(根据掩码选择)
    static Vec blend(const mask_t& mask, const Vec& a, const Vec& b) {
        return Vec(Traits::blend(mask, a.data, b.data));
    }

    // 水平操作
    scalar_t horizontal_sum() const { return Traits::horizontal_sum(data); }

    scalar_t horizontal_max() const { return Traits::horizontal_max(data); }

    scalar_t horizontal_min() const { return Traits::horizontal_min(data); }

    // 便捷工厂方法
    static Vec zeros() { return Vec(Traits::zeros()); }

    static Vec ones() { return Vec(static_cast<T>(1)); }
};

//=====================================================================
// 类型别名 - 为常见的向量大小和类型定义
//=====================================================================

using float32x4_t = Vec<float, 4>;    // 128位浮点向量
using float32x8_t = Vec<float, 8>;    // 256位浮点向量
using float32x16_t = Vec<float, 16>;  // 512位浮点向量
using float64x2_t = Vec<double, 2>;   // 128位双精度向量
using float64x4_t = Vec<double, 4>;   // 256位双精度向量
using float64x8_t = Vec<double, 8>;   // 512位双精度向量
using int8x16_t = Vec<int8_t, 16>;    // 128位8位整数向量
using int16x8_t = Vec<int16_t, 8>;    // 128位16位整数向量
using int32x4_t = Vec<int32_t, 4>;    // 128位32位整数向量
using int64x2_t = Vec<int64_t, 2>;    // 128位64位整数向量
using uint8x16_t = Vec<uint8_t, 16>;  // 128位8位无符号整数向量
using uint16x8_t = Vec<uint16_t, 8>;  // 128位16位无符号整数向量
using uint32x4_t = Vec<uint32_t, 4>;  // 128位32位无符号整数向量
using uint64x2_t = Vec<uint64_t, 2>;  // 128位64位无符号整数向量

//=====================================================================
// 这里可以添加针对特定指令集的优化实现
// 例如x86的SSE/AVX系列，ARM的NEON/SVE等
//=====================================================================

}  // namespace simd

#endif  // SIMD_WRAPPER_HPP