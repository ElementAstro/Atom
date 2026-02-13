/*
 * stacktrace_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Utility functions for stacktrace processing

**************************************************/

#ifndef ATOM_ERROR_STACKTRACE_UTILS_HPP
#define ATOM_ERROR_STACKTRACE_UTILS_HPP

#include <cstdint>
#include <string>

namespace atom::error {
namespace stacktrace_utils {

/**
 * @brief Demangle C++ function name
 * @param mangled Mangled function name
 * @return Demangled function name, or original if demangling fails
 */
std::string demangle(const std::string& mangled);

/**
 * @brief Prettify stacktrace output
 * @param input Raw stacktrace string
 * @return Prettified stacktrace string
 */
std::string prettify(const std::string& input);

/**
 * @brief Format memory address
 * @param address Memory address
 * @return Formatted address string
 */
std::string formatAddress(uintptr_t address);

/**
 * @brief Get base name from file path
 * @param path Full file path
 * @return Base name (filename only)
 */
std::string getBaseName(const std::string& path);

/**
 * @brief Check if a string contains a mangled C++ name
 * @param str String to check
 * @return true if string appears to contain mangled names
 */
bool containsMangledNames(const std::string& str);

}  // namespace stacktrace_utils
}  // namespace atom::error

#endif  // ATOM_ERROR_STACKTRACE_UTILS_HPP
