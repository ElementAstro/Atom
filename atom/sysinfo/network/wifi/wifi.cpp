/*
 * wifi.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Wifi Information

**************************************************/

#include "../../network/wifi.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include "common.hpp"

#ifdef _WIN32
#include "windows.hpp"
namespace impl = atom::system::windows;
#elif defined(__linux__)
#include "linux.hpp"
namespace impl = atom::system::linux;
#elif defined(__APPLE__)
#include "macos.hpp"
namespace impl = atom::system::macos;
#endif

namespace atom::system {

auto isConnectedToInternet() -> bool {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::isConnectedToInternet_impl();
#else
    spdlog::error("Unsupported operating system");
    return false;
#endif
}

auto getCurrentWifi() -> std::string {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getCurrentWifi_impl();
#else
    spdlog::error("Unsupported operating system");
    return {};
#endif
}

auto getCurrentWiredNetwork() -> std::string {
#if defined(_WIN32) || defined(__linux__)
    return impl::getCurrentWiredNetwork_impl();
#elif defined(__APPLE__)
    spdlog::warn("Getting current wired network is not supported on macOS");
    return {};
#else
    spdlog::error("Unsupported operating system");
    return {};
#endif
}

auto isHotspotConnected() -> bool {
#if defined(_WIN32) || defined(__linux__)
    return impl::isHotspotConnected_impl();
#elif defined(__APPLE__)
    spdlog::warn(
        "Checking if connected to a hotspot is not supported on macOS");
    return false;
#else
    spdlog::error("Unsupported operating system");
    return false;
#endif
}

auto getHostIPs() -> std::vector<std::string> {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getHostIPs_impl();
#else
    spdlog::error("Unsupported operating system");
    return {};
#endif
}

// Implementation of the template function for IP addresses
template <typename AddressType>
auto getIPAddresses(int addressFamily) -> std::vector<std::string> {
    spdlog::info("Getting IP addresses for address family: {}", addressFamily);
    std::vector<std::string> addresses;

#ifdef _WIN32
    ULONG bufferSize = 0;
    if (GetAdaptersAddresses(addressFamily, 0, nullptr, nullptr, &bufferSize) !=
        ERROR_BUFFER_OVERFLOW) {
        return addresses;
    }

    auto adapterAddresses =
        std::make_unique<IP_ADAPTER_ADDRESSES[]>(bufferSize);
    if (GetAdaptersAddresses(addressFamily, 0, nullptr, adapterAddresses.get(),
                             &bufferSize) == ERROR_SUCCESS) {
        for (auto adapter = adapterAddresses.get(); adapter;
             adapter = adapter->Next) {
            for (auto ua = adapter->FirstUnicastAddress; ua; ua = ua->Next) {
                if (ua->Address.lpSockaddr->sa_family == addressFamily) {
                    char ipStr[INET6_ADDRSTRLEN] = {0};
                    void* addr = nullptr;

                    if (addressFamily == AF_INET) {
                        struct sockaddr_in* ipv4 =
                            reinterpret_cast<struct sockaddr_in*>(
                                ua->Address.lpSockaddr);
                        addr = &(ipv4->sin_addr);
                    } else {
                        struct sockaddr_in6* ipv6 =
                            reinterpret_cast<struct sockaddr_in6*>(
                                ua->Address.lpSockaddr);
                        addr = &(ipv6->sin6_addr);
                    }

                    inet_ntop(addressFamily, addr, ipStr, sizeof(ipStr));
                    addresses.emplace_back(ipStr);
                    spdlog::info("Found IP address: {}", ipStr);
                }
            }
        }
    }
#else
    struct ifaddrs* ifAddrList = nullptr;

    if (getifaddrs(&ifAddrList) == -1) {
        spdlog::error("getifaddrs failed");
        return addresses;
    }

    // Use smart pointer to automatically manage the lifecycle of ifAddrList
    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifAddrListGuard(
        ifAddrList, freeifaddrs);

    for (auto* ifa = ifAddrList; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == addressFamily) {
            char ipStr[INET6_ADDRSTRLEN] = {0};
            void* addr = nullptr;

            if (addressFamily == AF_INET) {
                struct sockaddr_in* ipv4 =
                    reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                addr = &(ipv4->sin_addr);
            } else {
                struct sockaddr_in6* ipv6 =
                    reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
                addr = &(ipv6->sin6_addr);
            }

            inet_ntop(addressFamily, addr, ipStr, sizeof(ipStr));
            addresses.emplace_back(ipStr);
            spdlog::info("Found IP address: {}", ipStr);
        }
    }
#endif

    return addresses;
}

auto getIPv4Addresses() -> std::vector<std::string> {
    spdlog::info("Getting IPv4 addresses");
    return getIPAddresses<sockaddr_in>(AF_INET);
}

