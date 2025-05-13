/*
 * random.cpp
 *
 * Copyright (C) 2023-2024 Max Q. <contact@lightapt.com>
 */

/*************************************************

Date: 2023-12-25

Description: Simple random number generator

**************************************************/

#include "random.hpp"
#include <execution>  // For parallel algorithms
#include <random>     // For random number generation
#include "exception.hpp"

namespace atom::utils {

namespace {
// Thread-safe random device initialization
std::random_device& getRandomDevice() {
    static std::random_device rd;
    return rd;
}

// Default charset for random strings
const std::string& getDefaultCharset() {
    static const std::string DEFAULT_CHARSET =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    return DEFAULT_CHARSET;
}
}  // namespace

auto generateRandomString(int length,
                          const std::string& charset = std::string(),
                          bool secure = false) -> std::string {
    if (length <= 0) {
        THROW_INVALID_ARGUMENT("Length must be a positive integer.");
    }

    const std::string& chars = charset.empty() ? getDefaultCharset() : charset;

    if (chars.empty()) {
        THROW_INVALID_ARGUMENT("Character set cannot be empty.");
    }

    std::string result(length, '\0');

    if (secure) {
        // Use random_device directly for better entropy in secure mode
        std::random_device rd;
        std::uniform_int_distribution<size_t> distribution(0, chars.size() - 1);

        for (int i = 0; i < length; ++i) {
            result[i] = chars[distribution(rd)];
        }
    } else {
        // Use faster mt19937_64 with parallel execution for non-secure mode
        std::mt19937_64 engine(getRandomDevice()());
        std::uniform_int_distribution<size_t> dist(0, chars.size() - 1);

        std::generate(std::execution::par_unseq, result.begin(), result.end(),
                      [&]() { return chars[dist(engine)]; });
    }

    return result;
}

}  // namespace atom::utils
