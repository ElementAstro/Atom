#include "tea.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <future>
#include <span>
#include <thread>
#include <vector>

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
constexpr std::size_t hardware_destructive_interference_size = 64;
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
constexpr uint32_t DELTA = 0x9E3779B9;
constexpr int NUM_ROUNDS = 32;
constexpr int SHIFT_4 = 4;
constexpr int SHIFT_5 = 5;
constexpr int BYTE_SHIFT = 8;
constexpr size_t MIN_ROUNDS = 6;
constexpr size_t MAX_ROUNDS = 52;
constexpr int SHIFT_3 = 3;
constexpr int SHIFT_2 = 2;
constexpr uint32_t KEY_MASK = 3;
constexpr int SHIFT_11 = 11;

// Helper function to validate key
static inline bool isValidKey(const std::array<uint32_t, 4>& key) noexcept {
    // 检查密钥是否全为0，这通常是不安全的
    return !(key[0] == 0 && key[1] == 0 && key[2] == 0 && key[3] == 0);
}

// TEA encryption function
auto teaEncrypt(uint32_t& value0, uint32_t& value1,
                const std::array<uint32_t, 4>& key) noexcept(false) -> void {
    try {
        if (!isValidKey(key)) {
            throw TEAException("Invalid key for TEA encryption");
        }

        uint32_t sum = 0;
        for (int i = 0; i < NUM_ROUNDS; ++i) {
            sum += DELTA;
            value0 += ((value1 << SHIFT_4) + key[0]) ^ (value1 + sum) ^
                      ((value1 >> SHIFT_5) + key[1]);
            value1 += ((value0 << SHIFT_4) + key[2]) ^ (value0 + sum) ^
                      ((value0 >> SHIFT_5) + key[3]);
        }
    } catch (const TEAException&) {
        throw;  // 重新抛出 TEA 特定异常
    } catch (const std::exception& e) {
        throw TEAException(std::string("TEA encryption error: ") + e.what());
    }
}

