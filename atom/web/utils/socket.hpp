/*
 * socket.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_WEB_UTILS_SOCKET_HPP
#define ATOM_WEB_UTILS_SOCKET_HPP

#include <chrono>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#endif

struct sockaddr;

namespace atom::web {

/**
 * @brief Initialize networking subsystem (Windows-specific)
 *
 * @return true if initialization succeeded, false otherwise
 * @throws std::runtime_error if initialization fails with a specific error
 * message
 */
auto initializeWindowsSocketAPI() -> bool;

/**
 * @brief Create a new socket
 *
 * @return int Socket file descriptor, -1 on error
 * @throws std::runtime_error if socket creation fails
 */
auto createSocket() -> int;

/**
 * @brief Bind a socket to a specific port
 *
 * @param sockfd Socket file descriptor
 * @param port Port number to bind to
 * @return bool True if binding succeeded, false otherwise
 */
auto bindSocket(int sockfd, uint16_t port) -> bool;

/**
 * @brief Set a socket to non-blocking mode
 *
 * @param sockfd Socket file descriptor
 * @return bool True if setting succeeded, false otherwise
 */
auto setSocketNonBlocking(int sockfd) -> bool;

/**
 * @brief Connect to a remote address with a timeout
 *
 * @param sockfd Socket file descriptor
 * @param addr Remote address to connect to
 * @param addrlen Length of the address structure
 * @param timeout Maximum time to wait for connection
 * @return bool True if connection succeeded, false otherwise
 */
auto connectWithTimeout(int sockfd, const struct sockaddr* addr,
                        socklen_t addrlen, std::chrono::milliseconds timeout)
    -> bool;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_SOCKET_HPP
