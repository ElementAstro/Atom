/*
 * common.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Common WiFi Implementations

**************************************************/

#include "common.hpp"
#include <algorithm>
#include <regex>
#include <thread>
#include <future>

#ifdef _WIN32
#include <ws2tcpip.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/wait.h>
#include <net/if.h>
#include <errno.h>
#endif

namespace atom::system {

void freeAddresses(IF_ADDRS addrs) {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    if (addrs) {
        HeapFree(GetProcessHeap(), 0, addrs);
    }
#else
    if (addrs) {
        freeifaddrs(addrs);
    }
#endif
}

auto getAddresses(int family, IF_ADDRS* addrs) -> int {
    if (!addrs) {
        LOG_F(ERROR, "getAddresses: Invalid addrs parameter");
        return -1;
    }

#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    DWORD rv = 0;
    ULONG bufLen = 15000;  // recommended size from Windows API docs
    ULONG iter = 0;

    do {
        *addrs = static_cast<IP_ADAPTER_ADDRESSES*>(
            HeapAlloc(GetProcessHeap(), 0, bufLen));
        if (*addrs == nullptr) {
            LOG_F(ERROR, "HeapAlloc failed while allocating memory for adapter addresses");
            return -1;
        }

        rv = GetAdaptersAddresses(family, GAA_FLAG_INCLUDE_PREFIX, nullptr,
                                  *addrs, &bufLen);
        if (rv == ERROR_BUFFER_OVERFLOW) {
            freeAddresses(*addrs);
            *addrs = nullptr;
            bufLen *= 2;  // Double buffer length for the next attempt
        } else {
            break;
        }
        iter++;
    } while ((rv == ERROR_BUFFER_OVERFLOW) && (iter < 3));

    if (rv != NO_ERROR) {
        LOG_F(ERROR, "GetAdaptersAddresses failed with error code: {}", rv);
        if (*addrs) {
            freeAddresses(*addrs);
            *addrs = nullptr;
        }
        return -1;
    }
    return 0;
#else
    int result = getifaddrs(addrs);
    if (result != 0) {
        LOG_F(ERROR, "getifaddrs failed with error: {}", strerror(errno));
        return -1;
    }
    return 0;
#endif
}

auto measurePing(const std::string& host, int timeout_ms) -> std::pair<float, WiFiError> {
    if (host.empty()) {
        LOG_F(ERROR, "measurePing: Empty host parameter");
        return {-1.0f, WiFiError::INVALID_PARAMETER};
    }

    if (timeout_ms <= 0 || timeout_ms > 60000) {
        LOG_F(ERROR, "measurePing: Invalid timeout value: {}", timeout_ms);
        return {-1.0f, WiFiError::INVALID_PARAMETER};
    }

    LOG_F(INFO, "Measuring ping to host: {}, timeout: {} ms", host, timeout_ms);

#ifdef _WIN32
    HANDLE hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "IcmpCreateFile failed");
        return {-1.0f, WiFiError::SYSTEM_CALL_FAILED};
    }

    char sendData[32] = "Hello World!";
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData);
    auto replyBuffer = std::make_unique<char[]>(replySize);

    auto start = std::chrono::high_resolution_clock::now();
    DWORD dwRetVal = IcmpSendEcho(hIcmpFile, inet_addr(host.c_str()),
                                  sendData, sizeof(sendData), nullptr,
                                  replyBuffer.get(), replySize, timeout_ms);
    auto end = std::chrono::high_resolution_clock::now();

    IcmpCloseHandle(hIcmpFile);

    if (dwRetVal != 0) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        float latency = duration.count() / 1000.0f;
        LOG_F(INFO, "Ping successful, latency: {:.2f} ms", latency);
        return {latency, WiFiError::SUCCESS};
    } else {
        LOG_F(ERROR, "Ping failed for host: {}", host);
        return {-1.0f, WiFiError::NETWORK_UNREACHABLE};
    }
