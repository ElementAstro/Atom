#include "network_manager.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <netioapi.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>
#endif

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"
#include "atom/system/command.hpp"
#include "atom/utils/string.hpp"
#include "atom/utils/to_string.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

class NetworkInterface::NetworkInterfaceImpl {
public:
    std::string name;
    std::vector<std::string> addresses;
    std::string mac;
    bool isUp;

    NetworkInterfaceImpl(std::string name, std::vector<std::string> addresses,
                         std::string mac, bool isUp)
        : name(std::move(name)),
          addresses(std::move(addresses)),
          mac(std::move(mac)),
          isUp(isUp) {}
};

NetworkInterface::NetworkInterface(std::string name,
                                   std::vector<std::string> addresses,
                                   std::string mac, bool isUp)
    : impl_(std::make_shared<NetworkInterfaceImpl>(
          std::move(name), std::move(addresses), std::move(mac), isUp)) {}

auto NetworkInterface::getName() const -> const std::string& {
    return impl_->name;
}

auto NetworkInterface::getAddresses() const -> const std::vector<std::string>& {
    return impl_->addresses;
}

auto NetworkInterface::getAddresses() -> std::vector<std::string>& {
    return impl_->addresses;
}

auto NetworkInterface::getMac() const -> const std::string& {
    return impl_->mac;
}

auto NetworkInterface::isUp() const -> bool { return impl_->isUp; }

class NetworkManager::NetworkManagerImpl {
public:
    std::mutex mtx_;
    std::atomic<bool> running_{true};
#ifdef _WIN32
    WSADATA wsaData_;
#endif
};

NetworkManager::NetworkManager()
    : impl_(std::make_unique<NetworkManagerImpl>()) {
#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &impl_->wsaData_) != 0) {
        THROW_RUNTIME_ERROR("WSAStartup failed");
    }
#endif
}

NetworkManager::~NetworkManager() {
    impl_->running_ = false;
#ifdef _WIN32
    WSACleanup();
#endif
}

auto NetworkManager::getNetworkInterfaces() -> std::vector<NetworkInterface> {
    std::lock_guard lock(impl_->mtx_);
    std::vector<NetworkInterface> interfaces;
    interfaces.reserve(8);

#ifdef _WIN32
    constexpr ULONG initialBufferSize = 15000;
    ULONG outBufLen = initialBufferSize;
    std::vector<BYTE> buffer(outBufLen);
    constexpr ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    auto* pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    constexpr ULONG family = AF_UNSPEC;

    DWORD dwRetVal =
        GetAdaptersAddresses(family, flags, nullptr, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        dwRetVal = GetAdaptersAddresses(family, flags, nullptr, pAddresses,
                                        &outBufLen);
    }

    if (dwRetVal != NO_ERROR) {
        THROW_RUNTIME_ERROR("GetAdaptersAddresses failed with error: " +
                            std::to_string(dwRetVal));
    }

    for (auto* pCurrAddresses = pAddresses; pCurrAddresses != nullptr;
         pCurrAddresses = pCurrAddresses->Next) {
        std::vector<std::string> ips;
        ips.reserve(4);

        for (auto* pUnicast = pCurrAddresses->FirstUnicastAddress;
             pUnicast != nullptr; pUnicast = pUnicast->Next) {
            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            int result = getnameinfo(
                pUnicast->Address.lpSockaddr, pUnicast->Address.iSockaddrLength,
                ipStr.data(), ipStr.size(), nullptr, 0, NI_NUMERICHOST);
            if (result == 0) {
                ips.emplace_back(ipStr.data());
            }
        }

        bool isUp = (pCurrAddresses->OperStatus == IfOperStatusUp);
        auto macAddr =
            getMacAddress(pCurrAddresses->AdapterName).value_or("N/A");
        interfaces.emplace_back(pCurrAddresses->AdapterName, std::move(ips),
                                std::move(macAddr), isUp);
    }
#else
    struct ifaddrs* ifAddrStruct = nullptr;
    if (getifaddrs(&ifAddrStruct) == -1) {
        THROW_RUNTIME_ERROR("getifaddrs failed");
    }

    std::unordered_map<std::string, std::vector<std::string> > interfaceIPs;
    std::unordered_map<std::string, bool> interfaceStatus;

    for (auto* ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            std::string name = ifa->ifa_name;
            std::array<char, INET_ADDRSTRLEN> address{};

            inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr,
                      address.data(), address.size());

            interfaceIPs[name].emplace_back(address.data());
            interfaceStatus[name] = (ifa->ifa_flags & IFF_UP) != 0;
        }
    }

    freeifaddrs(ifAddrStruct);

    interfaces.reserve(interfaceIPs.size());
    for (auto& [name, ips] : interfaceIPs) {
        auto macAddr = getMacAddress(name).value_or("N/A");
        bool isUp = interfaceStatus[name];
        interfaces.emplace_back(std::move(name), std::move(ips),
                                std::move(macAddr), isUp);
    }
