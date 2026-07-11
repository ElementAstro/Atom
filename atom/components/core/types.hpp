/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Basic Component Types Definition and Some Utilities

**************************************************/

#ifndef ATOM_COMPONENT_TYPES_HPP
#define ATOM_COMPONENT_TYPES_HPP

#include <any>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

#include "atom/meta/enum.hpp"

namespace atom::components {

/**
 * @brief Identifier returned by event subscription APIs; 0 is invalid.
 */
using EventCallbackId = std::uint64_t;

/**
 * @brief Event payload exchanged between components and the registry.
 */
struct Event {
    std::string name;
    std::any data;
    std::string source;
    std::chrono::steady_clock::time_point timestamp{
        std::chrono::steady_clock::now()};
};

/**
 * @brief Callback invoked when a subscribed event fires.
 */
using EventCallback = std::function<void(const Event&)>;

}  // namespace atom::components

enum class ComponentType {
    NONE,
    SHARED,
    SHARED_INJECTED,
    SCRIPT,
    EXECUTABLE,
    TASK,
    LAST_ENUM_VALUE
};

template <>
struct atom::meta::EnumTraits<ComponentType> {
    static constexpr std::array<ComponentType, 7> VALUES = {
        ComponentType::NONE,
        ComponentType::SHARED,
        ComponentType::SHARED_INJECTED,
        ComponentType::SCRIPT,
        ComponentType::EXECUTABLE,
        ComponentType::TASK,
        ComponentType::LAST_ENUM_VALUE};

    static constexpr std::array<std::string_view, 7> NAMES = {
        "NONE",       "SHARED", "SHARED_INJECTED", "SCRIPT",
        "EXECUTABLE", "TASK",   "LAST_ENUM_VALUE"};
};

#endif  // ATOM_COMPONENT_TYPES_HPP
