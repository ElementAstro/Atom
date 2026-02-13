/*
 * random.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Secure random number generation - implementations

**************************************************/

#include "random.hpp"

#include <random>

namespace atom::algorithm {

auto secureRandom() noexcept -> std::optional<u64> {
    try {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<u64> dist;
        return dist(gen);
    } catch (...) {
        return std::nullopt;
    }
}

auto randomInRange(u64 min, u64 max) noexcept -> std::optional<u64> {
    if (min > max) {
        return std::nullopt;
    }

    try {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<u64> dist(min, max);
        return dist(gen);
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace atom::algorithm
