/*
 * wifi.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Wifi Information

**************************************************/

#include "atom/sysinfo/wifi.hpp"

#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <iptypes.h>
#include <wlanapi.h>
#include <ws2tcpip.h>
#include <pdh.h>
#include <thread>
#include <icmpapi.h>
#undef max
#undef min
// clang-format on
#if !defined(__MINGW32__) && !defined(__MINGW64__)
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "pdh.lib")
#endif
#elif defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/CaptiveNetwork.h>
#endif

#include "atom/log/loguru.hpp"

#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
using IF_ADDRS = PIP_ADAPTER_ADDRESSES;
using IF_ADDRS_UNICAST = PIP_ADAPTER_UNICAST_ADDRESS;
#else
using IF_ADDRS = struct ifaddrs*;
using IF_ADDRS_UNICAST = struct ifaddrs*;
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

namespace atom::system {
bool isConnectedToInternet() {
    LOG_F(INFO, "Checking internet connection");
    bool connected = false;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != -1) {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
#ifdef _WIN32
        server.sin_addr.s_addr = inet_addr("8.8.8.8");
#else
        if (inet_pton(AF_INET, "8.8.8.8", &(server.sin_addr)) != -1) {
#endif
        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) != -1) {
            connected = true;
            LOG_F(INFO, "Successfully connected to 8.8.8.8");
        } else {
            LOG_F(ERROR, "Failed to connect to 8.8.8.8");
        }
#ifdef _WIN32
        closesocket(sock);
#else
            close(sock);
        }
#endif
    } else {
        LOG_F(ERROR, "Failed to create socket");
    }
    return connected;
}

// 获取当前连接的WIFI
auto getCurrentWifi() -> std::string {
    LOG_F(INFO, "Getting current WiFi connection");
    std::string wifiName;

#ifdef _WIN32
    DWORD negotiatedVersion;
    HANDLE handle;
    if (WlanOpenHandle(2, nullptr, &negotiatedVersion, &handle) ==
        ERROR_SUCCESS) {
        WLAN_INTERFACE_INFO_LIST* interfaceInfoList;
        if (WlanEnumInterfaces(handle, nullptr, &interfaceInfoList) ==
            ERROR_SUCCESS) {
            for (DWORD i = 0; i < interfaceInfoList->dwNumberOfItems; ++i) {
                WLAN_INTERFACE_INFO* interfaceInfo =
                    &interfaceInfoList->InterfaceInfo[i];
                if (interfaceInfo->isState == wlan_interface_state_connected) {
                    WLAN_CONNECTION_ATTRIBUTES* connectionAttributes;
                    DWORD dataSize = 0;
                    if (WlanQueryInterface(
                            handle, &interfaceInfo->InterfaceGuid,
                            wlan_intf_opcode_current_connection, nullptr,
                            &dataSize,
                            reinterpret_cast<void**>(&connectionAttributes),
                            nullptr) == ERROR_SUCCESS) {
                        wifiName = reinterpret_cast<const char*>(
                            connectionAttributes->wlanAssociationAttributes
                                .dot11Ssid.ucSSID);
                        LOG_F(INFO, "Connected to WiFi: {}", wifiName);
                        break;
                    } else {
                        LOG_F(ERROR, "WlanQueryInterface failed");
                    }
                }
            }
        } else {
            LOG_F(ERROR, "WlanEnumInterfaces failed");
        }
        WlanCloseHandle(handle, nullptr);
    } else {
        LOG_F(ERROR, "WlanOpenHandle failed");
    }
#elif defined(__linux__)
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
                wifiName = tokens[0].substr(0, tokens[0].find(':'));
                LOG_F(INFO, "Connected to WiFi: {}", wifiName);
                break;
            }
        }
    }
#elif defined(__APPLE__)
    CFArrayRef interfaces = CNCopySupportedInterfaces();
    if (interfaces != nullptr) {
        CFDictionaryRef info =
            CNCopyCurrentNetworkInfo(CFArrayGetValueAtIndex(interfaces, 0));
        if (info != nullptr) {
            CFStringRef ssid = static_cast<CFStringRef>(
                CFDictionaryGetValue(info, kCNNetworkInfoKeySSID));
            if (ssid != nullptr) {
                char buffer[256];
                CFStringGetCString(ssid, buffer, sizeof(buffer),
                                   kCFStringEncodingUTF8);
                wifiName = buffer;
                LOG_F(INFO, "Connected to WiFi: {}", wifiName);
            }
            CFRelease(info);
        }
        CFRelease(interfaces);
    } else {
        LOG_F(ERROR, "CNCopySupportedInterfaces failed");
    }
