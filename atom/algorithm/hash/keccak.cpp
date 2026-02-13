/*
 * keccak.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Keccak-256 cryptographic hash function implementation.

**************************************************/

#include "keccak.hpp"

#include <algorithm>
#include <bit>

namespace atom::algorithm {

// Keccak state constants
constexpr usize K_KECCAK_F_RATE = 1088;  // For Keccak-256
constexpr usize K_ROUNDS = 24;
constexpr usize K_STATE_SIZE = 5;
constexpr usize K_RATE_IN_BYTES = K_KECCAK_F_RATE / 8;
constexpr u8 K_PADDING_BYTE = 0x06;
constexpr u8 K_PADDING_LAST_BYTE = 0x80;

// Round constants for Keccak
constexpr std::array<u64, K_ROUNDS> K_ROUND_CONSTANTS = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000080008008ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

// Rotation offsets
constexpr std::array<std::array<usize, K_STATE_SIZE>, K_STATE_SIZE>
    K_ROTATION_CONSTANTS = {{{0, 1, 62, 28, 27},
                             {36, 44, 6, 55, 20},
                             {3, 10, 43, 25, 39},
                             {41, 45, 15, 21, 8},
                             {18, 2, 61, 56, 14}}};

// Keccak state as 5x5 matrix of 64-bit integers
using StateArray = std::array<std::array<u64, K_STATE_SIZE>, K_STATE_SIZE>;

// Keccak helper functions - optimized using C++20 features
// θ step: XOR each column and then propagate changes across the state
inline void theta(StateArray &stateArray) noexcept {
    std::array<u64, K_STATE_SIZE> column{}, diff{};

    // Use explicit loop unrolling for compiler to generate more efficient code
    for (usize colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        column[colIndex] = stateArray[colIndex][0] ^ stateArray[colIndex][1] ^
                           stateArray[colIndex][2] ^ stateArray[colIndex][3] ^
                           stateArray[colIndex][4];
    }

    for (usize colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        diff[colIndex] = column[(colIndex + 4) % K_STATE_SIZE] ^
                         std::rotl(column[(colIndex + 1) % K_STATE_SIZE], 1);
    }

    for (usize colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        for (usize rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
            stateArray[colIndex][rowIndex] ^= diff[colIndex];
        }
    }
}

// ρ step: Rotate each bit-plane by pre-determined offsets
inline void rho(StateArray &stateArray) noexcept {
    // Use fast bit rotation
    for (usize colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        for (usize rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
            stateArray[colIndex][rowIndex] = std::rotl(
                stateArray[colIndex][rowIndex],
                static_cast<i32>(K_ROTATION_CONSTANTS[colIndex][rowIndex]));
        }
    }
}

// π step: Permute bits to new positions based on a fixed pattern
inline void pi(StateArray &stateArray) noexcept {
    StateArray temp = stateArray;
    for (usize colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
        for (usize rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
            stateArray[colIndex][rowIndex] =
                temp[(colIndex + 3 * rowIndex) % K_STATE_SIZE][colIndex];
        }
    }
}

// χ step: Non-linear step XORs data across rows, producing diffusion
inline void chi(StateArray &stateArray) noexcept {
    for (usize rowIndex = 0; rowIndex < K_STATE_SIZE; ++rowIndex) {
        std::array<u64, K_STATE_SIZE> temp = {};
        for (usize colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
            temp[colIndex] = stateArray[colIndex][rowIndex];
        }

        for (usize colIndex = 0; colIndex < K_STATE_SIZE; ++colIndex) {
            stateArray[colIndex][rowIndex] ^=
                (~temp[(colIndex + 1) % K_STATE_SIZE] &
                 temp[(colIndex + 2) % K_STATE_SIZE]);
        }
    }
}

// ι step: XOR a round constant into the first state element
inline void iota(StateArray &stateArray, usize round) noexcept {
    stateArray[0][0] ^= K_ROUND_CONSTANTS[round];
}

// Keccak-p permutation: 24 rounds of transformations on the state
inline void keccakP(StateArray &stateArray) noexcept {
    for (usize round = 0; round < K_ROUNDS; ++round) {
        theta(stateArray);
        rho(stateArray);
        pi(stateArray);
        chi(stateArray);
        iota(stateArray, round);
    }
}

// Absorb phase: XOR input into the state and permute
void absorb(StateArray &state, std::span<const u8> input) noexcept {
    usize length = input.size();
    const u8 *data = input.data();

    while (length >= K_RATE_IN_BYTES) {
        for (usize i = 0; i < K_RATE_IN_BYTES / 8; ++i) {
            // Use std::bit_cast instead of boolean expressions to avoid
            // undefined behavior
            std::array<u8, 8> bytes;
            std::copy_n(data + i * 8, 8, bytes.begin());
            state[i % K_STATE_SIZE][i / K_STATE_SIZE] ^=
                std::bit_cast<u64>(bytes);
        }
        keccakP(state);
        data += K_RATE_IN_BYTES;
        length -= K_RATE_IN_BYTES;
    }

    // Process the last incomplete block
    if (length > 0) {
        std::array<u8, K_RATE_IN_BYTES> paddedBlock = {};
        std::copy_n(data, length, paddedBlock.begin());
        paddedBlock[length] = K_PADDING_BYTE;
        paddedBlock.back() |= K_PADDING_LAST_BYTE;

        for (usize i = 0; i < K_RATE_IN_BYTES / 8; ++i) {
            std::array<u8, 8> bytes;
            std::copy_n(paddedBlock.data() + i * 8, 8, bytes.begin());
            state[i % K_STATE_SIZE][i / K_STATE_SIZE] ^=
                std::bit_cast<u64>(bytes);
        }
        keccakP(state);
    }
}

// Squeeze phase: Extract output from the state
void squeeze(StateArray &state, std::span<u8> output) noexcept {
    usize outputLength = output.size();
    u8 *data = output.data();

    while (outputLength >= K_RATE_IN_BYTES) {
        for (usize i = 0; i < K_RATE_IN_BYTES / 8; ++i) {
            const u64 value = state[i % K_STATE_SIZE][i / K_STATE_SIZE];
            const auto bytes = std::bit_cast<std::array<u8, 8>>(value);
            std::copy_n(bytes.begin(), 8, data + i * 8);
        }
        keccakP(state);
        data += K_RATE_IN_BYTES;
        outputLength -= K_RATE_IN_BYTES;
    }

    if (outputLength > 0) {
        for (usize i = 0; i < outputLength / 8; ++i) {
            const u64 value = state[i % K_STATE_SIZE][i / K_STATE_SIZE];
            const auto bytes = std::bit_cast<std::array<u8, 8>>(value);
            std::copy_n(bytes.begin(), 8, data + i * 8);
        }

        // Process remaining incomplete bytes
        const usize remainingBytes = outputLength % 8;
        if (remainingBytes > 0) {
            const usize fullWords = outputLength / 8;
            const u64 value =
                state[fullWords % K_STATE_SIZE][fullWords / K_STATE_SIZE];
            const auto bytes = std::bit_cast<std::array<u8, 8>>(value);
            std::copy_n(bytes.begin(), remainingBytes, data + fullWords * 8);
        }
    }
}

// Keccak-256 hashing function - using span interface
auto keccak256(std::span<const u8> input) -> std::array<u8, K_HASH_SIZE> {
    StateArray state = {};

    // Process input data
    absorb(state, input);

    // If no data provided or size is multiple of rate, padding is needed
    if (input.empty() || input.size() % K_RATE_IN_BYTES == 0) {
        std::array<u8, 1> padBlock = {K_PADDING_BYTE};
        absorb(state, std::span<const u8>(padBlock));
    }

    // Extract result
    std::array<u8, K_HASH_SIZE> hash = {};
    squeeze(state, std::span<u8>(hash));
    return hash;
}

}  // namespace atom::algorithm
