/**
 * @file process/command.hpp
 * @brief Command execution API.
 *
 * The implementation now lives in the modular `atom/system/command/` tree
 * (executor, advanced/async executors, process manager, history, cache,
 * security, statistics, rate limiting, ...). This header forwards to the
 * command umbrella so existing `#include "atom/system/process/command.hpp"`
 * call sites keep resolving to the consolidated implementation.
 */

#ifndef ATOM_SYSTEM_COMMAND_HPP
#define ATOM_SYSTEM_COMMAND_HPP

#include "atom/system/command/command.hpp"

#endif  // ATOM_SYSTEM_COMMAND_HPP
