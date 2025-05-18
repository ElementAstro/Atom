/*
 * port.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-17

Description: Port management and scanning functions

**************************************************/

#ifndef ATOM_WEB_UTILS_PORT_HPP
#define ATOM_WEB_UTILS_PORT_HPP

#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include "common.hpp"

namespace atom::web {

/**
 * @brief Check if a port is in use.
 *
 * This function checks if a port is in use by attempting to bind a socket to
 * the port. If the socket can be bound, the port is not in use.
 *
 * @param port The port number to check.
 * @return `true` if the port is in use, `false` otherwise.
 *
 * @code
 * if (atom::web::isPortInUse(8080)) {
 *     std::cout << "Port 8080 is in use." << std::endl;
 * } else {
 *     std::cout << "Port 8080 is available." << std::endl;
 * }
 * @endcode
 *
 * @throws std::invalid_argument if port is outside valid range
 * @throws std::runtime_error if socket operations fail
 */
auto isPortInUse(PortNumber auto port) -> bool;

/**
 * @brief Asynchronously check if a port is in use
 *
 * @param port The port number to check
 * @return std::future<bool> Future result of the check
 */
auto isPortInUseAsync(PortNumber auto port) -> std::future<bool>;

/**
 * @brief Get the process ID of the program running on a specific port
 *
 * @param port The port number to check
 * @return std::optional<int> The process ID if found, empty optional otherwise
 * @throws std::invalid_argument if port is outside valid range
 * @throws std::runtime_error if command execution fails
 */
auto getProcessIDOnPort(PortNumber auto port) -> std::optional<int>;

/**
 * @brief Check if there is any program running on the specified port and kill
 * it if found.
 *
 * This function checks if there is any program running on the specified port by
 * querying the system. If a program is found, it will be terminated.
 *
 * @param port The port number to check.
 * @return `true` if a program was found and terminated, `false` otherwise.
 *
 * @code
 * if (atom::web::checkAndKillProgramOnPort(8080)) {
 *     std::cout << "Program on port 8080 was terminated." << std::endl;
 * } else {
 *     std::cout << "No program running on port 8080." << std::endl;
 * }
 * @endcode
 *
 * @throws std::invalid_argument if port is outside valid range
 * @throws std::runtime_error if socket operations fail
 * @throws std::system_error if process termination fails
 */
auto checkAndKillProgramOnPort(PortNumber auto port) -> bool;

/**
 * @brief Scan a specific port on a given host to check if it's open
 *
 * @param host The hostname or IP address to scan
 * @param port The port number to scan
 * @param timeout The maximum time to wait for a connection (default: 2 seconds)
 * @return true if the port is open, false otherwise
 */
auto scanPort(const std::string& host, uint16_t port,
              std::chrono::milliseconds timeout =
                  std::chrono::milliseconds(2000)) -> bool;

/**
 * @brief Scan a range of ports on a given host to find open ones
 *
 * @param host The hostname or IP address to scan
 * @param startPort The beginning of the port range to scan
 * @param endPort The end of the port range to scan
 * @param timeout The maximum time to wait for each connection attempt
 * @return std::vector<uint16_t> List of open ports
 */
auto scanPortRange(
    const std::string& host, uint16_t startPort, uint16_t endPort,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    -> std::vector<uint16_t>;

/**
 * @brief Asynchronously scan a range of ports on a given host
 *
 * @param host The hostname or IP address to scan
 * @param startPort The beginning of the port range to scan
 * @param endPort The end of the port range to scan
 * @param timeout The maximum time to wait for each connection attempt
 * @return std::future<std::vector<uint16_t>> Future result containing list of
 * open ports
 */
auto scanPortRangeAsync(
    const std::string& host, uint16_t startPort, uint16_t endPort,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    -> std::future<std::vector<uint16_t>>;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_PORT_HPP
