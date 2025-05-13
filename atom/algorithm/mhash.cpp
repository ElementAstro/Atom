/*
 * mhash.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Implementation of murmur3 hash and quick hash

**************************************************/

#include "mhash.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <random>
#include <stdexcept>
#include <system_error>

#include "atom/utils/random.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#ifdef ATOM_USE_BOOST
#include <boost/exception/all.hpp>
#include <boost/scope_exit.hpp>
#endif

namespace atom::algorithm {
// Keccak state constants
constexpr size_t K_KECCAK_F_RATE = 1088;  // For Keccak-256
constexpr size_t K_ROUNDS = 24;
constexpr size_t K_STATE_SIZE = 5;
constexpr size_t K_RATE_IN_BYTES = K_KECCAK_F_RATE / 8;
constexpr uint8_t K_PADDING_BYTE = 0x06;
constexpr uint8_t K_PADDING_LAST_BYTE = 0x80;

// Round constants for Keccak
constexpr std::array<uint64_t, K_ROUNDS> K_ROUND_CONSTANTS = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

// Rotation offsets
constexpr std::array<std::array<size_t, K_STATE_SIZE>, K_STATE_SIZE>
    K_ROTATION_CONSTANTS = {{{0, 1, 62, 28, 27},
                             {36, 44, 6, 55, 20},
                             {3, 10, 43, 25, 39},
                             {41, 45, 15, 21, 8},
                             {18, 2, 61, 56, 14}}};

// Keccak state as 5x5 matrix of 64-bit integers
using StateArray = std::array<std::array<uint64_t, K_STATE_SIZE>, K_STATE_SIZE>;

// Thread-local PMR memory resource pool for managing small memory allocations
thread_local std::pmr::synchronized_pool_resource tls_memory_pool{};

namespace {
#if USE_OPENCL
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
#endif
}  // anonymous namespace

// RAII wrapper for managing OpenSSL contexts
struct HashContext::ContextImpl {
    EVP_MD_CTX *ctx{nullptr};
    bool initialized{false};

    ContextImpl() noexcept : ctx(EVP_MD_CTX_new()) {}

    ~ContextImpl() noexcept {
        if (ctx) {
            EVP_MD_CTX_free(ctx);
        }
    }

    // Disable copy operations
    ContextImpl(const ContextImpl &) = delete;
    ContextImpl &operator=(const ContextImpl &) = delete;

    // Implement move operations
    ContextImpl(ContextImpl &&other) noexcept
        : ctx(std::exchange(other.ctx, nullptr)),
          initialized(other.initialized) {
        other.initialized = false;
    }

    ContextImpl &operator=(ContextImpl &&other) noexcept {
        if (this != &other) {
            if (ctx) {
                EVP_MD_CTX_free(ctx);
            }
            ctx = std::exchange(other.ctx, nullptr);
            initialized = other.initialized;
            other.initialized = false;
        }
        return *this;
    }

    bool init() noexcept {
        if (!ctx)
            return false;

        initialized = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1;
        return initialized;
    }
};

HashContext::HashContext() noexcept : impl_(std::make_unique<ContextImpl>()) {
    if (impl_) {
        impl_->init();
    }
}

HashContext::~HashContext() noexcept = default;

HashContext::HashContext(HashContext &&other) noexcept = default;
HashContext &HashContext::operator=(HashContext &&other) noexcept = default;

bool HashContext::update(const void *data, size_t length) noexcept {
    if (!impl_ || !impl_->initialized || !data)
        return false;
    return EVP_DigestUpdate(impl_->ctx, data, length) == 1;
}

bool HashContext::update(std::string_view data) noexcept {
    return update(data.data(), data.size());
}

bool HashContext::update(std::span<const std::byte> data) noexcept {
    return update(data.data(), data.size_bytes());
}

std::optional<std::array<uint8_t, K_HASH_SIZE>>
HashContext::finalize() noexcept {
    if (!impl_ || !impl_->initialized)
        return std::nullopt;

    std::array<uint8_t, K_HASH_SIZE> result{};
    unsigned int resultLen = 0;

    if (EVP_DigestFinal_ex(impl_->ctx, result.data(), &resultLen) != 1 ||
        resultLen != K_HASH_SIZE) {
        return std::nullopt;
    }

    return result;
}

MinHash::MinHash(size_t num_hashes) noexcept(false)
#if USE_OPENCL
    : opencl_available_(false)
#endif
{
    if (num_hashes == 0) {
        throw std::invalid_argument(
            "Number of hash functions must be greater than zero");
    }

    try {
        hash_functions_.reserve(num_hashes);
        for (size_t i = 0; i < num_hashes; ++i) {
            hash_functions_.emplace_back(generateHashFunction());
        }
    } catch (const std::exception &e) {
        throw std::runtime_error(
            std::string("Failed to initialize hash functions: ") + e.what());
    }

#if USE_OPENCL
    initializeOpenCL();
#endif
}

MinHash::~MinHash() noexcept = default;

#if USE_OPENCL
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
            size_t log_size;
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
    static thread_local utils::Random<std::mt19937_64,
                                      std::uniform_int_distribution<uint64_t>>
        rand(1, std::numeric_limits<uint64_t>::max() - 1);

