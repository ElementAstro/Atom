/*
 * linux.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Linux WiFi Implementation

**************************************************/

#ifdef __linux__

#include "linux.hpp"

namespace atom::system::linux {

auto isConnectedToInternet_impl() -> bool {
    LOG_F(INFO, "Checking internet connection");
    bool connected = false;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != -1) {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
        if (inet_pton(AF_INET, "8.8.8.8", &(server.sin_addr)) != -1) {
            if (connect(sock, (struct sockaddr*)&server, sizeof(server)) != -1) {
                connected = true;
                LOG_F(INFO, "Connected to internet");
            } else {
                LOG_F(ERROR, "Failed to connect to internet");
            }
            close(sock);
        } else {
            LOG_F(ERROR, "inet_pton failed");
            close(sock);
        }
    } else {
        LOG_F(ERROR, "Failed to create socket");
    }
    return connected;
}

auto getCurrentWifi_impl() -> std::string {
    LOG_F(INFO, "Getting current WiFi connection");
    std::string wifiName;

    std::ifstream file("/proc/net/wireless");
    std::string line;
    while (std::getline(file, line)) {
        if (line.find(":") != std::string::npos) {
            std::istringstream iss(line);
            std::vector<std::string> tokens(
                std::istream_iterator<std::string>{iss},
                std::istream_iterator<std::string>());
            if (tokens.size() >= 2 && tokens[1] != "off/any" &&
                tokens[1] != "any") {
                std::string interface = tokens[0];
                interface = interface.substr(0, interface.find(':'));
                
                // Try to find the SSID for this interface
                std::string cmd = "iwgetid " + interface + " -r 2>/dev/null";
                FILE* pipe = popen(cmd.c_str(), "r");
                if (pipe) {
                    char buffer[128];
                    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                        wifiName = buffer;
                        // Remove trailing newline if present
                        if (!wifiName.empty() && wifiName[wifiName.length()-1] == '\n') {
                            wifiName.erase(wifiName.length()-1);
                        }
                    }
                    pclose(pipe);
                }
                
                if (!wifiName.empty()) {
                    break;
                }
            }
        }
    }

    LOG_F(INFO, "Current WiFi: {}", wifiName);
    return wifiName;
}

auto getCurrentWiredNetwork_impl() -> std::string {
    LOG_F(INFO, "Getting current wired network connection");
    std::string wiredNetworkName;

    std::ifstream file("/sys/class/net");
    std::string line;
    while (std::getline(file, line)) {
        if (line != "." && line != "..") {
            std::string path = "/sys/class/net/" + line + "/operstate";
            std::ifstream operStateFile(path);
            if (operStateFile.is_open()) {
                std::string state;
                std::getline(operStateFile, state);
                
                // Check if this is a wired interface (typically eth0, enp0s3, etc.)
                if (state == "up" && (line.substr(0, 2) == "en" || line.substr(0, 3) == "eth")) {
                    wiredNetworkName = line;
                    break;
                }
            }
        }
    }
    
    LOG_F(INFO, "Current wired network: {}", wiredNetworkName);
    return wiredNetworkName;
}

auto isHotspotConnected_impl() -> bool {
    LOG_F(INFO, "Checking if connected to a hotspot");
    bool isConnected = false;

    std::ifstream file("/proc/net/dev");
    std::string line;
    while (std::getline(file, line)) {
        if (line.find(":") != std::string::npos) {
            std::istringstream iss(line);
            std::vector<std::string> tokens(
                std::istream_iterator<std::string>{iss},
                std::istream_iterator<std::string>());
            constexpr int WIFI_INDEX = 5;
            if (tokens.size() >= 17 &&
                tokens[1].substr(0, WIFI_INDEX) == "wlx00") {
                // This is a typical pattern for USB WiFi adapters often used as hotspots
                isConnected = true;
                break;
            }
        }
    }

    // Additional check using iw command
    if (!isConnected) {
        FILE* pipe = popen("iw dev | grep -A 2 Interface | grep -i 'type ap'", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                // If we found any AP mode interfaces, hotspot is likely enabled
                isConnected = true;
            }
            pclose(pipe);
        }
    }
    
    LOG_F(INFO, "Hotspot connected: {}", isConnected ? "yes" : "no");
    return isConnected;
}

