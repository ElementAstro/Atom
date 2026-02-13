/*
 * statistics.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_STATISTICS_HPP
#define ATOM_SYSTEM_COMMAND_STATISTICS_HPP

#include <string>

#include "atom/macro.hpp"
#include "config.hpp"

namespace atom::system {

/**
 * @brief Get execution statistics and performance metrics
 *
 * @return String containing formatted statistics
 */
ATOM_NODISCARD auto getExecutionStatistics() -> std::string;

/**
 * @brief Clear execution statistics
 */
void clearExecutionStatistics();

/**
 * @brief Get execution metrics
 * @return Current execution metrics
 */
ATOM_NODISCARD auto getExecutionMetrics() -> const CommandSystemMetrics&;

/**
 * @brief Reset execution statistics
 */
void resetExecutionStatistics();

/**
 * @brief Increment total execution count
 */
void incrementTotalExecutions();

/**
 * @brief Increment successful execution count
 */
void incrementSuccessfulExecutions();

/**
 * @brief Increment failed execution count
 */
void incrementFailedExecutions();

/**
 * @brief Increment timed out execution count
 */
void incrementTimedOutExecutions();

/**
 * @brief Add to total execution time
 * @param milliseconds Time to add in milliseconds
 */
void addExecutionTime(long long milliseconds);

}  // namespace atom::system

#endif  // ATOM_SYSTEM_COMMAND_STATISTICS_HPP
