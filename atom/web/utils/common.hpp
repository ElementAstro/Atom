/*
 * common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-17

Description: Common Network Utils Definitions

**************************************************/

#ifndef ATOM_WEB_UTILS_COMMON_HPP
#define ATOM_WEB_UTILS_COMMON_HPP

#include <concepts>

namespace atom::web {

// C++20 concept to ensure a type is a valid port number
template <typename T>
concept PortNumber = std::integral<T> && requires(T port) {
    { port >= 0 && port <= 65535 } -> std::same_as<bool>;
};

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_COMMON_HPP