auto getHostIPs_impl() -> std::vector<std::string> {
    LOG_F(INFO, "Getting host IP addresses");
    std::vector<std::string> hostIPs;

    ifaddrs* ifaddr;

    if (getifaddrs(&ifaddr) == -1) {
        LOG_F(ERROR, "getifaddrs failed");
        return hostIPs;
    }

    for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            std::array<char, INET6_ADDRSTRLEN> ipstr{};
            void* addr;

            if (family == AF_INET) {
                addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            } else {
                addr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
            }

            inet_ntop(family, addr, ipstr.data(), ipstr.size());
            hostIPs.emplace_back(ipstr.data());
            LOG_F(INFO, "Found IP address: {}", ipstr.data());
        }
    }

    freeifaddrs(ifaddr);
    return hostIPs;
}

auto getInterfaceNames_impl() -> std::vector<std::string> {
    LOG_F(INFO, "Getting interface names");
    std::vector<std::string> interfaceNames;
    IF_ADDRS allAddrs = nullptr;

    if (atom::system::getAddresses(AF_UNSPEC, &allAddrs) != 0) {
        LOG_F(ERROR, "getAddresses failed");
        return interfaceNames;
    }

    for (auto* addr = allAddrs; addr != nullptr; addr = addr->ifa_next) {
        if (addr->ifa_name != nullptr) {
            interfaceNames.emplace_back(addr->ifa_name);
            LOG_F(INFO, "Found interface: {}", addr->ifa_name);
        }
    }

    if (allAddrs != nullptr) {
        atom::system::freeAddresses(allAddrs);
    }
    return interfaceNames;
}

auto measurePing_impl(const std::string& host, int timeout) -> float {
    LOG_F(INFO, "Measuring ping to host: {}, timeout: {} ms", host, timeout);
    float latency = -1.0f;

    // Linux下使用系统命令实现ping
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W %d %s 2>/dev/null", timeout / 1000,
             host.c_str());

    FILE* pipe = popen(cmd, "r");
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            if (strstr(buffer, "time=") || strstr(buffer, "time ")) {
                char* timePos = strstr(buffer, "time=");
                if (!timePos) {
                    timePos = strstr(buffer, "time ");
                }
                if (timePos) {
                    timePos += 5; // Skip "time=" or "time "
                    latency = std::strtof(timePos, nullptr);
                }
            }
        }
        pclose(pipe);
    }

    if (latency < 0) {
        LOG_F(ERROR, "Ping failed for host: {}", host);
    } else {
        LOG_F(INFO, "Ping successful, latency: {:.2f} ms", latency);
    }

    return latency;
}

auto getNetworkStats_impl() -> NetworkStats {
    LOG_F(INFO, "Getting network statistics");
    NetworkStats stats{};

    // 读取/proc/net/dev获取网络统计信息
    std::ifstream netdev("/proc/net/dev");
    std::string line;
    unsigned long long bytesRecv = 0, bytesSent = 0;

    while (std::getline(netdev, line)) {
        if (line.find(':') != std::string::npos) {
            std::istringstream iss(line.substr(line.find(':') + 1));
            unsigned long long recv, send;
            iss >> recv >> std::ws >> send;
            bytesRecv += recv;
            bytesSent += send;
        }
    }

    // These aren't actual speeds but rather total bytes - 
    // for speed we'd need to measure over time
    stats.downloadSpeed = bytesRecv / 1024.0 / 1024;  // Convert to MB
    stats.uploadSpeed = bytesSent / 1024.0 / 1024;

    // 测量网络延迟
    std::string host = "8.8.8.8";
    int timeout = 1000;
    stats.latency = measurePing_impl(host, timeout);

    // 使用iwconfig获取信号强度
    FILE* pipe = popen("iwconfig 2>/dev/null | grep 'Signal level'", "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            if (strstr(buffer, "Signal level=")) {
                char* levelPos = strstr(buffer, "Signal level=");
                if (levelPos) {
                    levelPos += 13; // Skip "Signal level="
                    stats.signalStrength = std::strtof(levelPos, nullptr);
                }
            }
        }
        pclose(pipe);
    }

    LOG_F(INFO,
          "Network stats - Download: {:.2f} MB/s, Upload: {:.2f} MB/s, "
          "Latency: {:.1f} ms, Signal: {:.1f} dBm",
          stats.downloadSpeed, stats.uploadSpeed, stats.latency,
          stats.signalStrength);

    return stats;
}

} // namespace atom::system::linux

#endif // __linux__
