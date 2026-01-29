/*
 * port.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "port.hpp"

#include <algorithm>
#include <format>
#include <future>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#endif

#include <spdlog/spdlog.h>
#include "atom/system/process/command.hpp"
#include "socket.hpp"

namespace atom::web {

namespace {
constexpr uint16_t MIN_PORT = 1;
constexpr uint16_t MAX_PORT = 65535;
constexpr int INVALID_SOCKET_VALUE = -1;

class SocketGuard {
public:
    explicit SocketGuard(int socket) : socket_(socket) {}

    ~SocketGuard() {
        if (socket_ != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            closesocket(socket_);
            WSACleanup();
#else
            ::close(socket_);
#endif
        }
    }

    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    SocketGuard(SocketGuard&&) = delete;
    SocketGuard& operator=(SocketGuard&&) = delete;

private:
    int socket_;
};

auto validatePort(auto port) -> void {
    if (port < MIN_PORT || port > MAX_PORT) {
        throw std::invalid_argument("Port number must be between 1 and 65535");
    }
}

auto getSystemCommand(auto port) -> std::string {
#ifdef _WIN32
    return std::format("netstat -ano | findstr \"LISTENING\" | findstr \"{}\"",
                       port);
#else
    return std::format("lsof -i :{} -t", port);
#endif
}

auto parseProcessId(const std::string& output) -> std::optional<int> {
    if (output.empty()) {
        return std::nullopt;
    }

    try {
#ifdef _WIN32
        std::regex pidPattern(R"(\s+(\d+)\s*$)");
        std::smatch matches;
        if (std::regex_search(output, matches, pidPattern) &&
            matches.size() > 1) {
            return std::stoi(matches[1].str());
        }
#else
        std::string trimmed = output;
        trimmed.erase(trimmed.find_last_not_of(" \n\r\t") + 1);
        if (!trimmed.empty()) {
            return std::stoi(trimmed);
        }
#endif
    } catch (const std::exception& e) {
        spdlog::debug("Failed to parse process ID from output '{}': {}", output,
                      e.what());
    }

    return std::nullopt;
}
}  // namespace

auto getProcessIDOnPort(PortNumber auto port) -> std::optional<int> {
    try {
        validatePort(port);

        std::string cmd = getSystemCommand(port);
        spdlog::trace("Executing command: {}", cmd);

        std::string output = atom::system::executeCommand(cmd);

        return parseProcessId(output);

    } catch (const std::invalid_argument& e) {
        spdlog::error("Invalid port argument: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error getting process ID on port {}: {}", port,
                      e.what());
        return std::nullopt;
    }
}

auto isPortInUse(PortNumber auto port) -> bool {
    try {
        validatePort(port);

        if (!initializeWindowsSocketAPI()) {
            spdlog::error("Failed to initialize Windows Socket API");
            return true;
        }

        int sockfd = createSocket();
        if (sockfd < 0) {
            spdlog::error("Failed to create socket for port check");
            return true;
        }

        SocketGuard guard(sockfd);

        bool inUse = !bindSocket(sockfd, static_cast<uint16_t>(port));
        spdlog::trace("Port {} is {}", port, inUse ? "in use" : "available");

        return inUse;

    } catch (const std::invalid_argument& e) {
        spdlog::error("Invalid port argument: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error checking if port {} is in use: {}", port,
                      e.what());
        return true;
    }
}

auto isPortInUseAsync(PortNumber auto port) -> std::future<bool> {
    return std::async(std::launch::async,
                      [port]() { return isPortInUse(port); });
}

auto checkAndKillProgramOnPort(PortNumber auto port) -> bool {
    try {
        validatePort(port);

        if (!isPortInUse(port)) {
            spdlog::info("Port {} is not in use", port);
            return false;
        }

        auto processID = getProcessIDOnPort(port);
        if (!processID) {
            spdlog::info("No process found using port {}", port);
            return false;
        }

        std::string killCmd;
#ifdef _WIN32
        killCmd = std::format("taskkill /F /PID {}", *processID);
#else
        killCmd = std::format("kill -9 {}", *processID);
#endif

        spdlog::info("Killing process {} on port {}", *processID, port);

        try {
            static_cast<void>(atom::system::executeCommand(killCmd));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            bool killed = !isPortInUse(port);
            if (killed) {
                spdlog::info("Successfully killed process {} on port {}",
                             *processID, port);
            } else {
                spdlog::warn("Failed to kill process {} on port {}", *processID,
                             port);
            }
            return killed;

        } catch (const std::exception& e) {
            spdlog::error("Failed to execute kill command: {}", e.what());
            return false;
        }

    } catch (const std::invalid_argument& e) {
        spdlog::error("Invalid port argument: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error checking and killing program on port {}: {}", port,
                      e.what());
        return false;
    }
}

auto scanPort(const std::string& host, uint16_t port,
              std::chrono::milliseconds timeout) -> bool {
    try {
        if (host.empty()) {
            spdlog::error("Host cannot be empty");
            return false;
        }

        if (!initializeWindowsSocketAPI()) {
            spdlog::error("Failed to initialize Windows Socket API");
            return false;
        }

        int sockfd = createSocket();
        if (sockfd < 0) {
            spdlog::error("Failed to create socket for port scan");
            return false;
        }

        SocketGuard guard(sockfd);

        struct addrinfo hints {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo* result = nullptr;
        int ret = getaddrinfo(host.c_str(), std::to_string(port).c_str(),
                              &hints, &result);
        if (ret != 0) {
            spdlog::error("Failed to resolve host '{}': {}", host,
                          gai_strerror(ret));
            return false;
        }

        std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> addrGuard(
            result, freeaddrinfo);

        for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
            if (connectWithTimeout(sockfd, p->ai_addr, p->ai_addrlen,
                                   timeout)) {
                spdlog::trace("Port {} on host '{}' is open", port, host);
                return true;
            }
        }

        spdlog::trace("Port {} on host '{}' is closed", port, host);
        return false;

    } catch (const std::exception& e) {
        spdlog::error("Port scan error for {}:{}: {}", host, port, e.what());
        return false;
    }
}

auto scanPortRange(const std::string& host, uint16_t startPort,
                   uint16_t endPort,
                   std::chrono::milliseconds timeout) -> std::vector<uint16_t> {
    std::vector<uint16_t> openPorts;

    try {
        if (startPort > endPort) {
            throw std::invalid_argument(std::format(
                "Invalid port range: start port {} is greater than end port {}",
                startPort, endPort));
        }

        if (host.empty()) {
            throw std::invalid_argument("Host cannot be empty");
        }

        const size_t portCount = endPort - startPort + 1;
        openPorts.reserve((std::min)(portCount, size_t{100}));

        spdlog::debug("Scanning {} ports on host '{}' from {} to {}", portCount,
                      host, startPort, endPort);

        for (uint16_t port = startPort; port <= endPort; ++port) {
            if (scanPort(host, port, timeout)) {
                openPorts.push_back(port);
                spdlog::info("Found open port {} on host '{}'", port, host);
            }
        }

        spdlog::debug("Scan completed: found {} open ports on host '{}'",
                      openPorts.size(), host);

    } catch (const std::exception& e) {
        spdlog::error("Port range scan error for {}:{}-{}: {}", host, startPort,
                      endPort, e.what());
    }

    return openPorts;
}

auto getServiceName(uint16_t port) -> std::string {
    static const std::unordered_map<uint16_t, std::string> portToService = {
        {21, "ftp"},
        {22, "ssh"},
        {23, "telnet"},
        {25, "smtp"},
        {53, "dns"},
        {80, "http"},
        {110, "pop3"},
        {143, "imap"},
        {443, "https"},
        {465, "smtps"},
        {993, "imaps"},
        {995, "pop3s"},
        {3306, "mysql"},
        {5432, "postgresql"},
        {6379, "redis"},
        {27017, "mongodb"},
        {9200, "elasticsearch"},
        {5672, "rabbitmq"}};

    auto it = portToService.find(port);
    return it != portToService.end() ? it->second : "";
}

auto getServicePort(std::string_view serviceName) -> std::optional<uint16_t> {
    const auto& services = getCommonServices();
    std::string lowerName(serviceName);
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = services.find(lowerName);
    return it != services.end() ? std::optional<uint16_t>(it->second)
                                : std::nullopt;
}

auto findAvailablePort(uint16_t startPort, uint16_t endPort,
                       std::string_view host) -> std::optional<uint16_t> {
    (void)host;  // Host parameter reserved for future use
    for (uint16_t port = startPort; port <= endPort; ++port) {
        if (!isPortInUse<uint16_t>(port)) {
            return port;
        }
    }
    return std::nullopt;
}

auto scanPortsConcurrent(std::string_view host,
                         const std::vector<uint16_t>& ports,
                         std::chrono::milliseconds timeout,
                         size_t maxConcurrent) -> std::vector<uint16_t> {
    std::vector<uint16_t> openPorts;
    std::mutex mutex;
    std::vector<std::future<void>> futures;

    std::string hostStr(host);

    for (size_t i = 0; i < ports.size(); ++i) {
        if (futures.size() >= maxConcurrent) {
            for (auto& f : futures) {
                if (f.valid()) {
                    f.wait();
                }
            }
            futures.clear();
        }

        futures.push_back(std::async(
            std::launch::async,
            [&openPorts, &mutex, hostStr, port = ports[i], timeout]() {
                if (atom::web::scanPort(hostStr, port, timeout)) {
                    std::lock_guard<std::mutex> lock(mutex);
                    openPorts.push_back(port);
                }
            }));
    }

    for (auto& f : futures) {
        if (f.valid()) {
            f.wait();
        }
    }

    std::sort(openPorts.begin(), openPorts.end());
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

auto getCommonServices() -> const std::unordered_map<std::string, uint16_t>& {
    static const std::unordered_map<std::string, uint16_t> services = {
        {"ftp", 21},
        {"ssh", 22},
        {"telnet", 23},
        {"smtp", 25},
        {"dns", 53},
        {"http", 80},
        {"pop3", 110},
        {"imap", 143},
        {"https", 443},
        {"smtps", 465},
        {"imaps", 993},
        {"pop3s", 995},
        {"mysql", 3306},
        {"postgresql", 5432},
        {"redis", 6379},
        {"mongodb", 27017},
        {"elasticsearch", 9200},
        {"rabbitmq", 5672}};
    return services;
}

// uint16_t
template bool isPortInUse<uint16_t>(uint16_t);
template std::future<bool> isPortInUseAsync<uint16_t>(uint16_t);
template std::optional<int> getProcessIDOnPort<uint16_t>(uint16_t);
template bool checkAndKillProgramOnPort<uint16_t>(uint16_t);

// int (for test compatibility)
template bool isPortInUse<int>(int);
template std::future<bool> isPortInUseAsync<int>(int);
template std::optional<int> getProcessIDOnPort<int>(int);
template bool checkAndKillProgramOnPort<int>(int);

}  // namespace atom::web
