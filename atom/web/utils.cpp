#include "utils.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <future>
#include <memory>
#include <ranges>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
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
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#define WIN_FLAG false
#endif

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/command.hpp"

namespace atom::web {

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

// ====== Socket Creation and Management ======

auto createSocket() -> int {
    try {
        int sockfd =
            static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (sockfd < 0) {
            std::array<char, 256> buf{};
            throw std::runtime_error(
                std::format("Socket creation failed: {}",
                            strerror_r(errno, buf.data(), buf.size())));
        }
        return sockfd;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to create socket: {}", e.what());
#ifdef _WIN32
        WSACleanup();
#endif
        throw;  // Rethrow the exception
    }
}

auto bindSocket(int sockfd, uint16_t port) -> bool {
    try {
        struct sockaddr_in addr {};
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(addr)) != 0) {
            std::array<char, 256> buf{};
            if (errno == EADDRINUSE) {
                DLOG_F(WARNING, "Port {} is already in use", port);
                return false;
            }
            throw std::runtime_error(
                std::format("Socket bind failed: {}",
                            strerror_r(errno, buf.data(), buf.size())));
        }
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to bind socket: {}", e.what());
        return false;
    }
}

// ====== Port and Process Management ======

auto getProcessIDOnPort(PortNumber auto port) -> std::optional<int> {
    try {
        // Validate port range
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
                    // For Windows, extract PID from netstat output
                    std::regex portRegex(
                        std::format(".*:{}\\s+.*LISTENING\\s+(\\d+)", port));
                    std::smatch match;
                    if (std::regex_search(line, match, portRegex) &&
                        match.size() > 1) {
                        return true;
                    }
                    return false;
                } else {
                    // For Linux/Mac, lsof already returns PIDs directly
                    return !line.empty();
                }
            });

        if (pidStr.empty()) {
            return std::nullopt;
        }

        // Clean up the output and extract the PID
        pidStr.erase(pidStr.find_last_not_of('\n') + 1);

        if (WIN_FLAG) {
            // Extract the PID from Windows netstat output
            std::regex pidRegex(R"(\s+(\d+)\s*)");
            std::smatch match;
            if (std::regex_search(pidStr, match, pidRegex) &&
                match.size() > 1) {
                return std::stoi(match[1].str());
            }
        } else {
            // For Linux/Mac
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
        // Validate port range
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

        // Use RAII for socket cleanup
        auto socketGuard = [sockfd]() {
            close(sockfd);
#ifdef _WIN32
            WSACleanup();
#endif
        };
        std::unique_ptr<void, decltype(socketGuard)> socketCloser(nullptr,
                                                                  socketGuard);

        bool inUse = !bindSocket(sockfd, static_cast<uint16_t>(port));
        return inUse;
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Invalid port argument: {}", e.what());
        throw;  // Rethrow to caller
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error checking if port {} is in use: {}", port, e.what());
        return true;  // Assume port is in use on error
    }
}

auto isPortInUseAsync(PortNumber auto port) -> std::future<bool> {
    return std::async(std::launch::async,
                      [port]() { return isPortInUse(port); });
}

