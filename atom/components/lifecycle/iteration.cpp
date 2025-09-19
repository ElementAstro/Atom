/*
 * iteration.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "iteration.hpp"

#include <algorithm>
#include <cstring>
#include <thread>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace atom::components {

// ComponentBatchProcessor implementation
ComponentBatchProcessor::ComponentBatchProcessor() : config_({}) {
    if (config_.numThreads == 0) {
        config_.numThreads = std::thread::hardware_concurrency();
    }
}

ComponentBatchProcessor::ComponentBatchProcessor(const Config& config)
    : config_(config) {
    if (config_.numThreads == 0) {
        config_.numThreads = std::thread::hardware_concurrency();
    }
}

template <typename ComponentType, typename Func>
void ComponentBatchProcessor::processBatches(ComponentType* components,
                                             size_t count, Func&& func) {
    const auto startTime = std::chrono::high_resolution_clock::now();

    const size_t batchSize = config_.batchSize;
    const size_t numBatches = (count + batchSize - 1) / batchSize;

    statistics_.totalBatches += numBatches;
    statistics_.totalComponents += count;

    if (config_.numThreads > 1 && numBatches > 1) {
        // Parallel processing
        std::vector<std::thread> threads;
        const size_t batchesPerThread =
            (numBatches + config_.numThreads - 1) / config_.numThreads;

        for (size_t threadId = 0; threadId < config_.numThreads; ++threadId) {
            threads.emplace_back([&, threadId]() {
                const size_t startBatch = threadId * batchesPerThread;
                const size_t endBatch =
                    std::min(startBatch + batchesPerThread, numBatches);

                for (size_t batchIdx = startBatch; batchIdx < endBatch;
                     ++batchIdx) {
                    const size_t startIdx = batchIdx * batchSize;
                    const size_t endIdx = std::min(startIdx + batchSize, count);
                    const size_t currentBatchSize = endIdx - startIdx;

                    if (config_.enablePrefetch && batchIdx + 1 < endBatch) {
                        // Prefetch next batch
                        const size_t nextStartIdx = (batchIdx + 1) * batchSize;
                        if (nextStartIdx < count) {
                            for (size_t prefetchIdx = 0;
                                 prefetchIdx < config_.prefetchDistance;
                                 ++prefetchIdx) {
                                const size_t prefetchAddr =
                                    nextStartIdx + prefetchIdx;
                                if (prefetchAddr < count) {
#ifdef __builtin_prefetch
                                    __builtin_prefetch(
                                        &components[prefetchAddr], 0, 3);
#endif
                                }
                            }
                        }
                    }

                    func(&components[startIdx], currentBatchSize);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    } else {
        // Sequential processing
        for (size_t batchIdx = 0; batchIdx < numBatches; ++batchIdx) {
            const size_t startIdx = batchIdx * batchSize;
            const size_t endIdx = std::min(startIdx + batchSize, count);
            const size_t currentBatchSize = endIdx - startIdx;

            if (config_.enablePrefetch && batchIdx + 1 < numBatches) {
                // Prefetch next batch
                const size_t nextStartIdx = (batchIdx + 1) * batchSize;
                if (nextStartIdx < count) {
                    for (size_t prefetchIdx = 0;
                         prefetchIdx < config_.prefetchDistance;
                         ++prefetchIdx) {
                        const size_t prefetchAddr = nextStartIdx + prefetchIdx;
                        if (prefetchAddr < count) {
#ifdef __builtin_prefetch
                            __builtin_prefetch(&components[prefetchAddr], 0, 3);
#endif
                        }
                    }
                }
            }

            func(&components[startIdx], currentBatchSize);
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);

    statistics_.totalProcessingTime += duration;
    if (statistics_.totalBatches > 0) {
        statistics_.avgBatchTime = std::chrono::microseconds{
            statistics_.totalProcessingTime.count() / statistics_.totalBatches};
    }
}

template <typename T>
typename std::enable_if_t<is_simd_compatible_v<T>, void>
ComponentBatchProcessor::simdBatchUpdate(T* data, size_t count,
                                         std::function<T(T)> updateFunc) {
    if (!config_.enableSIMD) {
        // Fallback to scalar processing
        for (size_t i = 0; i < count; ++i) {
            data[i] = updateFunc(data[i]);
        }
        return;
    }

#ifdef __AVX2__
    if constexpr (sizeof(T) == 4) {  // float or int32_t
        const size_t simdWidth = 8;  // AVX2 processes 8 floats at once
        const size_t simdCount = count - (count % simdWidth);

        // Process SIMD-aligned data
        for (size_t i = 0; i < simdCount; i += simdWidth) {
            // Load 8 elements
            if constexpr (std::is_same_v<T, float>) {
                __m256 vec = _mm256_load_ps(&data[i]);

                // Apply function element-wise (simplified - would need
                // vectorized function)
                alignas(32) float temp[8];
                _mm256_store_ps(temp, vec);

                for (int j = 0; j < 8; ++j) {
                    temp[j] = updateFunc(temp[j]);
                }

                vec = _mm256_load_ps(temp);
                _mm256_store_ps(&data[i], vec);
            }
        }

        // Process remaining elements
        for (size_t i = simdCount; i < count; ++i) {
            data[i] = updateFunc(data[i]);
        }

        statistics_.simdUtilization = static_cast<double>(simdCount) / count;
    } else
#endif
    {
        // Fallback to scalar processing
        for (size_t i = 0; i < count; ++i) {
            data[i] = updateFunc(data[i]);
        }
        statistics_.simdUtilization = 0.0;
    }
}

void ComponentBatchProcessor::resetStatistics() noexcept {
    statistics_ = Statistics{};
}

template <typename T>
void ComponentBatchProcessor::simdAdd(T* a, const T* b, size_t count) {
#ifdef __AVX2__
    if constexpr (sizeof(T) == 4) {  // float or int32_t
        const size_t simdWidth = 8;
        const size_t simdCount = count - (count % simdWidth);

        for (size_t i = 0; i < simdCount; i += simdWidth) {
            if constexpr (std::is_same_v<T, float>) {
                __m256 vecA = _mm256_load_ps(&a[i]);
                __m256 vecB = _mm256_load_ps(&b[i]);
                __m256 result = _mm256_add_ps(vecA, vecB);
                _mm256_store_ps(&a[i], result);
            }
        }

        // Process remaining elements
        for (size_t i = simdCount; i < count; ++i) {
            a[i] += b[i];
        }
    } else
#endif
    {
        // Scalar fallback
        for (size_t i = 0; i < count; ++i) {
            a[i] += b[i];
        }
    }
}

template <typename T>
void ComponentBatchProcessor::simdMultiply(T* a, const T* b, size_t count) {
#ifdef __AVX2__
    if constexpr (sizeof(T) == 4) {  // float or int32_t
        const size_t simdWidth = 8;
        const size_t simdCount = count - (count % simdWidth);

        for (size_t i = 0; i < simdCount; i += simdWidth) {
            if constexpr (std::is_same_v<T, float>) {
                __m256 vecA = _mm256_load_ps(&a[i]);
                __m256 vecB = _mm256_load_ps(&b[i]);
                __m256 result = _mm256_mul_ps(vecA, vecB);
                _mm256_store_ps(&a[i], result);
            }
        }

        // Process remaining elements
        for (size_t i = simdCount; i < count; ++i) {
            a[i] *= b[i];
        }
    } else
#endif
    {
        // Scalar fallback
        for (size_t i = 0; i < count; ++i) {
            a[i] *= b[i];
        }
    }
}

template <typename T>
void ComponentBatchProcessor::simdApplyFunction(T* data, size_t count,
                                                std::function<T(T)> func) {
    // For now, use scalar processing as vectorizing arbitrary functions is
    // complex
    for (size_t i = 0; i < count; ++i) {
        data[i] = func(data[i]);
    }
}

// CacheOptimizedIterator implementation
template <typename ComponentType>
CacheOptimizedIterator<ComponentType>::CacheOptimizedIterator(
    pointer ptr, size_t prefetchDistance)
    : ptr_(ptr), prefetchDistance_(prefetchDistance) {
    prefetch();
}

template <typename ComponentType>
typename CacheOptimizedIterator<ComponentType>::reference
CacheOptimizedIterator<ComponentType>::operator*() const {
    return *ptr_;
}

template <typename ComponentType>
typename CacheOptimizedIterator<ComponentType>::pointer
CacheOptimizedIterator<ComponentType>::operator->() const {
    return ptr_;
}

template <typename ComponentType>
CacheOptimizedIterator<ComponentType>&
CacheOptimizedIterator<ComponentType>::operator++() {
    ++ptr_;
    prefetch();
    return *this;
}

template <typename ComponentType>
void CacheOptimizedIterator<ComponentType>::prefetch() const {
#ifdef __builtin_prefetch
    // Prefetch future cache lines
    for (size_t i = 1; i <= prefetchDistance_; ++i) {
        const void* prefetchAddr =
            reinterpret_cast<const char*>(ptr_) + (i * CACHE_LINE_SIZE);
        __builtin_prefetch(prefetchAddr, 0, 3);  // Read, high temporal locality
    }
#endif
}

// Explicit template instantiations for common types
template class CacheOptimizedIterator<Component>;
template void ComponentBatchProcessor::processBatches<
    Component, std::function<void(Component*, size_t)>>(
    Component*, size_t, std::function<void(Component*, size_t)>&&);
template void ComponentBatchProcessor::simdBatchUpdate<float>(
    float*, size_t, std::function<float(float)>);
template void ComponentBatchProcessor::simdAdd<float>(float*, const float*,
                                                      size_t);
template void ComponentBatchProcessor::simdMultiply<float>(float*, const float*,
                                                           size_t);

}  // namespace atom::components
