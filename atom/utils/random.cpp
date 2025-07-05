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

#include <random>

#include "atom/error/exception.hpp"

namespace atom::utils {

namespace {
thread_local std::mt19937_64 thread_engine{std::random_device{}()};

const std::string& getDefaultCharset() {
    static const std::string DEFAULT_CHARSET =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    return DEFAULT_CHARSET;
}
}

auto generateRandomString(int length, const std::string& charset, bool secure) -> std::string {
    if (length <= 0) {
        THROW_INVALID_ARGUMENT("Length must be a positive integer.");
    }

    const std::string& chars = charset.empty() ? getDefaultCharset() : charset;

    if (chars.empty()) {
        THROW_INVALID_ARGUMENT("Character set cannot be empty.");
    }

    std::string result;
    result.reserve(length);

    if (secure) {
        std::random_device rd;
        std::uniform_int_distribution<size_t> distribution(0, chars.size() - 1);

        for (int i = 0; i < length; ++i) {
            result.push_back(chars[distribution(rd)]);
        }
    } else {
        std::uniform_int_distribution<size_t> dist(0, chars.size() - 1);

        for (int i = 0; i < length; ++i) {
            result.push_back(chars[dist(thread_engine)]);
        }
    }

    return result;
}

}  // namespace atom::utils
