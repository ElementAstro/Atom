#include "utils.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <future>
#include <memory>
#include <mutex>
#include <ranges>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN_FLAG true
#define close closesocket
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#define WIN_FLAG false
#endif

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/command.hpp"

namespace atom::web {

// ====== DNS Cache Implementation ======

class DNSCache {
private:
    struct CacheEntry {
        std::vector<std::string> ipAddresses;
        std::chrono::time_point<std::chrono::steady_clock> expiryTime;
    };

    std::unordered_map<std::string, CacheEntry> cache;
    std::mutex cacheMutex;
    std::chrono::seconds ttl{300};

public:
    void setTTL(std::chrono::seconds newTtl) { ttl = newTtl; }

    bool get(const std::string& hostname,
             std::vector<std::string>& ipAddresses) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto now = std::chrono::steady_clock::now();
        auto it = cache.find(hostname);

        if (it != cache.end() && now < it->second.expiryTime) {
            ipAddresses = it->second.ipAddresses;
            return true;
        }
        return false;
    }

    void put(const std::string& hostname,
             const std::vector<std::string>& ipAddresses) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto expiryTime = std::chrono::steady_clock::now() + ttl;
        cache[hostname] = {ipAddresses, expiryTime};
    }

    void clearExpiredEntries() {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = cache.begin(); it != cache.end();) {
            if (now >= it->second.expiryTime) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
    }
};

static DNSCache g_dnsCache;

// ====== Windows Socket API Initialization ======

auto initializeWindowsSocketAPI() -> bool {
#ifdef _WIN32
    try {
        WSADATA wsaData;
        int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (ret != 0) {
            throw std::runtime_error(std::format(
                "Windows Socket API initialization failed with error code: {}",
                ret));
        }
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to initialize Windows Socket API: {}", e.what());
        return false;
    }
#endif
    return true;
}

// ====== IP Address Validation and Conversion ======

auto isValidIPv4(const std::string& ipAddress) -> bool {
    try {
        struct sockaddr_in sa;
        return inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) == 1;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "IPv4 validation error: {}", e.what());
        return false;
    }
}

auto isValidIPv6(const std::string& ipAddress) -> bool {
    try {
        struct sockaddr_in6 sa;
        return inet_pton(AF_INET6, ipAddress.c_str(), &(sa.sin6_addr)) == 1;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "IPv6 validation error: {}", e.what());
        return false;
    }
}

auto ipToString(const struct sockaddr* addr, char* strBuf, size_t bufSize)
    -> bool {
    if (!addr || !strBuf || bufSize == 0) {
        return false;
    }

    const void* src = nullptr;
    if (addr->sa_family == AF_INET) {
        src = &(reinterpret_cast<const struct sockaddr_in*>(addr))->sin_addr;
    } else if (addr->sa_family == AF_INET6) {
        src = &(reinterpret_cast<const struct sockaddr_in6*>(addr))->sin6_addr;
    } else {
        return false;
    }

    return inet_ntop(addr->sa_family, src, strBuf,
                     static_cast<socklen_t>(bufSize)) != nullptr;
}

auto getIPAddresses(const std::string& hostname) -> std::vector<std::string> {
    try {
        std::vector<std::string> results;
        if (g_dnsCache.get(hostname, results)) {
            return results;
        }

        auto addrInfo = getAddrInfo(hostname, "");
        if (!addrInfo) {
            return {};
        }

        for (const struct addrinfo* p = addrInfo.get(); p != nullptr;
             p = p->ai_next) {
            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            if (ipToString(p->ai_addr, ipStr.data(), ipStr.size())) {
                results.push_back(ipStr.data());
            }
        }

        if (!results.empty()) {
            g_dnsCache.put(hostname, results);
        }

        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error getting IP addresses for {}: {}", hostname,
              e.what());
        return {};
    }
}

