/*
 * to_string.cpp
 *
 * Copyright (C) 2023-2024 Max Qian
 */

#include "to_string.hpp"

namespace atom::utils {

// Explicit instantiation of toString for std::string to resolve linker issues
template <>
auto toString<std::string>(const std::string& value) -> std::string {
    return value;
}
}  // namespace atom::utils