#else
    // Use system ping command for Unix-like systems
    std::string command = "ping -c 1 -W " + std::to_string(timeout_ms / 1000) + " " + host + " 2>/dev/null";
    auto [output, error] = executeCommand(command, timeout_ms + 1000);

    if (error != WiFiError::SUCCESS) {
        return {-1.0f, error};
    }

    // Parse ping output to extract latency
    std::regex timeRegex(R"(time=([0-9.]+)\s*ms)");
    std::smatch match;
    if (std::regex_search(output, match, timeRegex)) {
        float latency = std::stof(match[1].str());
        LOG_F(INFO, "Ping successful, latency: {:.2f} ms", latency);
        return {latency, WiFiError::SUCCESS};
    }

    LOG_F(ERROR, "Failed to parse ping output for host: {}", host);
    return {-1.0f, WiFiError::NETWORK_UNREACHABLE};
#endif
}

auto isValidIPAddress(const std::string& ip) -> bool {
    if (ip.empty()) {
        return false;
    }

    // Check IPv4
    struct sockaddr_in sa4;
    if (inet_pton(AF_INET, ip.c_str(), &(sa4.sin_addr)) == 1) {
        return true;
    }

    // Check IPv6
    struct sockaddr_in6 sa6;
    if (inet_pton(AF_INET6, ip.c_str(), &(sa6.sin6_addr)) == 1) {
        return true;
    }

    return false;
}

auto getNetworkInterfaces() -> std::vector<NetworkInterface> {
    std::vector<NetworkInterface> interfaces;
    IF_ADDRS addrs = nullptr;

    if (getAddresses(AF_UNSPEC, &addrs) != 0) {
        LOG_F(ERROR, "Failed to get network addresses");
        return interfaces;
    }

    std::unique_ptr<std::remove_pointer_t<IF_ADDRS>, decltype(&freeAddresses)>
        addrsGuard(addrs, freeAddresses);

#ifdef _WIN32
    for (auto adapter = addrs; adapter != nullptr; adapter = adapter->Next) {
        NetworkInterface iface;
        iface.name = adapter->AdapterName;
        iface.description = adapter->Description;
        iface.is_up = (adapter->OperStatus == IfOperStatusUp);
        iface.is_wireless = (adapter->IfType == IF_TYPE_IEEE80211);

        // Get IP addresses
        for (auto ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            char ipStr[INET6_ADDRSTRLEN] = {0};
            void* addr = nullptr;

            if (ua->Address.lpSockaddr->sa_family == AF_INET) {
                auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(ua->Address.lpSockaddr);
                addr = &(ipv4->sin_addr);
                if (inet_ntop(AF_INET, addr, ipStr, sizeof(ipStr))) {
                    iface.ipv4_addresses.emplace_back(ipStr);
                }
            } else if (ua->Address.lpSockaddr->sa_family == AF_INET6) {
                auto* ipv6 = reinterpret_cast<struct sockaddr_in6*>(ua->Address.lpSockaddr);
                addr = &(ipv6->sin6_addr);
                if (inet_ntop(AF_INET6, addr, ipStr, sizeof(ipStr))) {
                    iface.ipv6_addresses.emplace_back(ipStr);
                }
            }
        }

        interfaces.push_back(std::move(iface));
    }
#else
    std::unordered_map<std::string, NetworkInterface> ifaceMap;

    for (auto* ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        std::string name = ifa->ifa_name;
        auto& iface = ifaceMap[name];

        if (iface.name.empty()) {
            iface.name = name;
            iface.description = name;
            iface.is_up = (ifa->ifa_flags & IFF_UP) != 0;
            iface.is_wireless = name.find("wl") == 0 || name.find("wifi") == 0;
        }

        char ipStr[INET6_ADDRSTRLEN] = {0};
        void* addr = nullptr;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            addr = &(ipv4->sin_addr);
            if (inet_ntop(AF_INET, addr, ipStr, sizeof(ipStr))) {
                iface.ipv4_addresses.emplace_back(ipStr);
            }
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            auto* ipv6 = reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
            addr = &(ipv6->sin6_addr);
            if (inet_ntop(AF_INET6, addr, ipStr, sizeof(ipStr))) {
                iface.ipv6_addresses.emplace_back(ipStr);
            }
        }
    }

    for (auto& [name, iface] : ifaceMap) {
        interfaces.push_back(std::move(iface));
    }