#else
    LOG_F(ERROR, "Unsupported operating system");
#endif

    return wifiName;
}

// 获取当前连接的有线网络
auto getCurrentWiredNetwork() -> std::string {
    LOG_F(INFO, "Getting current wired network connection");
    std::string wiredNetworkName;

#ifdef _WIN32
    PIP_ADAPTER_INFO adapterInfo = nullptr;
    ULONG bufferLength = 0;

    if (GetAdaptersInfo(adapterInfo, &bufferLength) == ERROR_BUFFER_OVERFLOW) {
        adapterInfo =
            reinterpret_cast<IP_ADAPTER_INFO*>(new char[bufferLength]);
        if (GetAdaptersInfo(adapterInfo, &bufferLength) == NO_ERROR) {
            for (PIP_ADAPTER_INFO adapter = adapterInfo; adapter != nullptr;
                 adapter = adapter->Next) {
                if (adapter->Type == MIB_IF_TYPE_ETHERNET) {
                    wiredNetworkName = adapter->AdapterName;
                    LOG_F(INFO, "Connected to wired network: {}",
                          wiredNetworkName);
                    break;
                }
            }
        } else {
            LOG_F(ERROR, "GetAdaptersInfo failed");
        }
        delete[] reinterpret_cast<char*>(adapterInfo);
    } else {
        LOG_F(ERROR, "GetAdaptersInfo failed");
    }
#elif defined(__linux__)
    std::ifstream file("/sys/class/net");
    std::string line;
    while (std::getline(file, line)) {
        if (line != "." && line != "..") {
            std::string path = "/sys/class/net/" + line + "/operstate";
            std::ifstream operStateFile(path);
            if (operStateFile.is_open()) {
                std::string operState;
                std::getline(operStateFile, operState);
                if (operState == "up") {
                    wiredNetworkName = line;
                    LOG_F(INFO, "Connected to wired network: {}",
                          wiredNetworkName);
                    break;
                }
            }
        }
    }
#elif defined(__APPLE__)
    // macOS下暂不支持获取当前连接的有线网络
    LOG_F(WARNING, "Getting current wired network is not supported on macOS");
#else
    LOG_F(ERROR, "Unsupported operating system");
#endif

    return wiredNetworkName;
}

// 检查是否连接到热点
auto isHotspotConnected() -> bool {
    LOG_F(INFO, "Checking if connected to a hotspot");
    bool isConnected = false;

#ifdef _WIN32
    DWORD negotiatedVersion;
    HANDLE handle;
    if (WlanOpenHandle(2, nullptr, &negotiatedVersion, &handle) ==
        ERROR_SUCCESS) {
        WLAN_INTERFACE_INFO_LIST* interfaceInfoList;
        if (WlanEnumInterfaces(handle, nullptr, &interfaceInfoList) ==
            ERROR_SUCCESS) {
            for (DWORD i = 0; i < interfaceInfoList->dwNumberOfItems; ++i) {
                WLAN_INTERFACE_INFO* interfaceInfo =
                    &interfaceInfoList->InterfaceInfo[i];
                if (interfaceInfo->isState == wlan_interface_state_connected) {
                    WLAN_CONNECTION_ATTRIBUTES* connectionAttributes;
                    DWORD dataSize = 0;
                    if (WlanQueryInterface(
                            handle, &interfaceInfo->InterfaceGuid,
                            wlan_intf_opcode_current_connection, nullptr,
                            &dataSize,
                            reinterpret_cast<void**>(&connectionAttributes),
                            nullptr) == ERROR_SUCCESS) {
                        if (connectionAttributes->isState ==
                                wlan_interface_state_connected &&
                            connectionAttributes->wlanAssociationAttributes
                                    .dot11BssType ==
                                dot11_BSS_type_independent) {
                            isConnected = true;
                            LOG_F(INFO, "Connected to a hotspot");
                            break;
                        }
                    } else {
                        LOG_F(ERROR, "WlanQueryInterface failed");
                    }
                }
            }
        } else {
            LOG_F(ERROR, "WlanEnumInterfaces failed");
        }
        WlanCloseHandle(handle, nullptr);
    } else {
        LOG_F(ERROR, "WlanOpenHandle failed");
    }
#elif defined(__linux__)
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
                isConnected = true;
                LOG_F(INFO, "Connected to a hotspot");
                break;
            }
        }
    }
#elif defined(__APPLE__)
    // macOS下暂不支持检查是否连接到热点
    LOG_F(WARNING,
          "Checking if connected to a hotspot is not supported on macOS");
