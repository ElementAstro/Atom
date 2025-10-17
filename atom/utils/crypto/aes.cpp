/*
 * aes.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "aes.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

#include <spdlog/spdlog.h>

#include "atom/error/exception.hpp"

namespace atom::utils {

// Template functions and helper classes are now in aes_impl.hpp
// Only non-template functions remain in this file

auto calculateHash(const std::string& data,
                   const EVP_MD* (*hashFunction)()) noexcept -> std::string {
    try {
        if (data.empty()) {
            spdlog::warn("Empty data provided for hash calculation");
            return "";
        }

        MessageDigestContext context;
        const EVP_MD* messageDigest = hashFunction();

        if (!messageDigest) {
            spdlog::error("Invalid hash function");
            return "";
        }

        if (EVP_DigestInit_ex(context.get(), messageDigest, nullptr) != 1) {
            spdlog::error("Failed to initialize digest");
            return "";
        }

        if (EVP_DigestUpdate(context.get(), data.c_str(), data.size()) != 1) {
            spdlog::error("Failed to update digest");
            return "";
        }

        std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
        unsigned int lengthOfHash = 0;

        if (EVP_DigestFinal_ex(context.get(), hash.data(), &lengthOfHash) !=
            1) {
            spdlog::error("Failed to finalize digest");
            return "";
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < lengthOfHash; ++i) {
            oss << std::setw(2) << static_cast<int>(hash[i]);
        }

        return oss.str();
    } catch (const std::exception& ex) {
        spdlog::error("Exception in calculateHash: {}", ex.what());
        return "";
    }
}

auto calculateSha224(const std::string& data) noexcept -> std::string {
    spdlog::info("Calculating SHA-224 hash");
    return calculateHash(data, EVP_sha224);
}

auto calculateSha384(const std::string& data) noexcept -> std::string {
    spdlog::info("Calculating SHA-384 hash");
    return calculateHash(data, EVP_sha384);
}

auto calculateSha512(const std::string& data) noexcept -> std::string {
    spdlog::info("Calculating SHA-512 hash");
    return calculateHash(data, EVP_sha512);
}

}  // namespace atom::utils
