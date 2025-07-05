/*
 * utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_UTILS_HPP
#define ATOM_SYSTEM_COMMAND_UTILS_HPP

#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Check if a command is available in the system.
 *
 * @param command The command to check.
 * @return A boolean indicating whether the command is available.
 */
auto isCommandAvailable(const std::string &command) -> bool;

/**
 * @brief Execute a command and return its output as a list of lines.
 *
 * @param command The command to execute.
 * @return A vector of strings, each representing a line of output.
 */
ATOM_NODISCARD auto executeCommandGetLines(const std::string &command)
    -> std::vector<std::string>;

/**
 * @brief Pipe the output of one command to another command.
 *
 * @param firstCommand The first command to execute.
 * @param secondCommand The second command that receives the output of the
 * first.
 * @return The output of the second command.
 */
ATOM_NODISCARD auto pipeCommands(const std::string &firstCommand,
                                 const std::string &secondCommand)
    -> std::string;

}  // namespace atom::system

#endif
