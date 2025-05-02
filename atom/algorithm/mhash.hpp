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

// Use C++20 concepts to define hashable types
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

inline constexpr size_t K_HASH_SIZE = 32;

#ifdef ATOM_USE_BOOST
// Boost small_vector type, suitable for short hash value storage, avoids heap
// allocation
template <typename T, size_t N>
using SmallVector = boost::container::small_vector<T, N>;

// Use Boost's shared mutex type
using SharedMutex = boost::shared_mutex;
using SharedLock = boost::shared_lock<SharedMutex>;
using UniqueLock = boost::unique_lock<SharedMutex>;
#else
// Standard library small_vector alternative, uses PMR for compact memory layout
template <typename T, size_t N>
using SmallVector = std::vector<T, std::pmr::polymorphic_allocator<T>>;

// Use standard library's shared mutex type
using SharedMutex = std::shared_mutex;
using SharedLock = std::shared_lock<SharedMutex>;
using UniqueLock = std::unique_lock<SharedMutex>;
#endif

/**
 * @brief Converts a string to a hexadecimal string representation.
 *
 * @param data The input string.
 * @return std::string The hexadecimal string representation.
 * @throws std::bad_alloc If memory allocation fails
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
 * @throws std::bad_alloc If memory allocation fails
 */
ATOM_NODISCARD auto dataFromHexstring(std::string_view data) noexcept(false)
    -> std::string;

