/*
 * windows.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Windows WiFi Implementation

**************************************************/

#ifdef _WIN32

#include "windows.hpp"

#include <thread>

namespace atom::system::windows {

auto isConnectedToInternet_impl() -> bool {
    LOG_F(INFO, "Checking internet connection");
    bool connected = false;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != -1) {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
        server.sin_addr.s_addr = inet_addr("8.8.8.8");
        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) != -1) {
            connected = true;
            LOG_F(INFO, "Connected to internet");
        } else {
            LOG_F(ERROR, "Failed to connect to internet");
        }
        closesocket(sock);
    } else {
        LOG_F(ERROR, "Failed to create socket");
    }
    return connected;
}

auto getCurrentWifi_impl() -> std::string {
    LOG_F(INFO, "Getting current WiFi connection");
    std::string wifiName;

    DWORD negotiatedVersion;
    HANDLE handle;
    if (WlanOpenHandle(2, nullptr, &negotiatedVersion, &handle) ==
        ERROR_SUCCESS) {
        WLAN_INTERFACE_INFO_LIST* interfaceInfoList;
        if (WlanEnumInterfaces(handle, nullptr, &interfaceInfoList) ==
            ERROR_SUCCESS) {
            for (DWORD i = 0; i < interfaceInfoList->dwNumberOfItems; ++i) {
                WLAN_INTERFACE_INFO* interfaceInfo = &interfaceInfoList->InterfaceInfo[i];
                if (interfaceInfo->isState == wlan_interface_state_connected) {
                    WLAN_CONNECTION_ATTRIBUTES* connectionAttributes = nullptr;
                    DWORD dataSize = 0;
                    if (WlanQueryInterface(handle, &interfaceInfo->InterfaceGuid,
                                          wlan_intf_opcode_current_connection,
                                          nullptr, &dataSize,
                                          (PVOID*)&connectionAttributes,
                                          nullptr) == ERROR_SUCCESS) {
                        wchar_t ssid[33] = {0};
                        memcpy(ssid, connectionAttributes->wlanAssociationAttributes.dot11Ssid.ucSSID, 
                               connectionAttributes->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
                        
                        // Convert wide string to narrow string
                        int size_needed = WideCharToMultiByte(CP_UTF8, 0, ssid, -1, NULL, 0, NULL, NULL);
                        std::vector<char> buffer(size_needed);
                        WideCharToMultiByte(CP_UTF8, 0, ssid, -1, buffer.data(), size_needed, NULL, NULL);
                        wifiName = buffer.data();
                        
                        WlanFreeMemory(connectionAttributes);
                        break;
                    }
                }
            }
            WlanFreeMemory(interfaceInfoList);
        } else {
            LOG_F(ERROR, "WlanEnumInterfaces failed");
        }
        WlanCloseHandle(handle, nullptr);
    } else {
        LOG_F(ERROR, "WlanOpenHandle failed");
    }
    
    LOG_F(INFO, "Current WiFi: {}", wifiName);
    return wifiName;
}

auto getCurrentWiredNetwork_impl() -> std::string {
    LOG_F(INFO, "Getting current wired network connection");
    std::string wiredNetworkName;

    PIP_ADAPTER_INFO adapterInfo = nullptr;
    ULONG bufferLength = 0;

    if (GetAdaptersInfo(adapterInfo, &bufferLength) == ERROR_BUFFER_OVERFLOW) {
        adapterInfo =
            reinterpret_cast<IP_ADAPTER_INFO*>(new char[bufferLength]);
        if (GetAdaptersInfo(adapterInfo, &bufferLength) == NO_ERROR) {
            for (PIP_ADAPTER_INFO adapter = adapterInfo; adapter != nullptr;
                 adapter = adapter->Next) {
                // Check if it's an Ethernet adapter
                if (adapter->Type == MIB_IF_TYPE_ETHERNET && 
                    adapter->AddressLength > 0 && 
                    adapter->IpAddressList.IpAddress.String[0] != '0') {
                    wiredNetworkName = adapter->Description;
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
    
    LOG_F(INFO, "Current wired network: {}", wiredNetworkName);
    return wiredNetworkName;
}

auto isHotspotConnected_impl() -> bool {
    LOG_F(INFO, "Checking if connected to a hotspot");
    bool isConnected = false;

    DWORD negotiatedVersion;
    HANDLE handle;
    if (WlanOpenHandle(2, nullptr, &negotiatedVersion, &handle) ==
        ERROR_SUCCESS) {
        WLAN_INTERFACE_INFO_LIST* interfaceInfoList;
        if (WlanEnumInterfaces(handle, nullptr, &interfaceInfoList) ==
            ERROR_SUCCESS) {
            for (DWORD i = 0; i < interfaceInfoList->dwNumberOfItems; ++i) {
                WLAN_INTERFACE_INFO* interfaceInfo = &interfaceInfoList->InterfaceInfo[i];
                
                // Typically, a hotspot connection has a specific SSID pattern or network type
                if (interfaceInfo->isState == wlan_interface_state_connected) {
                    WLAN_CONNECTION_ATTRIBUTES* connectionAttributes = nullptr;
                    DWORD dataSize = 0;
                    if (WlanQueryInterface(handle, &interfaceInfo->InterfaceGuid,
                                          wlan_intf_opcode_current_connection,
                                          nullptr, &dataSize,
                                          (PVOID*)&connectionAttributes,
                                          nullptr) == ERROR_SUCCESS) {
                        // Check if this is an ad-hoc network (often indicates a hotspot)
                        if (connectionAttributes->wlanAssociationAttributes.dot11BssType == dot11_BSS_type_independent) {
                            isConnected = true;
                        }
                        
                        // Additional check - some hotspots have specific SSID patterns
                        wchar_t ssid[33] = {0};
                        memcpy(ssid, connectionAttributes->wlanAssociationAttributes.dot11Ssid.ucSSID,
                               connectionAttributes->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
                        
                        std::wstring wssid(ssid);
                        if (wssid.find(L"AndroidAP") != std::wstring::npos || 
                            wssid.find(L"iPhone") != std::wstring::npos || 
                            wssid.find(L"Mobile Hotspot") != std::wstring::npos) {
                            isConnected = true;
                        }
                        
                        WlanFreeMemory(connectionAttributes);
                        if (isConnected) break;
                    }
                }
            }
            WlanFreeMemory(interfaceInfoList);
        } else {
            LOG_F(ERROR, "WlanEnumInterfaces failed");
        }
        WlanCloseHandle(handle, nullptr);
    } else {
        LOG_F(ERROR, "WlanOpenHandle failed");
    }
    
    LOG_F(INFO, "Hotspot connected: {}", isConnected ? "yes" : "no");
    return isConnected;
}

auto getHostIPs_impl() -> std::vector<std::string> {
    LOG_F(INFO, "Getting host IP addresses");
    std::vector<std::string> hostIPs;

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

    if (allAddrs != nullptr) {
        atom::system::freeAddresses(allAddrs);
    }
    return interfaceNames;
}

auto measurePing_impl(const std::string& host, int timeout) -> float {
    LOG_F(INFO, "Measuring ping to host: {}, timeout: {} ms", host, timeout);
    float latency = -1.0f;

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

    return latency;
}

auto getNetworkStats_impl() -> NetworkStats {
    LOG_F(INFO, "Getting network statistics");
    NetworkStats stats{};

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

    // 测量网络延迟
    std::string host = "8.8.8.8";
    int timeout = 1000;
    stats.latency = measurePing_impl(host, timeout);

    // 获取信号强度
    HANDLE hClient = NULL;
    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    PWLAN_AVAILABLE_NETWORK_LIST pBssList = NULL;

    DWORD dwVersion = 0;
    if (WlanOpenHandle(2, NULL, &dwVersion, &hClient) == ERROR_SUCCESS) {
        if (WlanEnumInterfaces(hClient, NULL, &pIfList) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
                WLAN_INTERFACE_INFO* interfaceInfo = &pIfList->InterfaceInfo[i];
                if (interfaceInfo->isState == wlan_interface_state_connected) {
                    if (WlanGetAvailableNetworkList(hClient, &interfaceInfo->InterfaceGuid,
                                                  0, NULL, &pBssList) == ERROR_SUCCESS) {
                        for (DWORD j = 0; j < pBssList->dwNumberOfItems; j++) {
                            WLAN_AVAILABLE_NETWORK* network = &pBssList->Network[j];
                            if (network->dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) {
                                // RSSI is in dBm units
                                stats.signalStrength = network->wlanSignalQuality;
                                // Convert percentage to dBm (approximate conversion)
                                stats.signalStrength = -100.0 + stats.signalStrength / 2.0;
                                break;
                            }
                        }
                        WlanFreeMemory(pBssList);
                    }
                }
            }
            WlanFreeMemory(pIfList);
        }
        WlanCloseHandle(hClient, NULL);
    }

    // We'll leave packet loss calculation as a placeholder
    stats.packetLoss = 0.0; // Would need multiple pings to calculate

    LOG_F(INFO,
          "Network stats - Download: {:.2f} MB/s, Upload: {:.2f} MB/s, "
          "Latency: {:.1f} ms, Signal: {:.1f} dBm",
          stats.downloadSpeed, stats.uploadSpeed, stats.latency,
          stats.signalStrength);

    return stats;
}

} // namespace atom::system::windows

#endif // _WIN32