auto getLocalIPAddresses() -> std::vector<std::string> {
    std::vector<std::string> results;
    try {
#ifdef _WIN32
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) != 0) {
            throw std::runtime_error("Failed to get hostname");
        }

        auto addrInfo = getAddrInfo(hostname, "");
        if (!addrInfo) {
            return {};
        }

        for (const struct addrinfo* p = addrInfo.get(); p != nullptr;
             p = p->ai_next) {
            if (p->ai_family != AF_INET && p->ai_family != AF_INET6) {
                continue;
            }

            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            if (ipToString(p->ai_addr, ipStr.data(), ipStr.size())) {
                std::string ip = ipStr.data();
                if (ip != "127.0.0.1" && ip != "::1") {
                    results.push_back(ip);
                }
            }
        }

#elif defined(__linux__) || defined(__APPLE__)
        struct ifaddrs* ifAddrStruct = nullptr;
        if (getifaddrs(&ifAddrStruct) == -1) {
            throw std::runtime_error("Failed to get interface addresses");
        }

        auto cleanup = [&ifAddrStruct](void*) {
            if (ifAddrStruct) {
                freeifaddrs(ifAddrStruct);
            }
        };
        std::unique_ptr<void, decltype(cleanup)> cleanupGuard(nullptr, cleanup);

        for (struct ifaddrs* ifa = ifAddrStruct; ifa != nullptr;
             ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }

            if ((ifa->ifa_addr->sa_family != AF_INET &&
                 ifa->ifa_addr->sa_family != AF_INET6) ||
                (ifa->ifa_flags & IFF_LOOPBACK)) {
                continue;
            }

            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            if (ipToString(ifa->ifa_addr, ipStr.data(), ipStr.size())) {
                results.push_back(ipStr.data());
            }
        }
#endif
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error getting local IP addresses: {}", e.what());
        return results;
    }
}

// ====== Socket Creation and Management ======

auto createSocket() -> int {
    try {
        int sockfd =
            static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (sockfd < 0) {
            std::array<char, 256> buf{};
#ifdef _WIN32
            int error = WSAGetLastError();
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error,
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                         buf.data(), buf.size(), NULL);
#else
            char* err_str = strerror_r(errno, buf.data(), buf.size());
            if (err_str != buf.data()) {
                // In GNU version, strerror_r might return a pointer to a static
                // string that is different from the buffer we provided
                strncpy(buf.data(), err_str, buf.size() - 1);
                buf[buf.size() - 1] = '\0';
            }
#endif
            throw std::runtime_error(
                std::format("Socket creation failed: {}", buf.data()));
        }
        return sockfd;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to create socket: {}", e.what());
#ifdef _WIN32
        WSACleanup();
#endif
        throw;
    }
}

auto bindSocket(int sockfd, uint16_t port) -> bool {
    try {
        struct sockaddr_in addr{};
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(addr)) != 0) {
            std::array<char, 256> buf{};
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEADDRINUSE) {
                DLOG_F(WARNING, "Port {} is already in use", port);
                return false;
            }
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error,
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                         buf.data(), buf.size(), NULL);
            throw std::runtime_error(
                std::format("Socket bind failed: {}", buf.data()));
#else
            if (errno == EADDRINUSE) {
                DLOG_F(WARNING, "Port {} is already in use", port);
                return false;
            }
            throw std::runtime_error(
                std::format("Socket bind failed: {}", strerror(errno)));
#endif
        }
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to bind socket: {}", e.what());
        return false;
    }
}

auto setSocketNonBlocking(int sockfd) -> bool {
    try {
#ifdef _WIN32
        unsigned long mode = 1;
        return ioctlsocket(sockfd, FIONBIO, &mode) == 0;
#else
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }
        return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to set socket non-blocking: {}", e.what());
        return false;
    }
}

auto connectWithTimeout(int sockfd, const struct sockaddr* addr,
                        socklen_t addrlen, std::chrono::milliseconds timeout)
    -> bool {
    try {
        if (!setSocketNonBlocking(sockfd)) {
            LOG_F(ERROR,
                  "Failed to set socket non-blocking for connect timeout");
            return false;
        }

        int ret = connect(sockfd, addr, addrlen);
        if (ret == 0) {
            return true;
        }

#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            return false;
        }
#endif

#ifdef _WIN32
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sockfd, &writefds);

        struct timeval tv;
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

        ret = select(sockfd + 1, nullptr, &writefds, nullptr, &tv);