    // Use large prime to improve hash quality
    constexpr size_t LARGE_PRIME = 0xFFFFFFFFFFFFFFC5ULL;  // 2^64 - 59 (prime)

    uint64_t a = rand();
    uint64_t b = rand();

    // Generate a closure to implement the hash function - capture by value to
    // improve cache locality
    return [a, b](size_t x) -> size_t {
        return static_cast<size_t>((a * static_cast<uint64_t>(x) + b) %
                                   LARGE_PRIME);
    };
}

auto MinHash::jaccardIndex(std::span<const size_t> sig1,
                           std::span<const size_t> sig2) noexcept(false)
    -> double {
    // Verify input signatures have the same length
    if (sig1.size() != sig2.size()) {
        throw std::invalid_argument("Signatures must have the same length");
    }

    if (sig1.empty()) {
        return 0.0;  // Empty signatures, similarity is 0
    }

    // Use parallel algorithm to calculate number of equal elements
    const size_t totalSize = sig1.size();

    // Use SSE/AVX-friendly data access pattern
    constexpr size_t VECTOR_SIZE = 16;  // Suitable for SSE registers
    const size_t alignedSize = totalSize - (totalSize % VECTOR_SIZE);

    size_t equalCount = 0;

    // Vectorized main loop, allowing compiler to use SIMD instructions
    for (size_t i = 0; i < alignedSize; i += VECTOR_SIZE) {
        size_t localCount = 0;
        for (size_t j = 0; j < VECTOR_SIZE; ++j) {
            localCount += (sig1[i + j] == sig2[i + j]) ? 1 : 0;
        }
        equalCount += localCount;
    }

    // Process remaining elements
    for (size_t i = alignedSize; i < totalSize; ++i) {
        equalCount += (sig1[i] == sig2[i]) ? 1 : 0;
    }

    return static_cast<double>(equalCount) / totalSize;
}

auto hexstringFromData(std::string_view data) noexcept(false) -> std::string {
    const char *hexChars = "0123456789ABCDEF";

    // Create string using PMR memory resource to reduce memory allocations
    std::pmr::string output(&tls_memory_pool);

    try {
        output.reserve(data.size() * 2);  // Reserve sufficient space

        // Use std::transform to convert bytes to hexadecimal
        for (unsigned char byte : data) {
            output.push_back(hexChars[(byte >> 4) & 0x0F]);
            output.push_back(hexChars[byte & 0x0F]);
        }
    } catch (const std::exception &e) {
#ifdef ATOM_USE_BOOST
        throw boost::enable_error_info(std::runtime_error(
            std::string("Failed to convert to hex: ") + e.what()));
#else
        throw std::runtime_error(std::string("Failed to convert to hex: ") +
                                 e.what());
#endif
    }

    return std::string(output);
}