/**
 * @brief Checks if a string can be converted to hexadecimal.
 *
 * @param str The string to check.
 * @return bool True if convertible to hexadecimal, false otherwise.
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
     * @throws std::bad_alloc If memory allocation fails
     * @throws std::invalid_argument If num_hashes is 0
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
     * @throws std::bad_alloc If memory allocation fails
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
                // If OpenCL execution fails, fall back to CPU implementation
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
     * @throws std::invalid_argument If signature lengths do not match
     */
    [[nodiscard]] static auto jaccardIndex(
        std::span<const size_t> sig1,
        std::span<const size_t> sig2) noexcept(false) -> double;

    /**
     * @brief Gets the number of hash functions.
     *
     * @return size_t The number of hash functions.
     */
    [[nodiscard]] size_t getHashFunctionCount() const noexcept {
        // Use shared lock to protect read operations
        SharedLock lock(mutex_);
        return hash_functions_.size();
    }

    /**
     * @brief Checks if OpenCL acceleration is supported.
     *
     * @return bool True if OpenCL is supported, false otherwise.
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
     * @brief Shared mutex to protect concurrent access to hash functions.
     */
    mutable SharedMutex mutex_;

    /**
     * @brief Thread-local storage buffer for performance improvement.
     */
    inline static std::vector<size_t>& get_tls_buffer() {
        static thread_local std::vector<size_t> tls_buffer_{};
        return tls_buffer_;
    }

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

        // Acquire shared read lock
        SharedLock lock(mutex_);

        auto& tls_buffer = get_tls_buffer();

        // Optimization 1: Use thread-local storage to precompute hash values
        const auto setSize = static_cast<size_t>(std::ranges::distance(set));
        if (tls_buffer.capacity() < setSize) {
            tls_buffer.reserve(setSize);
        }
        tls_buffer.clear();

        // Use std::ranges to iterate and precompute hash values
        for (const auto& element : set) {
            tls_buffer.push_back(std::hash<ValueType>{}(element));
        }

        // Optimization 2: Loop unrolling to leverage SIMD and instruction-level
        // parallelism
        constexpr size_t UNROLL_FACTOR = 4;
        const size_t hash_count = hash_functions_.size();
        const size_t hash_count_aligned =
            hash_count - (hash_count % UNROLL_FACTOR);

        // Use range-based for loop to iterate over precomputed hash values
        for (const auto element_hash : tls_buffer) {
            // Main loop, processing UNROLL_FACTOR hash functions per iteration
            for (size_t i = 0; i < hash_count_aligned; i += UNROLL_FACTOR) {
                for (size_t j = 0; j < UNROLL_FACTOR; ++j) {
                    signature[i + j] = std::min(
                        signature[i + j], hash_functions_[i + j](element_hash));
                }
            }

            // Process remaining hash functions
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
     * @brief RAII wrapper for OpenCL memory buffers.
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

        // Disable copy
        CLMemWrapper(const CLMemWrapper&) = delete;
        CLMemWrapper& operator=(const CLMemWrapper&) = delete;

        // Enable move
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
     * @throws std::runtime_error If an OpenCL operation fails
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

        // Acquire shared read lock
        SharedLock lock(mutex_);

        size_t numHashes = hash_functions_.size();
        size_t numElements = std::ranges::distance(set);

        if (numElements == 0) {
            return;  // Empty set, keep signature unchanged
        }

        using ValueType = std::ranges::range_value_t<Range>;

        // Optimization: Use thread-local storage to precompute hash values
        auto& tls_buffer = get_tls_buffer();  // Use the member function
        if (tls_buffer.capacity() < numElements) {
            tls_buffer.reserve(numElements);
        }
        tls_buffer.clear();

        // Use C++20 ranges to precompute all hash values
        for (const auto& element : set) {
            tls_buffer.push_back(std::hash<ValueType>{}(element));
        }

        std::vector<size_t> aValues(numHashes);
        std::vector<size_t> bValues(numHashes);
        // Extract hash function parameters
        for (size_t i = 0; i < numHashes; ++i) {
            // Implement logic to extract a and b parameters
            // TODO: Replace with actual parameter extraction from
            // hash_functions_
            aValues[i] = i + 1;      // Temporary example value
            bValues[i] = i * 2 + 1;  // Temporary example value
        }

        try {
            // Create memory buffers
            CLMemWrapper hashesBuffer(opencl_resources_->context,
                                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                      numElements * sizeof(size_t),
                                      tls_buffer.data());

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

            // Set kernel arguments
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

            // Optimization: Use multi-dimensional work-group structure for
            // better parallelism
            constexpr size_t WORK_GROUP_SIZE = 256;
            size_t globalWorkSize = (numHashes + WORK_GROUP_SIZE - 1) /
                                    WORK_GROUP_SIZE * WORK_GROUP_SIZE;

            err = clEnqueueNDRangeKernel(opencl_resources_->queue,
                                         opencl_resources_->minhash_kernel, 1,
                                         nullptr, &globalWorkSize,
                                         &WORK_GROUP_SIZE, 0, nullptr, nullptr);
            if (err != CL_SUCCESS)
                throw std::runtime_error("Failed to enqueue kernel");

            // Read results
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
 * @param input Span of input data
 * @return std::array<uint8_t, K_HASH_SIZE> The computed hash
 * @throws std::bad_alloc If memory allocation fails
 */
[[nodiscard]] auto keccak256(std::span<const uint8_t> input) noexcept(false)
    -> std::array<uint8_t, K_HASH_SIZE>;

/**
 * @brief Computes the Keccak-256 hash of the input string
 *
 * @param input Input string
 * @return std::array<uint8_t, K_HASH_SIZE> The computed hash
 * @throws std::bad_alloc If memory allocation fails
 */
[[nodiscard]] inline auto keccak256(std::string_view input) noexcept(false)
    -> std::array<uint8_t, K_HASH_SIZE> {
    return keccak256(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(input.data()), input.size()));
}

/**
 * @brief Context management class for hash computation.
 *
 * Provides RAII-style context management for hash computation, simplifying the
 * process.
 */
class HashContext {
public:
    /**
     * @brief Constructs a new hash context.
     */
    HashContext() noexcept;

    /**
     * @brief Destructor, automatically cleans up resources.
     */
    ~HashContext() noexcept;

    /**
     * @brief Disable copy operations.
     */
    HashContext(const HashContext&) = delete;
    HashContext& operator=(const HashContext&) = delete;

    /**
     * @brief Enable move operations.
     */
    HashContext(HashContext&&) noexcept;
    HashContext& operator=(HashContext&&) noexcept;

    /**
     * @brief Updates the hash computation with data.
     *
     * @param data Pointer to the data.
     * @param length Length of the data.
     * @return bool True if the operation was successful, false otherwise.
     */
    bool update(const void* data, size_t length) noexcept;

    /**
     * @brief Updates the hash computation with data from a string view.
     *
     * @param data Input string view.
     * @return bool True if the operation was successful, false otherwise.
     */
    bool update(std::string_view data) noexcept;

    /**
     * @brief Updates the hash computation with data from a span.
     *
     * @param data Input data span.
     * @return bool True if the operation was successful, false otherwise.
     */
    bool update(std::span<const std::byte> data) noexcept;

    /**
     * @brief Finalizes the hash computation and retrieves the result.
     *
     * @return std::optional<std::array<uint8_t, K_HASH_SIZE>> The hash result,
     * or std::nullopt on failure.
     */
    [[nodiscard]] std::optional<std::array<uint8_t, K_HASH_SIZE>>
    finalize() noexcept;

private:
    struct ContextImpl;
    std::unique_ptr<ContextImpl> impl_;
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MHASH_HPP