#else
        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLOUT;

        ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
#endif

        if (ret <= 0) {
            return false;
        }

        int error = 0;
        socklen_t len = sizeof(error);
        ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
                         reinterpret_cast<char*>(&error), &len);

        return ret == 0 && error == 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Connect with timeout failed: {}", e.what());
        return false;
    }
}

// ====== Port and Process Management ======

auto getProcessIDOnPort(PortNumber auto port) -> std::optional<int> {
    try {
        if (port < 0 || port > 65535) {
            throw std::invalid_argument(
                std::format("Invalid port number: {}", port));
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
            cmd, false, [port](const std::string& line) {
                if (WIN_FLAG) {
                    std::regex portRegex(
                        std::format(".*:{}\\s+.*LISTENING\\s+(\\d+)", port));
                    std::smatch match;
                    if (std::regex_search(line, match, portRegex) &&
                        match.size() > 1) {
                        return true;
                    }
                    return false;
                } else {
                    return !line.empty();
                }
            });

        if (pidStr.empty()) {
            return std::nullopt;
        }

        pidStr.erase(pidStr.find_last_not_of('\n') + 1);

        if (WIN_FLAG) {
            std::regex pidRegex(R"(\s+(\d+)\s*)");
            std::smatch match;
            if (std::regex_search(pidStr, match, pidRegex) &&
                match.size() > 1) {
                return std::stoi(match[1].str());
            }
        } else {
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
            throw std::invalid_argument(
                std::format("Invalid port number: {}", port));
        }

        if (!initializeWindowsSocketAPI()) {
            throw std::runtime_error(
                "Failed to initialize networking subsystem");
        }

        int sockfd = createSocket();
        if (sockfd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        auto socketGuard = [](void* ptr) {
            if (ptr) {
                close(reinterpret_cast<intptr_t>(ptr));
#ifdef _WIN32
                WSACleanup();
#endif
            }
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
            throw std::invalid_argument(
                std::format("Invalid port number: {}", port));
        }

        if (isPortInUse(port)) {
            auto pid_opt = getProcessIDOnPort(port);
            if (!pid_opt) {
                LOG_F(WARNING,
                      "Port {} is in use but couldn't identify the process",
                      port);
                return false;
            }

            int pid = pid_opt.value();
            LOG_F(INFO, "Attempting to kill process {} on port {}", pid, port);

            try {
                atom::system::killProcessByPID(pid, 15);

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (isPortInUse(port)) {
                    LOG_F(WARNING,
                          "Process {} did not terminate gracefully, using "
                          "force kill",
                          pid);
                    atom::system::killProcessByPID(pid, 9);

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    if (isPortInUse(port)) {
                        LOG_F(ERROR, "Failed to kill process {} on port {}",
                              pid, port);
                        return false;
                    }
                }

                LOG_F(INFO, "Successfully killed process {} on port {}", pid,
                      port);
                return true;
            } catch (const atom::error::SystemCollapse& e) {
                LOG_F(ERROR, "System error killing process on port {}: {}",
                      port, e.what());
                return false;
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Unexpected error: {}", e.what());
                return false;
            }
        } else {
            LOG_F(INFO, "No program is using port {}", port);
            return true;
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
            if (ptr) {
                close(reinterpret_cast<intptr_t>(ptr));
#ifdef _WIN32
                WSACleanup();
#endif
            }
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
            LOG_F(ERROR, "getaddrinfo failed: {}", gai_strerror(ret));
            return false;
        }

        auto addrInfoGuard = [](void* ptr) {
            if (ptr) {
                freeaddrinfo(reinterpret_cast<struct addrinfo*>(ptr));
            }
        };

        std::unique_ptr<void, decltype(addrInfoGuard)> addrInfoCloser(
            result, addrInfoGuard);

        for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
            if (connectWithTimeout(sockfd, p->ai_addr, p->ai_addrlen,
                                   timeout)) {
                return true;
            }

            close(sockfd);
            sockfd = createSocket();
            if (sockfd < 0) {
                return false;
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
            throw std::invalid_argument(
                "Start port must be less than or equal to end port");
        }

        for (uint16_t port = startPort; port <= endPort; ++port) {
            if (scanPort(host, port, timeout)) {
                openPorts.push_back(port);
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

// ====== Network Connection Status ======

auto checkInternetConnectivity() -> bool {
    try {
        const std::vector<std::string> reliableHosts = {"8.8.8.8", "1.1.1.1",
                                                        "208.67.222.222"};

        for (const auto& host : reliableHosts) {
            if (scanPort(host, 53, std::chrono::milliseconds(2000))) {
                return true;
            }
        }

        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error checking internet connectivity: {}", e.what());
        return false;
    }
}

#if defined(__linux__) || defined(__APPLE__)

// ====== Address Info Management ======

auto dumpAddrInfo(
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>& dst,
    const struct addrinfo* src) -> int {
    try {
        if (src == nullptr) {
            throw std::invalid_argument("Source addrinfo cannot be null");
        }

        struct addrinfo* aiDst = nullptr;
        const struct addrinfo* aiSrc = src;
        struct addrinfo* aiCur = nullptr;
        [[maybe_unused]] struct addrinfo* aiPrev = nullptr;

        while (aiSrc != nullptr) {
            size_t aiSize = sizeof(struct addrinfo) + aiSrc->ai_addrlen;
            auto ai = std::unique_ptr<struct addrinfo, decltype(&free)>(
                static_cast<struct addrinfo*>(calloc(1, aiSize)), free);

            if (!ai) {
                throw std::runtime_error(
                    "Memory allocation failed for addrinfo");
            }

            *ai = *aiSrc;

            ai->ai_addr = reinterpret_cast<struct sockaddr*>(ai.get() + 1);
            std::memcpy(ai->ai_addr, aiSrc->ai_addr, aiSrc->ai_addrlen);

            if (aiSrc->ai_canonname != nullptr) {
                ai->ai_canonname = strdup(aiSrc->ai_canonname);
                if (ai->ai_canonname == nullptr) {
                    throw std::runtime_error(
                        "Memory allocation failed for canonical name");
                }
            } else {
                ai->ai_canonname = nullptr;
            }

            ai->ai_next = nullptr;

            if (aiDst == nullptr) {
                aiDst = ai.release();
                aiCur = aiDst;
            } else {
                aiCur->ai_next = ai.release();
                aiPrev = aiCur;
                aiCur = aiCur->ai_next;
            }

            aiSrc = aiSrc->ai_next;
        }

        dst.reset(aiDst);
        return 0;

    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid argument: {}", e.what());
        return -1;
    } catch (const std::runtime_error& e) {
        LOG_F(ERROR, "Runtime error: {}", e.what());
        return -1;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error: {}", e.what());
        return -1;
    }
}

auto addrInfoToString(const struct addrinfo* addrInfo, bool jsonFormat)
    -> std::string {
    try {
        if (addrInfo == nullptr) {
            throw std::invalid_argument("addrInfo cannot be null");
        }

        std::ostringstream oss;
        if (jsonFormat) {
            oss << "[\n";
        }

        int count = 0;
        const struct addrinfo* current = addrInfo;

        while (current != nullptr) {
            count++;

            if (jsonFormat) {
                oss << "  {\n";
                oss << "    \"ai_flags\": " << current->ai_flags << ",\n";
                oss << "    \"ai_family\": " << current->ai_family << ",\n";
                oss << "    \"ai_socktype\": " << current->ai_socktype << ",\n";
                oss << "    \"ai_protocol\": " << current->ai_protocol << ",\n";
                oss << "    \"ai_addrlen\": " << current->ai_addrlen << ",\n";
                oss << R"(    "ai_canonname": ")"
                    << (current->ai_canonname ? current->ai_canonname : "null")
                    << "\",\n";

                if (current->ai_family == AF_INET) {
                    auto addr_in = reinterpret_cast<const struct sockaddr_in*>(
                        current->ai_addr);
                    std::array<char, INET_ADDRSTRLEN> ip_str{};
                    const char* result =
                        inet_ntop(AF_INET, &addr_in->sin_addr, ip_str.data(),
                                  ip_str.size());
                    oss << R"(    "address": ")"
                        << (result ? ip_str.data() : "unknown") << "\",\n";
                    oss << R"(    "port": )" << ntohs(addr_in->sin_port)
                        << "\n";
                } else if (current->ai_family == AF_INET6) {
                    auto addr_in6 =
                        reinterpret_cast<const struct sockaddr_in6*>(
                            current->ai_addr);
                    std::array<char, INET6_ADDRSTRLEN> ip_str{};
                    const char* result =
                        inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_str.data(),
                                  ip_str.size());
                    oss << R"(    "address": ")"
                        << (result ? ip_str.data() : "unknown") << "\",\n";
                    oss << R"(    "port": )" << ntohs(addr_in6->sin6_port)
                        << "\n";
                } else {
                    oss << R"(    "address": "unknown family type",)" << "\n";
                    oss << R"(    "port": 0)" << "\n";
                }

                if (current->ai_next != nullptr) {
                    oss << "  },\n";
                } else {
                    oss << "  }\n";
                }
            } else {
                oss << "Address Info #" << count << ":\n";
                oss << "  ai_flags: " << current->ai_flags << "\n";
                oss << "  ai_family: " << current->ai_family;

                switch (current->ai_family) {
                    case AF_INET:
                        oss << " (IPv4)";
                        break;
                    case AF_INET6:
                        oss << " (IPv6)";
                        break;
                    case AF_UNSPEC:
                        oss << " (Unspecified)";
                        break;
                    default:
                        oss << " (Other)";
                        break;
                }
                oss << "\n";

                oss << "  ai_socktype: " << current->ai_socktype;

                switch (current->ai_socktype) {
                    case SOCK_STREAM:
                        oss << " (Stream/TCP)";
                        break;
                    case SOCK_DGRAM:
                        oss << " (Datagram/UDP)";
                        break;
                    case SOCK_RAW:
                        oss << " (Raw)";
                        break;
                    default:
                        oss << " (Other)";
                        break;
                }
                oss << "\n";

                oss << "  ai_protocol: " << current->ai_protocol << "\n";
                oss << "  ai_addrlen: " << current->ai_addrlen << "\n";
                oss << "  ai_canonname: "
                    << (current->ai_canonname ? current->ai_canonname : "null")
                    << "\n";

                if (current->ai_family == AF_INET) {
                    auto addr_in = reinterpret_cast<const struct sockaddr_in*>(
                        current->ai_addr);
                    std::array<char, INET_ADDRSTRLEN> ip_str{};
                    const char* result =
                        inet_ntop(AF_INET, &addr_in->sin_addr, ip_str.data(),
                                  ip_str.size());
                    oss << "  Address (IPv4): "
                        << (result ? ip_str.data() : "unknown") << "\n";
                    oss << "  Port: " << ntohs(addr_in->sin_port) << "\n";
                } else if (current->ai_family == AF_INET6) {
                    auto addr_in6 =
                        reinterpret_cast<const struct sockaddr_in6*>(
                            current->ai_addr);
                    std::array<char, INET6_ADDRSTRLEN> ip_str{};
                    const char* result =
                        inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_str.data(),
                                  ip_str.size());
                    oss << "  Address (IPv6): "
                        << (result ? ip_str.data() : "unknown") << "\n";
                    oss << "  Port: " << ntohs(addr_in6->sin6_port) << "\n";
                }
                oss << "-------------------------\n";
            }

            current = current->ai_next;
        }

        if (jsonFormat) {
            oss << "]\n";
        }

        return oss.str();
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid argument in addrInfoToString: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in addrInfoToString: {}", e.what());
        throw;
    }
}

