/*
 * wifi.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Wifi Information

**************************************************/

#include "atom/sysinfo/wifi/wifi.hpp"
#include "atom/sysinfo/wifi/common.hpp"
#include "atom/sysinfo/wifi/error_handler.hpp"

#ifdef _WIN32
#include "atom/sysinfo/wifi/windows.hpp"
namespace impl = atom::system::windows;
#elif defined(__linux__)
#include "atom/sysinfo/wifi/linux.hpp"
namespace impl = atom::system::linux;
#elif defined(__APPLE__)
#include "atom/sysinfo/wifi/macos.hpp"
namespace impl = atom::system::macos;
#endif

namespace atom::system {

auto isConnectedToInternet() -> bool {
    WIFI_ERROR_CONTEXT();

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    try {
        return impl::isConnectedToInternet_impl();
    } catch (const std::exception& e) {
        WIFI_LOG_ERROR(WiFiError::SYSTEM_CALL_FAILED, "Exception in isConnectedToInternet: " + std::string(e.what()));
        return false;
    }
#else
    WIFI_LOG_ERROR(WiFiError::PLATFORM_NOT_SUPPORTED, "Unsupported operating system for internet connectivity check");
    return false;
#endif
}

auto getCurrentWifi() -> std::string {
    WIFI_ERROR_CONTEXT();

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    try {
        auto result = impl::getCurrentWifi_impl();
        if (result.empty()) {
            WIFI_ADD_CONTEXT("reason", "No active WiFi connection found");
        }
        return result;
    } catch (const std::exception& e) {
        WIFI_LOG_ERROR(WiFiError::NETWORK_INTERFACE_ERROR, "Exception in getCurrentWifi: " + std::string(e.what()));
        return {};
    }
#else
    WIFI_LOG_ERROR(WiFiError::PLATFORM_NOT_SUPPORTED, "Unsupported operating system for WiFi information retrieval");
    return {};
#endif
}

auto getCurrentWiredNetwork() -> std::string {
#if defined(_WIN32) || defined(__linux__)
    return impl::getCurrentWiredNetwork_impl();
#elif defined(__APPLE__)
    LOG_F(WARNING, "getCurrentWiredNetwork: Retrieving current wired network is not supported on macOS.");
    return {};
#else
    LOG_F(ERROR, "getCurrentWiredNetwork: Unsupported operating system. Unable to retrieve current wired network.");
    return {};
#endif
}

auto isHotspotConnected() -> bool {
#if defined(_WIN32) || defined(__linux__)
    return impl::isHotspotConnected_impl();
#elif defined(__APPLE__)
    LOG_F(WARNING, "isHotspotConnected: Checking hotspot connectivity is not supported on macOS.");
    return false;
#else
    LOG_F(ERROR, "isHotspotConnected: Unsupported operating system. Unable to determine hotspot connectivity.");
    return false;
#endif
}

auto getHostIPs() -> std::vector<std::string> {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getHostIPs_impl();
#else
    LOG_F(ERROR, "getHostIPs: Unsupported operating system. Unable to retrieve host IP addresses.");
    return {};
#endif
}

auto getIPv4Addresses() -> std::vector<std::string> {
    LOG_F(INFO, "getIPv4Addresses: Retrieving all IPv4 addresses.");
    auto [addresses, error] = atom::system::getIPAddresses<sockaddr_in>(AF_INET);
    if (error != WiFiError::SUCCESS) {
        LOG_F(ERROR, "getIPv4Addresses: Failed to retrieve IPv4 addresses: {}", wifiErrorToString(error));
    }
    return addresses;
}

auto getIPv6Addresses() -> std::vector<std::string> {
    LOG_F(INFO, "getIPv6Addresses: Retrieving all IPv6 addresses.");
    auto [addresses, error] = atom::system::getIPAddresses<sockaddr_in6>(AF_INET6);
    if (error != WiFiError::SUCCESS) {
        LOG_F(ERROR, "getIPv6Addresses: Failed to retrieve IPv6 addresses: {}", wifiErrorToString(error));
    }
    return addresses;
}

auto getInterfaceNames() -> std::vector<std::string> {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getInterfaceNames_impl();
#else
    LOG_F(ERROR, "getInterfaceNames: Unsupported operating system. Unable to retrieve interface names.");
    return {};
#endif
}

auto getNetworkStats() -> NetworkStats {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getNetworkStats_impl();
#else
    LOG_F(ERROR, "getNetworkStats: Unsupported operating system. Unable to retrieve network statistics.");
    return {};
#endif
}

// Static cache for network history
static NetworkCache<std::vector<NetworkStats>> history_cache(10);

auto getNetworkHistory(std::chrono::minutes duration) -> std::vector<NetworkStats> {
    LOG_F(INFO, "getNetworkHistory: Retrieving network history for the past {} minutes.", duration.count());

    std::string cache_key = "network_history_" + std::to_string(duration.count());
    if (auto cached = history_cache.get(cache_key)) {
        return *cached;
    }

    std::vector<NetworkStats> history;

    // Collect current stats as a baseline
    auto current_stats = getNetworkStats();
    history.push_back(current_stats);

    // For a real implementation, you would:
    // 1. Read historical data from a persistent storage
    // 2. Collect data at regular intervals
    // 3. Store timestamps with each measurement

    // Cache the result for 60 seconds
    history_cache.put(cache_key, history, std::chrono::seconds(60));

    return history;
}

auto scanAvailableNetworks() -> std::vector<std::string> {
    LOG_F(INFO, "scanAvailableNetworks: Scanning for available WiFi networks.");

    static NetworkCache<std::vector<std::string>> scan_cache(5);
    static std::string cache_key = "available_networks";

    if (auto cached = scan_cache.get(cache_key)) {
        return *cached;
    }

    std::vector<std::string> networks;

#ifdef __linux__
    // Use iwlist to scan for networks
    auto [output, error] = executeCommand("iwlist scan 2>/dev/null | grep 'ESSID:' | cut -d'\"' -f2 | sort -u", 10000);
    if (error == WiFiError::SUCCESS && !output.empty()) {
        std::istringstream iss(output);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line != "\"\"") {
                networks.push_back(line);
            }
        }
    }
