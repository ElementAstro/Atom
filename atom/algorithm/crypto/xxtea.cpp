#include "xxtea.hpp"

#include <algorithm>
#include <future>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "atom/algorithm/common/parallel.hpp"

#ifdef __cpp_lib_hardware_interference_size
#ifdef __has_include
#if __has_include(<new>)
#include <new>
using std::hardware_destructive_interference_size;
#else
constexpr usize hardware_destructive_interference_size = 64;
#endif
#else
constexpr usize hardware_destructive_interference_size = 64;
#endif
#else
constexpr usize hardware_destructive_interference_size = 64;
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace atom::algorithm {

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
            sum += TEA_DELTA;
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
    u32 sum = numRounds * TEA_DELTA;

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
            sum -= TEA_DELTA;
        }
    } catch (const std::exception& e) {
        spdlog::error("XXTEA decryption error: {}", e.what());
        throw TEAException(std::string("XXTEA decryption error: ") + e.what());
    }

    return result;
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

}  // namespace atom::algorithm
