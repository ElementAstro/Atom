/*
 * mhash.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Implementation of murmur3 hash and quick hash

**************************************************/

#ifndef ATOM_ALGORITHM_MHASH_HPP
#define ATOM_ALGORITHM_MHASH_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if USE_OPENCL
#include <CL/cl.h>
#include <memory>
#endif

#include "atom/macro.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/thread/shared_mutex.hpp>
#endif

namespace atom::algorithm {

// 使用C++20 concepts定义可哈希类型
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

inline constexpr size_t K_HASH_SIZE = 32;

#ifdef ATOM_USE_BOOST
// Boost小型向量类型，适用于短哈希值存储，避免堆分配
template <typename T, size_t N>
using SmallVector = boost::container::small_vector<T, N>;

// 使用Boost的共享互斥锁类型
using SharedMutex = boost::shared_mutex;
using SharedLock = boost::shared_lock<SharedMutex>;
using UniqueLock = boost::unique_lock<SharedMutex>;
#else
// 标准库小型向量替代，使用PMR实现紧凑内存布局
template <typename T, size_t N>
using SmallVector = std::vector<T, std::pmr::polymorphic_allocator<T>>;

// 使用标准库的共享互斥锁类型
using SharedMutex = std::shared_mutex;
using SharedLock = std::shared_lock<SharedMutex>;
using UniqueLock = std::unique_lock<SharedMutex>;
#endif

/**
 * @brief Converts a string to a hexadecimal string representation.
 *
 * @param data The input string.
 * @return std::string The hexadecimal string representation.
 * @throws std::bad_alloc 如果内存分配失败
 */
ATOM_NODISCARD auto hexstringFromData(std::string_view data) noexcept(false)
    -> std::string;

/**
 * @brief Converts a hexadecimal string representation to binary data.
 *
 * @param data The input hexadecimal string.
 * @return std::string The binary data.
 * @throws std::invalid_argument If the input hexstring is not a valid
 * hexadecimal string.
 * @throws std::bad_alloc 如果内存分配失败
 */
ATOM_NODISCARD auto dataFromHexstring(std::string_view data) noexcept(false)
    -> std::string;

/**
 * @brief 检查字符串是否可以转换为十六进制
 *
 * @param str 待检查的字符串
 * @return bool 若可以转换为十六进制返回true，否则返回false
 */
[[nodiscard]] bool supportsHexStringConversion(std::string_view str) noexcept;

/**
 * @brief Implements the MinHash algorithm for estimating Jaccard similarity.
 *
 * The MinHash algorithm generates hash signatures for sets and estimates the
 * Jaccard index between sets based on these signatures.
 */
class MinHash {
public:
    /**
     * @brief Type definition for a hash function used in MinHash.
     */
    using HashFunction = std::function<size_t(size_t)>;

    /**
     * @brief Hash signature type using memory-efficient vector
     */
    using HashSignature = SmallVector<size_t, 64>;

    /**
     * @brief Constructs a MinHash object with a specified number of hash
     * functions.
     *
     * @param num_hashes The number of hash functions to use for MinHash.
     * @throws std::bad_alloc 如果内存分配失败
     * @throws std::invalid_argument 如果num_hashes为0
     */
    explicit MinHash(size_t num_hashes) noexcept(false);

    /**
     * @brief Destructor to clean up OpenCL resources.
     */
    ~MinHash() noexcept;

    /**
     * @brief Deleted copy constructor and assignment operator to prevent
     * copying.
     */
    MinHash(const MinHash&) = delete;
    MinHash& operator=(const MinHash&) = delete;

    /**
     * @brief Computes the MinHash signature (hash values) for a given set.
     *
     * @tparam Range Type of the range representing the set elements, must be a
     * range with hashable elements
     * @param set The set for which to compute the MinHash signature.
     * @return HashSignature MinHash signature (hash values) for the set.
     * @throws std::bad_alloc 如果内存分配失败
     */
    template <std::ranges::range Range>
        requires Hashable<std::ranges::range_value_t<Range>>
    [[nodiscard]] auto computeSignature(const Range& set) const noexcept(false)
        -> HashSignature {
        if (hash_functions_.empty()) {
            return {};
        }

        HashSignature signature(hash_functions_.size(),
                                std::numeric_limits<size_t>::max());
#if USE_OPENCL
        if (opencl_available_) {
            try {
                computeSignatureOpenCL(set, signature);
            } catch (...) {
                // 如果OpenCL执行失败，回退到CPU实现
                computeSignatureCPU(set, signature);
            }
        } else {
#endif
            computeSignatureCPU(set, signature);
#if USE_OPENCL
        }
#endif
        return signature;
    }