#endif

    return interfaces;
}

auto NetworkManager::getMacAddress(const std::string& interfaceName)
    -> std::optional<std::string> {
#ifdef _WIN32
    ULONG outBufLen = sizeof(IP_ADAPTER_ADDRESSES);
    auto pAddresses = std::unique_ptr<BYTE[]>(new BYTE[outBufLen]);
    auto* pAdapterAddresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(pAddresses.get());

    DWORD dwRetVal = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr,
                                          pAdapterAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        pAddresses = std::unique_ptr<BYTE[]>(new BYTE[outBufLen]);
        pAdapterAddresses =
            reinterpret_cast<PIP_ADAPTER_ADDRESSES>(pAddresses.get());
        dwRetVal = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr,
                                        pAdapterAddresses, &outBufLen);
    }

    if (dwRetVal != NO_ERROR) {
        THROW_RUNTIME_ERROR("GetAdaptersAddresses failed with error: " +
                            std::to_string(dwRetVal));
    }

    for (auto* pCurr = pAdapterAddresses; pCurr != nullptr;
         pCurr = pCurr->Next) {
        if (interfaceName == pCurr->AdapterName &&
            pCurr->PhysicalAddressLength > 0) {
            return std::format(
                "{:02X}-{:02X}-{:02X}-{:02X}-{:02X}-{:02X}",
                pCurr->PhysicalAddress[0], pCurr->PhysicalAddress[1],
                pCurr->PhysicalAddress[2], pCurr->PhysicalAddress[3],
                pCurr->PhysicalAddress[4], pCurr->PhysicalAddress[5]);
        }
    }
    return std::nullopt;
#else
    int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd < 0) {
        THROW_RUNTIME_ERROR(
            "Failed to create socket for MAC address retrieval");
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ - 1);

    if (ioctl(socketFd, SIOCGIFHWADDR, &ifr) < 0) {
        close(socketFd);
        return std::nullopt;
    }
    close(socketFd);

    const auto* mac = reinterpret_cast<unsigned char*>(ifr.ifr_hwaddr.sa_data);
    return std::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", mac[0],
                       mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
}

auto NetworkManager::isInterfaceUp(const std::string& interfaceName) -> bool {
    auto interfaces = getNetworkInterfaces();
    auto it = std::find_if(interfaces.begin(), interfaces.end(),
                           [&interfaceName](const auto& iface) {
                               return iface.getName() == interfaceName;
                           });
    return it != interfaces.end() && it->isUp();
}

void NetworkManager::enableInterface(const std::string& interfaceName) {
#ifdef _WIN32
    MIB_IFROW ifRow{};
    strncpy_s(reinterpret_cast<char*>(ifRow.wszName),
              MAX_ADAPTER_NAME_LENGTH + 4, interfaceName.c_str(),
              interfaceName.size());

    if (GetIfEntry(&ifRow) == NO_ERROR) {
        ifRow.dwAdminStatus = MIB_IF_ADMIN_STATUS_UP;
        if (SetIfEntry(&ifRow) != NO_ERROR) {
            THROW_RUNTIME_ERROR("Failed to enable interface: " + interfaceName);
        }
    } else {
        THROW_RUNTIME_ERROR("Failed to get interface entry: " + interfaceName);
    }
#else
    std::string command = "sudo ip link set " + interfaceName + " up";
    if (executeCommandWithStatus(command).second != 0) {
        THROW_RUNTIME_ERROR("Failed to enable interface: " + interfaceName);
    }
#endif
}