#elif defined(_WIN32)
    // Use netsh to scan for networks
    auto [output, error] = executeCommand("netsh wlan show profiles", 5000);
    if (error == WiFiError::SUCCESS && !output.empty()) {
        std::regex profileRegex(R"(All User Profile\s*:\s*(.+))");
        std::sregex_iterator iter(output.begin(), output.end(), profileRegex);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            std::string network = (*iter)[1].str();
            // Trim whitespace
            network.erase(0, network.find_first_not_of(" \t"));
            network.erase(network.find_last_not_of(" \t") + 1);
            if (!network.empty()) {
                networks.push_back(network);
            }
        }
    }
#elif defined(__APPLE__)
    // Use airport utility on macOS
    auto [output, error] = executeCommand("/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport -s", 10000);
    if (error == WiFiError::SUCCESS && !output.empty()) {
        std::istringstream iss(output);
        std::string line;
        std::getline(iss, line); // Skip header

        while (std::getline(iss, line)) {
            if (!line.empty()) {
                // Extract SSID (first column)
                size_t space_pos = line.find(' ');
                if (space_pos != std::string::npos) {
                    std::string ssid = line.substr(0, space_pos);
                    if (!ssid.empty()) {
                        networks.push_back(ssid);
                    }
                }
            }
        }
    }
#endif

    // Cache the result for 30 seconds
    scan_cache.put(cache_key, networks, std::chrono::seconds(30));

    LOG_F(INFO, "Found {} available networks", networks.size());
    return networks;
}

