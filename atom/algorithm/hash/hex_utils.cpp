/*
 * hex_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Hexadecimal string conversion utilities implementation.

**************************************************/

#include "hex_utils.hpp"

#include <algorithm>
#include <charconv>
#include <memory_resource>
#include <stdexcept>
#include <system_error>

#include "../core/hex_utils.hpp"  // isHexDigit (shared primitive)
#include "atom/error/exception.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/exception/all.hpp>
#include <boost/scope_exit.hpp>
#endif

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

// Thread-local PMR memory resource pool for managing small memory allocations
thread_local std::pmr::synchronized_pool_resource hex_tls_memory_pool{};

auto hexstringFromData(std::string_view data) noexcept(false) -> std::string {
    const char *hexChars = "0123456789ABCDEF";

    // Create string using PMR memory resource to reduce memory allocations
    std::pmr::string output(&hex_tls_memory_pool);

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
        THROW_RUNTIME_ERROR(std::string("Failed to convert to hex: ") +
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
        THROW_INVALID_ARGUMENT("Hex string length must be even");
#endif
    }

    // Use memory resource pool to improve small allocation performance
    std::pmr::string result(&hex_tls_memory_pool);

    try {
        result.resize(data.size() / 2);

        // Process conversions in parallel to improve performance
        const usize length = data.size() / 2;

        // Use block processing to enhance data locality
        constexpr usize BLOCK_SIZE = 64;
        const usize numBlocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;

        for (usize block = 0; block < numBlocks; ++block) {
            const usize blockStart = block * BLOCK_SIZE;
            const usize blockEnd = std::min(blockStart + BLOCK_SIZE, length);

            for (usize i = blockStart; i < blockEnd; ++i) {
                const usize pos = i * 2;
                u8 byte = 0;

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
                    THROW_INVALID_ARGUMENT(
                        "Invalid hex character at position " +
                        std::to_string(pos));
#endif
                }

                result[i] = static_cast<char>(byte);
            }
        }
    } catch (const atom::error::InvalidArgument &) {
        throw;  // Rethrow InvalidArgument exceptions directly
    } catch (const std::exception &e) {
#ifdef ATOM_USE_BOOST
        throw boost::enable_error_info(std::runtime_error(
            std::string("Failed to convert from hex: ") + e.what()));
#else
        THROW_RUNTIME_ERROR(std::string("Failed to convert from hex: ") +
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
                       [](char c) { return isHexDigit(c); });
}

}  // namespace atom::algorithm
