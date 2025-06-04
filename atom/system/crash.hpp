/*
 * crash.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-4

Description: Crash Report

**************************************************/

#ifndef ATOM_SYSTEM_CRASH_HPP
#define ATOM_SYSTEM_CRASH_HPP

#include <string_view>

namespace atom::system {
/**
 * @brief Save crash log with detailed system information
 * @param error_msg The detailed information of the crash log.
 *
 * This function is used to save the log information when the program crashes,
 * which is helpful for further debugging and analysis. The function automatically
 * collects system information, stack traces, environment variables, and creates
 * crash dump files (on Windows).
 *
 * @note Make sure the crash log directory is writable before calling this function.
 * On Windows, this function will also create a minidump file for advanced debugging.
 */
void saveCrashLog(std::string_view error_msg);

/**
 * @brief Get comprehensive system information for crash reports
 * @return A formatted string containing system information including OS, CPU, memory, and disk usage
 *
 * This function collects detailed system information that is useful for crash analysis,
 * including operating system details, CPU usage and specifications, memory status,
 * and disk usage information.
 */
[[nodiscard]] auto getSystemInfo() -> std::string;

}  // namespace atom::system

#endif