auto getAddrInfo(const std::string& hostname, const std::string& service)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> {
    try {
        if (hostname.empty()) {
            throw std::invalid_argument("Hostname cannot be empty");
        }

        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_CANONNAME;

        struct addrinfo* result = nullptr;
        int ret = getaddrinfo(hostname.c_str(),
                              service.empty() ? nullptr : service.c_str(),
                              &hints, &result);

        if (ret != 0) {
            throw std::runtime_error(
                std::format("getaddrinfo error: {}", gai_strerror(ret)));
        }

        return {result, ::freeaddrinfo};
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid argument in getAddrInfo: {}", e.what());
        throw;
    } catch (const std::runtime_error& e) {
        LOG_F(ERROR, "Runtime error in getAddrInfo: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error in getAddrInfo: {}", e.what());
        throw;
    }
}

auto compareAddrInfo(const struct addrinfo* addrInfo1,
                     const struct addrinfo* addrInfo2) -> bool {
    try {
        if (addrInfo1 == nullptr || addrInfo2 == nullptr) {
            throw std::invalid_argument("Address info pointers cannot be null");
        }

        if (addrInfo1->ai_family != addrInfo2->ai_family ||
            addrInfo1->ai_socktype != addrInfo2->ai_socktype ||
            addrInfo1->ai_protocol != addrInfo2->ai_protocol ||
            addrInfo1->ai_addrlen != addrInfo2->ai_addrlen) {
            return false;
        }

        if (addrInfo1->ai_addr == nullptr && addrInfo2->ai_addr == nullptr) {
            return true;
        } else if (addrInfo1->ai_addr == nullptr ||
                   addrInfo2->ai_addr == nullptr) {
            return false;
        }

        return memcmp(addrInfo1->ai_addr, addrInfo2->ai_addr,
                      addrInfo1->ai_addrlen) == 0;
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid argument in compareAddrInfo: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in compareAddrInfo: {}", e.what());
        throw;
    }
}