void NetworkManager::disableInterface(const std::string& interfaceName) {
#ifdef _WIN32
    MIB_IFROW ifRow{};
    strncpy_s(reinterpret_cast<char*>(ifRow.wszName),
              MAX_ADAPTER_NAME_LENGTH + 4, interfaceName.c_str(),
              interfaceName.size());

    if (GetIfEntry(&ifRow) == NO_ERROR) {
        ifRow.dwAdminStatus = MIB_IF_ADMIN_STATUS_DOWN;
        if (SetIfEntry(&ifRow) != NO_ERROR) {
            THROW_RUNTIME_ERROR("Failed to disable interface: " +
                                interfaceName);
        }
    } else {
        THROW_RUNTIME_ERROR("Failed to get interface entry: " + interfaceName);
    }
#else
    std::string command = "sudo ip link set " + interfaceName + " down";
    if (executeCommandWithStatus(command).second != 0) {
        THROW_RUNTIME_ERROR("Failed to disable interface: " + interfaceName);
    }
#endif
}

auto NetworkManager::resolveDNS(const std::string& hostname) -> std::string {
    struct addrinfo hints{};
    struct addrinfo* res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
    if (ret != 0) {
        THROW_RUNTIME_ERROR("DNS resolution failed for " + hostname + ": " +
                            gai_strerror(ret));
    }

    std::array<char, INET_ADDRSTRLEN> ipStr{};
    inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr,
              ipStr.data(), ipStr.size());
    freeaddrinfo(res);
    return std::string(ipStr.data());
}

auto NetworkManager::getDNSServers() -> std::vector<std::string> {
    std::vector<std::string> dnsServers;
    dnsServers.reserve(4);

#ifdef _WIN32
    DWORD bufLen = 0;
    GetNetworkParams(nullptr, &bufLen);
    auto buffer = std::make_unique<BYTE[]>(bufLen);
    auto* pFixedInfo = reinterpret_cast<FIXED_INFO*>(buffer.get());

    if (GetNetworkParams(pFixedInfo, &bufLen) != NO_ERROR) {
        THROW_RUNTIME_ERROR("GetNetworkParams failed");
    }

    for (auto* pAddr = &pFixedInfo->DnsServerList; pAddr; pAddr = pAddr->Next) {
        dnsServers.emplace_back(pAddr->IpAddress.String);
    }
#else
    std::ifstream resolvFile("/etc/resolv.conf");
    if (!resolvFile.is_open()) {
        THROW_RUNTIME_ERROR("Failed to open /etc/resolv.conf");
    }

    std::string line;
    while (std::getline(resolvFile, line)) {
        if (line.starts_with("nameserver")) {
            std::istringstream iss(line);
            std::string keyword, ip;
            if (iss >> keyword >> ip) {
                dnsServers.emplace_back(std::move(ip));
            }
        }
    }
#endif
    return dnsServers;
}

