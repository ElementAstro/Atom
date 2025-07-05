/*
 * env_scoped.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Scoped environment variable management implementation

**************************************************/

#include "env_scoped.hpp"

#include "env_core.hpp"
#include <spdlog/spdlog.h>

namespace atom::utils {

ScopedEnv::ScopedEnv(const String& key, const String& value)
    : mKey(key), mHadValue(false) {
    spdlog::debug("Creating scoped environment variable: {}={}", key, value);
    mOriginalValue = EnvCore::getEnv(key, "");
    mHadValue = !mOriginalValue.empty();
    EnvCore::setEnv(key, value);
}

ScopedEnv::~ScopedEnv() {
    spdlog::debug("Destroying scoped environment variable: {}", mKey);
    if (mHadValue) {
        EnvCore::setEnv(mKey, mOriginalValue);
    } else {
        EnvCore::unsetEnv(mKey);
    }
}

auto EnvScoped::createScopedEnv(const String& key, const String& value)
    -> std::shared_ptr<ScopedEnv> {
    return std::make_shared<ScopedEnv>(key, value);
}

auto EnvScoped::createUniqueScopedEnv(const String& key, const String& value)
    -> std::unique_ptr<ScopedEnv> {
    return std::make_unique<ScopedEnv>(key, value);
}

}  // namespace atom::utils
