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

auto generateRandomString(int length, const std::string& charset)
    -> std::string {
    if (length <= 0) {
        throw std::invalid_argument("Length must be a positive integer.");
    }

    const std::string& chars = charset.empty() ? getDefaultCharset() : charset;

    if (chars.empty()) {
        throw std::invalid_argument("Character set cannot be empty.");
    }

    // 修复: 直接使用随机数生成器而不是 Random 模板类
    std::mt19937_64 engine(getRandomDevice()());
    std::uniform_int_distribution<size_t> dist(0, chars.size() - 1);

    std::string randomString(length, '\0');
    std::generate(std::execution::par_unseq, randomString.begin(),
                  randomString.end(), [&]() { return chars[dist(engine)]; });

    return randomString;
}

auto generateSecureRandomString(int length) -> std::string {
    if (length <= 0) {
        throw std::invalid_argument("Length must be a positive integer.");
    }

    std::string result(length, '\0');
    std::random_device& rd = getRandomDevice();
    const std::string& chars = getDefaultCharset();

    std::independent_bits_engine<std::random_device, CHAR_BIT, uint32_t> engine(
        rd);
    for (int i = 0; i < length; ++i) {
        result[i] = chars[engine() % chars.size()];
    }

    return result;
}

}  // namespace atom::utils