void NetworkManager::setDNSServers(const std::vector<std::string>& dnsServers) {
#ifdef _WIN32
    ULONG outBufLen = 15000;
    std::vector<BYTE> buffer(outBufLen);
    auto* pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    constexpr ULONG family = AF_UNSPEC;
    constexpr ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    DWORD dwRetVal =
        GetAdaptersAddresses(family, flags, nullptr, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        dwRetVal = GetAdaptersAddresses(family, flags, nullptr, pAddresses,
                                        &outBufLen);
    }

    if (dwRetVal != NO_ERROR) {
        THROW_RUNTIME_ERROR("GetAdaptersAddresses failed with error: " +
                            std::to_string(dwRetVal));
    }

    for (auto* pCurrAddresses = pAddresses; pCurrAddresses != nullptr;
         pCurrAddresses = pCurrAddresses->Next) {
        std::wstring command =
            L"netsh interface ip set dns name=\"" +
            std::wstring(pCurrAddresses->FriendlyName) + L"\" static " +
            (dnsServers.empty()
                 ? L"none"
                 : std::wstring(dnsServers[0].begin(), dnsServers[0].end()));

        if (executeCommandWithStatus(atom::utils::wstringToString(command))
                .second != 0) {
            THROW_RUNTIME_ERROR("Failed to set DNS servers for adapter: " +
                                std::string(pCurrAddresses->AdapterName));
        }

        for (size_t i = 1; i < dnsServers.size(); ++i) {
            std::wstring addCommand =
                L"netsh interface ip add dns name=\"" +
                std::wstring(pCurrAddresses->FriendlyName) + L"\" " +
                std::wstring(dnsServers[i].begin(), dnsServers[i].end()) +
                L" index=" + std::to_wstring(i + 1);
            ATOM_UNUSED_RESULT(executeCommandWithStatus(
                atom::utils::wstringToString(addCommand)));
        }
    }
#else
    if (executeCommandSimple("pgrep NetworkManager > /dev/null") == 0) {
        for (const auto& dns : dnsServers) {
            std::string command = "nmcli device modify eth0 ipv4.dns " + dns;
            if (executeCommandWithStatus(command).second != 0) {
                THROW_RUNTIME_ERROR("Failed to set DNS server: " + dns);
            }
        }
        if (executeCommandSimple("nmcli connection reload") != 0) {
            THROW_RUNTIME_ERROR("Failed to reload NetworkManager connection");
        }
    } else {
        std::ofstream resolvFile("/etc/resolv.conf", std::ios::trunc);
        if (!resolvFile.is_open()) {
            THROW_RUNTIME_ERROR("Failed to open /etc/resolv.conf for writing");
        }

        for (const auto& dns : dnsServers) {
            resolvFile << "nameserver " << dns << "\n";
        }
    }
#endif
}

void NetworkManager::addDNSServer(const std::string& dns) {
    auto dnsServers = getDNSServers();
    if (std::find(dnsServers.begin(), dnsServers.end(), dns) !=
        dnsServers.end()) {
        spdlog::info("DNS server {} already exists", dns);
        return;
    }
    dnsServers.emplace_back(dns);
    setDNSServers(dnsServers);
}

void NetworkManager::removeDNSServer(const std::string& dns) {
    auto dnsServers = getDNSServers();
    auto it = std::remove(dnsServers.begin(), dnsServers.end(), dns);
    if (it == dnsServers.end()) {
        spdlog::info("DNS server {} not found", dns);
        return;
    }
    dnsServers.erase(it, dnsServers.end());
    setDNSServers(dnsServers);
}

void NetworkManager::monitorConnectionStatus() {
    std::thread([this]() {
        constexpr auto sleepDuration = std::chrono::seconds(5);
        while (impl_->running_.load()) {
            std::this_thread::sleep_for(sleepDuration);
            std::lock_guard lock(impl_->mtx_);
            try {
                auto interfaces = getNetworkInterfaces();
                spdlog::info("----- Network Interfaces Status -----");
                for (const auto& iface : interfaces) {
                    spdlog::info(
                        "Interface: {} | Status: {} | IPs: {} | MAC: {}",
                        iface.getName(), iface.isUp() ? "Up" : "Down",
                        atom::utils::toString(iface.getAddresses()),
                        iface.getMac());
                }
                spdlog::info("--------------------------------------");
            } catch (const std::exception& e) {
                spdlog::error("Error in monitorConnectionStatus: {}", e.what());
            }
        }
    }).detach();
}

auto NetworkManager::getInterfaceStatus(const std::string& interfaceName)
    -> std::string {
    auto interfaces = getNetworkInterfaces();
    auto it = std::find_if(interfaces.begin(), interfaces.end(),
                           [&interfaceName](const auto& iface) {
                               return iface.getName() == interfaceName;
                           });
    if (it != interfaces.end()) {
        return it->isUp() ? "Up" : "Down";
    }
    THROW_RUNTIME_ERROR("Interface not found: " + interfaceName);
}

auto parseAddressPort(const std::string& addressPort)
    -> std::pair<std::string, int> {
    size_t colonPos = addressPort.find_last_of(':');
    if (colonPos != std::string::npos) {
        std::string address = addressPort.substr(0, colonPos);
        int port = std::stoi(addressPort.substr(colonPos + 1));
        return {std::move(address), port};
    }
    return {"", 0};
}