auto filterAddrInfo(const struct addrinfo* addrInfo, int family)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> {
    try {
        if (addrInfo == nullptr) {
            throw std::invalid_argument("addrInfo cannot be null");
        }

        struct addrinfo* result = nullptr;
        struct addrinfo* tail = nullptr;

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
        auto addrInfoView = std::ranges::views::iota(0) |
                            std::ranges::views::take_while([&addrInfo](int) {
                                return addrInfo != nullptr;
                            }) |
                            std::views::transform([&addrInfo](int) {
                                auto current = addrInfo;
                                addrInfo = addrInfo->ai_next;
                                return current;
                            });

        for (const auto* node : addrInfoView) {
            if (node->ai_family == family) {
                auto newNode =
                    std::unique_ptr<struct addrinfo, decltype(&free)>(
                        static_cast<struct addrinfo*>(
                            malloc(sizeof(struct addrinfo))),
                        free);

                if (!newNode) {
                    throw std::runtime_error(
                        "Memory allocation failed for addrinfo");
                }

                *newNode = *node;
                newNode->ai_next = nullptr;

                if (node->ai_addr != nullptr) {
                    newNode->ai_addr =
                        static_cast<struct sockaddr*>(malloc(node->ai_addrlen));
                    if (newNode->ai_addr == nullptr) {
                        throw std::runtime_error(
                            "Memory allocation failed for sockaddr");
                    }
                    std::memcpy(newNode->ai_addr, node->ai_addr,
                                node->ai_addrlen);
                }

                if (node->ai_canonname != nullptr) {
                    newNode->ai_canonname = strdup(node->ai_canonname);
                    if (newNode->ai_canonname == nullptr) {
                        free(newNode->ai_addr);
                        throw std::runtime_error(
                            "Memory allocation failed for canonical name");
                    }
                }

                struct addrinfo* nodePtr = newNode.release();
                if (result == nullptr) {
                    result = nodePtr;
                } else {
                    tail->ai_next = nodePtr;
                }
                tail = nodePtr;
            }
        }
#else
        for (const struct addrinfo* node = addrInfo; node != nullptr;
             node = node->ai_next) {
            if (node->ai_family == family) {
                auto newNode = static_cast<struct addrinfo*>(
                    malloc(sizeof(struct addrinfo)));
                if (newNode == nullptr) {
                    throw std::runtime_error(
                        "Memory allocation failed for addrinfo");
                }

                *newNode = *node;
                newNode->ai_next = nullptr;

                if (node->ai_addr != nullptr) {
                    newNode->ai_addr =
                        static_cast<struct sockaddr*>(malloc(node->ai_addrlen));
                    if (newNode->ai_addr == nullptr) {
                        free(newNode);
                        throw std::runtime_error(
                            "Memory allocation failed for sockaddr");
                    }
                    std::memcpy(newNode->ai_addr, node->ai_addr,
                                node->ai_addrlen);
                }

                if (node->ai_canonname != nullptr) {
                    newNode->ai_canonname = strdup(node->ai_canonname);
                    if (newNode->ai_canonname == nullptr) {
                        free(newNode->ai_addr);
                        free(newNode);
                        throw std::runtime_error(
                            "Memory allocation failed for canonical name");
                    }
                }

                if (result == nullptr) {
                    result = newNode;
                } else {
                    tail->ai_next = newNode;
                }
                tail = newNode;
            }
        }
#endif

        return {result, ::freeaddrinfo};
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid argument in filterAddrInfo: {}", e.what());
        throw;
    } catch (const std::runtime_error& e) {
        LOG_F(ERROR, "Runtime error in filterAddrInfo: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error in filterAddrInfo: {}", e.what());
        throw;
    }
}

