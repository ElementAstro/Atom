/*
 * env_scoped.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Scoped environment variable management

**************************************************/

#ifndef ATOM_SYSTEM_ENV_SCOPED_HPP
#define ATOM_SYSTEM_ENV_SCOPED_HPP

#include <memory>

#include "atom/containers/high_performance.hpp"

namespace atom::utils {

using atom::containers::String;

/**
 * @brief Temporary environment variable scope class
 */
class ScopedEnv {
public:
    /**
     * @brief Constructor, sets temporary environment variable
     * @param key Environment variable name
     * @param value Environment variable value
     */
    ScopedEnv(const String& key, const String& value);

    /**
     * @brief Destructor, restores original environment variable value
     */
    ~ScopedEnv();

    // Non-copyable but movable
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
    ScopedEnv(ScopedEnv&&) = default;
    ScopedEnv& operator=(ScopedEnv&&) = default;

private:
    String mKey;
    String mOriginalValue;
    bool mHadValue;
};

/**
 * @brief Scoped environment variable management utilities
 */
class EnvScoped {
public:
    /**
     * @brief Creates a temporary environment variable scope
     * @param key Environment variable name
     * @param value Environment variable value
     * @return Shared pointer to scope object
     */
    static auto createScopedEnv(const String& key, const String& value)
        -> std::shared_ptr<ScopedEnv>;

    /**
     * @brief Creates a unique scoped environment variable
     * @param key Environment variable name
     * @param value Environment variable value
     * @return Unique pointer to scope object
     */
    static auto createUniqueScopedEnv(const String& key, const String& value)
        -> std::unique_ptr<ScopedEnv>;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_SCOPED_HPP