auto dataFromHexstring(std::string_view data) noexcept(false) -> std::string {
    if (data.empty()) {
        return "";
    }

    if (data.size() % 2 != 0) {
#ifdef ATOM_USE_BOOST
        throw boost::enable_error_info(
            std::invalid_argument("Hex string length must be even"));
#else
        throw std::invalid_argument("Hex string length must be even");
#endif
    }

    // Use memory resource pool to improve small allocation performance
    std::pmr::string result(&tls_memory_pool);

    try {
        result.resize(data.size() / 2);

        // Process conversions in parallel to improve performance
        const size_t length = data.size() / 2;

        // Use block processing to enhance data locality
        constexpr size_t BLOCK_SIZE = 64;
        const size_t numBlocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;

        for (size_t block = 0; block < numBlocks; ++block) {
            const size_t blockStart = block * BLOCK_SIZE;
            const size_t blockEnd = std::min(blockStart + BLOCK_SIZE, length);

            for (size_t i = blockStart; i < blockEnd; ++i) {
                const size_t pos = i * 2;
                uint8_t byte = 0;

                // Use C++17 from_chars, not dependent on errno
                auto [ptr, ec] = std::from_chars(
                    data.data() + pos, data.data() + pos + 2, byte, 16);

                if (ec != std::errc{}) {
#ifdef ATOM_USE_BOOST
                    BOOST_SCOPE_EXIT_ALL(&){
                        // Clean up resources
                    };
                    throw boost::enable_error_info(std::invalid_argument(
                        "Invalid hex character at position " +
                        std::to_string(pos)));
#else
                    throw std::invalid_argument(
                        "Invalid hex character at position " +
                        std::to_string(pos));
#endif
                }

                result[i] = static_cast<char>(byte);
            }
        }
    } catch (const std::exception &e) {
        if (dynamic_cast<const std::invalid_argument *>(&e)) {
            throw;  // Rethrow original exception
        }
#ifdef ATOM_USE_BOOST
        throw boost::enable_error_info(std::runtime_error(
            std::string("Failed to convert from hex: ") + e.what()));
#else
        throw std::runtime_error(std::string("Failed to convert from hex: ") +
                                 e.what());
#endif
    }

    return std::string(result);
}

bool supportsHexStringConversion(std::string_view str) noexcept {
    if (str.empty()) {
        return false;
    }

    return std::all_of(str.begin(), str.end(),
                       [](unsigned char c) { return std::isxdigit(c); });
}

// Keccak helper functions - optimized using C++20 features
// θ step: XOR each column and then propagate changes across the state
inline void theta(StateArray &stateArray) noexcept {
    std::array<uint64_t, K_STATE_SIZE> column{}, diff{};

    // Use explicit loop unrolling for compiler to generate more efficient code
    for (size_t colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        column[colIndex] = stateArray[colIndex][0] ^ stateArray[colIndex][1] ^
                           stateArray[colIndex][2] ^ stateArray[colIndex][3] ^
                           stateArray[colIndex][4];
    }

    for (size_t colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        diff[colIndex] = column[(colIndex + 4) % K_STATE_SIZE] ^
                         std::rotl(column[(colIndex + 1) % K_STATE_SIZE], 1);
    }

    for (size_t colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        for (size_t rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
            stateArray[colIndex][rowIndex] ^= diff[colIndex];
        }
    }
}

// ρ step: Rotate each bit-plane by pre-determined offsets
inline void rho(StateArray &stateArray) noexcept {
    // Use fast bit rotation
    for (size_t colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        for (size_t rowIndex = 0; colIndex < K_STATE_SIZE; ++rowIndex) {
            stateArray[colIndex][rowIndex] = std::rotl(
                stateArray[colIndex][rowIndex],
                static_cast<int>(K_ROTATION_CONSTANTS[colIndex][rowIndex]));
        }
    }
}

// π step: Permute bits to new positions based on a fixed pattern
inline void pi(StateArray &stateArray) noexcept {
    StateArray temp = stateArray;
    for (size_t colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        for (size_t rowIndex = 0; colIndex < K_STATE_SIZE; ++rowIndex) {
            stateArray[colIndex][rowIndex] =
                temp[(colIndex + 3 * rowIndex) % K_STATE_SIZE][colIndex];
        }
    }
}

// χ step: Non-linear step XORs data across rows, producing diffusion
inline void chi(StateArray &stateArray) noexcept {
    for (size_t rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
        std::array<uint64_t, K_STATE_SIZE> temp = {};
        for (size_t colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
            temp[colIndex] = stateArray[colIndex][rowIndex];
        }

        for (size_t colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
            stateArray[colIndex][rowIndex] ^=
                (~temp[(colIndex + 1) % K_STATE_SIZE] &
                 temp[(colIndex + 2) % K_STATE_SIZE]);
        }
    }
}

// ι step: XOR a round constant into the first state element
inline void iota(StateArray &stateArray, size_t round) noexcept {
    stateArray[0][0] ^= K_ROUND_CONSTANTS[round];
}