auto sortAddrInfo(const struct addrinfo* addrInfo)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> {
    try {
        if (addrInfo == nullptr) {
            throw std::invalid_argument("addrInfo cannot be null");
        }

        std::vector<const struct addrinfo*> nodes;
        for (const struct addrinfo* node = addrInfo; node != nullptr;
             node = node->ai_next) {
            nodes.push_back(node);
        }

        std::sort(nodes.begin(), nodes.end(),
                  [](const struct addrinfo* a, const struct addrinfo* b) {
                      if (a->ai_family != b->ai_family) {
                          return a->ai_family < b->ai_family;
                      }
                      if (a->ai_socktype != b->ai_socktype) {
                          return a->ai_socktype < b->ai_socktype;
                      }
                      return a->ai_protocol < b->ai_protocol;
                  });

        struct addrinfo* result = nullptr;
        struct addrinfo* tail = nullptr;

        for (const struct addrinfo* node : nodes) {
            auto newNode =
                static_cast<struct addrinfo*>(malloc(sizeof(struct addrinfo)));
            if (newNode == nullptr) {
                throw std::runtime_error(
                    "Memory allocation failed for addrinfo");
            }

            *newNode = *node;
            newNode->ai_next = nullptr;

            if (node->ai_addr != nullptr) {
                newNode->ai_addr =
                    static_cast<struct sockaddr*>(malloc(node->ai_addrlen));
                if (newNode->ai_addr == nullptr) {
                    free(newNode);
                    throw std::runtime_error(
                        "Memory allocation failed for sockaddr");
                }
                std::memcpy(newNode->ai_addr, node->ai_addr, node->ai_addrlen);
            }

            if (node->ai_canonname != nullptr) {
                newNode->ai_canonname = strdup(node->ai_canonname);
                if (newNode->ai_canonname == nullptr) {
                    free(newNode->ai_addr);
                    free(newNode);
                    throw std::runtime_error(
                        "Memory allocation failed for canonical name");
                }
            }

            if (result == nullptr) {
                result = newNode;
            } else {
                tail->ai_next = newNode;
            }
            tail = newNode;
        }

        return {result, ::freeaddrinfo};
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid argument in sortAddrInfo: {}", e.what());
        throw;
    } catch (const std::runtime_error& e) {
        LOG_F(ERROR, "Runtime error in sortAddrInfo: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error in sortAddrInfo: {}", e.what());
        throw;
    }
}
#endif
}  // namespace atom::web
