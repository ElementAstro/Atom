/*
 * validation.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_VALIDATION_HPP
#define ATOM_SYSTEM_COMMAND_VALIDATION_HPP

#include <string>

#include "atom/macro.hpp"
#include "types.hpp"

namespace atom::system {

/**
 * @brief Validate a command for security and safety
 *
 * @param command The command to validate
 * @return true if command is safe to execute
 */
ATOM_NODISCARD auto validateCommand(const std::string &command) -> bool;

/**
 * @brief Validate a command with detailed security analysis
 *
 * @param command The command to validate
 * @return ValidationResult with detailed validation information
 */
ATOM_NODISCARD auto validateCommandDetailed(const std::string &command)
    -> ValidationResult;

/**
 * @brief Sanitize a command by removing or escaping dangerous elements
 *
 * @param command The command to sanitize
 * @return Sanitized command string
 */
ATOM_NODISCARD auto sanitizeCommand(const std::string &command) -> std::string;

/**
 * @brief Check if a command requires elevated privileges
 *
 * @param command The command to check
 * @return true if the command typically requires elevated privileges
 */
ATOM_NODISCARD auto requiresElevatedPrivileges(const std::string &command) -> bool;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_COMMAND_VALIDATION_HPP