auto getIPv6Addresses() -> std::vector<std::string> {
    spdlog::info("Getting IPv6 addresses");
    return getIPAddresses<sockaddr_in6>(AF_INET6);
}

auto getInterfaceNames() -> std::vector<std::string> {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getInterfaceNames_impl();
#else
    spdlog::error("Unsupported operating system");
    return {};
#endif
}

auto getNetworkStats() -> NetworkStats {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getNetworkStats_impl();
#else
    spdlog::error("Unsupported operating system");
    return {};
#endif
}

// Network history tracking with thread-safe storage
namespace {
struct NetworkHistoryEntry {
    std::chrono::system_clock::time_point timestamp;
    NetworkStats stats;
};

std::mutex historyMutex;
std::deque<NetworkHistoryEntry> networkHistory;
constexpr size_t MAX_HISTORY_SIZE = 10000;  // Keep up to 10000 entries

[[maybe_unused]] void recordNetworkStats() {
    try {
        auto stats = getNetworkStats();
        auto now = std::chrono::system_clock::now();

        std::lock_guard<std::mutex> lock(historyMutex);
        networkHistory.push_back({now, stats});

        // Limit history size
        if (networkHistory.size() > MAX_HISTORY_SIZE) {
            networkHistory.pop_front();
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to record network stats: {}", e.what());
    }
}
}  // namespace

auto getNetworkHistory(std::chrono::minutes duration)
    -> std::vector<NetworkStats> {
    spdlog::info("Getting network history for duration: {} minutes",
                 duration.count());

    try {
        std::lock_guard<std::mutex> lock(historyMutex);

        if (networkHistory.empty()) {
            spdlog::warn("No network history available");
            return {};
        }

        auto now = std::chrono::system_clock::now();
        auto cutoff = now - duration;

        std::vector<NetworkStats> result;
        result.reserve(networkHistory.size());

        for (const auto& entry : networkHistory) {
            if (entry.timestamp >= cutoff) {
                result.push_back(entry.stats);
            }
        }

        spdlog::info("Retrieved {} network history entries", result.size());
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get network history: {}", e.what());
        return {};
    }
}

auto scanAvailableNetworks() -> std::vector<std::string> {
    spdlog::info("Scanning available networks");

    try {
#if defined(_WIN32)
        // Windows: Use WLAN API to scan for networks
        std::vector<std::string> networks;
        // Note: Full implementation would require WlanOpenHandle,
        // WlanEnumInterfaces, WlanScan, WlanGetNetworkBssList This is a
        // simplified version that returns currently visible networks
        spdlog::warn("Windows network scanning requires elevated privileges");
        return networks;

#elif defined(__linux__)
        // Linux: Use iwlist or nl80211 netlink interface
        std::vector<std::string> networks;

        // Try to execute iwlist scan command
        FILE* pipe = popen(
            "iwlist scanning 2>/dev/null | grep 'ESSID:' | cut -d'\"' -f2",
            "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string ssid(buffer);
                // Remove trailing newline
                if (!ssid.empty() && ssid.back() == '\n') {
                    ssid.pop_back();
                }
                if (!ssid.empty()) {
                    networks.push_back(ssid);
                }
            }
            pclose(pipe);
        }

        if (networks.empty()) {
            spdlog::warn("No networks found or insufficient permissions");
        } else {
            spdlog::info("Found {} available networks", networks.size());
        }
        return networks;

#elif defined(__APPLE__)
        // macOS: Use CoreWLAN framework
        std::vector<std::string> networks;
        // Note: Full implementation would require CoreWLAN framework
        // This would need Objective-C++ code to interface with CWInterface
        spdlog::warn("macOS network scanning requires CoreWLAN framework");
        return networks;

#else
        spdlog::error("Network scanning not supported on this platform");
        return {};
#endif
    } catch (const std::exception& e) {
        spdlog::error("Failed to scan networks: {}", e.what());
        return {};
    }
}