#endif

    LOG_F(INFO, "Found {} network interfaces", interfaces.size());
    return interfaces;
}

auto wifiErrorToString(WiFiError error) -> std::string {
    switch (error) {
        case WiFiError::SUCCESS:
            return "Success";
        case WiFiError::PLATFORM_NOT_SUPPORTED:
            return "Platform not supported";
        case WiFiError::NETWORK_INTERFACE_ERROR:
            return "Network interface error";
        case WiFiError::PERMISSION_DENIED:
            return "Permission denied";
        case WiFiError::TIMEOUT:
            return "Operation timeout";
        case WiFiError::INVALID_PARAMETER:
            return "Invalid parameter";
        case WiFiError::MEMORY_ALLOCATION_FAILED:
            return "Memory allocation failed";
        case WiFiError::SYSTEM_CALL_FAILED:
            return "System call failed";
        case WiFiError::NETWORK_UNREACHABLE:
            return "Network unreachable";
        case WiFiError::CACHE_MISS:
            return "Cache miss";
        default:
            return "Unknown error";
    }
}

auto executeCommand(const std::string& command, int timeout_ms) -> std::pair<std::string, WiFiError> {
    if (command.empty()) {
        LOG_F(ERROR, "executeCommand: Empty command");
        return {"", WiFiError::INVALID_PARAMETER};
    }

    LOG_F(INFO, "Executing command: {}", command);

#ifdef _WIN32
    HANDLE hChildStdoutRd, hChildStdoutWr;
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
        LOG_F(ERROR, "CreatePipe failed");
        return {"", WiFiError::SYSTEM_CALL_FAILED};
    }

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hChildStdoutWr;
    si.hStdError = hChildStdoutWr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmdLine = "cmd.exe /C " + command;
    if (!CreateProcess(nullptr, const_cast<char*>(cmdLine.c_str()), nullptr, nullptr,
                       TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hChildStdoutRd);
        CloseHandle(hChildStdoutWr);
        LOG_F(ERROR, "CreateProcess failed");
        return {"", WiFiError::SYSTEM_CALL_FAILED};
    }

    CloseHandle(hChildStdoutWr);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeout_ms);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hChildStdoutRd);
        LOG_F(ERROR, "Command execution timeout");
        return {"", WiFiError::TIMEOUT};
    }

    std::string output;
    DWORD bytesRead;
    char buffer[4096];
    while (ReadFile(hChildStdoutRd, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdoutRd);

    return {output, WiFiError::SUCCESS};
#else
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        LOG_F(ERROR, "popen failed for command: {}", command);
        return {"", WiFiError::SYSTEM_CALL_FAILED};
    }

    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    int status = pclose(pipe);
    if (status == -1) {
        LOG_F(ERROR, "pclose failed");
        return {output, WiFiError::SYSTEM_CALL_FAILED};
    }

    return {output, WiFiError::SUCCESS};
#endif
}