    /**
     * @brief Computes the Jaccard index between two sets based on their MinHash
     * signatures.
     *
     * @param sig1 MinHash signature of the first set.
     * @param sig2 MinHash signature of the second set.
     * @return double Estimated Jaccard index between the two sets.
     * @throws std::invalid_argument 如果签名长度不一致
     */
    [[nodiscard]] static auto jaccardIndex(
        std::span<const size_t> sig1,
        std::span<const size_t> sig2) noexcept(false) -> double;

    /**
     * @brief 获取哈希函数数量
     *
     * @return size_t 哈希函数数量
     */
    [[nodiscard]] size_t getHashFunctionCount() const noexcept {
        // 使用共享锁保护读取操作
        SharedLock lock(mutex_);
        return hash_functions_.size();
    }

    /**
     * @brief 检查是否支持OpenCL加速
     *
     * @return bool 是否支持OpenCL
     */
    [[nodiscard]] bool supportsOpenCL() const noexcept {
#if USE_OPENCL
        return opencl_available_.load(std::memory_order_acquire);
#else
        return false;
#endif
    }

private:
    /**
     * @brief Vector of hash functions used for MinHash.
     */
    std::vector<HashFunction> hash_functions_;

    /**
     * @brief 共享互斥锁保护哈希函数的并发访问
     */
    mutable SharedMutex mutex_;

    /**
     * @brief 线程本地存储缓存，提高性能
     */
    static inline thread_local std::vector<size_t> tls_buffer_{};

    /**
     * @brief Generates a hash function suitable for MinHash.
     *
     * @return HashFunction Generated hash function.
     */
    [[nodiscard]] static auto generateHashFunction() noexcept -> HashFunction;

    /**
     * @brief Computes signature using CPU implementation
     * @tparam Range Type of the range with hashable elements
     * @param set Input set
     * @param signature Output signature
     */
    template <std::ranges::range Range>
        requires Hashable<std::ranges::range_value_t<Range>>
    void computeSignatureCPU(const Range& set,
                             HashSignature& signature) const noexcept {
        using ValueType = std::ranges::range_value_t<Range>;

        // 获取共享读锁
        SharedLock lock(mutex_);

        // 优化1: 使用线程本地存储提前计算哈希值
        const auto setSize = std::ranges::distance(set);
        if (tls_buffer_.capacity() < setSize) {
            tls_buffer_.reserve(setSize);
        }
        tls_buffer_.clear();

        // 使用std::ranges进行遍历并预先计算哈希值
        for (const auto& element : set) {
            tls_buffer_.push_back(std::hash<ValueType>{}(element));
        }

        // 优化2: 循环展开以利用SIMD和指令级并行
        constexpr size_t UNROLL_FACTOR = 4;
        const size_t hash_count = hash_functions_.size();
        const size_t hash_count_aligned =
            hash_count - (hash_count % UNROLL_FACTOR);

        // 使用范围for循环遍历预计算的哈希值
        for (const auto element_hash : tls_buffer_) {
            // 主循环，每次处理UNROLL_FACTOR个哈希函数
            for (size_t i = 0; i < hash_count_aligned; i += UNROLL_FACTOR) {
                for (size_t j = 0; j < UNROLL_FACTOR; ++j) {
                    signature[i + j] = std::min(
                        signature[i + j], hash_functions_[i + j](element_hash));
                }
            }

            // 处理剩余的哈希函数
            for (size_t i = hash_count_aligned; i < hash_count; ++i) {
                signature[i] =
                    std::min(signature[i], hash_functions_[i](element_hash));
            }
        }
    }

#if USE_OPENCL
    /**
     * @brief OpenCL resources and state.
     */
    struct OpenCLResources {
        cl_context context{nullptr};
        cl_command_queue queue{nullptr};
        cl_program program{nullptr};
        cl_kernel minhash_kernel{nullptr};