auto getNetworkSecurity() -> std::string {
    LOG_F(INFO, "getNetworkSecurity: Retrieving network security information.");

    static NetworkCache<std::string> security_cache(5);
    static std::string cache_key = "network_security";

    if (auto cached = security_cache.get(cache_key)) {
        return *cached;
    }

    std::string security_info;
    std::string current_wifi = getCurrentWifi();

    if (current_wifi.empty()) {
        security_info = "No active WiFi connection";
    } else {
#ifdef __linux__
        // Get security information using iwconfig
        std::string cmd = "iwconfig 2>/dev/null | grep -A 5 '" + current_wifi + "'";
        auto [output, error] = executeCommand(cmd, 5000);
        if (error == WiFiError::SUCCESS && !output.empty()) {
            if (output.find("Encryption key:on") != std::string::npos) {
                if (output.find("WPA") != std::string::npos) {
                    security_info = "WPA/WPA2 Encrypted";
                } else if (output.find("WEP") != std::string::npos) {
                    security_info = "WEP Encrypted";
                } else {
                    security_info = "Encrypted (Unknown type)";
                }
            } else {
                security_info = "Open Network (No encryption)";
            }
        } else {
            security_info = "Security information unavailable";
        }
#elif defined(_WIN32)
        // Use netsh to get security information
        std::string cmd = "netsh wlan show profile \"" + current_wifi + "\" key=clear";
        auto [output, error] = executeCommand(cmd, 5000);
        if (error == WiFiError::SUCCESS && !output.empty()) {
            if (output.find("Authentication") != std::string::npos) {
                if (output.find("WPA2") != std::string::npos) {
                    security_info = "WPA2 Encrypted";
                } else if (output.find("WPA") != std::string::npos) {
                    security_info = "WPA Encrypted";
                } else if (output.find("WEP") != std::string::npos) {
                    security_info = "WEP Encrypted";
                } else {
                    security_info = "Encrypted (Unknown type)";
                }
            } else {
                security_info = "Open Network";
            }
        } else {
            security_info = "Security information unavailable";
        }
#else
        security_info = "Security information not available on this platform";
#endif
    }

    // Cache the result for 60 seconds
    security_cache.put(cache_key, security_info, std::chrono::seconds(60));

    LOG_F(INFO, "Network security: {}", security_info);
    return security_info;
}

auto measureBandwidth() -> std::pair<double, double> {
    LOG_F(INFO, "measureBandwidth: Measuring network bandwidth.");

    // Use the improved speed measurement from common
    auto result = measureNetworkSpeed("8.8.8.8");

    if (result.error != WiFiError::SUCCESS) {
        LOG_F(ERROR, "measureBandwidth: Failed to measure bandwidth: {}", wifiErrorToString(result.error));
        return {0.0, 0.0};
    }

    // For a real implementation, you would:
    // 1. Download a test file to measure download speed
    // 2. Upload data to measure upload speed
    // 3. Use multiple connections for more accurate results
    // 4. Calculate actual throughput over time

    LOG_F(INFO, "Bandwidth measurement completed - Download: {:.2f} Mbps, Upload: {:.2f} Mbps",
          result.download_mbps, result.upload_mbps);

    return {result.download_mbps, result.upload_mbps};
}

