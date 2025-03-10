// cpp
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

#include <bit>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <limits>
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

namespace {
#if USE_OPENCL
// 使用模板字符串简化OpenCL内核代码
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
        
        // 批量处理以利用局部性
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

MinHash::~MinHash() noexcept {
    // 使用智能指针自动管理OpenCL资源
    // 无需显式调用cleanupOpenCL
}

#if USE_OPENCL
void MinHash::initializeOpenCL() noexcept {
    try {
        cl_int err;
        cl_platform_id platform;
        cl_device_id device;

        // 初始化平台
        err = clGetPlatformIDs(1, &platform, nullptr);
        if (err != CL_SUCCESS) {
            return;
        }

        // 获取设备
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
        if (err != CL_SUCCESS) {
            // 尝试退回到CPU
            err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device,
                                 nullptr);
            if (err != CL_SUCCESS) {
                return;
            }
        }

        // 创建OpenCL资源对象
        opencl_resources_ = std::make_unique<OpenCLResources>();

        // 创建上下文
        opencl_resources_->context =
            clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
        if (err != CL_SUCCESS) {
            return;
        }

        // 创建命令队列
        opencl_resources_->queue =
            clCreateCommandQueue(opencl_resources_->context, device, 0, &err);
        if (err != CL_SUCCESS) {
            return;
        }

        // 创建程序
        opencl_resources_->program = clCreateProgramWithSource(
            opencl_resources_->context, 1, &minhashKernelSource, nullptr, &err);
        if (err != CL_SUCCESS) {
            return;
        }

        // 构建程序
        err = clBuildProgram(opencl_resources_->program, 1, &device, nullptr,
                             nullptr, nullptr);
        if (err != CL_SUCCESS) {
            // 获取构建日志以便调试
            size_t log_size;
            clGetProgramBuildInfo(opencl_resources_->program, device,
                                  CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
            if (log_size > 1) {
                std::string log(log_size, ' ');
                clGetProgramBuildInfo(opencl_resources_->program, device,
                                      CL_PROGRAM_BUILD_LOG, log_size,
                                      log.data(), nullptr);
                // 调试日志可以存储或输出
            }
            return;
        }

        // 创建内核
        opencl_resources_->minhash_kernel =
            clCreateKernel(opencl_resources_->program, "minhash_kernel", &err);
        if (err == CL_SUCCESS) {
            opencl_available_ = true;
        }
    } catch (...) {
        // 确保任何异常不会传播出这个函数
        opencl_available_ = false;
        opencl_resources_.reset();
    }
}
#endif

auto MinHash::generateHashFunction() noexcept -> HashFunction {
    static thread_local utils::Random<std::mt19937_64,
                                      std::uniform_int_distribution<uint64_t>>
        rand(1, std::numeric_limits<uint64_t>::max() - 1);

    // 使用大质数来提高哈希质量
    constexpr size_t LARGE_PRIME = 0xFFFFFFFFFFFFFFC5ULL;  // 2^64 - 59 (质数)

    uint64_t a = rand();
    uint64_t b = rand();

    // 生成一个闭包来实现哈希函数
    return [a, b](size_t x) -> size_t {
        return static_cast<size_t>((a * static_cast<uint64_t>(x) + b) %
                                   LARGE_PRIME);
    };
}

auto MinHash::jaccardIndex(std::span<const size_t> sig1,
                           std::span<const size_t> sig2) noexcept(false)
    -> double {
    // 验证输入签名长度相同
    if (sig1.size() != sig2.size()) {
        throw std::invalid_argument("Signatures must have the same length");
    }

    if (sig1.empty()) {
        return 0.0;  // 空签名，相似度为0
    }

    // 使用并行算法计算相等元素数量
    const size_t totalSize = sig1.size();

    size_t equalCount = 0;

    // 使用SIMD友好的方式遍历，以便编译器可以自动向量化
    for (size_t i = 0; i < totalSize; ++i) {
        equalCount += (sig1[i] == sig2[i]) ? 1 : 0;
    }

    return static_cast<double>(equalCount) / totalSize;
}

auto hexstringFromData(std::string_view data) noexcept(false) -> std::string {
    const char *hexChars = "0123456789ABCDEF";
    std::string output;

    try {
        output.reserve(data.size() * 2);  // 预留足够空间

        // 使用std::transform来转换字节到十六进制
        for (unsigned char byte : data) {
            output.push_back(hexChars[(byte >> 4) & 0x0F]);
            output.push_back(hexChars[byte & 0x0F]);
        }
    } catch (const std::exception &e) {
        throw std::runtime_error(std::string("Failed to convert to hex: ") +
                                 e.what());
    }

    return output;
}

auto dataFromHexstring(std::string_view data) noexcept(false) -> std::string {
    if (data.empty()) {
        return "";
    }

    if (data.size() % 2 != 0) {
#ifdef ATOM_USE_BOOST
        throw boost::enable_error_info(InvalidArgumentException())
            << boost::errinfo_api_function("Hex string length must be even");
#else
        throw std::invalid_argument("Hex string length must be even");
#endif
    }

    std::string result;

    try {
        result.resize(data.size() / 2);

        // 并行处理转换，提高性能
        const size_t length = data.size() / 2;
        for (size_t i = 0; i < length; ++i) {
            const size_t pos = i * 2;
            uint8_t byte = 0;

            auto [ptr, ec] = std::from_chars(data.data() + pos,
                                             data.data() + pos + 2, byte, 16);

            if (ec != std::errc{}) {
#ifdef ATOM_USE_BOOST
                throw boost::enable_error_info(InvalidArgumentException())
                    << boost::errinfo_api_function("Invalid hex character");
#else
                throw std::invalid_argument(
                    "Invalid hex character at position " + std::to_string(pos));
#endif
            }

            result[i] = static_cast<char>(byte);
        }
    } catch (const std::exception &e) {
        if (dynamic_cast<const std::invalid_argument *>(&e)) {
            throw;  // 重新抛出原始异常
        }
        throw std::runtime_error(std::string("Failed to convert from hex: ") +
                                 e.what());
    }

    return result;
}

// Keccak辅助函数 - 使用C++20特性优化
// θ step: XOR each column and then propagate changes across the state
inline void theta(StateArray &stateArray) noexcept {
    std::array<uint64_t, K_STATE_SIZE> column{}, diff{};

    // 使用显式循环展开以便编译器生成更高效的代码
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
    // 使用快速位旋转
    for (size_t colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        for (size_t rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
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
        for (size_t rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
            stateArray[colIndex][rowIndex] =
                temp[(colIndex + 3 * rowIndex) % K_STATE_SIZE][colIndex];
        }
    }
}

// χ step: Non-linear step XORs data across rows, producing diffusion
inline void chi(StateArray &stateArray) noexcept {
    for (size_t rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
        std::array<uint64_t, K_STATE_SIZE> temp = stateArray[rowIndex];
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
void absorb(StateArray &state, const uint8_t *input, size_t length) noexcept {
    while (length >= K_RATE_IN_BYTES) {
        for (size_t i = 0; i < K_RATE_IN_BYTES / 8; ++i) {
#ifdef ATOM_USE_BOOST
            state[i % K_STATE_SIZE][i / K_STATE_SIZE] ^=
                boost::bit_cast<uint64_t>(input + i * 8);
#else
            state[i % K_STATE_SIZE][i / K_STATE_SIZE] ^=
                std::bit_cast<uint64_t>(input + i * 8);
#endif
        }
        keccakP(state);
        input += K_RATE_IN_BYTES;
        length -= K_RATE_IN_BYTES;
    }
}

// Padding and absorbing the last block
void padAndAbsorb(StateArray &state, const uint8_t *input,
                  size_t length) noexcept {
    std::array<uint8_t, K_RATE_IN_BYTES> paddedBlock = {};
    std::memcpy(paddedBlock.data(), input, length);
    paddedBlock[length] = K_PADDING_BYTE;       // Keccak padding
    paddedBlock.back() |= K_PADDING_LAST_BYTE;  // Set last bit to 1
    absorb(state, paddedBlock.data(), paddedBlock.size());
}

// Squeeze phase: Extract output from the state
void squeeze(StateArray &state, uint8_t *output, size_t outputLength) noexcept {
    while (outputLength >= K_RATE_IN_BYTES) {
        for (size_t i = 0; i < K_RATE_IN_BYTES / 8; ++i) {
#ifdef ATOM_USE_BOOST
            std::memcpy(output + i * 8,
                        &state[i % K_STATE_SIZE][i / K_STATE_SIZE], 8);
#else
            std::memcpy(output + i * 8,
                        &state[i % K_STATE_SIZE][i / K_STATE_SIZE], 8);
#endif
        }
        keccakP(state);
        output += K_RATE_IN_BYTES;
        outputLength -= K_RATE_IN_BYTES;
    }
    for (size_t i = 0; i < outputLength / 8; ++i) {
        std::memcpy(output + i * 8, &state[i % K_STATE_SIZE][i / K_STATE_SIZE],
                    8);
    }
}

// Keccak-256 hashing function
auto keccak256(const uint8_t *input,
               size_t length) -> std::array<uint8_t, K_HASH_SIZE> {
    StateArray state = {};
    padAndAbsorb(state, input, length);

    std::array<uint8_t, K_HASH_SIZE> hash = {};
    squeeze(state, hash.data(), hash.size());
    return hash;
}

bool supportsHexStringConversion(const std::string &str) {
    if (str.empty()) {
        return false;
    }

    for (char c : str) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }

    return true;
}

}  // namespace atom::algorithm