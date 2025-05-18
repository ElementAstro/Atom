/*
 * addr_info.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "addr_info.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#if defined(__linux__) || defined(__APPLE__)
#include <netdb.h>
#include <sys/socket.h>
#elif defined(_WIN32)
#include <ws2tcpip.h>
#endif
#endif

#include "atom/log/loguru.hpp"

namespace atom::web {

auto dumpAddrInfo(
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>& dst,
    const struct addrinfo* src) -> int {
    try {
        if (src == nullptr) {
            throw std::invalid_argument("Source addrinfo is null");
        }

        struct addrinfo* aiDst = nullptr;
        const struct addrinfo* aiSrc = src;
        struct addrinfo* aiCur = nullptr;
        [[maybe_unused]] struct addrinfo* aiPrev = nullptr;

        while (aiSrc != nullptr) {
            struct addrinfo* aiNew = new struct addrinfo;
            std::memset(aiNew, 0, sizeof(struct addrinfo));

            aiNew->ai_flags = aiSrc->ai_flags;
            aiNew->ai_family = aiSrc->ai_family;
            aiNew->ai_socktype = aiSrc->ai_socktype;
            aiNew->ai_protocol = aiSrc->ai_protocol;
            aiNew->ai_addrlen = aiSrc->ai_addrlen;
            aiNew->ai_canonname = aiSrc->ai_canonname
                                     ? strdup(aiSrc->ai_canonname)
                                     : nullptr;

            if (aiSrc->ai_addr != nullptr) {
                aiNew->ai_addr = reinterpret_cast<struct sockaddr*>(
                    malloc(aiSrc->ai_addrlen));
                if (aiNew->ai_addr == nullptr) {
                    throw std::runtime_error("Failed to allocate memory for address");
                }
                std::memcpy(aiNew->ai_addr, aiSrc->ai_addr, aiSrc->ai_addrlen);
            }

            aiNew->ai_next = nullptr;

            if (aiDst == nullptr) {
                aiDst = aiNew;
                aiCur = aiDst;
            } else {
                aiCur->ai_next = aiNew;
                aiPrev = aiCur;
                aiCur = aiNew;
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
            throw std::invalid_argument("addrInfo is null");
        }

        std::ostringstream oss;
        if (jsonFormat) {
            oss << "[";
        }

        int count = 0;
        const struct addrinfo* current = addrInfo;

        while (current != nullptr) {
            char host[NI_MAXHOST];
            char serv[NI_MAXSERV];

            int ret = getnameinfo(current->ai_addr, current->ai_addrlen,
                                 host, sizeof(host),
                                 serv, sizeof(serv),
                                 NI_NUMERICHOST | NI_NUMERICSERV);

            if (jsonFormat) {
                if (count > 0) oss << ",";
                oss << "{";
                oss << "\"family\":" << current->ai_family << ",";
                oss << "\"socktype\":" << current->ai_socktype << ",";
                oss << "\"protocol\":" << current->ai_protocol << ",";
                if (ret == 0) {
                    oss << "\"host\":\"" << host << "\",";
                    oss << "\"service\":\"" << serv << "\"";
                }
                oss << "}";
            } else {
                oss << "addrinfo[" << count << "]:" << std::endl;
                oss << "  Family: " << current->ai_family << std::endl;
                oss << "  Socktype: " << current->ai_socktype << std::endl;
                oss << "  Protocol: " << current->ai_protocol << std::endl;
                if (ret == 0) {
                    oss << "  Host: " << host << std::endl;
                    oss << "  Service: " << serv << std::endl;
                }
            }

            current = current->ai_next;
            count++;
        }

        if (jsonFormat) {
            oss << "]";
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
            throw std::runtime_error(std::string("getaddrinfo failed: ") +
                                    gai_strerror(ret));
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
            throw std::invalid_argument("addrInfo cannot be null");
        }

        if (addrInfo1->ai_family != addrInfo2->ai_family ||
            addrInfo1->ai_socktype != addrInfo2->ai_socktype ||
            addrInfo1->ai_protocol != addrInfo2->ai_protocol ||
            addrInfo1->ai_addrlen != addrInfo2->ai_addrlen) {
            return false;
        }

        if (addrInfo1->ai_addr == nullptr && addrInfo2->ai_addr == nullptr) {
            return true;
        } else if (addrInfo1->ai_addr == nullptr || addrInfo2->ai_addr == nullptr) {
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
            throw std::invalid_argument("addrInfo is null");
        }

        struct addrinfo* result = nullptr;
        struct addrinfo* tail = nullptr;

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
        auto addrInfoView = std::ranges::views::iota(0) |
                            std::ranges::views::take_while([&addrInfo](int) {
                                return addrInfo != nullptr;
                            }) |
                            std::views::transform([&addrInfo](int) {
                                const auto* ai = addrInfo;
                                addrInfo = addrInfo->ai_next;
                                return ai;
                            });

        for (const auto* node : addrInfoView) {
            if (node->ai_family == family) {
                struct addrinfo* newNode = new struct addrinfo;
                std::memset(newNode, 0, sizeof(struct addrinfo));

                newNode->ai_flags = node->ai_flags;
                newNode->ai_family = node->ai_family;
                newNode->ai_socktype = node->ai_socktype;
                newNode->ai_protocol = node->ai_protocol;
                newNode->ai_addrlen = node->ai_addrlen;
                
                if (node->ai_canonname) {
                    newNode->ai_canonname = strdup(node->ai_canonname);
                }
                
                if (node->ai_addr) {
                    newNode->ai_addr = reinterpret_cast<struct sockaddr*>(
                        malloc(node->ai_addrlen));
                    std::memcpy(newNode->ai_addr, node->ai_addr, node->ai_addrlen);
                }
                
                newNode->ai_next = nullptr;

                if (result == nullptr) {
                    result = newNode;
                    tail = result;
                } else {
                    tail->ai_next = newNode;
                    tail = newNode;
                }
            }
        }
#else
        for (const struct addrinfo* node = addrInfo; node != nullptr;
             node = node->ai_next) {
            if (node->ai_family == family) {
                struct addrinfo* newNode = new struct addrinfo;
                std::memset(newNode, 0, sizeof(struct addrinfo));

                newNode->ai_flags = node->ai_flags;
                newNode->ai_family = node->ai_family;
                newNode->ai_socktype = node->ai_socktype;
                newNode->ai_protocol = node->ai_protocol;
                newNode->ai_addrlen = node->ai_addrlen;
                
                if (node->ai_canonname) {
                    newNode->ai_canonname = strdup(node->ai_canonname);
                }
                
                if (node->ai_addr) {
                    newNode->ai_addr = reinterpret_cast<struct sockaddr*>(
                        malloc(node->ai_addrlen));
                    std::memcpy(newNode->ai_addr, node->ai_addr, node->ai_addrlen);
                }
                
                newNode->ai_next = nullptr;

                if (result == nullptr) {
                    result = newNode;
                    tail = result;
                } else {
                    tail->ai_next = newNode;
                    tail = newNode;
                }
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
            throw std::invalid_argument("addrInfo is null");
        }

        std::vector<const struct addrinfo*> nodes;
        for (const struct addrinfo* node = addrInfo; node != nullptr;
             node = node->ai_next) {
            nodes.push_back(node);
        }

        std::sort(nodes.begin(), nodes.end(),
                  [](const struct addrinfo* a, const struct addrinfo* b) {
                      return a->ai_family < b->ai_family;
                  });

        struct addrinfo* result = nullptr;
        struct addrinfo* tail = nullptr;

        for (const struct addrinfo* node : nodes) {
            struct addrinfo* newNode = new struct addrinfo;
            std::memset(newNode, 0, sizeof(struct addrinfo));

            newNode->ai_flags = node->ai_flags;
            newNode->ai_family = node->ai_family;
            newNode->ai_socktype = node->ai_socktype;
            newNode->ai_protocol = node->ai_protocol;
            newNode->ai_addrlen = node->ai_addrlen;
            
            if (node->ai_canonname) {
                newNode->ai_canonname = strdup(node->ai_canonname);
            }
            
            if (node->ai_addr) {
                newNode->ai_addr = reinterpret_cast<struct sockaddr*>(
                    malloc(node->ai_addrlen));
                std::memcpy(newNode->ai_addr, node->ai_addr, node->ai_addrlen);
            }
            
            newNode->ai_next = nullptr;

            if (result == nullptr) {
                result = newNode;
                tail = result;
            } else {
                tail->ai_next = newNode;
                tail = newNode;
            }
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

}  // namespace atom::web
