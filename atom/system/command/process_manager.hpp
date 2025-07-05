/*
 * process_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_PROCESS_MANAGER_HPP
#define ATOM_SYSTEM_COMMAND_PROCESS_MANAGER_HPP

#include <string>
#include <vector>

namespace atom::system {

/**
 * @brief Kill a process by its name.
 *
 * @param processName The name of the process to kill.
 * @param signal The signal to send to the process.
 */
void killProcessByName(const std::string &processName, int signal);

/**
 * @brief Kill a process by its PID.
 *
 * @param pid The PID of the process to kill.
 * @param signal The signal to send to the process.
 */
void killProcessByPID(int pid, int signal);

/**
 * @brief Start a process and return the process ID and handle.
 *
 * @param command The command to execute.
 * @return A pair containing the process ID as an integer and the process handle
 * as a void pointer.
 */
auto startProcess(const std::string &command) -> std::pair<int, void *>;

/**
 * @brief Get a list of running processes containing the specified substring.
 *
 * @param substring The substring to search for in process names.
 * @return A vector of pairs containing PIDs and process names.
 */
auto getProcessesBySubstring(const std::string &substring)
    -> std::vector<std::pair<int, std::string>>;

}  // namespace atom::system

#endif