auto getNetworkConnections(int pid) -> std::vector<NetworkConnection> {
    std::vector<NetworkConnection> connections;
    connections.reserve(16);

#ifdef _WIN32
    MIB_TCPTABLE_OWNER_PID* pTCPInfo = nullptr;
    DWORD dwSize = 0;
    GetExtendedTcpTable(nullptr, &dwSize, false, AF_INET,
                        TCP_TABLE_OWNER_PID_ALL, 0);

    auto tcpBuffer = std::make_unique<BYTE[]>(dwSize);
    pTCPInfo = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(tcpBuffer.get());

    if (GetExtendedTcpTable(pTCPInfo, &dwSize, false, AF_INET,
                            TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < pTCPInfo->dwNumEntries; ++i) {
            if (static_cast<int>(pTCPInfo->table[i].dwOwningPid) == pid) {
                NetworkConnection conn;
                conn.protocol = "TCP";
                conn.localAddress = inet_ntoa(*reinterpret_cast<in_addr*>(
                    &pTCPInfo->table[i].dwLocalAddr));
                conn.localPort =
                    ntohs(static_cast<u_short>(pTCPInfo->table[i].dwLocalPort));
                conn.remoteAddress = inet_ntoa(*reinterpret_cast<in_addr*>(
                    &pTCPInfo->table[i].dwRemoteAddr));
                conn.remotePort = ntohs(
                    static_cast<u_short>(pTCPInfo->table[i].dwRemotePort));
                connections.emplace_back(std::move(conn));

                spdlog::info(
                    "Found TCP connection: Local {}:{} -> Remote {}:{}",
                    conn.localAddress, conn.localPort, conn.remoteAddress,
                    conn.remotePort);
            }
        }
    } else {
        spdlog::error("Failed to get TCP table. Error: {}", GetLastError());
    }

#elif __APPLE__
    std::array<char, 128> buffer{};
    std::string command = "lsof -i -n -P | grep " + std::to_string(pid);

    auto pipe = std::unique_ptr<FILE, decltype(&pclose)>(
        popen(command.c_str(), "r"), pclose);

    if (!pipe) {
        spdlog::error("Failed to run lsof command");
        return connections;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::istringstream iss(buffer.data());
        std::string proto, local, remote, ignore;
        iss >> ignore >> ignore >> ignore >> proto >> local >> remote;

        auto [localAddr, localPort] = parseAddressPort(local);
        auto [remoteAddr, remotePort] = parseAddressPort(remote);

        connections.emplace_back(
            NetworkConnection{std::move(proto), std::move(localAddr),
                              std::move(remoteAddr), localPort, remotePort});

        spdlog::info("Found {} connection: Local {}:{} -> Remote {}:{}", proto,
                     localAddr, localPort, remoteAddr, remotePort);
    }

#else
    for (const auto& [protocol, path] :
         std::array<std::pair<const char*, const char*>, 2>{
             {{"TCP", "net/tcp"}, {"UDP", "net/udp"}}}) {
        std::ifstream netFile("/proc/" + std::to_string(pid) + "/" + path);
        if (!netFile.is_open()) {
            spdlog::error("Failed to open: /proc/{}/{}", pid, path);
            continue;
        }

        std::string line;
        std::getline(netFile, line);

        while (std::getline(netFile, line)) {
            std::istringstream iss(line);
            std::string localAddress, remoteAddress, ignore;
            int state, inode;

            iss >> ignore >> localAddress >> remoteAddress >> std::hex >>
                state >> ignore >> ignore >> ignore >> inode;

            auto [localAddr, localPort] = parseAddressPort(localAddress);
            auto [remoteAddr, remotePort] = parseAddressPort(remoteAddress);

            connections.emplace_back(NetworkConnection{
                protocol, std::move(localAddr), std::move(remoteAddr),
                localPort, remotePort});

            spdlog::info("Found {} connection: Local {}:{} -> Remote {}:{}",
                         protocol, localAddr, localPort, remoteAddr,
                         remotePort);
        }
    }
#endif

    return connections;
}

}  // namespace atom::system
