#include "tea.hpp"

#include <algorithm>
#include <array>
#include <future>
#include <span>
#include <thread>
#include <vector>

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
constexpr usize hardware_destructive_interference_size = 64;
#endif

#ifdef ATOM_USE_BOOST
#include <boost/endian/conversion.hpp>
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace atom::algorithm {
// Constants for TEA
constexpr u32 DELTA = 0x9E3779B9;
constexpr i32 NUM_ROUNDS = 32;
constexpr i32 SHIFT_4 = 4;
constexpr i32 SHIFT_5 = 5;
constexpr i32 BYTE_SHIFT = 8;
constexpr usize MIN_ROUNDS = 6;
constexpr usize MAX_ROUNDS = 52;
constexpr i32 SHIFT_3 = 3;
constexpr i32 SHIFT_2 = 2;
constexpr u32 KEY_MASK = 3;
constexpr i32 SHIFT_11 = 11;

// Helper function to validate key
static inline bool isValidKey(const std::array<u32, 4>& key) noexcept {
    // Check if the key is all zeros, which is generally insecure
    return !(key[0] == 0 && key[1] == 0 && key[2] == 0 && key[3] == 0);
}

// TEA encryption function
auto teaEncrypt(u32& value0, u32& value1,
                const std::array<u32, 4>& key) noexcept(false) -> void {
    try {
        if (!isValidKey(key)) {
            spdlog::error("Invalid key provided for TEA encryption");
            throw TEAException("Invalid key for TEA encryption");
        }

        u32 sum = 0;
        for (i32 i = 0; i < NUM_ROUNDS; ++i) {
            sum += DELTA;
            value0 += ((value1 << SHIFT_4) + key[0]) ^ (value1 + sum) ^
                      ((value1 >> SHIFT_5) + key[1]);
            value1 += ((value0 << SHIFT_4) + key[2]) ^ (value0 + sum) ^
                      ((value0 >> SHIFT_5) + key[3]);
        }
    } catch (const TEAException&) {
        throw;  // Re-throw TEA specific exceptions
    } catch (const std::exception& e) {
        spdlog::error("TEA encryption error: {}", e.what());
        throw TEAException(std::string("TEA encryption error: ") + e.what());
    }
}

// TEA decryption function
auto teaDecrypt(u32& value0, u32& value1,
                const std::array<u32, 4>& key) noexcept(false) -> void {
    try {
        if (!isValidKey(key)) {
            spdlog::error("Invalid key provided for TEA decryption");
            throw TEAException("Invalid key for TEA decryption");
        }

        u32 sum = DELTA * NUM_ROUNDS;
        for (i32 i = 0; i < NUM_ROUNDS; ++i) {
            value1 -= ((value0 << SHIFT_4) + key[2]) ^ (value0 + sum) ^
                      ((value0 >> SHIFT_5) + key[3]);
            value0 -= ((value1 << SHIFT_4) + key[0]) ^ (value1 + sum) ^
                      ((value1 >> SHIFT_5) + key[1]);
            sum -= DELTA;
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("TEA decryption error: {}", e.what());
        throw TEAException(std::string("TEA decryption error: ") + e.what());
    }
}

// Optimized byte conversion function using compile-time conditional branches
static inline u32 byteToNative(u8 byte, i32 position) noexcept {
    u32 value = static_cast<u32>(byte) << (position * BYTE_SHIFT);
#ifdef ATOM_USE_BOOST
    if constexpr (std::endian::native != std::endian::little) {
        return boost::endian::little_to_native(value);
    }
#endif
    return value;
}

static inline u8 nativeToByte(u32 value, i32 position) noexcept {
#ifdef ATOM_USE_BOOST
    if constexpr (std::endian::native != std::endian::little) {
        value = boost::endian::native_to_little(value);
    }
#endif
    return static_cast<u8>(value >> (position * BYTE_SHIFT));
}

// Implementation of non-template versions of toUint32Vector and toByteArray for
// internal use
auto toUint32VectorImpl(std::span<const u8> data) -> std::vector<u32> {
    usize numElements = (data.size() + 3) / 4;
    std::vector<u32> result(numElements, 0);

    for (usize index = 0; index < data.size(); ++index) {
        result[index / 4] |= byteToNative(data[index], index % 4);
    }

    return result;
}

auto toByteArrayImpl(std::span<const u32> data) -> std::vector<u8> {
    std::vector<u8> result(data.size() * 4);

    for (usize index = 0; index < data.size(); ++index) {
        for (i32 bytePos = 0; bytePos < 4; ++bytePos) {
            result[index * 4 + bytePos] = nativeToByte(data[index], bytePos);
        }
    }

    return result;
}

// XXTEA functions with optimized implementations
namespace detail {
constexpr u32 MX(u32 sum, u32 y, u32 z, i32 p, u32 e, const u32* k) noexcept {
    return ((z >> SHIFT_5 ^ y << SHIFT_2) + (y >> SHIFT_3 ^ z << SHIFT_4)) ^
           ((sum ^ y) + (k[(p & 3) ^ e] ^ z));
}
}  // namespace detail

// XXTEA encryption implementation (non-template version)
auto xxteaEncryptImpl(std::span<const u32> inputData,
                      std::span<const u32, 4> inputKey) -> std::vector<u32> {
    if (inputData.empty()) {
        spdlog::error("Empty data provided for XXTEA encryption");
        throw TEAException("Empty data provided for XXTEA encryption");
    }

    usize numElements = inputData.size();
    if (numElements < 2) {
        return {inputData.begin(), inputData.end()};  // Return a copy
    }

    std::vector<u32> result(inputData.begin(), inputData.end());

    u32 sum = 0;
    u32 lastElement = result[numElements - 1];
    usize numRounds = MIN_ROUNDS + MAX_ROUNDS / numElements;

    try {
        for (usize roundIndex = 0; roundIndex < numRounds; ++roundIndex) {
            sum += DELTA;
            u32 keyIndex = (sum >> SHIFT_2) & KEY_MASK;

            for (usize elementIndex = 0; elementIndex < numElements - 1;
                 ++elementIndex) {
                u32 currentElement = result[elementIndex + 1];
                result[elementIndex] +=
                    detail::MX(sum, currentElement, lastElement, elementIndex,
                               keyIndex, inputKey.data());
                lastElement = result[elementIndex];
            }

            u32 currentElement = result[0];
            result[numElements - 1] +=
                detail::MX(sum, currentElement, lastElement, numElements - 1,
                           keyIndex, inputKey.data());
            lastElement = result[numElements - 1];
        }
    } catch (const std::exception& e) {
        spdlog::error("XXTEA encryption error: {}", e.what());
        throw TEAException(std::string("XXTEA encryption error: ") + e.what());
    }

    return result;
}

// XXTEA decryption implementation (non-template version)
auto xxteaDecryptImpl(std::span<const u32> inputData,
                      std::span<const u32, 4> inputKey) -> std::vector<u32> {
    if (inputData.empty()) {
        spdlog::error("Empty data provided for XXTEA decryption");
        throw TEAException("Empty data provided for XXTEA decryption");
    }

    usize numElements = inputData.size();
    if (numElements < 2) {
        return {inputData.begin(), inputData.end()};
    }

    std::vector<u32> result(inputData.begin(), inputData.end());
    usize numRounds = MIN_ROUNDS + MAX_ROUNDS / numElements;
    u32 sum = numRounds * DELTA;

    try {
        for (usize roundIndex = 0; roundIndex < numRounds; ++roundIndex) {
            u32 keyIndex = (sum >> SHIFT_2) & KEY_MASK;
            u32 currentElement = result[0];

            for (usize elementIndex = numElements - 1; elementIndex > 0;
                 --elementIndex) {
                u32 lastElement = result[elementIndex - 1];
                result[elementIndex] -=
                    detail::MX(sum, currentElement, lastElement, elementIndex,
                               keyIndex, inputKey.data());
                currentElement = result[elementIndex];
            }

            u32 lastElement = result[numElements - 1];
            result[0] -= detail::MX(sum, currentElement, lastElement, 0,
                                    keyIndex, inputKey.data());
            currentElement = result[0];
            sum -= DELTA;
        }
    } catch (const std::exception& e) {
        spdlog::error("XXTEA decryption error: {}", e.what());
        throw TEAException(std::string("XXTEA decryption error: ") + e.what());
    }

    return result;
}

// XTEA encryption function with enhanced security and validation
auto xteaEncrypt(u32& value0, u32& value1, const XTEAKey& key) noexcept(false)
    -> void {
    try {
        if (!isValidKey(key)) {
            spdlog::error("Invalid key provided for XTEA encryption");
            throw TEAException("Invalid key for XTEA encryption");
        }

        u32 sum = 0;
        for (i32 i = 0; i < NUM_ROUNDS; ++i) {
            value0 += (((value1 << SHIFT_4) ^ (value1 >> SHIFT_5)) + value1) ^
                      (sum + key[sum & KEY_MASK]);
            sum += DELTA;
            value1 += (((value0 << SHIFT_4) ^ (value0 >> SHIFT_5)) + value0) ^
                      (sum + key[(sum >> SHIFT_11) & KEY_MASK]);
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("XTEA encryption error: {}", e.what());
        throw TEAException(std::string("XTEA encryption error: ") + e.what());
    }
}

// XTEA decryption function with enhanced security and validation
auto xteaDecrypt(u32& value0, u32& value1, const XTEAKey& key) noexcept(false)
    -> void {
    try {
        if (!isValidKey(key)) {
            spdlog::error("Invalid key provided for XTEA decryption");
            throw TEAException("Invalid key for XTEA decryption");
        }

        u32 sum = DELTA * NUM_ROUNDS;
        for (i32 i = 0; i < NUM_ROUNDS; ++i) {
            value1 -= (((value0 << SHIFT_4) ^ (value0 >> SHIFT_5)) + value0) ^
                      (sum + key[(sum >> SHIFT_11) & KEY_MASK]);
            sum -= DELTA;
            value0 -= (((value1 << SHIFT_4) ^ (value1 >> SHIFT_5)) + value1) ^
                      (sum + key[sum & KEY_MASK]);
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("XTEA decryption error: {}", e.what());
        throw TEAException(std::string("XTEA decryption error: ") + e.what());
    }
}

// Parallel processing function using thread pool for large data sets
auto xxteaEncryptParallelImpl(std::span<const u32> inputData,
                              std::span<const u32, 4> inputKey,
                              usize numThreads) -> std::vector<u32> {
    const usize dataSize = inputData.size();

    if (dataSize < 1024) {  // For small data sets, use single-threaded version
        return xxteaEncryptImpl(inputData, inputKey);
    }

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0)
            numThreads = 4;  // Default value
    }

    // Ensure each thread processes at least 512 elements to avoid overhead
    // exceeding benefits
    numThreads = std::min(numThreads, dataSize / 512 + 1);

    const usize blockSize = (dataSize + numThreads - 1) / numThreads;
    std::vector<std::future<std::vector<u32>>> futures;
    std::vector<u32> result(dataSize);

    spdlog::debug("Parallel XXTEA encryption started with {} threads",
                  numThreads);

    // Launch multiple threads to process blocks
    for (usize i = 0; i < numThreads; ++i) {
        usize startIdx = i * blockSize;
        usize endIdx = std::min(startIdx + blockSize, dataSize);

        if (startIdx >= dataSize)
            break;

        // Create a separate copy of data for each block to handle overlap
        // issues
        std::vector<u32> blockData(inputData.begin() + startIdx,
                                   inputData.begin() + endIdx);

        futures.push_back(std::async(
            std::launch::async, [blockData = std::move(blockData), inputKey]() {
                return xxteaEncryptImpl(blockData, inputKey);
            }));
    }

    // Collect results
    usize offset = 0;
    for (auto& future : futures) {
        auto blockResult = future.get();
        std::copy(blockResult.begin(), blockResult.end(),
                  result.begin() + offset);
        offset += blockResult.size();
    }

    spdlog::debug("Parallel XXTEA encryption completed successfully");
    return result;
}

auto xxteaDecryptParallelImpl(std::span<const u32> inputData,
                              std::span<const u32, 4> inputKey,
                              usize numThreads) -> std::vector<u32> {
    const usize dataSize = inputData.size();

    if (dataSize < 1024) {
        return xxteaDecryptImpl(inputData, inputKey);
    }

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0)
            numThreads = 4;
    }

    numThreads = std::min(numThreads, dataSize / 512 + 1);

    const usize blockSize = (dataSize + numThreads - 1) / numThreads;
    std::vector<std::future<std::vector<u32>>> futures;
    std::vector<u32> result(dataSize);

    spdlog::debug("Parallel XXTEA decryption started with {} threads",
                  numThreads);

    for (usize i = 0; i < numThreads; ++i) {
        usize startIdx = i * blockSize;
        usize endIdx = std::min(startIdx + blockSize, dataSize);

        if (startIdx >= dataSize)
            break;

        std::vector<u32> blockData(inputData.begin() + startIdx,
                                   inputData.begin() + endIdx);

        futures.push_back(std::async(
            std::launch::async, [blockData = std::move(blockData), inputKey]() {
                return xxteaDecryptImpl(blockData, inputKey);
            }));
    }

    usize offset = 0;
    for (auto& future : futures) {
        auto blockResult = future.get();
        std::copy(blockResult.begin(), blockResult.end(),
                  result.begin() + offset);
        offset += blockResult.size();
    }

    spdlog::debug("Parallel XXTEA decryption completed successfully");
    return result;
}

// Explicit template instantiations for common cases
template auto xxteaEncrypt<std::vector<u32>>(const std::vector<u32>& inputData,
                                             std::span<const u32, 4> inputKey)
    -> std::vector<u32>;

template auto xxteaDecrypt<std::vector<u32>>(const std::vector<u32>& inputData,
                                             std::span<const u32, 4> inputKey)
    -> std::vector<u32>;

template auto xxteaEncryptParallel<std::vector<u32>>(
    const std::vector<u32>& inputData, std::span<const u32, 4> inputKey,
    usize numThreads) -> std::vector<u32>;

template auto xxteaDecryptParallel<std::vector<u32>>(
    const std::vector<u32>& inputData, std::span<const u32, 4> inputKey,
    usize numThreads) -> std::vector<u32>;

template auto toUint32Vector<std::vector<u8>>(const std::vector<u8>& data)
    -> std::vector<u32>;

template auto toByteArray<std::vector<u32>>(const std::vector<u32>& data)
    -> std::vector<u8>;
}  // namespace atom::algorithm