#else
    LOG_F(ERROR, "Unsupported operating system");
#endif

    return isConnected;
}

auto getHostIPs() -> std::vector<std::string> {
    LOG_F(INFO, "Getting host IP addresses");
    std::vector<std::string> hostIPs;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_F(ERROR, "WSAStartup failed");
        return hostIPs;
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        LOG_F(ERROR, "gethostname failed");
        WSACleanup();
        return hostIPs;
    }

    addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        LOG_F(ERROR, "getaddrinfo failed");
        WSACleanup();
        return hostIPs;
    }

    for (addrinfo* p = res; p != NULL; p = p->ai_next) {
        void* addr;
        char ipstr[INET6_ADDRSTRLEN];
        if (p->ai_family == AF_INET) {
            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(p->ai_addr);
            addr = &(ipv4->sin_addr);
        } else {
            sockaddr_in6* ipv6 = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
            addr = &(ipv6->sin6_addr);
        }
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
        hostIPs.push_back(std::string(ipstr));
        LOG_F(INFO, "Found IP address: {}", ipstr);
    }

    freeaddrinfo(res);
    WSACleanup();
#else
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
                auto* ipv4 = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
                addr = &(ipv4->sin_addr);
            } else {  // AF_INET6
                auto* ipv6 = reinterpret_cast<sockaddr_in6*>(ifa->ifa_addr);
                addr = &(ipv6->sin6_addr);
            }

            inet_ntop(family, addr, ipstr.data(), ipstr.size());
            hostIPs.emplace_back(ipstr.data());
            LOG_F(INFO, "Found IP address: {}", ipstr.data());
        }
    }

    freeifaddrs(ifaddr);
#endif

    return hostIPs;
}

template <typename AddressType>
auto getIPAddresses(int addressFamily) -> std::vector<std::string> {
    LOG_F(INFO, "Getting IP addresses for address family: {}", addressFamily);
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
            for (auto unicastAddress = adapter->FirstUnicastAddress;
                 unicastAddress; unicastAddress = unicastAddress->Next) {
                auto sockAddr = reinterpret_cast<AddressType*>(
                    unicastAddress->Address.lpSockaddr);
                char addressBuffer[std::max(INET_ADDRSTRLEN,
                                            INET6_ADDRSTRLEN)] = {0};
                void* addrPtr =
                    (addressFamily == AF_INET)
                        ? static_cast<void*>(
                              &reinterpret_cast<sockaddr_in*>(sockAddr)
                                   ->sin_addr)
                        : static_cast<void*>(
                              &reinterpret_cast<sockaddr_in6*>(sockAddr)
                                   ->sin6_addr);
                if (inet_ntop(addressFamily, addrPtr, addressBuffer,
                              sizeof(addressBuffer))) {
                    addresses.emplace_back(addressBuffer);
                    LOG_F(INFO, "Found IP address: {}", addressBuffer);
                }
            }
        }
    }
#else
    struct ifaddrs* ifAddrList = nullptr;

    if (getifaddrs(&ifAddrList) == -1) {
        LOG_F(ERROR, "getifaddrs failed");
        return addresses;
    }

    // Use smart pointer to automatically manage the lifecycle of ifAddrList
    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifAddrListGuard(
        ifAddrList, freeifaddrs);

    for (auto* ifa = ifAddrList; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == addressFamily) {
            auto* sockAddr = reinterpret_cast<sockaddr*>(ifa->ifa_addr);
            // Using std::array to manage the address buffer
            std::array<char, std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)>
                addressBuffer{};
            void* addrPtr = nullptr;

            // Determine the type of the IP address and set the pointer
            // accordingly
            if (addressFamily == AF_INET) {
                addrPtr = &reinterpret_cast<sockaddr_in*>(sockAddr)->sin_addr;
            } else if (addressFamily == AF_INET6) {
                addrPtr = &reinterpret_cast<sockaddr_in6*>(sockAddr)->sin6_addr;
            }

            // Convert the IP address from binary to text form
            if (inet_ntop(addressFamily, addrPtr, addressBuffer.data(),
                          addressBuffer.size())) {
                addresses.emplace_back(addressBuffer.data());
                LOG_F(INFO, "Found IP address: {}", addressBuffer.data());
            }
        }
    }

#endif

    return addresses;
}

auto getIPv4Addresses() -> std::vector<std::string> {
    LOG_F(INFO, "Getting IPv4 addresses");
    return getIPAddresses<sockaddr_in>(AF_INET);
}

auto getIPv6Addresses() -> std::vector<std::string> {
    LOG_F(INFO, "Getting IPv6 addresses");
    return getIPAddresses<sockaddr_in6>(AF_INET6);
}