auto analyzeNetworkQuality() -> std::string {
    LOG_F(INFO, "analyzeNetworkQuality: Analyzing network quality.");

    static NetworkCache<std::string> quality_cache(5);
    static std::string cache_key = "network_quality";

    if (auto cached = quality_cache.get(cache_key)) {
        return *cached;
    }

    std::string quality_analysis;
    auto stats = getNetworkStats();

    // Analyze latency
    std::string latency_quality;
    if (stats.latency < 0) {
        latency_quality = "Unknown";
    } else if (stats.latency < 20) {
        latency_quality = "Excellent";
    } else if (stats.latency < 50) {
        latency_quality = "Good";
    } else if (stats.latency < 100) {
        latency_quality = "Fair";
    } else {
        latency_quality = "Poor";
    }

    // Analyze signal strength
    std::string signal_quality;
    if (stats.signalStrength > -30) {
        signal_quality = "Excellent";
    } else if (stats.signalStrength > -50) {
        signal_quality = "Good";
    } else if (stats.signalStrength > -70) {
        signal_quality = "Fair";
    } else if (stats.signalStrength > -80) {
        signal_quality = "Weak";
    } else {
        signal_quality = "Very Weak";
    }

    // Overall quality assessment
    std::string overall_quality;
    if ((latency_quality == "Excellent" || latency_quality == "Good") &&
        (signal_quality == "Excellent" || signal_quality == "Good")) {
        overall_quality = "Excellent";
    } else if (latency_quality != "Poor" && signal_quality != "Very Weak") {
        overall_quality = "Good";
    } else {
        overall_quality = "Poor";
    }

    quality_analysis = "Overall: " + overall_quality +
                      ", Latency: " + latency_quality + " (" + std::to_string(static_cast<int>(stats.latency)) + "ms)" +
                      ", Signal: " + signal_quality + " (" + std::to_string(static_cast<int>(stats.signalStrength)) + "dBm)";

    // Cache the result for 30 seconds
    quality_cache.put(cache_key, quality_analysis, std::chrono::seconds(30));

    LOG_F(INFO, "Network quality analysis: {}", quality_analysis);
    return quality_analysis;
}

auto getConnectedDevices() -> std::vector<std::string> {
    LOG_F(INFO, "getConnectedDevices: Retrieving list of connected devices.");

    static NetworkCache<std::vector<std::string>> devices_cache(5);
    static std::string cache_key = "connected_devices";

    if (auto cached = devices_cache.get(cache_key)) {
        return *cached;
    }

    std::vector<std::string> devices;

#ifdef __linux__
    // Use ARP table to find connected devices
    auto [output, error] = executeCommand("arp -a | grep -E '([0-9]{1,3}\\.){3}[0-9]{1,3}' | awk '{print $2}' | tr -d '()'", 5000);
    if (error == WiFiError::SUCCESS && !output.empty()) {
        std::istringstream iss(output);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && isValidIPAddress(line)) {
                devices.push_back(line);
            }
        }
    }

    // Also try to get device names from DHCP leases if available
    auto [dhcp_output, dhcp_error] = executeCommand("cat /var/lib/dhcp/dhcpd.leases 2>/dev/null | grep 'client-hostname' | awk '{print $2}' | tr -d ';\"' | sort -u", 3000);
    if (dhcp_error == WiFiError::SUCCESS && !dhcp_output.empty()) {
        std::istringstream dhcp_iss(dhcp_output);
        std::string hostname;
        while (std::getline(dhcp_iss, hostname)) {
            if (!hostname.empty()) {
                devices.push_back(hostname + " (hostname)");
            }
        }
    }
#elif defined(_WIN32)
    // Use arp command on Windows
    auto [output, error] = executeCommand("arp -a", 5000);
    if (error == WiFiError::SUCCESS && !output.empty()) {
        std::regex ipRegex(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
        std::sregex_iterator iter(output.begin(), output.end(), ipRegex);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            std::string ip = (*iter)[1].str();
            if (isValidIPAddress(ip)) {
                devices.push_back(ip);
            }
        }
    }
#elif defined(__APPLE__)
    // Use arp command on macOS
    auto [output, error] = executeCommand("arp -a | grep -E '([0-9]{1,3}\\.){3}[0-9]{1,3}' | awk '{print $2}' | tr -d '()'", 5000);
    if (error == WiFiError::SUCCESS && !output.empty()) {
        std::istringstream iss(output);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && isValidIPAddress(line)) {
                devices.push_back(line);
            }
        }
    }
#endif

    // Remove duplicates
    std::sort(devices.begin(), devices.end());
    devices.erase(std::unique(devices.begin(), devices.end()), devices.end());

    // Cache the result for 60 seconds
    devices_cache.put(cache_key, devices, std::chrono::seconds(60));

    LOG_F(INFO, "Found {} connected devices", devices.size());
    return devices;
}

}  // namespace atom::system
