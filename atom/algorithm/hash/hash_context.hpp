/*
 * hash_context.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: RAII-style context management for hash computation using
             OpenSSL EVP interface.

**************************************************/

#ifndef ATOM_ALGORITHM_HASH_HASH_CONTEXT_HPP
#define ATOM_ALGORITHM_HASH_HASH_CONTEXT_HPP

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include "../core/rust_numeric.hpp"
#include "keccak.hpp"  // For K_HASH_SIZE

namespace atom::algorithm {

/**
 * @brief Context management class for hash computation.
 *
 * Provides RAII-style context management for hash computation, simplifying the
 * process.
 */
class HashContext {
public:
    /**
     * @brief Constructs a new hash context.
     */
    HashContext() noexcept;

    /**
     * @brief Destructor, automatically cleans up resources.
     */
    ~HashContext() noexcept;

    /**
     * @brief Disable copy operations.
     */
    HashContext(const HashContext&) = delete;
    HashContext& operator=(const HashContext&) = delete;

    /**
     * @brief Enable move operations.
     */
    HashContext(HashContext&&) noexcept;
    HashContext& operator=(HashContext&&) noexcept;

    /**
     * @brief Updates the hash computation with data.
     *
     * @param data Pointer to the data.
     * @param length Length of the data.
     * @return bool True if the operation was successful, false otherwise.
     */
    bool update(const void* data, usize length) noexcept;

    /**
     * @brief Updates the hash computation with data from a string view.
     *
     * @param data Input string view.
     * @return bool True if the operation was successful, false otherwise.
     */
    bool update(std::string_view data) noexcept;

    /**
     * @brief Updates the hash computation with data from a span.
     *
     * @param data Input data span.
     * @return bool True if the operation was successful, false otherwise.
     */
    bool update(std::span<const std::byte> data) noexcept;

    /**
     * @brief Finalizes the hash computation and retrieves the result.
     *
     * @return std::optional<std::array<u8, K_HASH_SIZE>> The hash result,
     * or std::nullopt on failure.
     */
    [[nodiscard]] std::optional<std::array<u8, K_HASH_SIZE>>
    finalize() noexcept;

private:
    struct ContextImpl;
    std::unique_ptr<ContextImpl> impl_;
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_HASH_HASH_CONTEXT_HPP