void freeAddresses(IF_ADDRS addrs) {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    HeapFree(GetProcessHeap(), 0, addrs);
#else
    freeifaddrs(addrs);
#endif
}

auto getAddresses(int family, IF_ADDRS* addrs) -> int {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    DWORD rv = 0;
    ULONG bufLen =
        15000;  // recommended size from Windows API docs to avoid error
    ULONG iter = 0;
    do {
        *addrs = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, bufLen);
        if (*addrs == nullptr) {
            LOG_F(ERROR, "HeapAlloc failed");
            return -1;
        }

        rv = GetAdaptersAddresses(family, GAA_FLAG_INCLUDE_PREFIX, NULL, *addrs,
                                  &bufLen);
        if (rv == ERROR_BUFFER_OVERFLOW) {
            freeAddresses(*addrs);
            *addrs = nullptr;
            bufLen = bufLen * 2;  // Double buffer length for the next attempt
        } else {
            break;
        }
        iter++;
    } while ((rv == ERROR_BUFFER_OVERFLOW) && (iter < 3));
    if (rv != NO_ERROR) {
        LOG_F(ERROR, "GetAdaptersAddresses failed");
        return -1;
    }
    return 0;
#else
    (void)family;
    return getifaddrs(addrs);
#endif
}

auto getInterfaceNames() -> std::vector<std::string> {
    LOG_F(INFO, "Getting interface names");
    std::vector<std::string> interfaceNames;
    IF_ADDRS allAddrs = nullptr;

    if (getAddresses(AF_UNSPEC, &allAddrs) != 0) {
        LOG_F(ERROR, "getAddresses failed");
        return interfaceNames;
    }

#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    for (auto adapter = allAddrs; adapter != nullptr; adapter = adapter->Next) {
        std::string interfaceName;
        if (adapter->FriendlyName) {
            // Convert wide string to UTF-8 without using deprecated
            // wstring_convert
            int size_needed = WideCharToMultiByte(
                CP_UTF8, 0, adapter->FriendlyName, -1, NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                std::vector<char> buffer(size_needed);
                WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1,
                                    buffer.data(), size_needed, NULL, NULL);
                interfaceName = buffer.data();
            }
        }
        if (!interfaceName.empty()) {
            interfaceNames.push_back(interfaceName);
            LOG_F(INFO, "Found interface: {}", interfaceName);
        }
    }
#else
    for (auto* addr = allAddrs; addr != nullptr; addr = addr->ifa_next) {
        if (addr->ifa_name != nullptr) {
            interfaceNames.emplace_back(addr->ifa_name);
            LOG_F(INFO, "Found interface: {}", addr->ifa_name);
        }
    }
#endif

    if (allAddrs != nullptr) {
        freeAddresses(allAddrs);
    }
    return interfaceNames;
}

float measurePing(const std::string& host, int timeout) {
    LOG_F(INFO, "Measuring ping to host: {}, timeout: {} ms", host, timeout);
    float latency = -1.0f;

#ifdef _WIN32
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_F(ERROR, "WSAStartup failed");
        return latency;
    }

    // 创建ICMP句柄
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "IcmpCreateFile failed");
        WSACleanup();
        return latency;
    }

    // 解析目标IP
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    if (getaddrinfo(host.c_str(), NULL, &hints, &res) != 0) {
        LOG_F(ERROR, "getaddrinfo failed for host: {}", host);
        IcmpCloseHandle(hIcmp);
        WSACleanup();
        return latency;
    }

    dest.sin_addr = ((struct sockaddr_in*)(res->ai_addr))->sin_addr;
    freeaddrinfo(res);

    // Ping数据准备
    const int PING_DATA_SIZE = 32;
    unsigned char pingData[PING_DATA_SIZE];
    memset(pingData, 0, sizeof(pingData));

    // 分配响应缓冲区
    const int REPLY_BUFFER_SIZE = sizeof(ICMP_ECHO_REPLY) + PING_DATA_SIZE + 8;
    unsigned char* replyBuffer = new unsigned char[REPLY_BUFFER_SIZE];

    // 发送Echo请求
    DWORD replyCount =
        IcmpSendEcho(hIcmp, dest.sin_addr.S_un.S_addr, pingData, PING_DATA_SIZE,
                     NULL, replyBuffer, REPLY_BUFFER_SIZE, timeout);

    if (replyCount > 0) {
        PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)replyBuffer;
        latency = static_cast<float>(pEchoReply->RoundTripTime);
        LOG_F(INFO, "Ping successful, latency: {:.1f} ms", latency);
    } else {
        LOG_F(ERROR, "Ping failed, error code: {}", GetLastError());
    }

    delete[] replyBuffer;
    IcmpCloseHandle(hIcmp);
    WSACleanup();

