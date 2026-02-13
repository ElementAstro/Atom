/*
 * multicast_helper.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************
Date: 2024-5-24
Description: Multicast operations helper for UDP sockets
*************************************************/

#ifndef ATOM_CONNECTION_UDP_MULTICAST_HELPER_HPP
#define ATOM_CONNECTION_UDP_MULTICAST_HELPER_HPP

#include <set>
#include <string>
#include <mutex>

#include "udp_common.hpp"
#include "udp_platform.hpp"

namespace atom::connection::udp {

/**
 * @brief Helper class for multicast operations on UDP sockets
 *
 * This class provides a convenient interface for joining/leaving multicast
 * groups and sending multicast messages. It tracks joined groups and provides
 * automatic cleanup.
 */
class MulticastHelper {
public:
    /**
     * @brief Construct a MulticastHelper for a socket
     * @param socket The socket handle to perform multicast operations on
     */
    explicit MulticastHelper(platform::SocketHandle socket) : socket_(socket) {}

    /**
     * @brief Destructor - leaves all joined multicast groups
     */
    ~MulticastHelper() { leaveAllGroups(); }

    MulticastHelper(const MulticastHelper&) = delete;
    MulticastHelper& operator=(const MulticastHelper&) = delete;

    MulticastHelper(MulticastHelper&& other) noexcept
        : socket_(other.socket_) {
        std::lock_guard<std::mutex> lock(other.mutex_);
        joinedGroups_ = std::move(other.joinedGroups_);
        other.socket_ = platform::INVALID_SOCKET_HANDLE;
    }

    MulticastHelper& operator=(MulticastHelper&& other) noexcept {
        if (this != &other) {
            leaveAllGroups();
            std::lock_guard<std::mutex> lock(other.mutex_);
            socket_ = other.socket_;
            joinedGroups_ = std::move(other.joinedGroups_);
            other.socket_ = platform::INVALID_SOCKET_HANDLE;
        }
        return *this;
    }

    /**
     * @brief Update the socket handle
     * @param socket New socket handle
     */
    void setSocket(platform::SocketHandle socket) noexcept {
        leaveAllGroups();
        socket_ = socket;
    }

    /**
     * @brief Join a multicast group
     * @param groupAddress The multicast group IP address (e.g., "224.0.0.1")
     * @param interfaceAddress Optional local interface address (empty = any)
     * @return Result with true on success or error code
     */
    [[nodiscard]] UdpResult<bool> joinGroup(
        const std::string& groupAddress,
        const std::string& interfaceAddress = "") noexcept {
        if (socket_ == platform::INVALID_SOCKET_HANDLE) {
            return type::unexpected(UdpError::NotInitialized);
        }

        if (!platform::isMulticastAddress(groupAddress)) {
            return type::unexpected(UdpError::InvalidAddress);
        }

        // Check if already joined
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (joinedGroups_.contains(groupAddress)) {
                return true;  // Already joined
            }
        }

        struct ip_mreq mreq{};

        // Set the multicast group address
        if (inet_pton(AF_INET, groupAddress.c_str(), &mreq.imr_multiaddr) <= 0) {
            return type::unexpected(UdpError::InvalidAddress);
        }

        // Set the local interface
        if (interfaceAddress.empty()) {
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        } else {
            if (inet_pton(AF_INET, interfaceAddress.c_str(),
                          &mreq.imr_interface) <= 0) {
                return type::unexpected(UdpError::InvalidAddress);
            }
        }

        // Join the multicast group
        if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                       reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0) {
            return type::unexpected(UdpError::MulticastError);
        }

        // Track the joined group
        {
            std::lock_guard<std::mutex> lock(mutex_);
            joinedGroups_.insert(groupAddress);
        }