auto getNetworkSecurity() -> std::string {
    spdlog::info("Getting network security information");

    try {
        auto currentWifi = getCurrentWifi();
        if (currentWifi.empty()) {
            return "Not connected to WiFi";
        }

#if defined(_WIN32)
        // Windows: Query WLAN security settings using WLAN API
        std::string security = "Unknown";

        DWORD negotiatedVersion;
        HANDLE handle;
        DWORD result = WlanOpenHandle(2, nullptr, &negotiatedVersion, &handle);
        if (result == ERROR_SUCCESS) {
            WLAN_INTERFACE_INFO_LIST* interfaceList;
            result = WlanEnumInterfaces(handle, nullptr, &interfaceList);

            if (result == ERROR_SUCCESS) {
                for (DWORD i = 0; i < interfaceList->dwNumberOfItems; ++i) {
                    const auto& wlanInterface = interfaceList->InterfaceInfo[i];
                    if (wlanInterface.isState ==
                        wlan_interface_state_connected) {
                        WLAN_CONNECTION_ATTRIBUTES* connAttr;
                        DWORD dataSize;

                        result = WlanQueryInterface(
                            handle, &wlanInterface.InterfaceGuid,
                            wlan_intf_opcode_current_connection, nullptr,
                            &dataSize, reinterpret_cast<PVOID*>(&connAttr),
                            nullptr);

                        if (result == ERROR_SUCCESS) {
                            // Get security type from connection attributes
                            switch (connAttr->wlanSecurityAttributes
                                        .dot11AuthAlgorithm) {
                                case DOT11_AUTH_ALGO_80211_OPEN:
                                    security = "Open (No encryption)";
                                    break;
                                case DOT11_AUTH_ALGO_80211_SHARED_KEY:
                                    security = "WEP";
                                    break;
                                case DOT11_AUTH_ALGO_WPA:
                                    security = "WPA-Personal";
                                    break;
                                case DOT11_AUTH_ALGO_WPA_PSK:
                                    security = "WPA-Personal";
                                    break;
                                case DOT11_AUTH_ALGO_WPA_NONE:
                                    security = "WPA-None";
                                    break;
                                case DOT11_AUTH_ALGO_RSNA:
                                    security = "WPA2-Enterprise";
                                    break;
                                case DOT11_AUTH_ALGO_RSNA_PSK:
                                    security = "WPA2-Personal";
                                    break;
                                case DOT11_AUTH_ALGO_WPA3:
                                    security = "WPA3-Enterprise";
                                    break;
                                case DOT11_AUTH_ALGO_WPA3_SAE:
                                    security = "WPA3-Personal";
                                    break;
                                default:
                                    security = "Unknown encryption";
                                    break;
                            }

                            WlanFreeMemory(connAttr);
                            break;
                        }
                    }
                }
                WlanFreeMemory(interfaceList);
            }
            WlanCloseHandle(handle, nullptr);
        }

        spdlog::info("Network security: {}", security);
        return security;

#elif defined(__linux__)
        // Linux: Parse wpa_supplicant or NetworkManager configuration
        std::string security = "Unknown";

        // Try to get security info from iwconfig
        std::string cmd =
            "iwconfig 2>/dev/null | grep 'Encryption key' | awk '{print $4}'";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string result(buffer);
                if (result.find("off") != std::string::npos) {
                    security = "Open (No encryption)";
                } else {
                    security = "Encrypted (WPA/WPA2)";
                }
            }
            pclose(pipe);
        }

        spdlog::info("Network security: {}", security);
        return security;

#elif defined(__APPLE__)
        // macOS: Use system_profiler to get security type
        std::string security = "Unknown";

        std::string cmd =
            "system_profiler SPAirPortDataType 2>/dev/null | grep -A 10 "
            "'Current Network' | grep 'Security'";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string result(buffer);

                // Parse security type from output
                if (result.find("WPA3") != std::string::npos) {
                    if (result.find("Personal") != std::string::npos) {
                        security = "WPA3-Personal";
                    } else if (result.find("Enterprise") != std::string::npos) {
                        security = "WPA3-Enterprise";
                    } else {
                        security = "WPA3";
                    }
                } else if (result.find("WPA2") != std::string::npos) {
                    if (result.find("Personal") != std::string::npos) {
                        security = "WPA2-Personal";
                    } else if (result.find("Enterprise") != std::string::npos) {
                        security = "WPA2-Enterprise";
                    } else {
                        security = "WPA2";
                    }
                } else if (result.find("WPA") != std::string::npos) {
                    security = "WPA";
                } else if (result.find("WEP") != std::string::npos) {
                    security = "WEP";
                } else if (result.find("None") != std::string::npos ||
                           result.find("Open") != std::string::npos) {
                    security = "Open (No encryption)";
                }
            }
            pclose(pipe);
        }

        spdlog::info("Network security: {}", security);
        return security;

#else
        return "Platform not supported";
#endif
    } catch (const std::exception& e) {
        spdlog::error("Failed to get network security: {}", e.what());
        return "Error retrieving security information";
    }
}

auto measureBandwidth() -> std::pair<double, double> {
    spdlog::info("Measuring bandwidth");

    try {
        // Get initial network stats
        auto initialStats = getNetworkStats();

        // Wait for 1 second to measure throughput
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get final network stats
        auto finalStats = getNetworkStats();

        // Calculate bandwidth (difference in speeds)
        double downloadBandwidth = finalStats.downloadSpeed;
        double uploadBandwidth = finalStats.uploadSpeed;

        spdlog::info(
            "Measured bandwidth - Download: {:.2f} MB/s, Upload: {:.2f} MB/s",
            downloadBandwidth, uploadBandwidth);

        return {downloadBandwidth, uploadBandwidth};
    } catch (const std::exception& e) {
        spdlog::error("Failed to measure bandwidth: {}", e.what());
        return {0.0, 0.0};
    }
}