auto measureNetworkSpeed(const std::string& test_host) -> SpeedTestResult {
    SpeedTestResult result{0.0, 0.0, -1.0, WiFiError::SUCCESS};

    if (test_host.empty()) {
        LOG_F(ERROR, "measureNetworkSpeed: Empty test host");
        result.error = WiFiError::INVALID_PARAMETER;
        return result;
    }

    LOG_F(INFO, "Measuring network speed to host: {}", test_host);

    // Measure latency first
    auto [latency, ping_error] = measurePing(test_host, 5000);
    if (ping_error == WiFiError::SUCCESS) {
        result.latency_ms = latency;
    } else {
        LOG_F(WARNING, "Failed to measure latency: {}", wifiErrorToString(ping_error));
    }

    // For now, return basic results. In a real implementation, you would:
    // 1. Download a test file to measure download speed
    // 2. Upload data to measure upload speed
    // 3. Use multiple connections for more accurate results

    // Placeholder implementation - would need actual speed test logic
    result.download_mbps = 0.0;
    result.upload_mbps = 0.0;

    LOG_F(INFO, "Network speed measurement completed - Latency: {:.2f}ms", result.latency_ms);
    return result;
}

// Template specializations for IP address retrieval
template<>
auto getIPAddresses<sockaddr_in>(int addressFamily) -> std::pair<std::vector<std::string>, WiFiError> {
    std::vector<std::string> addresses;
    IF_ADDRS addrs = nullptr;

    if (getAddresses(addressFamily, &addrs) != 0) {
        return {addresses, WiFiError::NETWORK_INTERFACE_ERROR};
    }

    std::unique_ptr<std::remove_pointer_t<IF_ADDRS>, decltype(&freeAddresses)>
        addrsGuard(addrs, freeAddresses);

#ifdef _WIN32
    for (auto adapter = addrs; adapter != nullptr; adapter = adapter->Next) {
        for (auto ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family == addressFamily) {
                char ipStr[INET_ADDRSTRLEN] = {0};
                auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(ua->Address.lpSockaddr);
                if (inet_ntop(addressFamily, &(ipv4->sin_addr), ipStr, sizeof(ipStr))) {
                    addresses.emplace_back(ipStr);
                }
            }
        }
    }
#else
    for (auto* ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == addressFamily) {
            char ipStr[INET_ADDRSTRLEN] = {0};
            auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            if (inet_ntop(addressFamily, &(ipv4->sin_addr), ipStr, sizeof(ipStr))) {
                std::string ip = ipStr;
                if (ip != "127.0.0.1") {  // Skip loopback
                    addresses.emplace_back(std::move(ip));
                }
            }
        }
    }
#endif

    return {addresses, WiFiError::SUCCESS};
}

template<>
auto getIPAddresses<sockaddr_in6>(int addressFamily) -> std::pair<std::vector<std::string>, WiFiError> {
    std::vector<std::string> addresses;
    IF_ADDRS addrs = nullptr;

    if (getAddresses(addressFamily, &addrs) != 0) {
        return {addresses, WiFiError::NETWORK_INTERFACE_ERROR};
    }

    std::unique_ptr<std::remove_pointer_t<IF_ADDRS>, decltype(&freeAddresses)>
        addrsGuard(addrs, freeAddresses);

#ifdef _WIN32
    for (auto adapter = addrs; adapter != nullptr; adapter = adapter->Next) {
        for (auto ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family == addressFamily) {
                char ipStr[INET6_ADDRSTRLEN] = {0};
                auto* ipv6 = reinterpret_cast<struct sockaddr_in6*>(ua->Address.lpSockaddr);
                if (inet_ntop(addressFamily, &(ipv6->sin6_addr), ipStr, sizeof(ipStr))) {
                    addresses.emplace_back(ipStr);
                }
            }
        }
    }
#else
    for (auto* ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == addressFamily) {
            char ipStr[INET6_ADDRSTRLEN] = {0};
            auto* ipv6 = reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
            if (inet_ntop(addressFamily, &(ipv6->sin6_addr), ipStr, sizeof(ipStr))) {
                std::string ip = ipStr;
                if (ip != "::1") {  // Skip loopback
                    addresses.emplace_back(std::move(ip));
                }
            }
        }
    }
#endif

    return {addresses, WiFiError::SUCCESS};
}

} // namespace atom::system
