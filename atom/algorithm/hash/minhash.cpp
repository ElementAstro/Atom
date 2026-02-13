/*
 * minhash.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: MinHash algorithm implementation for estimating Jaccard
             similarity between sets.

**************************************************/

#include "minhash.hpp"

#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>

#include "atom/error/exception.hpp"
#include "atom/utils/random.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/exception/all.hpp>
#include <boost/scope_exit.hpp>
#endif

namespace atom::algorithm {

#ifdef ATOM_USE_OPENCL
namespace {
// Using template string to simplify OpenCL kernel code
constexpr const char *minhashKernelSource = R"CLC(
__kernel void minhash_kernel(
    __global const size_t* hashes,
    __global size_t* signature,
    __global const size_t* a_values,
    __global const size_t* b_values,
    const size_t p,
    const size_t num_hashes,
    const size_t num_elements
) {
    int gid = get_global_id(0);
    if (gid < num_hashes) {
        size_t min_hash = SIZE_MAX;
        size_t a = a_values[gid];
        size_t b = b_values[gid];

        // Batch processing to leverage locality
        for (size_t i = 0; i < num_elements; ++i) {
            size_t h = (a * hashes[i] + b) % p;
            min_hash = (h < min_hash) ? h : min_hash;
        }

        signature[gid] = min_hash;
    }
}
)CLC";
}  // anonymous namespace
#endif

MinHash::MinHash(usize num_hashes) noexcept(false)
#ifdef ATOM_USE_OPENCL
    : opencl_available_(false)
#endif
{
    if (num_hashes == 0) {
        THROW_INVALID_ARGUMENT(
            "Number of hash functions must be greater than zero");
    }

    try {
        hash_functions_.reserve(num_hashes);
        for (usize i = 0; i < num_hashes; ++i) {
            hash_functions_.emplace_back(generateHashFunction());
        }
    } catch (const std::exception &e) {
        THROW_RUNTIME_ERROR(
            std::string("Failed to initialize hash functions: ") + e.what());
    }

#ifdef ATOM_USE_OPENCL
    initializeOpenCL();
#endif
}

MinHash::~MinHash() noexcept = default;

#ifdef ATOM_USE_OPENCL
void MinHash::initializeOpenCL() noexcept {
    try {
        cl_int err;
        cl_platform_id platform;
        cl_device_id device;

        // Initialize platform
        err = clGetPlatformIDs(1, &platform, nullptr);
        if (err != CL_SUCCESS) {
            return;
        }

        // Get device
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
        if (err != CL_SUCCESS) {
            // Try falling back to CPU
            err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device,
                                 nullptr);
            if (err != CL_SUCCESS) {
                return;
            }
        }

        // Create OpenCL resource objects
        opencl_resources_ = std::make_unique<OpenCLResources>();

        // Create context
        opencl_resources_->context =
            clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
        if (err != CL_SUCCESS) {
            return;
        }

        // Create command queue
        opencl_resources_->queue =
            clCreateCommandQueue(opencl_resources_->context, device, 0, &err);
        if (err != CL_SUCCESS) {
            return;
        }

        // Create program
        opencl_resources_->program = clCreateProgramWithSource(
            opencl_resources_->context, 1, &minhashKernelSource, nullptr, &err);
        if (err != CL_SUCCESS) {
            return;
        }

        // Build program
        err = clBuildProgram(opencl_resources_->program, 1, &device, nullptr,
                             nullptr, nullptr);
        if (err != CL_SUCCESS) {
            // Get build log for debugging
            usize log_size;
            clGetProgramBuildInfo(opencl_resources_->program, device,
                                  CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
            if (log_size > 1) {
                std::string log(log_size, ' ');
                clGetProgramBuildInfo(opencl_resources_->program, device,
                                      CL_PROGRAM_BUILD_LOG, log_size,
                                      log.data(), nullptr);
                // Debug log can be stored or output
            }
            return;
        }

        // Create kernel
        opencl_resources_->minhash_kernel =
            clCreateKernel(opencl_resources_->program, "minhash_kernel", &err);
        if (err == CL_SUCCESS) {
            opencl_available_.store(true, std::memory_order_release);
        }
    } catch (...) {
        // Ensure no exceptions propagate out of this function
        opencl_available_.store(false, std::memory_order_release);
        opencl_resources_.reset();
    }
}
#endif

auto MinHash::generateHashFunction() noexcept -> HashFunction {
    // Use standard library random instead of atom::utils::Random to avoid
    // include issues
    static thread_local std::mt19937_64 gen(std::random_device{}());
    static thread_local std::uniform_int_distribution<u64> dist(
        1, std::numeric_limits<u64>::max() - 1);

    // Use large prime to improve hash quality
    constexpr usize LARGE_PRIME = 0xFFFFFFFFFFFFFFC5ULL;  // 2^64 - 59 (prime)

    u64 a = dist(gen);
    u64 b = dist(gen);

    // Generate a closure to implement the hash function - capture by value to
    // improve cache locality
    return [a, b](usize x) -> usize {
        return static_cast<usize>((a * static_cast<u64>(x) + b) % LARGE_PRIME);
    };
}

auto MinHash::jaccardIndex(std::span<const usize> sig1,
                           std::span<const usize> sig2) noexcept(false) -> f64 {
    // Verify input signatures have the same length
    if (sig1.size() != sig2.size()) {
        THROW_INVALID_ARGUMENT("Signatures must have the same length");
    }

    if (sig1.empty()) {
        return 0.0;  // Empty signatures, similarity is 0
    }

    // Use parallel algorithm to calculate number of equal elements
    const usize totalSize = sig1.size();

    // Use SSE/AVX-friendly data access pattern
    constexpr usize VECTOR_SIZE = 16;  // Suitable for SSE registers
    const usize alignedSize = totalSize - (totalSize % VECTOR_SIZE);

    usize equalCount = 0;

    // Vectorized main loop, allowing compiler to use SIMD instructions
    for (usize i = 0; i < alignedSize; i += VECTOR_SIZE) {
        usize localCount = 0;
        for (usize j = 0; j < VECTOR_SIZE; ++j) {
            localCount += (sig1[i + j] == sig2[i + j]) ? 1 : 0;
        }
        equalCount += localCount;
    }

    // Process remaining elements
    for (usize i = alignedSize; i < totalSize; ++i) {
        equalCount += (sig1[i] == sig2[i]) ? 1 : 0;
    }

    return static_cast<f64>(equalCount) / totalSize;
}

thread_local std::vector<usize> tls_buffer_{};

}  // namespace atom::algorithm