        ~OpenCLResources() noexcept {
            if (minhash_kernel)
                clReleaseKernel(minhash_kernel);
            if (program)
                clReleaseProgram(program);
            if (queue)
                clReleaseCommandQueue(queue);
            if (context)
                clReleaseContext(context);
        }
    };

    std::unique_ptr<OpenCLResources> opencl_resources_;
    std::atomic<bool> opencl_available_{false};

    /**
     * @brief OpenCL内存缓冲区的RAII包装
     */
    class CLMemWrapper {
    public:
        CLMemWrapper(cl_context ctx, cl_mem_flags flags, size_t size,
                     void* host_ptr = nullptr)
            : context_(ctx), mem_(nullptr) {
            cl_int error;
            mem_ = clCreateBuffer(ctx, flags, size, host_ptr, &error);
            if (error != CL_SUCCESS) {
                throw std::runtime_error("Failed to create OpenCL buffer");
            }
        }

        ~CLMemWrapper() noexcept {
            if (mem_)
                clReleaseMemObject(mem_);
        }

        // 禁用拷贝
        CLMemWrapper(const CLMemWrapper&) = delete;
        CLMemWrapper& operator=(const CLMemWrapper&) = delete;

        // 启用移动
        CLMemWrapper(CLMemWrapper&& other) noexcept
            : context_(other.context_), mem_(other.mem_) {
            other.mem_ = nullptr;
        }

        CLMemWrapper& operator=(CLMemWrapper&& other) noexcept {
            if (this != &other) {
                if (mem_)
                    clReleaseMemObject(mem_);
                mem_ = other.mem_;
                context_ = other.context_;
                other.mem_ = nullptr;
            }
            return *this;
        }

        cl_mem get() const noexcept { return mem_; }
        operator cl_mem() const noexcept { return mem_; }

    private:
        cl_context context_;
        cl_mem mem_;
    };

    /**
     * @brief Initializes OpenCL context and resources.
     */
    void initializeOpenCL() noexcept;

    /**
     * @brief Computes the MinHash signature using OpenCL.
     *
     * @tparam Range Type of the range representing the set elements.
     * @param set The set for which to compute the MinHash signature.
     * @param signature The vector to store the computed signature.
     * @throws std::runtime_error 如果OpenCL操作失败
     */
    template <std::ranges::range Range>
        requires Hashable<std::ranges::range_value_t<Range>>
    void computeSignatureOpenCL(const Range& set,
                                HashSignature& signature) const {
        if (!opencl_available_.load(std::memory_order_acquire) ||
            !opencl_resources_) {
            throw std::runtime_error("OpenCL not available");
        }

        cl_int err;

        // 获取共享读锁
        SharedLock lock(mutex_);

        size_t numHashes = hash_functions_.size();
        size_t numElements = std::ranges::distance(set);

        if (numElements == 0) {
            return;  // 空集合，保持signature不变
        }

        using ValueType = std::ranges::range_value_t<Range>;

        // 优化: 使用线程本地存储预先计算哈希值
        if (tls_buffer_.capacity() < numElements) {
            tls_buffer_.reserve(numElements);
        }
        tls_buffer_.clear();

        // 使用C++20 ranges预先计算所有hash值
        for (const auto& element : set) {
            tls_buffer_.push_back(std::hash<ValueType>{}(element));
        }

        std::vector<size_t> aValues(numHashes);
        std::vector<size_t> bValues(numHashes);
        // 提取hash函数的参数
        for (size_t i = 0; i < numHashes; ++i) {
            // 实现a和b参数的提取逻辑
            aValues[i] = i + 1;      // 临时示例值
            bValues[i] = i * 2 + 1;  // 临时示例值
        }

        try {
            // 创建内存缓冲区
            CLMemWrapper hashesBuffer(opencl_resources_->context,
                                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                      numElements * sizeof(size_t),
                                      tls_buffer_.data());

            CLMemWrapper signatureBuffer(opencl_resources_->context,
                                         CL_MEM_WRITE_ONLY,
                                         numHashes * sizeof(size_t));

            CLMemWrapper aValuesBuffer(opencl_resources_->context,
                                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       numHashes * sizeof(size_t),
                                       aValues.data());

            CLMemWrapper bValuesBuffer(opencl_resources_->context,
                                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       numHashes * sizeof(size_t),
                                       bValues.data());

            size_t p = std::numeric_limits<size_t>::max();

            // 设置内核参数
            err = clSetKernelArg(opencl_resources_->minhash_kernel, 0,
                                 sizeof(cl_mem), &hashesBuffer.get());
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 0");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 1,
                                 sizeof(cl_mem), &signatureBuffer.get());
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 1");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 2,
                                 sizeof(cl_mem), &aValuesBuffer.get());
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 2");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 3,
                                 sizeof(cl_mem), &bValuesBuffer.get());
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 3");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 4,
                                 sizeof(size_t), &p);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 4");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 5,
                                 sizeof(size_t), &numHashes);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 5");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 6,
                                 sizeof(size_t), &numElements);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 6");

            // 优化: 使用多维工作组结构以提高并行度
            constexpr size_t WORK_GROUP_SIZE = 256;
            size_t globalWorkSize = (numHashes + WORK_GROUP_SIZE - 1) /
                                    WORK_GROUP_SIZE * WORK_GROUP_SIZE;

            err = clEnqueueNDRangeKernel(opencl_resources_->queue,
                                         opencl_resources_->minhash_kernel, 1,
                                         nullptr, &globalWorkSize,
                                         &WORK_GROUP_SIZE, 0, nullptr, nullptr);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to enqueue kernel");

            // 读取结果
            err = clEnqueueReadBuffer(opencl_resources_->queue,
                                      signatureBuffer.get(), CL_TRUE, 0,
                                      numHashes * sizeof(size_t),
                                      signature.data(), 0, nullptr, nullptr);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to read results");

        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("OpenCL error: ") + e.what());
        }
    }