// TEA decryption function
auto teaDecrypt(uint32_t& value0, uint32_t& value1,
                const std::array<uint32_t, 4>& key) noexcept(false) -> void {
    try {
        if (!isValidKey(key)) {
            throw TEAException("Invalid key for TEA decryption");
        }

        uint32_t sum = DELTA * NUM_ROUNDS;
        for (int i = 0; i < NUM_ROUNDS; ++i) {
            value1 -= ((value0 << SHIFT_4) + key[2]) ^ (value0 + sum) ^
                      ((value0 >> SHIFT_5) + key[3]);
            value0 -= ((value1 << SHIFT_4) + key[0]) ^ (value1 + sum) ^
                      ((value1 >> SHIFT_5) + key[1]);
            sum -= DELTA;
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        throw TEAException(std::string("TEA decryption error: ") + e.what());
    }
}

// 优化的字节转换函数，使用编译时条件分支
static inline uint32_t byteToNative(uint8_t byte, int position) noexcept {
    uint32_t value = static_cast<uint32_t>(byte) << (position * BYTE_SHIFT);
#ifdef ATOM_USE_BOOST
    if constexpr (std::endian::native != std::endian::little) {
        return boost::endian::little_to_native(value);
    }
#endif
    return value;
}

static inline uint8_t nativeToByte(uint32_t value, int position) noexcept {
#ifdef ATOM_USE_BOOST
    if constexpr (std::endian::native != std::endian::little) {
        value = boost::endian::native_to_little(value);
    }
#endif
    return static_cast<uint8_t>(value >> (position * BYTE_SHIFT));
}

// 实现 toUint32Vector 和 toByteArray 的非模板版本用于内部使用
auto toUint32VectorImpl(std::span<const uint8_t> data)
    -> std::vector<uint32_t> {
    size_t numElements = (data.size() + 3) / 4;
    std::vector<uint32_t> result(numElements, 0);

    for (size_t index = 0; index < data.size(); ++index) {
        result[index / 4] |= byteToNative(data[index], index % 4);
    }

    return result;
}

auto toByteArrayImpl(std::span<const uint32_t> data) -> std::vector<uint8_t> {
    std::vector<uint8_t> result(data.size() * 4);

    for (size_t index = 0; index < data.size(); ++index) {
        for (int bytePos = 0; bytePos < 4; ++bytePos) {
            result[index * 4 + bytePos] = nativeToByte(data[index], bytePos);
        }
    }

    return result;
}

// XXTEA functions with optimized implementations
namespace detail {
constexpr uint32_t MX(uint32_t sum, uint32_t y, uint32_t z, int p, uint32_t e,
                      const uint32_t* k) noexcept {
    return ((z >> SHIFT_5 ^ y << SHIFT_2) + (y >> SHIFT_3 ^ z << SHIFT_4)) ^
           ((sum ^ y) + (k[(p & 3) ^ e] ^ z));
}
}  // namespace detail

// XXTEA 加密实现（非模板版本）
auto xxteaEncryptImpl(std::span<const uint32_t> inputData,
                      std::span<const uint32_t, 4> inputKey)
    -> std::vector<uint32_t> {
    if (inputData.empty()) {
        throw TEAException("Empty data provided for XXTEA encryption");
    }

    size_t numElements = inputData.size();
    if (numElements < 2) {
        return {inputData.begin(), inputData.end()};  // 返回副本
    }

    std::vector<uint32_t> result(inputData.begin(), inputData.end());

    uint32_t sum = 0;
    uint32_t lastElement = result[numElements - 1];
    size_t numRounds = MIN_ROUNDS + MAX_ROUNDS / numElements;

    try {
        for (size_t roundIndex = 0; roundIndex < numRounds; ++roundIndex) {
            sum += DELTA;
            uint32_t keyIndex = (sum >> SHIFT_2) & KEY_MASK;

            for (size_t elementIndex = 0; elementIndex < numElements - 1;
                 ++elementIndex) {
                uint32_t currentElement = result[elementIndex + 1];
                result[elementIndex] +=
                    detail::MX(sum, currentElement, lastElement, elementIndex,
                               keyIndex, inputKey.data());
                lastElement = result[elementIndex];
            }

            uint32_t currentElement = result[0];
            result[numElements - 1] +=
                detail::MX(sum, currentElement, lastElement, numElements - 1,
                           keyIndex, inputKey.data());
            lastElement = result[numElements - 1];
        }
    } catch (const std::exception& e) {
        throw TEAException(std::string("XXTEA encryption error: ") + e.what());
    }

    return result;
}

// XXTEA 解密实现（非模板版本）
auto xxteaDecryptImpl(std::span<const uint32_t> inputData,
                      std::span<const uint32_t, 4> inputKey)
    -> std::vector<uint32_t> {
    if (inputData.empty()) {
        throw TEAException("Empty data provided for XXTEA decryption");
    }

    size_t numElements = inputData.size();
    if (numElements < 2) {
        return {inputData.begin(), inputData.end()};
    }

    std::vector<uint32_t> result(inputData.begin(), inputData.end());
    size_t numRounds = MIN_ROUNDS + MAX_ROUNDS / numElements;
    uint32_t sum = numRounds * DELTA;

    try {
        for (size_t roundIndex = 0; roundIndex < numRounds; ++roundIndex) {
            uint32_t keyIndex = (sum >> SHIFT_2) & KEY_MASK;
            uint32_t currentElement = result[0];

            for (size_t elementIndex = numElements - 1; elementIndex > 0;
                 --elementIndex) {
                uint32_t lastElement = result[elementIndex - 1];
                result[elementIndex] -=
                    detail::MX(sum, currentElement, lastElement, elementIndex,
                               keyIndex, inputKey.data());
                currentElement = result[elementIndex];
            }

            uint32_t lastElement = result[numElements - 1];
            result[0] -= detail::MX(sum, currentElement, lastElement, 0,
                                    keyIndex, inputKey.data());
            currentElement = result[0];
            sum -= DELTA;
        }
    } catch (const std::exception& e) {
        throw TEAException(std::string("XXTEA decryption error: ") + e.what());
    }

    return result;
}

// XTEA encryption function with enhanced security and validation
auto xteaEncrypt(uint32_t& value0, uint32_t& value1,
                 const XTEAKey& key) noexcept(false) -> void {
    try {
        if (!isValidKey(key)) {
            throw TEAException("Invalid key for XTEA encryption");
        }

        uint32_t sum = 0;
        for (int i = 0; i < NUM_ROUNDS; ++i) {
            value0 += ((value1 << SHIFT_4) ^ (value1 >> SHIFT_5)) + value1 ^
                      (sum + key[sum & KEY_MASK]);
            sum += DELTA;
            value1 += ((value0 << SHIFT_4) ^ (value0 >> SHIFT_5)) + value0 ^
                      (sum + key[(sum >> SHIFT_11) & KEY_MASK]);
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        throw TEAException(std::string("XTEA encryption error: ") + e.what());
    }
}

// XTEA decryption function with enhanced security and validation
auto xteaDecrypt(uint32_t& value0, uint32_t& value1,
                 const XTEAKey& key) noexcept(false) -> void {
    try {
        if (!isValidKey(key)) {
            throw TEAException("Invalid key for XTEA decryption");
        }

        uint32_t sum = DELTA * NUM_ROUNDS;
        for (int i = 0; i < NUM_ROUNDS; ++i) {
            value1 -= ((value0 << SHIFT_4) ^ (value0 >> SHIFT_5)) + value0 ^
                      (sum + key[(sum >> SHIFT_11) & KEY_MASK]);
            sum -= DELTA;
            value0 -= ((value1 << SHIFT_4) ^ (value1 >> SHIFT_5)) + value1 ^
                      (sum + key[sum & KEY_MASK]);
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        throw TEAException(std::string("XTEA decryption error: ") + e.what());
    }
}

// 并行处理函数，使用线程池并分块处理大型数据
auto xxteaEncryptParallelImpl(std::span<const uint32_t> inputData,
                              std::span<const uint32_t, 4> inputKey,
                              size_t numThreads) -> std::vector<uint32_t> {
    const size_t dataSize = inputData.size();

    if (dataSize < 1024) {  // 数据量小时，使用单线程版本
        return xxteaEncryptImpl(inputData, inputKey);
    }

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0)
            numThreads = 4;  // 默认值
    }

    // 确保至少每个线程处理512个元素，避免线程开销超过收益
    numThreads = std::min(numThreads, dataSize / 512 + 1);

    const size_t blockSize = (dataSize + numThreads - 1) / numThreads;
    std::vector<std::future<std::vector<uint32_t>>> futures;
    std::vector<uint32_t> result(dataSize);

    // 启动多个线程分块处理
    for (size_t i = 0; i < numThreads; ++i) {
        size_t startIdx = i * blockSize;
        size_t endIdx = std::min(startIdx + blockSize, dataSize);

        if (startIdx >= dataSize)
            break;

        // 为每个块创建一个单独的数据副本以处理覆盖问题
        std::vector<uint32_t> blockData(inputData.begin() + startIdx,
                                        inputData.begin() + endIdx);

        futures.push_back(std::async(
            std::launch::async, [blockData = std::move(blockData), inputKey]() {
                return xxteaEncryptImpl(blockData, inputKey);
            }));
    }

    // 收集结果
    size_t offset = 0;
    for (auto& future : futures) {
        auto blockResult = future.get();
        std::copy(blockResult.begin(), blockResult.end(),
                  result.begin() + offset);
        offset += blockResult.size();
    }

    return result;
}

auto xxteaDecryptParallelImpl(std::span<const uint32_t> inputData,
                              std::span<const uint32_t, 4> inputKey,
                              size_t numThreads) -> std::vector<uint32_t> {
    const size_t dataSize = inputData.size();

    if (dataSize < 1024) {
        return xxteaDecryptImpl(inputData, inputKey);
    }

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0)
            numThreads = 4;
    }

    numThreads = std::min(numThreads, dataSize / 512 + 1);

    const size_t blockSize = (dataSize + numThreads - 1) / numThreads;
    std::vector<std::future<std::vector<uint32_t>>> futures;
    std::vector<uint32_t> result(dataSize);

    for (size_t i = 0; i < numThreads; ++i) {
        size_t startIdx = i * blockSize;
        size_t endIdx = std::min(startIdx + blockSize, dataSize);

        if (startIdx >= dataSize)
            break;

        std::vector<uint32_t> blockData(inputData.begin() + startIdx,
                                        inputData.begin() + endIdx);

        futures.push_back(std::async(
            std::launch::async, [blockData = std::move(blockData), inputKey]() {
                return xxteaDecryptImpl(blockData, inputKey);
            }));
    }

    size_t offset = 0;
    for (auto& future : futures) {
        auto blockResult = future.get();
        std::copy(blockResult.begin(), blockResult.end(),
                  result.begin() + offset);
        offset += blockResult.size();
    }

    return result;
}

// 显式实例化常用模板
template auto xxteaEncrypt<std::vector<uint32_t>>(
    const std::vector<uint32_t>& inputData,
    std::span<const uint32_t, 4> inputKey) -> std::vector<uint32_t>;

template auto xxteaDecrypt<std::vector<uint32_t>>(
    const std::vector<uint32_t>& inputData,
    std::span<const uint32_t, 4> inputKey) -> std::vector<uint32_t>;

template auto xxteaEncryptParallel<std::vector<uint32_t>>(
    const std::vector<uint32_t>& inputData,
    std::span<const uint32_t, 4> inputKey,
    size_t numThreads) -> std::vector<uint32_t>;

template auto xxteaDecryptParallel<std::vector<uint32_t>>(
    const std::vector<uint32_t>& inputData,
    std::span<const uint32_t, 4> inputKey,
    size_t numThreads) -> std::vector<uint32_t>;

template auto toUint32Vector<std::vector<uint8_t>>(
    const std::vector<uint8_t>& data) -> std::vector<uint32_t>;

template auto toByteArray<std::vector<uint32_t>>(
    const std::vector<uint32_t>& data) -> std::vector<uint8_t>;
}  // namespace atom::algorithm