auto checkAndKillProgramOnPort(PortNumber auto port) -> bool {
    try {
        // Validate port range
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
                atom::system::killProcessByPID(pid, 15);  // SIGTERM

                // Wait briefly and check if port is still in use
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (isPortInUse(port)) {
                    LOG_F(WARNING,
                          "Process {} did not terminate gracefully, using "
                          "force kill",
                          pid);
                    atom::system::killProcessByPID(pid, 9);  // SIGKILL

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
            // Allocate memory for the address info and sockaddr
            size_t aiSize = sizeof(struct addrinfo) + aiSrc->ai_addrlen;
            auto ai = std::unique_ptr<struct addrinfo, decltype(&free)>(
                static_cast<struct addrinfo*>(calloc(1, aiSize)), free);

            if (!ai) {
                throw std::runtime_error(
                    "Memory allocation failed for addrinfo");
            }

            // Copy the main structure
            *ai = *aiSrc;

            // Setup the sockaddr pointer to point just after the addrinfo
            // structure
            ai->ai_addr = reinterpret_cast<struct sockaddr*>(ai.get() + 1);
            std::memcpy(ai->ai_addr, aiSrc->ai_addr, aiSrc->ai_addrlen);

            // Copy canonical name if present
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

            // Add to the list
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

        // Update the destination
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

auto addrInfoToString(const struct addrinfo* addrInfo,
                      bool jsonFormat) -> std::string {
    try {
        if (addrInfo == nullptr) {
            throw std::invalid_argument("addrInfo cannot be null");
        }

        std::ostringstream oss;
        if (jsonFormat) {
            oss << "[\n";  // Start JSON array
        }

        int count = 0;
        const struct addrinfo* current = addrInfo;

        while (current != nullptr) {
            // Increment for proper JSON formatting
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

                // Handle address based on family
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

                // Close JSON object, handle commas
                if (current->ai_next != nullptr) {
                    oss << "  },\n";
                } else {
                    oss << "  }\n";
                }
            } else {
                // Plain text format
                oss << "Address Info #" << count << ":\n";
                oss << "  ai_flags: " << current->ai_flags << "\n";
                oss << "  ai_family: " << current->ai_family;

                // Add human-readable family names
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

                // Add human-readable socket type
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

                // Handle address based on family
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
                oss << "-------------------------\n";  // Separator for clarity
            }

            current = current->ai_next;
        }

        if (jsonFormat) {
            oss << "]\n";  // Close JSON array
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

        struct addrinfo hints {};
        hints.ai_family = AF_UNSPEC;      // Allow IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
        hints.ai_flags = AI_CANONNAME;    // Get canonical name

        struct addrinfo* result = nullptr;
        int ret = getaddrinfo(hostname.c_str(),
                              service.empty() ? nullptr : service.c_str(),
                              &hints, &result);

        if (ret != 0) {
            throw std::runtime_error(
                std::format("getaddrinfo error: {}", gai_strerror(ret)));
        }

        // Return as unique_ptr with custom deleter
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

        // Compare basic fields
        if (addrInfo1->ai_family != addrInfo2->ai_family ||
            addrInfo1->ai_socktype != addrInfo2->ai_socktype ||
            addrInfo1->ai_protocol != addrInfo2->ai_protocol ||
            addrInfo1->ai_addrlen != addrInfo2->ai_addrlen) {
            return false;
        }

        // Compare addresses (if available)
        if (addrInfo1->ai_addr == nullptr && addrInfo2->ai_addr == nullptr) {
            return true;  // Both have no address - considered equal
        } else if (addrInfo1->ai_addr == nullptr ||
                   addrInfo2->ai_addr == nullptr) {
            return false;  // One has address, other doesn't - not equal
        }

        // Compare the binary representation of addresses
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

        // Use C++20 ranges to filter nodes (if available)
#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
        // Create a generator view that traverses the linked list
        auto addrInfoView = std::ranges::views::iota(0) |
                            std::ranges::views::take_while([&addrInfo](int) {
                                return addrInfo != nullptr;
                            }) |
                            std::views::transform([&addrInfo](int) {
                                auto current = addrInfo;
                                addrInfo = addrInfo->ai_next;
                                return current;
                            });

        // Filter and process matching nodes
        for (const auto* node : addrInfoView) {
            if (node->ai_family == family) {
                // Create a copy of this node
                auto newNode =
                    std::unique_ptr<struct addrinfo, decltype(&free)>(
                        static_cast<struct addrinfo*>(
                            malloc(sizeof(struct addrinfo))),
                        free);

                if (!newNode) {
                    throw std::runtime_error(
                        "Memory allocation failed for addrinfo");
                }

                // Copy structure data
                *newNode = *node;
                newNode->ai_next = nullptr;

                // Copy address data if present
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

                // Copy canonical name if present
                if (node->ai_canonname != nullptr) {
                    newNode->ai_canonname = strdup(node->ai_canonname);
                    if (newNode->ai_canonname == nullptr) {
                        free(newNode->ai_addr);
                        throw std::runtime_error(
                            "Memory allocation failed for canonical name");
                    }
                }

                // Add to our result list
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
        // Traditional loop approach for older compilers
        for (const struct addrinfo* node = addrInfo; node != nullptr;
             node = node->ai_next) {
            if (node->ai_family == family) {
                // Create a copy of this node
                auto newNode = static_cast<struct addrinfo*>(
                    malloc(sizeof(struct addrinfo)));
                if (newNode == nullptr) {
                    throw std::runtime_error(
                        "Memory allocation failed for addrinfo");
                }

                // Copy structure data
                *newNode = *node;
                newNode->ai_next = nullptr;

                // Copy address data if present
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

                // Copy canonical name if present
                if (node->ai_canonname != nullptr) {
                    newNode->ai_canonname = strdup(node->ai_canonname);
                    if (newNode->ai_canonname == nullptr) {
                        free(newNode->ai_addr);
                        free(newNode);
                        throw std::runtime_error(
                            "Memory allocation failed for canonical name");
                    }
                }

                // Add to our result list
                if (result == nullptr) {
                    result = newNode;
                } else {
                    tail->ai_next = newNode;
                }
                tail = newNode;
            }
        }
#endif

        // Return as a unique_ptr with the proper deleter
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

        // Count the number of nodes and collect them in a vector
        std::vector<const struct addrinfo*> nodes;
        for (const struct addrinfo* node = addrInfo; node != nullptr;
             node = node->ai_next) {
            nodes.push_back(node);
        }

        // Sort the vector by family
        std::sort(nodes.begin(), nodes.end(),
                  [](const struct addrinfo* a, const struct addrinfo* b) {
                      // Primary sort by family
                      if (a->ai_family != b->ai_family) {
                          return a->ai_family < b->ai_family;
                      }
                      // Secondary sort by socktype
                      if (a->ai_socktype != b->ai_socktype) {
                          return a->ai_socktype < b->ai_socktype;
                      }
                      // Tertiary sort by protocol
                      return a->ai_protocol < b->ai_protocol;
                  });

        // Create a new linked list with the sorted nodes
        struct addrinfo* result = nullptr;
        struct addrinfo* tail = nullptr;

        for (const struct addrinfo* node : nodes) {
            // Create a copy of this node
            auto newNode =
                static_cast<struct addrinfo*>(malloc(sizeof(struct addrinfo)));
            if (newNode == nullptr) {
                throw std::runtime_error(
                    "Memory allocation failed for addrinfo");
            }

            // Copy structure data
            *newNode = *node;
            newNode->ai_next = nullptr;

            // Copy address data if present
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

            // Copy canonical name if present
            if (node->ai_canonname != nullptr) {
                newNode->ai_canonname = strdup(node->ai_canonname);
                if (newNode->ai_canonname == nullptr) {
                    free(newNode->ai_addr);
                    free(newNode);
                    throw std::runtime_error(
                        "Memory allocation failed for canonical name");
                }
            }

            // Add to our result list
            if (result == nullptr) {
                result = newNode;
            } else {
                tail->ai_next = newNode;
            }
            tail = newNode;
        }

        // Return as a unique_ptr with the proper deleter
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
