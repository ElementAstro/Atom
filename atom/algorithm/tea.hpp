#ifndef ATOM_ALGORITHM_TEA_HPP
#define ATOM_ALGORITHM_TEA_HPP

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace atom::algorithm {

// 自定义异常类
class TEAException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Key concepts
template <typename T>
concept UInt32Container = std::ranges::contiguous_range<T> && requires(T t) {
    { std::data(t) } -> std::convertible_to<const uint32_t *>;
    { std::size(t) } -> std::convertible_to<std::size_t>;
    requires sizeof(std::ranges::range_value_t<T>) == sizeof(uint32_t);
};

// 使用固定大小的数组类型作为密钥
using XTEAKey = std::array<uint32_t, 4>;

/**
 * @brief Encrypts two 32-bit values using the TEA algorithm.
 *
 * @param value0 The first 32-bit value to be encrypted.
 * @param value1 The second 32-bit value to be encrypted.
 * @param key The 128-bit key used for encryption.
 * @throws TEAException if the key is invalid
 */
auto teaEncrypt(uint32_t &value0, uint32_t &value1,
                const std::array<uint32_t, 4> &key) noexcept(false) -> void;

/**
 * @brief Decrypts two 32-bit values using the TEA algorithm.
 *
 * @param value0 The first 32-bit value to be decrypted.
 * @param value1 The second 32-bit value to be decrypted.
 * @param key The 128-bit key used for decryption.
 * @throws TEAException if the key is invalid
 */
auto teaDecrypt(uint32_t &value0, uint32_t &value1,
                const std::array<uint32_t, 4> &key) noexcept(false) -> void;

/**
 * @brief Encrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey The 128-bit key used for encryption.
 * @return A vector of encrypted 32-bit values.
 * @throws TEAException if the input data is too small or key is invalid
 */
template <UInt32Container Container>
auto xxteaEncrypt(const Container &inputData,
                  std::span<const uint32_t, 4> inputKey)
    -> std::vector<uint32_t>;

/**
 * @brief Decrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey The 128-bit key used for decryption.
 * @return A vector of decrypted 32-bit values.
 * @throws TEAException if the input data is too small or key is invalid
 */
template <UInt32Container Container>
auto xxteaDecrypt(const Container &inputData,
                  std::span<const uint32_t, 4> inputKey)
    -> std::vector<uint32_t>;

/**
 * @brief Encrypts two 32-bit values using the XTEA algorithm.
 *
 * @param value0 The first 32-bit value to be encrypted.
 * @param value1 The second 32-bit value to be encrypted.
 * @param key The 128-bit key used for encryption.
 * @throws TEAException if the key is invalid
 */
auto xteaEncrypt(uint32_t &value0, uint32_t &value1,
                 const XTEAKey &key) noexcept(false) -> void;

/**
 * @brief Decrypts two 32-bit values using the XTEA algorithm.
 *
 * @param value0 The first 32-bit value to be decrypted.
 * @param value1 The second 32-bit value to be decrypted.
 * @param key The 128-bit key used for decryption.
 * @throws TEAException if the key is invalid
 */
auto xteaDecrypt(uint32_t &value0, uint32_t &value1,
                 const XTEAKey &key) noexcept(false) -> void;

/**
 * @brief Converts a byte array to a vector of 32-bit unsigned integers.
 *
 * @param data The byte array to be converted.
 * @return A vector of 32-bit unsigned integers.
 */
template <typename T>
    requires std::ranges::contiguous_range<T> &&
                 std::same_as<std::ranges::range_value_t<T>, uint8_t>
auto toUint32Vector(const T &data) -> std::vector<uint32_t>;

/**
 * @brief Converts a vector of 32-bit unsigned integers back to a byte array.
 *
 * @param data The vector of 32-bit unsigned integers to be converted.
 * @return A byte array.
 */
template <UInt32Container Container>
auto toByteArray(const Container &data) -> std::vector<uint8_t>;

/**
 * @brief Parallel version of XXTEA encryption for large data sets
 *
 * @param inputData The container of 32-bit values to be encrypted
 * @param inputKey The 128-bit key used for encryption
 * @param numThreads Number of threads to use (default: hardware concurrency)
 * @return A vector of encrypted 32-bit values
 */
template <UInt32Container Container>
auto xxteaEncryptParallel(const Container &inputData,
                          std::span<const uint32_t, 4> inputKey,
                          size_t numThreads = 0) -> std::vector<uint32_t>;

/**
 * @brief Parallel version of XXTEA decryption for large data sets
 *
 * @param inputData The container of 32-bit values to be decrypted
 * @param inputKey The 128-bit key used for decryption
 * @param numThreads Number of threads to use (default: hardware concurrency)
 * @return A vector of decrypted 32-bit values
 */
template <UInt32Container Container>
auto xxteaDecryptParallel(const Container &inputData,
                          std::span<const uint32_t, 4> inputKey,
                          size_t numThreads = 0) -> std::vector<uint32_t>;

auto xxteaEncryptImpl(std::span<const uint32_t> inputData,
                      std::span<const uint32_t, 4> inputKey)
    -> std::vector<uint32_t>;
auto xxteaDecryptImpl(std::span<const uint32_t> inputData,
                      std::span<const uint32_t, 4> inputKey)
    -> std::vector<uint32_t>;
auto xxteaEncryptParallelImpl(std::span<const uint32_t> inputData,
                              std::span<const uint32_t, 4> inputKey,
                              size_t numThreads) -> std::vector<uint32_t>;
auto xxteaDecryptParallelImpl(std::span<const uint32_t> inputData,
                              std::span<const uint32_t, 4> inputKey,
                              size_t numThreads) -> std::vector<uint32_t>;
auto toUint32VectorImpl(std::span<const uint8_t> data) -> std::vector<uint32_t>;
auto toByteArrayImpl(std::span<const uint32_t> data) -> std::vector<uint8_t>;

// Template function implementations
template <UInt32Container Container>
auto xxteaEncrypt(const Container &inputData,
                  std::span<const uint32_t, 4> inputKey)
    -> std::vector<uint32_t> {
    return xxteaEncryptImpl(
        std::span<const uint32_t>{inputData.data(), inputData.size()},
        inputKey);
}

// xxteaDecrypt 模板实现
template <UInt32Container Container>
auto xxteaDecrypt(const Container &inputData,
                  std::span<const uint32_t, 4> inputKey)
    -> std::vector<uint32_t> {
    return xxteaDecryptImpl(
        std::span<const uint32_t>{inputData.data(), inputData.size()},
        inputKey);
}

// xxteaEncryptParallel 模板实现
template <UInt32Container Container>
auto xxteaEncryptParallel(const Container &inputData,
                          std::span<const uint32_t, 4> inputKey,
                          size_t numThreads) -> std::vector<uint32_t> {
    return xxteaEncryptParallelImpl(
        std::span<const uint32_t>{inputData.data(), inputData.size()}, inputKey,
        numThreads);
}

// xxteaDecryptParallel 模板实现
template <UInt32Container Container>
auto xxteaDecryptParallel(const Container &inputData,
                          std::span<const uint32_t, 4> inputKey,
                          size_t numThreads) -> std::vector<uint32_t> {
    return xxteaDecryptParallelImpl(
        std::span<const uint32_t>{inputData.data(), inputData.size()}, inputKey,
        numThreads);
}

// toUint32Vector 模板实现
template <typename T>
    requires std::ranges::contiguous_range<T> &&
                 std::same_as<std::ranges::range_value_t<T>, uint8_t>
auto toUint32Vector(const T &data) -> std::vector<uint32_t> {
    return toUint32VectorImpl(
        std::span<const uint8_t>{data.data(), data.size()});
}

// toByteArray 模板实现
template <UInt32Container Container>
auto toByteArray(const Container &data) -> std::vector<uint8_t> {
    return toByteArrayImpl(std::span<const uint32_t>{data.data(), data.size()});
}

}  // namespace atom::algorithm

#endif