        return true;
    }

    /**
     * @brief Leave a multicast group
     * @param groupAddress The multicast group IP address
     * @param interfaceAddress Optional local interface address
     * @return Result with true on success or error code
     */
    [[nodiscard]] UdpResult<bool> leaveGroup(
        const std::string& groupAddress,
        const std::string& interfaceAddress = "") noexcept {
        if (socket_ == platform::INVALID_SOCKET_HANDLE) {
            return type::unexpected(UdpError::NotInitialized);
        }

        // Check if we've joined this group
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!joinedGroups_.contains(groupAddress)) {
                return type::unexpected(UdpError::InvalidParameter);
            }
        }

        struct ip_mreq mreq{};

        // Set the multicast group address
        if (inet_pton(AF_INET, groupAddress.c_str(), &mreq.imr_multiaddr) <= 0) {
            return type::unexpected(UdpError::InvalidAddress);
        }

        // Set the local interface
        if (interfaceAddress.empty()) {
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        } else {
            if (inet_pton(AF_INET, interfaceAddress.c_str(),
                          &mreq.imr_interface) <= 0) {
                return type::unexpected(UdpError::InvalidAddress);
            }
        }

        // Leave the multicast group
        if (setsockopt(socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                       reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0) {
            return type::unexpected(UdpError::MulticastError);
        }

        // Remove from tracked groups
        {
            std::lock_guard<std::mutex> lock(mutex_);
            joinedGroups_.erase(groupAddress);
        }

        return true;
    }

    /**
     * @brief Leave all joined multicast groups
     */
    void leaveAllGroups() noexcept {
        std::set<std::string> groups;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            groups = joinedGroups_;
        }

        for (const auto& group : groups) {
            leaveGroup(group);  // Ignore errors during cleanup
        }
    }

    /**
     * @brief Set multicast TTL (time-to-live / hop limit)
     * @param ttl TTL value (1-255)
     * @return Result with true on success or error code
     */
    [[nodiscard]] UdpResult<bool> setMulticastTTL(int ttl) noexcept {
        if (socket_ == platform::INVALID_SOCKET_HANDLE) {
            return type::unexpected(UdpError::NotInitialized);
        }

        if (ttl < 1 || ttl > 255) {
            return type::unexpected(UdpError::InvalidParameter);
        }

        if (!platform::setSocketOptInt(socket_, IPPROTO_IP, IP_MULTICAST_TTL,
                                       ttl)) {
            return type::unexpected(UdpError::MulticastError);
        }

        return true;
    }

    /**
     * @brief Set multicast loopback (receive own messages)
     * @param enable true to enable loopback
     * @return Result with true on success or error code
     */
    [[nodiscard]] UdpResult<bool> setMulticastLoopback(bool enable) noexcept {
        if (socket_ == platform::INVALID_SOCKET_HANDLE) {
            return type::unexpected(UdpError::NotInitialized);
        }

        if (!platform::setSocketOptBool(socket_, IPPROTO_IP, IP_MULTICAST_LOOP,
                                        enable)) {
            return type::unexpected(UdpError::MulticastError);
        }

        return true;
    }

    /**
     * @brief Set the outgoing multicast interface
     * @param interfaceAddress Local interface IP address
     * @return Result with true on success or error code
     */
    [[nodiscard]] UdpResult<bool> setMulticastInterface(
        const std::string& interfaceAddress) noexcept {
        if (socket_ == platform::INVALID_SOCKET_HANDLE) {
            return type::unexpected(UdpError::NotInitialized);
        }

        struct in_addr addr{};
        if (inet_pton(AF_INET, interfaceAddress.c_str(), &addr) <= 0) {
            return type::unexpected(UdpError::InvalidAddress);
        }

        if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF,
                       reinterpret_cast<const char*>(&addr), sizeof(addr)) < 0) {
            return type::unexpected(UdpError::MulticastError);
        }

        return true;
    }

    /**
     * @brief Check if we've joined a specific multicast group
     * @param groupAddress The multicast group IP address
     * @return true if joined
     */
    [[nodiscard]] bool isJoined(const std::string& groupAddress) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return joinedGroups_.contains(groupAddress);
    }

    /**
     * @brief Get the number of joined multicast groups
     */
    [[nodiscard]] std::size_t joinedGroupCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return joinedGroups_.size();
    }

    /**
     * @brief Get a copy of all joined multicast groups
     */
    [[nodiscard]] std::set<std::string> getJoinedGroups() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return joinedGroups_;
    }

    /**
     * @brief Static helper to check if an address is multicast
     */
    [[nodiscard]] static bool isMulticastAddress(
        const std::string& address) noexcept {
        return platform::isMulticastAddress(address);
    }

private:
    platform::SocketHandle socket_;
    std::set<std::string> joinedGroups_;
    mutable std::mutex mutex_;
};

}  // namespace atom::connection::udp

#endif  // ATOM_CONNECTION_UDP_MULTICAST_HELPER_HPP
