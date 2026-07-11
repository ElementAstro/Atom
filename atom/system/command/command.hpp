/**
 * @file command/command.hpp
 * @brief Umbrella header for the modular command-execution subsystem.
 *
 * Aggregates the public command APIs (synchronous/streamed execution,
 * environment-aware and timed variants, process management and command
 * history) so a single include exposes the full surface that the legacy
 * `process/command.hpp` header provided.
 *
 * NOTE: async_executor.hpp is intentionally NOT included here. It declares an
 * `executeCommandAsync(const std::string&, const ExecutionConfig&, int)`
 * overload that would be ambiguous with the flat-compatible
 * `executeCommandAsync(const std::string&, bool, processLine)` in
 * executor.hpp (formerly advanced_executor) for default-argument call sites. Include
 * "atom/system/command/async_executor.hpp" explicitly when the
 * ExecutionConfig-based async API is required.
 */

#ifndef ATOM_SYSTEM_COMMAND_UMBRELLA_HPP
#define ATOM_SYSTEM_COMMAND_UMBRELLA_HPP

#include "executor.hpp"
#include "history.hpp"
#include "process_manager.hpp"
#include "utils.hpp"

#endif  // ATOM_SYSTEM_COMMAND_UMBRELLA_HPP