#endif
};

/**
 * @brief Computes the Keccak-256 hash of the input data
 *
 * @param input Pointer to input data
 * @param length Length of input data
 * @return std::array<uint8_t, K_HASH_SIZE> The computed hash
 * @throws std::bad_alloc 如果内存分配失败
 */
[[nodiscard]] auto keccak256(std::span<const uint8_t> input) noexcept(false)
    -> std::array<uint8_t, K_HASH_SIZE>;

/**
 * @brief Computes the Keccak-256 hash of the input string
 *
 * @param input Input string
 * @return std::array<uint8_t, K_HASH_SIZE> The computed hash
 * @throws std::bad_alloc 如果内存分配失败
 */
[[nodiscard]] inline auto keccak256(std::string_view input) noexcept(false)
    -> std::array<uint8_t, K_HASH_SIZE> {
    return keccak256(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(input.data()), input.size()));
}

/**
 * @brief 哈希计算的上下文管理类
 *
 * 提供RAII式的哈希计算上下文管理，简化哈希计算过程
 */
class HashContext {
public:
    /**
     * @brief 构造一个新的哈希上下文
     */
    HashContext() noexcept;

    /**
     * @brief 析构函数，自动清理资源
     */
    ~HashContext() noexcept;

    /**
     * @brief 禁用拷贝操作
     */
    HashContext(const HashContext&) = delete;
    HashContext& operator=(const HashContext&) = delete;

    /**
     * @brief 启用移动操作
     */
    HashContext(HashContext&&) noexcept;
    HashContext& operator=(HashContext&&) noexcept;

    /**
     * @brief 更新哈希计算的数据
     *
     * @param data 数据指针
     * @param length 数据长度
     * @return bool 操作是否成功
     */
    bool update(const void* data, size_t length) noexcept;

    /**
     * @brief 使用字符串视图更新哈希计算的数据
     *
     * @param data 输入字符串
     * @return bool 操作是否成功
     */
    bool update(std::string_view data) noexcept;

    /**
     * @brief 使用span更新哈希计算的数据
     *
     * @param data 输入数据span
     * @return bool 操作是否成功
     */
    bool update(std::span<const std::byte> data) noexcept;

    /**
     * @brief 完成哈希计算并获取结果
     *
     * @return std::optional<std::array<uint8_t, K_HASH_SIZE>>
     * 哈希结果，失败时返回std::nullopt
     */
    [[nodiscard]] std::optional<std::array<uint8_t, K_HASH_SIZE>>
    finalize() noexcept;

private:
    struct ContextImpl;
    std::unique_ptr<ContextImpl> impl_;
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MHASH_HPP
