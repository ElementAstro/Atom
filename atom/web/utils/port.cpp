/*
 * port.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "port.hpp"

#include <format>
#include <future>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN_FLAG true
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>  // 添加这个头文件用于 close()
#define WIN_FLAG false
#endif

#include "atom/log/loguru.hpp"
#include "atom/system/command.hpp"
#include "socket.hpp"

namespace atom::web {

auto getProcessIDOnPort(PortNumber auto port) -> std::optional<int> {
    try {
        if (port < 0 || port > 65535) {
            throw std::invalid_argument("Invalid port number");
        }

        std::string cmd;
#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
        cmd = std::format(
            "{}{}",
            (WIN_FLAG ? R"(netstat -ano | find "LISTENING" | find ")"
                      : "lsof -i :{} -t"),
            port);
#else
        cmd = fmt::format(
            "{}{}",
            (WIN_FLAG ? "netstat -ano | find \"LISTENING\" | find \""
                      : "lsof -i :{} -t"),
            port);
#endif

        std::string pidStr = atom::system::executeCommand(
            cmd, false, [port](const std::string& line) -> bool {
                if (WIN_FLAG) {
                    // For Windows, look for the line containing the port and
                    // extract PID
                    if (line.find(std::to_string(port)) != std::string::npos) {
                        return true;
                    }
                    return false;
                } else {
                    // For Linux/Mac, the command directly returns PID
                    return true;
                }
            });

        if (pidStr.empty()) {
            return std::nullopt;
        }

        pidStr.erase(pidStr.find_last_not_of('\n') + 1);

        if (WIN_FLAG) {
            // Extract PID from Windows netstat output format
            std::regex pidPattern(R"(\s+(\d+)\s*)");
            std::smatch matches;
            if (std::regex_search(pidStr, matches, pidPattern) &&
                matches.size() > 1) {
                return std::stoi(matches[1].str());
            }
        } else {
            // Direct PID from lsof output
            return std::stoi(pidStr);
        }

        return std::nullopt;
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid port argument: {}", e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error getting process ID on port {}: {}", port, e.what());
        return std::nullopt;
    }
}

auto isPortInUse(PortNumber auto port) -> bool {
    try {
        if (port < 0 || port > 65535) {
            throw std::invalid_argument("Invalid port number");
        }

        if (!initializeWindowsSocketAPI()) {
            LOG_F(ERROR, "Failed to initialize Windows Socket API");
            return true;  // Assume port in use if we can't check properly
        }

        int sockfd = createSocket();
        if (sockfd < 0) {
            LOG_F(ERROR, "Failed to create socket for port check");
            return true;  // Assume port in use if we can't create socket
        }

        auto socketGuard = [](void* ptr) {
            int fd = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
#ifdef _WIN32
            closesocket(fd);
            WSACleanup();
#else
            ::close(fd);
#endif
        };

        std::unique_ptr<void, decltype(socketGuard)> socketCloser(
            reinterpret_cast<void*>(static_cast<intptr_t>(sockfd)),
            socketGuard);

        bool inUse = !bindSocket(sockfd, static_cast<uint16_t>(port));
        return inUse;
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid port argument: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error checking if port {} is in use: {}", port, e.what());
        return true;
    }
}

auto isPortInUseAsync(PortNumber auto port) -> std::future<bool> {
    return std::async(std::launch::async,
                      [port]() { return isPortInUse(port); });
}

auto checkAndKillProgramOnPort(PortNumber auto port) -> bool {
    try {
        if (port < 0 || port > 65535) {
            throw std::invalid_argument("Invalid port number");
        }

        if (isPortInUse(port)) {
            auto processID = getProcessIDOnPort(port);
            if (!processID) {
                LOG_F(INFO, "No process found using port {}", port);
                return false;
            }

            std::string killCmd;
#if defined(_WIN32)
            killCmd = std::format("taskkill /F /PID {}", *processID);
#else
            killCmd = std::format("kill -9 {}", *processID);
#endif
            LOG_F(INFO, "Killing process {} on port {}", *processID, port);
            [[maybe_unused]] std::string result =
                atom::system::executeCommand(killCmd);

            // Wait a bit for the process to be killed
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            return !isPortInUse(port);
        } else {
            LOG_F(INFO, "Port {} is not in use", port);
            return false;
        }
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid port argument: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error checking port {}: {}", port, e.what());
        return false;
    }
}

auto scanPort(const std::string& host, uint16_t port,
              std::chrono::milliseconds timeout) -> bool {
    try {
        if (!initializeWindowsSocketAPI()) {
            LOG_F(ERROR, "Failed to initialize Windows Socket API");
            return false;
        }

        int sockfd = createSocket();
        if (sockfd < 0) {
            LOG_F(ERROR, "Failed to create socket for port scan");
            return false;
        }

        auto socketGuard = [](void* ptr) {
            int fd = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
#ifdef _WIN32
            closesocket(fd);
            WSACleanup();
#else
            ::close(fd);
#endif
        };

        std::unique_ptr<void, decltype(socketGuard)> socketCloser(
            reinterpret_cast<void*>(static_cast<intptr_t>(sockfd)),
            socketGuard);

        struct addrinfo hints{};
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo* result = nullptr;
        int ret = getaddrinfo(host.c_str(), std::to_string(port).c_str(),
                              &hints, &result);
        if (ret != 0) {
            LOG_F(ERROR, "Failed to resolve host '{}': {}", host,
                  gai_strerror(ret));
            return false;
        }

        auto addrInfoGuard = [](void* ptr) {
            if (ptr) {
                freeaddrinfo(static_cast<struct addrinfo*>(ptr));
            }
        };

        std::unique_ptr<void, decltype(addrInfoGuard)> addrInfoCloser(
            result, addrInfoGuard);

        for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
            if (connectWithTimeout(sockfd, p->ai_addr, p->ai_addrlen,
                                   timeout)) {
                return true;
            }
        }

        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Port scan error: {}", e.what());
        return false;
    }
}

auto scanPortRange(const std::string& host, uint16_t startPort,
                   uint16_t endPort, std::chrono::milliseconds timeout)
    -> std::vector<uint16_t> {
    std::vector<uint16_t> openPorts;
    try {
        if (startPort > endPort) {
            LOG_F(
                ERROR,
                "Invalid port range: start port {} is greater than end port {}",
                startPort, endPort);
            return openPorts;
        }

        for (uint16_t port = startPort; port <= endPort; ++port) {
            if (scanPort(host, port, timeout)) {
                openPorts.push_back(port);
                LOG_F(INFO, "Port {} on host '{}' is open", port, host);
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Port range scan error: {}", e.what());
    }
    return openPorts;
}

auto scanPortRangeAsync(const std::string& host, uint16_t startPort,
                        uint16_t endPort, std::chrono::milliseconds timeout)
    -> std::future<std::vector<uint16_t>> {
    return std::async(
        std::launch::async, [host, startPort, endPort, timeout]() {
            return scanPortRange(host, startPort, endPort, timeout);
        });
}

}  // namespace atom::web