#elif defined(__linux__)
    // Linux下使用系统命令实现ping
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W %d %s 2>/dev/null", timeout / 1000,
             host.c_str());

    FILE* pipe = popen(cmd, "r");
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            if (strstr(buffer, "time=") || strstr(buffer, "time ")) {
                // 解析ping输出获取延迟值
                char* timePos = strstr(buffer, "time=");
                if (!timePos)
                    timePos = strstr(buffer, "time ");
                if (timePos) {
                    timePos += 5;  // 跳过"time="或"time "
                    latency = atof(timePos);
                    LOG_F(INFO, "Ping successful, latency: {:.1f} ms", latency);
                }
            }
        }
        pclose(pipe);
    }

    if (latency < 0) {
        LOG_F(ERROR, "Ping failed for host: {}", host);
    }
#endif

    return latency;
}

auto getNetworkStats() -> NetworkStats {
    LOG_F(INFO, "Getting network statistics");
    NetworkStats stats{};

#ifdef _WIN32
    PDH_HQUERY query;
    PDH_HCOUNTER bytesRecvCounter, bytesSentCounter;
    if (PdhOpenQuery(NULL, 0, &query) == ERROR_SUCCESS) {
#ifdef UNICODE
        PdhAddCounterW(query, L"\\Network Interface(*)\\Bytes Received/sec", 0,
                       &bytesRecvCounter);
        PdhAddCounterW(query, L"\\Network Interface(*)\\Bytes Sent/sec", 0,
                       &bytesSentCounter);
#else
        PdhAddCounterA(query, "\\Network Interface(*)\\Bytes Received/sec", 0,
                       &bytesRecvCounter);
        PdhAddCounterA(query, "\\Network Interface(*)\\Bytes Sent/sec", 0,
                       &bytesSentCounter);
#endif

        PdhCollectQueryData(query);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        PdhCollectQueryData(query);

        PDH_FMT_COUNTERVALUE recvValue, sentValue;
        PdhGetFormattedCounterValue(bytesRecvCounter, PDH_FMT_DOUBLE, NULL,
                                    &recvValue);
        PdhGetFormattedCounterValue(bytesSentCounter, PDH_FMT_DOUBLE, NULL,
                                    &sentValue);

        stats.downloadSpeed =
            recvValue.doubleValue / 1024 / 1024;  // Convert to MB/s
        stats.uploadSpeed = sentValue.doubleValue / 1024 / 1024;

        PdhCloseQuery(query);
    }
#elif __linux__
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

    stats.downloadSpeed = bytesRecv / 1024.0 / 1024;  // Convert to MB/s
    stats.uploadSpeed = bytesSent / 1024.0 / 1024;
#endif

    // 测量网络延迟
    std::string host = "8.8.8.8";
    int timeout = 1000;
    stats.latency = measurePing(host, timeout);

    // 获取信号强度
#ifdef _WIN32
    HANDLE hClient = NULL;
    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    PWLAN_AVAILABLE_NETWORK_LIST pBssList = NULL;

    DWORD dwVersion = 0;
    if (WlanOpenHandle(2, NULL, &dwVersion, &hClient) == ERROR_SUCCESS) {
        if (WlanEnumInterfaces(hClient, NULL, &pIfList) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
                WlanGetAvailableNetworkList(
                    hClient, &pIfList->InterfaceInfo[i].InterfaceGuid, 0, NULL,
                    &pBssList);

                if (pBssList && pBssList->dwNumberOfItems > 0) {
                    stats.signalStrength =
                        pBssList->Network[0].wlanSignalQuality;
                }
            }
        }
        WlanCloseHandle(hClient, NULL);
    }
#elif __linux__
    // 使用iwconfig获取信号强度
    FILE* pipe = popen("iwconfig 2>/dev/null | grep 'Signal level'", "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            if (strstr(buffer, "Signal level") != nullptr) {
                sscanf(buffer, "%*[^=]=%le", &stats.signalStrength);
                break;
            }
        }
        pclose(pipe);
    }
#endif

    LOG_F(INFO,
          "Network stats - Download: {:.2f} MB/s, Upload: {:.2f} MB/s, "
          "Latency: {:.1f} ms, Signal: {:.1f} dBm",
          stats.downloadSpeed, stats.uploadSpeed, stats.latency,
          stats.signalStrength);

    return stats;
}
}  // namespace atom::system
