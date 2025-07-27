/*
 * windows.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifdef _WIN32

#include "windows.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <thread>


#undef interface

namespace atom::system::windows {

auto isConnectedToInternet_impl() -> bool {
    spdlog::debug("Checking internet connection");

    static constexpr const char* TEST_HOST = "8.8.8.8";
    static constexpr int TEST_PORT = 80;
    static constexpr int CONNECT_TIMEOUT = 5000;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        spdlog::error("Failed to create socket: {}", WSAGetLastError());
        return false;
    }

    DWORD timeout = CONNECT_TIMEOUT;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout),
               sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout),
               sizeof(timeout));

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(TEST_PORT);
    server.sin_addr.s_addr = inet_addr(TEST_HOST);

    bool connected = connect(sock, reinterpret_cast<sockaddr*>(&server),
                             sizeof(server)) != SOCKET_ERROR;
    closesocket(sock);

    spdlog::debug("Internet connection: {}",
                  connected ? "available" : "unavailable");
    return connected;
}

auto getCurrentWifi_impl() -> std::string {
    spdlog::debug("Getting current WiFi connection");

    DWORD negotiatedVersion;
    HANDLE handle;
    DWORD result = WlanOpenHandle(2, nullptr, &negotiatedVersion, &handle);
    if (result != ERROR_SUCCESS) {
        spdlog::error("WlanOpenHandle failed: {}", result);
        return {};
    }

    auto handleDeleter = [](HANDLE h) { WlanCloseHandle(h, nullptr); };
    std::unique_ptr<void, decltype(handleDeleter)> handleGuard(handle,
                                                               handleDeleter);

    WLAN_INTERFACE_INFO_LIST* interfaceList;
    result = WlanEnumInterfaces(handle, nullptr, &interfaceList);
    if (result != ERROR_SUCCESS) {
        spdlog::error("WlanEnumInterfaces failed: {}", result);
        return {};
    }

    std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&WlanFreeMemory)>
        listGuard(interfaceList, WlanFreeMemory);

    for (DWORD i = 0; i < interfaceList->dwNumberOfItems; ++i) {
        const auto& interface = interfaceList->InterfaceInfo[i];
        if (interface.isState != wlan_interface_state_connected)
            continue;

        WLAN_CONNECTION_ATTRIBUTES* connectionAttrs = nullptr;
        DWORD dataSize = 0;
        result = WlanQueryInterface(
            handle, &interface.InterfaceGuid,
            wlan_intf_opcode_current_connection, nullptr, &dataSize,
            reinterpret_cast<PVOID*>(&connectionAttrs), nullptr);

        if (result != ERROR_SUCCESS)
            continue;

        std::unique_ptr<WLAN_CONNECTION_ATTRIBUTES, decltype(&WlanFreeMemory)>
            attrsGuard(connectionAttrs, WlanFreeMemory);

        const auto& ssid = connectionAttrs->wlanAssociationAttributes.dot11Ssid;
        std::string wifiName(reinterpret_cast<const char*>(ssid.ucSSID),
                             ssid.uSSIDLength);

        spdlog::debug("Current WiFi: {}", wifiName);
        return wifiName;
    }

    spdlog::debug("No active WiFi connection found");
    return {};
}

auto getCurrentWiredNetwork_impl() -> std::string {
    spdlog::debug("Getting current wired network connection");

    ULONG bufferLength = 0;
    GetAdaptersInfo(nullptr, &bufferLength);

    if (bufferLength == 0) {
        spdlog::error("GetAdaptersInfo failed to get buffer size");
        return {};
    }

    auto buffer = std::make_unique<char[]>(bufferLength);
    auto adapterInfo = reinterpret_cast<IP_ADAPTER_INFO*>(buffer.get());

    if (GetAdaptersInfo(adapterInfo, &bufferLength) != NO_ERROR) {
        spdlog::error("GetAdaptersInfo failed");
        return {};
    }

    for (auto adapter = adapterInfo; adapter != nullptr;
         adapter = adapter->Next) {
        if (adapter->Type == MIB_IF_TYPE_ETHERNET &&
            adapter->AddressLength > 0 &&
            adapter->IpAddressList.IpAddress.String[0] != '0') {
            std::string networkName = adapter->Description;
            spdlog::debug("Current wired network: {}", networkName);
            return networkName;
        }
    }

    spdlog::debug("No active wired connection found");
    return {};
}

auto isHotspotConnected_impl() -> bool {
    spdlog::debug("Checking if connected to a hotspot");

    DWORD negotiatedVersion;
    HANDLE handle;
    if (WlanOpenHandle(2, nullptr, &negotiatedVersion, &handle) !=
        ERROR_SUCCESS) {
        spdlog::error("WlanOpenHandle failed");
        return false;
    }

    auto handleDeleter = [](HANDLE h) { WlanCloseHandle(h, nullptr); };
    std::unique_ptr<void, decltype(handleDeleter)> handleGuard(handle,
                                                               handleDeleter);

    WLAN_INTERFACE_INFO_LIST* interfaceList;
    if (WlanEnumInterfaces(handle, nullptr, &interfaceList) != ERROR_SUCCESS) {
        spdlog::error("WlanEnumInterfaces failed");
        return false;
    }

    std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&WlanFreeMemory)>
        listGuard(interfaceList, WlanFreeMemory);

    static const std::vector<std::string> hotspotPatterns = {
        "AndroidAP", "iPhone", "Mobile Hotspot", "DIRECT-"};

    for (DWORD i = 0; i < interfaceList->dwNumberOfItems; ++i) {
        const auto& interface = interfaceList->InterfaceInfo[i];
        if (interface.isState != wlan_interface_state_connected)
            continue;

        WLAN_CONNECTION_ATTRIBUTES* connectionAttrs = nullptr;
        DWORD dataSize = 0;
        if (WlanQueryInterface(handle, &interface.InterfaceGuid,
                               wlan_intf_opcode_current_connection, nullptr,
                               &dataSize,
                               reinterpret_cast<PVOID*>(&connectionAttrs),
                               nullptr) != ERROR_SUCCESS)
            continue;

        std::unique_ptr<WLAN_CONNECTION_ATTRIBUTES, decltype(&WlanFreeMemory)>
            attrsGuard(connectionAttrs, WlanFreeMemory);

        if (connectionAttrs->wlanAssociationAttributes.dot11BssType ==
            dot11_BSS_type_independent) {
            spdlog::debug("Hotspot detected: ad-hoc network");
            return true;
        }

        const auto& ssid = connectionAttrs->wlanAssociationAttributes.dot11Ssid;
        std::string ssidStr(reinterpret_cast<const char*>(ssid.ucSSID),
                            ssid.uSSIDLength);

        for (const auto& pattern : hotspotPatterns) {
            if (ssidStr.find(pattern) != std::string::npos) {
                spdlog::debug("Hotspot detected: SSID pattern match ({})",
                              pattern);
                return true;
            }
        }
    }

    spdlog::debug("No hotspot connection detected");
    return false;
}

auto getHostIPs_impl() -> std::vector<std::string> {
    spdlog::debug("Getting host IP addresses");

    std::vector<std::string> hostIPs;

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        spdlog::error("gethostname failed: {}", WSAGetLastError());
        return hostIPs;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res;
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0) {
        spdlog::error("getaddrinfo failed: {}", WSAGetLastError());
        return hostIPs;
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resGuard(res,
                                                                freeaddrinfo);

    for (auto p = res; p != nullptr; p = p->ai_next) {
        char ipstr[INET6_ADDRSTRLEN];
        void* addr;

        if (p->ai_family == AF_INET) {
            addr = &(reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr);
        } else if (p->ai_family == AF_INET6) {
            addr = &(reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_addr);
        } else {
            continue;
        }

        if (inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr))) {
            hostIPs.emplace_back(ipstr);
            spdlog::debug("Found IP address: {}", ipstr);
        }
    }

    return hostIPs;
}

auto getInterfaceNames_impl() -> std::vector<std::string> {
    spdlog::debug("Getting interface names");

    std::vector<std::string> interfaceNames;
    IF_ADDRS allAddrs = nullptr;

    if (atom::system::getAddresses(AF_UNSPEC, &allAddrs) != 0) {
        spdlog::error("getAddresses failed");
        return interfaceNames;
    }

    std::unique_ptr<IP_ADAPTER_ADDRESSES,
                    decltype(&atom::system::freeAddresses)>
        addrsGuard(allAddrs, atom::system::freeAddresses);

    for (auto adapter = allAddrs; adapter != nullptr; adapter = adapter->Next) {
        if (!adapter->FriendlyName)
            continue;

        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName,
                                             -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded <= 0)
            continue;

        std::string interfaceName(sizeNeeded - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1,
                            interfaceName.data(), sizeNeeded, nullptr, nullptr);

        interfaceNames.push_back(std::move(interfaceName));
        spdlog::debug("Found interface: {}", interfaceNames.back());
    }

    return interfaceNames;
}

auto measurePing_impl(const std::string& host, int timeout) -> float {
    spdlog::debug("Measuring ping to host: {}, timeout: {} ms", host, timeout);

    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        spdlog::error("IcmpCreateFile failed: {}", GetLastError());
        return -1.0f;
    }

    std::unique_ptr<void, decltype(&IcmpCloseHandle)> icmpGuard(
        hIcmp, IcmpCloseHandle);

    sockaddr_in dest{};
    dest.sin_family = AF_INET;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    addrinfo* res;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0) {
        spdlog::error("getaddrinfo failed for host: {}", host);
        return -1.0f;
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resGuard(res,
                                                                freeaddrinfo);
    dest.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;

    constexpr int PING_DATA_SIZE = 32;
    std::vector<unsigned char> pingData(PING_DATA_SIZE, 0xA5);

    constexpr int REPLY_BUFFER_SIZE =
        sizeof(ICMP_ECHO_REPLY) + PING_DATA_SIZE + 8;
    std::vector<unsigned char> replyBuffer(REPLY_BUFFER_SIZE);

    DWORD replyCount = IcmpSendEcho(
        hIcmp, dest.sin_addr.S_un.S_addr, pingData.data(), PING_DATA_SIZE,
        nullptr, replyBuffer.data(), REPLY_BUFFER_SIZE, timeout);

    if (replyCount > 0) {
        auto pEchoReply =
            reinterpret_cast<PICMP_ECHO_REPLY>(replyBuffer.data());
        float latency = static_cast<float>(pEchoReply->RoundTripTime);
        spdlog::debug("Ping successful, latency: {:.1f} ms", latency);
        return latency;
    }

    spdlog::error("Ping failed, error code: {}", GetLastError());
    return -1.0f;
}

auto getNetworkStats_impl() -> NetworkStats {
    spdlog::debug("Getting network statistics");

    NetworkStats stats{};

    PDH_HQUERY query;
    if (PdhOpenQuery(nullptr, 0, &query) != ERROR_SUCCESS) {
        spdlog::error("PdhOpenQuery failed");
        return stats;
    }

    std::unique_ptr<PDH_HQUERY, decltype(&PdhCloseQuery)> queryGuard(
        &query, PdhCloseQuery);

    PDH_HCOUNTER bytesRecvCounter, bytesSentCounter;

#ifdef UNICODE
    if (PdhAddCounterW(query, L"\\Network Interface(*)\\Bytes Received/sec", 0,
                       &bytesRecvCounter) != ERROR_SUCCESS ||
        PdhAddCounterW(query, L"\\Network Interface(*)\\Bytes Sent/sec", 0,
                       &bytesSentCounter) != ERROR_SUCCESS) {
#else
    if (PdhAddCounterA(query, "\\Network Interface(*)\\Bytes Received/sec", 0,
                       &bytesRecvCounter) != ERROR_SUCCESS ||
        PdhAddCounterA(query, "\\Network Interface(*)\\Bytes Sent/sec", 0,
                       &bytesSentCounter) != ERROR_SUCCESS) {
#endif
        spdlog::error("PdhAddCounter failed");
        return stats;
    }

    PdhCollectQueryData(query);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    PdhCollectQueryData(query);

    PDH_FMT_COUNTERVALUE recvValue, sentValue;
    if (PdhGetFormattedCounterValue(bytesRecvCounter, PDH_FMT_DOUBLE, nullptr,
                                    &recvValue) == ERROR_SUCCESS &&
        PdhGetFormattedCounterValue(bytesSentCounter, PDH_FMT_DOUBLE, nullptr,
                                    &sentValue) == ERROR_SUCCESS) {
        stats.downloadSpeed = recvValue.doubleValue / (1024.0 * 1024.0);
        stats.uploadSpeed = sentValue.doubleValue / (1024.0 * 1024.0);
    }

    stats.latency = measurePing_impl("8.8.8.8", 1000);

    HANDLE hClient = nullptr;
    DWORD dwVersion = 0;
    if (WlanOpenHandle(2, nullptr, &dwVersion, &hClient) == ERROR_SUCCESS) {
        auto clientDeleter = [](HANDLE h) { WlanCloseHandle(h, nullptr); };
        std::unique_ptr<void, decltype(clientDeleter)> clientGuard(
            hClient, clientDeleter);

        PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
        if (WlanEnumInterfaces(hClient, nullptr, &pIfList) == ERROR_SUCCESS) {
            std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&WlanFreeMemory)>
                ifListGuard(pIfList, WlanFreeMemory);

            for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
                if (pIfList->InterfaceInfo[i].isState !=
                    wlan_interface_state_connected)
                    continue;

                PWLAN_AVAILABLE_NETWORK_LIST pBssList = nullptr;
                if (WlanGetAvailableNetworkList(
                        hClient, &pIfList->InterfaceInfo[i].InterfaceGuid, 0,
                        nullptr, &pBssList) == ERROR_SUCCESS) {
                    std::unique_ptr<WLAN_AVAILABLE_NETWORK_LIST,
                                    decltype(&WlanFreeMemory)>
                        bssGuard(pBssList, WlanFreeMemory);

                    for (DWORD j = 0; j < pBssList->dwNumberOfItems; j++) {
                        if (pBssList->Network[j].dwFlags &
                            WLAN_AVAILABLE_NETWORK_CONNECTED) {
                            stats.signalStrength =
                                -100.0 +
                                pBssList->Network[j].wlanSignalQuality / 2.0;
                            goto signal_found;
                        }
                    }
                }
            }
        }
    }
signal_found:

    stats.packetLoss = 0.0;

    spdlog::debug(
        "Network stats - Download: {:.2f} MB/s, Upload: {:.2f} MB/s, "
        "Latency: {:.1f} ms, Signal: {:.1f} dBm",
        stats.downloadSpeed, stats.uploadSpeed, stats.latency,
        stats.signalStrength);

    return stats;
}

}  // namespace atom::system::windows

#endif  // _WIN32
