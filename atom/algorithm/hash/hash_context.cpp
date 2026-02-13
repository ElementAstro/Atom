/*
 * hash_context.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: RAII-style context management for hash computation using
             OpenSSL EVP interface.

**************************************************/

#include "hash_context.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <utility>

namespace atom::algorithm {

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

bool HashContext::update(const void *data, usize length) noexcept {
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

std::optional<std::array<u8, K_HASH_SIZE>> HashContext::finalize() noexcept {
    if (!impl_ || !impl_->initialized)
        return std::nullopt;

    std::array<u8, K_HASH_SIZE> result{};
    unsigned int resultLen = 0;

    if (EVP_DigestFinal_ex(impl_->ctx, result.data(), &resultLen) != 1 ||
        resultLen != K_HASH_SIZE) {
        return std::nullopt;
    }

    return result;
}

}  // namespace atom::algorithm