// Keccak-p permutation: 24 rounds of transformations on the state
inline void keccakP(StateArray &stateArray) noexcept {
    for (size_t round = 0; round < K_ROUNDS; ++round) {
        theta(stateArray);
        rho(stateArray);
        pi(stateArray);
        chi(stateArray);
        iota(stateArray, round);
    }
}

// Absorb phase: XOR input into the state and permute
void absorb(StateArray &state, std::span<const uint8_t> input) noexcept {
    size_t length = input.size();
    const uint8_t *data = input.data();

    while (length >= K_RATE_IN_BYTES) {
        for (size_t i = 0; i < K_RATE_IN_BYTES / 8; ++i) {
            // Use std::bit_cast instead of boolean expressions to avoid
            // undefined behavior
            std::array<uint8_t, 8> bytes;
            std::copy_n(data + i * 8, 8, bytes.begin());
            state[i % K_STATE_SIZE][i / K_STATE_SIZE] ^=
                std::bit_cast<uint64_t>(bytes);
        }
        keccakP(state);
        data += K_RATE_IN_BYTES;
        length -= K_RATE_IN_BYTES;
    }

    // Process the last incomplete block
    if (length > 0) {
        std::array<uint8_t, K_RATE_IN_BYTES> paddedBlock = {};
        std::copy_n(data, length, paddedBlock.begin());
        paddedBlock[length] = K_PADDING_BYTE;
        paddedBlock.back() |= K_PADDING_LAST_BYTE;

        for (size_t i = 0; i < K_RATE_IN_BYTES / 8; ++i) {
            std::array<uint8_t, 8> bytes;
            std::copy_n(paddedBlock.data() + i * 8, 8, bytes.begin());
            state[i % K_STATE_SIZE][i / K_STATE_SIZE] ^=
                std::bit_cast<uint64_t>(bytes);
        }
        keccakP(state);
    }
}

// Squeeze phase: Extract output from the state
void squeeze(StateArray &state, std::span<uint8_t> output) noexcept {
    size_t outputLength = output.size();
    uint8_t *data = output.data();

    while (outputLength >= K_RATE_IN_BYTES) {
        for (size_t i = 0; i < K_RATE_IN_BYTES / 8; ++i) {
            const uint64_t value = state[i % K_STATE_SIZE][i / K_STATE_SIZE];
            const auto bytes = std::bit_cast<std::array<uint8_t, 8>>(value);
            std::copy_n(bytes.begin(), 8, data + i * 8);
        }
        keccakP(state);
        data += K_RATE_IN_BYTES;
        outputLength -= K_RATE_IN_BYTES;
    }

    if (outputLength > 0) {
        for (size_t i = 0; i < outputLength / 8; ++i) {
            const uint64_t value = state[i % K_STATE_SIZE][i / K_STATE_SIZE];
            const auto bytes = std::bit_cast<std::array<uint8_t, 8>>(value);
            std::copy_n(bytes.begin(), 8, data + i * 8);
        }

        // Process remaining incomplete bytes
        const size_t remainingBytes = outputLength % 8;
        if (remainingBytes > 0) {
            const size_t fullWords = outputLength / 8;
            const uint64_t value =
                state[fullWords % K_STATE_SIZE][fullWords / K_STATE_SIZE];
            const auto bytes = std::bit_cast<std::array<uint8_t, 8>>(value);
            std::copy_n(bytes.begin(), remainingBytes, data + fullWords * 8);
        }
    }
}

// Keccak-256 hashing function - using span interface
auto keccak256(std::span<const uint8_t> input)
    -> std::array<uint8_t, K_HASH_SIZE> {
    StateArray state = {};

    // Process input data
    absorb(state, input);

    // If no data provided or size is multiple of rate, padding is needed
    if (input.empty() || input.size() % K_RATE_IN_BYTES == 0) {
        std::array<uint8_t, 1> padBlock = {K_PADDING_BYTE};
        absorb(state, std::span<const uint8_t>(padBlock));
    }

    // Extract result
    std::array<uint8_t, K_HASH_SIZE> hash = {};
    squeeze(state, std::span<uint8_t>(hash));
    return hash;
}

thread_local std::vector<size_t> tls_buffer_{};

}  // namespace atom::algorithm