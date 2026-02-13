#ifndef ATOM_ALGORITHM_CRYPTO_XXTEA_HPP
#define ATOM_ALGORITHM_CRYPTO_XXTEA_HPP

#include <span>
#include <vector>

#include "tea_common.hpp"

namespace atom::algorithm {

/**
 * @brief Encrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * The XXTEA algorithm is an extension of TEA, designed to correct some of TEA's
 * weaknesses.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of encrypted 32-bit values.
 * @throws TEAException if the input data is too small or the key is invalid.
 */
template <UInt32Container Container>
auto xxteaEncrypt(const Container &inputData, std::span<const u32, 4> inputKey)
    -> std::vector<u32>;

/**
 * @brief Decrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of decrypted 32-bit values.
 * @throws TEAException if the input data is too small or the key is invalid.
 */
template <UInt32Container Container>
auto xxteaDecrypt(const Container &inputData, std::span<const u32, 4> inputKey)
    -> std::vector<u32>;

/**
 * @brief Parallel version of XXTEA encryption for large data sets.
 *
 * This function uses multiple threads to encrypt the input data, which can
 * significantly improve performance for large data sets.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey The 128-bit key used for encryption.
 * @param numThreads The number of threads to use. If 0, the function uses the
 * number of hardware threads available.
 * @return A vector of encrypted 32-bit values.
 */
template <UInt32Container Container>
auto xxteaEncryptParallel(const Container &inputData,
                          std::span<const u32, 4> inputKey,
                          usize numThreads = 0) -> std::vector<u32>;

/**
 * @brief Parallel version of XXTEA decryption for large data sets.
 *
 * This function uses multiple threads to decrypt the input data, which can
 * significantly improve performance for large data sets.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey The 128-bit key used for decryption.
 * @param numThreads The number of threads to use. If 0, the function uses the
 * number of hardware threads available.
 * @return A vector of decrypted 32-bit values.
 */
template <UInt32Container Container>
auto xxteaDecryptParallel(const Container &inputData,
                          std::span<const u32, 4> inputKey,
                          usize numThreads = 0) -> std::vector<u32>;

/**
 * @brief Implementation detail for XXTEA encryption.
 *
 * This function performs the actual XXTEA encryption.
 *
 * @param inputData A span of 32-bit values to encrypt.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of encrypted 32-bit values.
 */
auto xxteaEncryptImpl(std::span<const u32> inputData,
                      std::span<const u32, 4> inputKey) -> std::vector<u32>;

/**
 * @brief Implementation detail for XXTEA decryption.
 *
 * This function performs the actual XXTEA decryption.
 *
 * @param inputData A span of 32-bit values to decrypt.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of decrypted 32-bit values.
 */
auto xxteaDecryptImpl(std::span<const u32> inputData,
                      std::span<const u32, 4> inputKey) -> std::vector<u32>;

/**
 * @brief Implementation detail for parallel XXTEA encryption.
 *
 * This function performs the actual parallel XXTEA encryption.
 *
 * @param inputData A span of 32-bit values to encrypt.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @param numThreads The number of threads to use for encryption.
 * @return A vector of encrypted 32-bit values.
 */
auto xxteaEncryptParallelImpl(std::span<const u32> inputData,
                              std::span<const u32, 4> inputKey,
                              usize numThreads) -> std::vector<u32>;

/**
 * @brief Implementation detail for parallel XXTEA decryption.
 *
 * This function performs the actual parallel XXTEA decryption.
 *
 * @param inputData A span of 32-bit values to decrypt.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @param numThreads The number of threads to use for decryption.
 * @return A vector of decrypted 32-bit values.
 */
auto xxteaDecryptParallelImpl(std::span<const u32> inputData,
                              std::span<const u32, 4> inputKey,
                              usize numThreads) -> std::vector<u32>;

/**
 * @brief Encrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * The XXTEA algorithm is an extension of TEA, designed to correct some of TEA's
 * weaknesses.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of encrypted 32-bit values.
 * @throws TEAException if the input data is too small or the key is invalid.
 */
template <UInt32Container Container>
auto xxteaEncrypt(const Container &inputData, std::span<const u32, 4> inputKey)
    -> std::vector<u32> {
    return xxteaEncryptImpl(
        std::span<const u32>{inputData.data(), inputData.size()}, inputKey);
}

/**
 * @brief Decrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of decrypted 32-bit values.
 * @throws TEAException if the input data is too small or the key is invalid.
 */
template <UInt32Container Container>
auto xxteaDecrypt(const Container &inputData, std::span<const u32, 4> inputKey)
    -> std::vector<u32> {
    return xxteaDecryptImpl(
        std::span<const u32>{inputData.data(), inputData.size()}, inputKey);
}

/**
 * @brief Parallel version of XXTEA encryption for large data sets.
 *
 * @warning SECURITY WARNING: This function processes data in independent
 * chunks, which effectively operates in ECB (Electronic Codebook) mode. This
 * breaks the semantic security of XXTEA as identical plaintext blocks produce
 * identical ciphertext blocks. Use only when:
 * - Performance is critical and security trade-offs are acceptable
 * - Data has high entropy (e.g., already compressed or random)
 * - You understand the security implications
 *
 * For secure encryption of structured data, use the non-parallel version
 * xxteaEncrypt() which processes the entire message as a single block.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey The 128-bit key used for encryption.
 * @param numThreads The number of threads to use. If 0, the function uses the
 * number of hardware threads available.
 * @return A vector of encrypted 32-bit values.
 */
template <UInt32Container Container>
[[deprecated(
    "Use xxteaEncrypt() for secure encryption. This parallel version "
    "operates in ECB mode which has security weaknesses.")]]
auto xxteaEncryptParallel(const Container &inputData,
                          std::span<const u32, 4> inputKey, usize numThreads)
    -> std::vector<u32> {
    return xxteaEncryptParallelImpl(
        std::span<const u32>{inputData.data(), inputData.size()}, inputKey,
        numThreads);
}

/**
 * @brief Parallel version of XXTEA decryption for large data sets.
 *
 * @warning SECURITY WARNING: This function is the counterpart to
 * xxteaEncryptParallel() and processes data in independent chunks (ECB mode).
 * Only use this to decrypt data that was encrypted with xxteaEncryptParallel().
 * See xxteaEncryptParallel() documentation for security implications.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey The 128-bit key used for decryption.
 * @param numThreads The number of threads to use. If 0, the function uses the
 * number of hardware threads available.
 * @return A vector of decrypted 32-bit values.
 */
template <UInt32Container Container>
[[deprecated(
    "Use xxteaDecrypt() for secure decryption. This parallel version "
    "operates in ECB mode which has security weaknesses.")]]
auto xxteaDecryptParallel(const Container &inputData,
                          std::span<const u32, 4> inputKey, usize numThreads)
    -> std::vector<u32> {
    return xxteaDecryptParallelImpl(
        std::span<const u32>{inputData.data(), inputData.size()}, inputKey,
        numThreads);
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CRYPTO_XXTEA_HPP
