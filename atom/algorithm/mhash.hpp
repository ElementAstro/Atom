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
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if USE_OPENCL
#include <CL/cl.h>
#include <memory>
#endif

#include "atom/macro.hpp"

namespace atom::algorithm {

// 使用C++20 concepts定义可哈希类型
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

constexpr size_t K_HASH_SIZE = 32;

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
     * @brief Constructs a MinHash object with a specified number of hash
     * functions.
     *
     * @param num_hashes The number of hash functions to use for MinHash.
     * @throws std::bad_alloc 如果内存分配失败
     */
    explicit MinHash(size_t num_hashes) noexcept(false);

    /**
     * @brief Destructor to clean up OpenCL resources.
     */
    ~MinHash() noexcept;

    // 禁用拷贝构造和赋值
    MinHash(const MinHash&) = delete;
    MinHash& operator=(const MinHash&) = delete;

    // 允许移动语义
    MinHash(MinHash&&) noexcept = default;
    MinHash& operator=(MinHash&&) noexcept = default;

    /**
     * @brief Computes the MinHash signature (hash values) for a given set.
     *
     * @tparam Range Type of the range representing the set elements, must be a
     * range with hashable elements
     * @param set The set for which to compute the MinHash signature.
     * @return std::vector<size_t> MinHash signature (hash values) for the set.
     * @throws std::bad_alloc 如果内存分配失败
     */
    template <std::ranges::range Range>
        requires Hashable<std::ranges::range_value_t<Range>>
    ATOM_NODISCARD auto computeSignature(const Range& set) const
        noexcept(false) -> std::vector<size_t> {
        if (hash_functions_.empty()) {
            return {};
        }

        std::vector<size_t> signature(hash_functions_.size(),
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
    ATOM_NODISCARD static auto jaccardIndex(
        std::span<const size_t> sig1,
        std::span<const size_t> sig2) noexcept(false) -> double;

private:
    /**
     * @brief Vector of hash functions used for MinHash.
     */
    std::vector<HashFunction> hash_functions_;

    /**
     * @brief Generates a hash function suitable for MinHash.
     *
     * @return HashFunction Generated hash function.
     */
    ATOM_NODISCARD static auto generateHashFunction() noexcept -> HashFunction;

    /**
     * @brief Computes signature using CPU implementation
     * @tparam Range Type of the range with hashable elements
     * @param set Input set
     * @param signature Output signature
     */
    template <std::ranges::range Range>
        requires Hashable<std::ranges::range_value_t<Range>>
    void computeSignatureCPU(const Range& set,
                             std::vector<size_t>& signature) const noexcept {
        using ValueType = std::ranges::range_value_t<Range>;

        // 使用std::ranges进行遍历
        for (const auto& element : set) {
            size_t elementHash = std::hash<ValueType>{}(element);

            // 使用SIMD-friendly算法，可以被编译器自动向量化
            for (size_t i = 0; i < hash_functions_.size(); ++i) {
                signature[i] =
                    std::min(signature[i], hash_functions_[i](elementHash));
            }
        }
    }

#if USE_OPENCL
    /**
     * @brief OpenCL resources and state.
     */
    struct OpenCLResources {
        cl_context context;
        cl_command_queue queue;
        cl_program program;
        cl_kernel minhash_kernel;

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
    bool opencl_available_;

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
                                std::vector<size_t>& signature) const {
        if (!opencl_available_ || !opencl_resources_) {
            throw std::runtime_error("OpenCL not available");
        }

        cl_int err;
        size_t numHashes = hash_functions_.size();
        size_t numElements = std::ranges::distance(set);

        if (numElements == 0) {
            return;  // 空集合，保持signature不变
        }

        using ValueType = std::ranges::range_value_t<Range>;
        std::vector<size_t> hashes;
        hashes.reserve(numElements);

        // 使用C++20 ranges预先计算所有hash值
        for (const auto& element : set) {
            hashes.push_back(std::hash<ValueType>{}(element));
        }

        std::vector<size_t> aValues(numHashes);
        std::vector<size_t> bValues(numHashes);
        // 提取hash函数的参数
        for (size_t i = 0; i < numHashes; ++i) {
            // 实现a和b参数的提取逻辑
            aValues[i] = i + 1;      // 临时示例值
            bValues[i] = i * 2 + 1;  // 临时示例值
        }

        // 使用RAII包装的OpenCL内存缓冲区
        struct CLMemWrapper {
            cl_mem mem;
            cl_context context;

            CLMemWrapper(cl_context ctx, cl_mem_flags flags, size_t size,
                         void* host_ptr = nullptr)
                : context(ctx) {
                cl_int error;
                mem = clCreateBuffer(ctx, flags, size, host_ptr, &error);
                if (error != CL_SUCCESS) {
                    throw std::runtime_error("Failed to create OpenCL buffer");
                }
            }

            ~CLMemWrapper() {
                if (mem)
                    clReleaseMemObject(mem);
            }
        };

        try {
            // 创建内存缓冲区
            CLMemWrapper hashesBuffer(opencl_resources_->context,
                                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                      numElements * sizeof(size_t),
                                      hashes.data());

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
                                 sizeof(cl_mem), &hashesBuffer.mem);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 0");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 1,
                                 sizeof(cl_mem), &signatureBuffer.mem);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 1");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 2,
                                 sizeof(cl_mem), &aValuesBuffer.mem);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to set kernel arg 2");

            err = clSetKernelArg(opencl_resources_->minhash_kernel, 3,
                                 sizeof(cl_mem), &bValuesBuffer.mem);
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

            // 执行内核
            size_t globalWorkSize = numHashes;
            err = clEnqueueNDRangeKernel(
                opencl_resources_->queue, opencl_resources_->minhash_kernel, 1,
                nullptr, &globalWorkSize, nullptr, 0, nullptr, nullptr);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to enqueue kernel");

            // 读取结果
            err = clEnqueueReadBuffer(opencl_resources_->queue,
                                      signatureBuffer.mem, CL_TRUE, 0,
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
ATOM_NODISCARD auto keccak256(const uint8_t* input, size_t length) noexcept(
    false) -> std::array<uint8_t, K_HASH_SIZE>;

/**
 * @brief Computes the Keccak-256 hash of the input string
 *
 * @param input Input string
 * @return std::array<uint8_t, K_HASH_SIZE> The computed hash
 * @throws std::bad_alloc 如果内存分配失败
 */
ATOM_NODISCARD inline auto keccak256(std::string_view input) noexcept(false)
    -> std::array<uint8_t, K_HASH_SIZE> {
    return keccak256(reinterpret_cast<const uint8_t*>(input.data()),
                     input.size());
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MHASH_HPP
