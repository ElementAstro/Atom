/*
 * addr_info.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "addr_info.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
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

#include <spdlog/spdlog.h>

namespace atom::web {

namespace {
constexpr size_t MAX_HOST_SIZE = NI_MAXHOST;
constexpr size_t MAX_SERV_SIZE = NI_MAXSERV;

void freeAddrInfoChain(struct addrinfo* head) noexcept {
    struct addrinfo* current = head;
    while (current != nullptr) {
        struct addrinfo* next = current->ai_next;
        free(current->ai_canonname);
        free(current->ai_addr);
        delete current;
        current = next;
    }
}

auto createAddrInfoNode(const struct addrinfo* src) -> struct addrinfo* {
    auto* newNode = new struct addrinfo;
    std::memset(newNode, 0, sizeof(struct addrinfo));

    newNode->ai_flags = src->ai_flags;
    newNode->ai_family = src->ai_family;
    newNode->ai_socktype = src->ai_socktype;
    newNode->ai_protocol = src->ai_protocol;
    newNode->ai_addrlen = src->ai_addrlen;

    if (src->ai_canonname) {
        newNode->ai_canonname = strdup(src->ai_canonname);
        if (!newNode->ai_canonname) {
            delete newNode;
            throw std::runtime_error("Failed to allocate memory for canonname");
        }
    }

    if (src->ai_addr) {
        newNode->ai_addr =
            static_cast<struct sockaddr*>(malloc(src->ai_addrlen));
        if (!newNode->ai_addr) {
            free(newNode->ai_canonname);
            delete newNode;
            throw std::runtime_error("Failed to allocate memory for address");
        }
        std::memcpy(newNode->ai_addr, src->ai_addr, src->ai_addrlen);
    }

    newNode->ai_next = nullptr;
    return newNode;
}
}  // namespace

auto dumpAddrInfo(
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>& dst,
    const struct addrinfo* src) -> int {
    if (!src) {
        spdlog::error("Source addrinfo is null");
        return -1;
    }

    try {
        struct addrinfo* head = nullptr;
        struct addrinfo* tail = nullptr;

        for (const struct addrinfo* current = src; current != nullptr;
             current = current->ai_next) {
            auto* newNode = createAddrInfoNode(current);

            if (!head) {
                head = tail = newNode;
            } else {
                tail->ai_next = newNode;
                tail = newNode;
            }
        }

        dst = std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>(
            head,
            [](struct addrinfo* ptr) noexcept { freeAddrInfoChain(ptr); });

        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to dump addrinfo: {}", e.what());
        return -1;
    }
}

auto addrInfoToString(const struct addrinfo* addrInfo, bool jsonFormat)
    -> std::string {
    if (!addrInfo) {
        throw std::invalid_argument("addrInfo is null");
    }

    std::ostringstream oss;
    if (jsonFormat) {
        oss << "[";
    }

    int count = 0;
    for (const struct addrinfo* current = addrInfo; current != nullptr;
         current = current->ai_next) {
        std::array<char, MAX_HOST_SIZE> host{};
        std::array<char, MAX_SERV_SIZE> serv{};

        int ret = getnameinfo(current->ai_addr, current->ai_addrlen,
                              host.data(), host.size(), serv.data(),
                              serv.size(), NI_NUMERICHOST | NI_NUMERICSERV);

        if (jsonFormat) {
            if (count > 0)
                oss << ",";
            oss << "{";
            oss << "\"family\":" << current->ai_family << ",";
            oss << "\"socktype\":" << current->ai_socktype << ",";
            oss << "\"protocol\":" << current->ai_protocol;
            if (ret == 0) {
                oss << ",\"host\":\"" << host.data() << "\",";
                oss << "\"service\":\"" << serv.data() << "\"";
            }
            oss << "}";
        } else {
            oss << "addrinfo[" << count << "]:\n";
            oss << "  Family: " << current->ai_family << "\n";
            oss << "  Socktype: " << current->ai_socktype << "\n";
            oss << "  Protocol: " << current->ai_protocol << "\n";
            if (ret == 0) {
                oss << "  Host: " << host.data() << "\n";
                oss << "  Service: " << serv.data() << "\n";
            }
        }

        count++;
    }

    if (jsonFormat) {
        oss << "]";
    }

    return oss.str();
}

auto getAddrInfo(const std::string& hostname, const std::string& service)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> {
    if (hostname.empty()) {
        throw std::invalid_argument("Hostname cannot be empty");
    }

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    struct addrinfo* result = nullptr;
    int ret = getaddrinfo(hostname.c_str(),
                          service.empty() ? nullptr : service.c_str(), &hints,
                          &result);

    if (ret != 0) {
        std::string error =
            "getaddrinfo failed for " + hostname + ": " + gai_strerror(ret);
        spdlog::error(error);
        throw std::runtime_error(error);
    }

    spdlog::debug("Successfully resolved hostname: {}", hostname);
    return {result, ::freeaddrinfo};
}

auto compareAddrInfo(const struct addrinfo* addrInfo1,
                     const struct addrinfo* addrInfo2) -> bool {
    if (!addrInfo1 || !addrInfo2) {
        throw std::invalid_argument("addrInfo cannot be null");
    }

    return (addrInfo1->ai_family == addrInfo2->ai_family &&
            addrInfo1->ai_socktype == addrInfo2->ai_socktype &&
            addrInfo1->ai_protocol == addrInfo2->ai_protocol &&
            addrInfo1->ai_addrlen == addrInfo2->ai_addrlen &&
            ((addrInfo1->ai_addr == nullptr && addrInfo2->ai_addr == nullptr) ||
             (addrInfo1->ai_addr != nullptr && addrInfo2->ai_addr != nullptr &&
              std::memcmp(addrInfo1->ai_addr, addrInfo2->ai_addr,
                          addrInfo1->ai_addrlen) == 0)));
}

auto filterAddrInfo(const struct addrinfo* addrInfo, int family)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> {
    if (!addrInfo) {
        throw std::invalid_argument("addrInfo is null");
    }

    try {
        struct addrinfo* head = nullptr;
        struct addrinfo* tail = nullptr;

        for (const struct addrinfo* current = addrInfo; current != nullptr;
             current = current->ai_next) {
            if (current->ai_family == family) {
                auto* newNode = createAddrInfoNode(current);

                if (!head) {
                    head = tail = newNode;
                } else {
                    tail->ai_next = newNode;
                    tail = newNode;
                }
            }
        }

        return std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>(
            head, ::freeaddrinfo);

    } catch (const std::exception& e) {
        spdlog::error("Failed to filter addrinfo: {}", e.what());
        throw;
    }
}

auto sortAddrInfo(const struct addrinfo* addrInfo)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> {
    if (!addrInfo) {
        throw std::invalid_argument("addrInfo is null");
    }

    try {
        std::vector<const struct addrinfo*> nodes;
        for (const struct addrinfo* current = addrInfo; current != nullptr;
             current = current->ai_next) {
            nodes.push_back(current);
        }

        std::sort(nodes.begin(), nodes.end(),
                  [](const struct addrinfo* a, const struct addrinfo* b) {
                      return a->ai_family < b->ai_family;
                  });

        struct addrinfo* head = nullptr;
        struct addrinfo* tail = nullptr;

        for (const struct addrinfo* node : nodes) {
            auto* newNode = createAddrInfoNode(node);

            if (!head) {
                head = tail = newNode;
            } else {
                tail->ai_next = newNode;
                tail = newNode;
            }
        }

        return std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>(
            head, ::freeaddrinfo);

    } catch (const std::exception& e) {
        spdlog::error("Failed to sort addrinfo: {}", e.what());
        throw;
    }
}

}  // namespace atom::web