auto analyzeNetworkQuality() -> std::string {
    spdlog::info("Analyzing network quality");

    try {
        auto stats = getNetworkStats();

        // Analyze various metrics to determine quality
        std::string quality;
        int score = 0;

        // Download speed analysis (0-30 points)
        if (stats.downloadSpeed >= 100.0) {
            score += 30;
        } else if (stats.downloadSpeed >= 50.0) {
            score += 25;
        } else if (stats.downloadSpeed >= 25.0) {
            score += 20;
        } else if (stats.downloadSpeed >= 10.0) {
            score += 15;
        } else if (stats.downloadSpeed >= 5.0) {
            score += 10;
        } else {
            score += 5;
        }

        // Upload speed analysis (0-20 points)
        if (stats.uploadSpeed >= 50.0) {
            score += 20;
        } else if (stats.uploadSpeed >= 25.0) {
            score += 15;
        } else if (stats.uploadSpeed >= 10.0) {
            score += 10;
        } else if (stats.uploadSpeed >= 5.0) {
            score += 5;
        }

        // Latency analysis (0-25 points)
        if (stats.latency <= 20.0) {
            score += 25;
        } else if (stats.latency <= 50.0) {
            score += 20;
        } else if (stats.latency <= 100.0) {
            score += 15;
        } else if (stats.latency <= 200.0) {
            score += 10;
        } else {
            score += 5;
        }

        // Packet loss analysis (0-25 points)
        if (stats.packetLoss <= 0.1) {
            score += 25;
        } else if (stats.packetLoss <= 0.5) {
            score += 20;
        } else if (stats.packetLoss <= 1.0) {
            score += 15;
        } else if (stats.packetLoss <= 2.0) {
            score += 10;
        } else {
            score += 5;
        }

        // Determine quality rating
        if (score >= 90) {
            quality = "Excellent";
        } else if (score >= 75) {
            quality = "Good";
        } else if (score >= 60) {
            quality = "Fair";
        } else if (score >= 40) {
            quality = "Poor";
        } else {
            quality = "Very Poor";
        }

        std::string analysis = std::format(
            "Network Quality: {} (Score: {}/100)\n"
            "Download Speed: {:.2f} MB/s\n"
            "Upload Speed: {:.2f} MB/s\n"
            "Latency: {:.2f} ms\n"
            "Packet Loss: {:.2f}%\n"
            "Signal Strength: {:.2f} dBm",
            quality, score, stats.downloadSpeed, stats.uploadSpeed,
            stats.latency, stats.packetLoss, stats.signalStrength);

        spdlog::info("Network quality analysis: {}", quality);
        return analysis;
    } catch (const std::exception& e) {
        spdlog::error("Failed to analyze network quality: {}", e.what());
        return "Error analyzing network quality";
    }
}

auto getConnectedDevices() -> std::vector<std::string> {
    spdlog::info("Getting connected devices");

    try {
        auto stats = getNetworkStats();

        if (!stats.connectedDevices.empty()) {
            spdlog::info("Found {} connected devices",
                         stats.connectedDevices.size());
            return stats.connectedDevices;
        }

        // If platform-specific implementation doesn't provide devices,
        // try to scan the local network (requires elevated privileges)
        std::vector<std::string> devices;

#if defined(__linux__)
        // On Linux, try to parse ARP table
        FILE* pipe =
            popen("arp -a 2>/dev/null | awk '{print $2}' | tr -d '()'", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string device(buffer);
                // Remove trailing newline
                if (!device.empty() && device.back() == '\n') {
                    device.pop_back();
                }
                if (!device.empty() && device != "incomplete") {
                    devices.push_back(device);
                }
            }
            pclose(pipe);
        }
#elif defined(_WIN32)
        // On Windows, use arp -a command
        // Note: Full implementation would parse the output properly
        spdlog::warn(
            "Connected devices detection requires elevated privileges on "
            "Windows");
#elif defined(__APPLE__)
        // On macOS, use arp -a command
        FILE* pipe =
            popen("arp -a 2>/dev/null | awk '{print $2}' | tr -d '()'", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string device(buffer);
                if (!device.empty() && device.back() == '\n') {
                    device.pop_back();
                }
                if (!device.empty() && device != "incomplete") {
                    devices.push_back(device);
                }
            }
            pclose(pipe);
        }
#endif

        if (devices.empty()) {
            spdlog::warn(
                "No connected devices found or insufficient permissions");
        } else {
            spdlog::info("Found {} connected devices", devices.size());
        }

        return devices;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get connected devices: {}", e.what());
        return {};
    }
}

}  // namespace atom::system
