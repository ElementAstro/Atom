#include "address.hpp"

#ifdef _WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <immintrin.h>
#include <algorithm>
#include <bitset>
#include <charconv>
#include <cstring>
#include <future>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/log/loguru.hpp"

namespace atom::web {
constexpr int IPV4_BIT_LENGTH = 32;
constexpr int IPV6_SEGMENT_COUNT = 8;
constexpr int IPV6_SEGMENT_BIT_LENGTH = 16;
#ifdef _WIN32
[[maybe_unused]] constexpr int UNIX_DOMAIN_PATH_MAX_LENGTH =
    MAX_PATH;
#else
constexpr int UNIX_DOMAIN_PATH_MAX_LENGTH = 108;
#endif
constexpr uint32_t BYTE_MASK = 0xFF;

// 初始化Windows套接字库
#ifdef _WIN32
namespace {
struct WinsockInitializer {
    WinsockInitializer() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            LOG_F(ERROR, "WSAStartup failed");
            throw std::runtime_error("Failed to initialize Winsock");
        }
    }
    ~WinsockInitializer() { WSACleanup(); }
};
// 全局对象确保WSA初始化
static WinsockInitializer winsockInit;
}  // namespace
#endif

// Factory method for creating Address objects
auto Address::createFromString(std::string_view addressStr)
    -> std::unique_ptr<Address> {
    std::unique_ptr<Address> address;

    try {
        // Try IPv4 first
        address = std::make_unique<IPv4>(addressStr);
        return address;
    } catch (const InvalidAddressFormat&) {
        // Not IPv4, try IPv6
        try {
            address = std::make_unique<IPv6>(addressStr);
            return address;
        } catch (const InvalidAddressFormat&) {
            // Not IPv6, try Unix domain
            try {
                address = std::make_unique<UnixDomain>(addressStr);
                return address;
            } catch (const InvalidAddressFormat&) {
                // Not a recognized address format
                return nullptr;
            }
        }
    }
}

// IPv4 Implementation

auto IPv4::isValidIPv4(std::string_view address) -> bool {
    static const std::regex ipv4Regex(
        "^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
        "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
        "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
        "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");

    return std::regex_match(address.begin(), address.end(), ipv4Regex);
}

IPv4::IPv4(std::string_view address) {
    if (!parse(address)) {
        throw InvalidAddressFormat(std::string(address));
    }
}

auto IPv4::parse(std::string_view address) -> bool {
    try {
        // Validate the format first
        if (!isValidIPv4(address)) {
            LOG_F(ERROR, "Invalid IPv4 address format: {}",
                  std::string(address).c_str());
            return false;
        }

        if (inet_pton(AF_INET, std::string(address).c_str(), &ipValue) != 1) {
            LOG_F(ERROR, "IPv4 address conversion failed: {}",
                  std::string(address).c_str());
            return false;
        }

        addressStr = std::string(address);
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception while parsing IPv4 address: {}", e.what());
        return false;
    }
}

auto IPv4::getPrefixLength(std::string_view cidr) -> std::optional<int> {
    size_t pos = cidr.find('/');
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    auto prefixStr = cidr.substr(pos + 1);
    int prefixLength = 0;

    try {
        // Using from_chars for more efficient and safer conversion
        auto result =
            std::from_chars(prefixStr.data(),
                            prefixStr.data() + prefixStr.size(), prefixLength);

        if (result.ec != std::errc() || prefixLength < 0 ||
            prefixLength > IPV4_BIT_LENGTH) {
            LOG_F(ERROR, "Invalid CIDR prefix length: %.*s",
                  static_cast<int>(prefixStr.size()), prefixStr.data());
            return std::nullopt;
        }

        return prefixLength;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in CIDR prefix parsing: {}", e.what());
        return std::nullopt;
    }
}

auto IPv4::parseCIDR(std::string_view cidr) -> bool {
    try {
        size_t pos = cidr.find('/');
        if (pos == std::string::npos) {
            return parse(cidr);
        }

        auto ipAddr = cidr.substr(0, pos);
        auto prefixLengthOpt = getPrefixLength(cidr);

        if (!prefixLengthOpt.has_value()) {
            return false;
        }

        int prefixLength = prefixLengthOpt.value();

        if (!parse(ipAddr)) {
            return false;
        }

        // Apply network mask based on prefix length
        uint32_t mask =
            (prefixLength == 0) ? 0 : (~0U << (IPV4_BIT_LENGTH - prefixLength));
        ipValue &= htonl(mask);

        addressStr = integerToIp(ipValue) + "/" + std::to_string(prefixLength);
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in CIDR parsing: {}", e.what());
        return false;
    }
}

void IPv4::printAddressType() const { LOG_F(INFO, "Address type: IPv4"); }

auto IPv4::isInRange(std::string_view start, std::string_view end) -> bool {
    try {
        uint32_t startIp = ipToInteger(start);
        uint32_t endIp = ipToInteger(end);
        uint32_t currentIp = ntohl(ipValue);

        if (startIp > endIp) {
            throw AddressRangeError("Start IP is greater than end IP");
        }

        return currentIp >= startIp && currentIp <= endIp;
    } catch (const InvalidAddressFormat& e) {
        LOG_F(ERROR, "Invalid address in range check: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in range check: {}", e.what());
        return false;
    }
}

auto IPv4::toBinary() const -> std::string {
    try {
// Using SIMD for faster binary conversion if supported
#ifdef __AVX2__
// SIMD implementation would go here for large-scale conversions
// For a single uint32_t, the standard approach is still efficient
#endif

        std::bitset<IPV4_BIT_LENGTH> bits(ntohl(ipValue));
        return bits.to_string();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in binary conversion: {}", e.what());
        return "";
    }
}

auto IPv4::toHex() const -> std::string {
    try {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << ntohl(ipValue);
        return ss.str();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in hex conversion: {}", e.what());
        return "";
    }
}

auto IPv4::isEqual(const Address& other) const -> bool {
    try {
        if (other.getType() != "IPv4") {
            return false;
        }

        const IPv4* ipv4Other = dynamic_cast<const IPv4*>(&other);
        if (!ipv4Other) {
            LOG_F(ERROR, "Dynamic cast failed in isEqual");
            return false;
        }

        return ipValue == ipv4Other->ipValue;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in equality check: {}", e.what());
        return false;
    }
}

auto IPv4::getType() const -> std::string_view { return "IPv4"; }

auto IPv4::getNetworkAddress(std::string_view mask) const -> std::string {
    try {
        uint32_t maskValue = ipToInteger(mask);
        uint32_t netAddr = ntohl(ipValue) & maskValue;
        return integerToIp(htonl(netAddr));
    } catch (const InvalidAddressFormat& e) {
        LOG_F(ERROR, "Invalid mask in network address: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in network address calculation: {}", e.what());
        return "";
    }
}

auto IPv4::getBroadcastAddress(std::string_view mask) const -> std::string {
    try {
        uint32_t maskValue = ipToInteger(mask);
        uint32_t broadcastAddr = (ntohl(ipValue) & maskValue) | (~maskValue);
        return integerToIp(htonl(broadcastAddr));
    } catch (const InvalidAddressFormat& e) {
        LOG_F(ERROR, "Invalid mask in broadcast address: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in broadcast address calculation: {}",
              e.what());
        return "";
    }
}

auto IPv4::isSameSubnet(const Address& other, std::string_view mask) const
    -> bool {
    try {
        if (other.getType() != "IPv4") {
            return false;
        }

        const IPv4* ipv4Other = dynamic_cast<const IPv4*>(&other);
        if (!ipv4Other) {
            LOG_F(ERROR, "Dynamic cast failed in isSameSubnet");
            return false;
        }

        uint32_t maskValue = ipToInteger(mask);
        uint32_t netAddr1 = ntohl(ipValue) & maskValue;
        uint32_t netAddr2 = ntohl(ipv4Other->ipValue) & maskValue;
        return netAddr1 == netAddr2;
    } catch (const InvalidAddressFormat& e) {
        LOG_F(ERROR, "Invalid mask in subnet check: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in subnet check: {}", e.what());
        return false;
    }
}

auto IPv4::ipToInteger(std::string_view ipAddr) const -> uint32_t {
    try {
        if (!isValidIPv4(ipAddr)) {
            throw InvalidAddressFormat(std::string(ipAddr));
        }

        uint32_t result = 0;
        if (inet_pton(AF_INET, std::string(ipAddr).c_str(), &result) != 1) {
            throw InvalidAddressFormat(std::string(ipAddr));
        }

        return ntohl(result);
    } catch (const InvalidAddressFormat&) {
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IP to integer conversion: {}", e.what());
        throw InvalidAddressFormat(std::string(ipAddr));
    }
}

auto IPv4::integerToIp(uint32_t ipAddr) const -> std::string {
    try {
        char buffer[INET_ADDRSTRLEN];
        struct in_addr addr{};
        addr.s_addr = ipAddr;

        if (inet_ntop(AF_INET, &addr, buffer, INET_ADDRSTRLEN) == nullptr) {
            LOG_F(ERROR, "Failed to convert integer to IP: %u", ipAddr);
            return "";
        }

        return std::string(buffer);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in integer to IP conversion: {}", e.what());
        return "";
    }
}

// IPv6 Implementation

auto IPv6::isValidIPv6(std::string_view address) -> bool {
    // Basic validation - more comprehensive validation is done by inet_pton
    if (address.empty() || address.length() > 39) {
        return false;
    }

    // Count colons - IPv6 has between 2 and 8 segments (1-7 colons)
    int colonCount = 0;
    for (char c : address) {
        if (c == ':')
            colonCount++;
    }

    return colonCount >= 1 && colonCount <= 7;
}

IPv6::IPv6(std::string_view address) {
    if (!parse(address)) {
        throw InvalidAddressFormat(std::string(address));
    }
}

auto IPv6::parse(std::string_view address) -> bool {
    try {
        std::array<uint8_t, 16> addrBuf{};

        if (inet_pton(AF_INET6, std::string(address).c_str(), addrBuf.data()) !=
            1) {
            LOG_F(ERROR, "Invalid IPv6 address: {}",
                  std::string(address).c_str());
            return false;
        }

        addressStr = std::string(address);

        // Convert to segments
        for (int i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            ipSegments[i] = (addrBuf[i * 2] << 8) | addrBuf[i * 2 + 1];
        }

        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception while parsing IPv6 address: {}", e.what());
        return false;
    }
}

auto IPv6::getPrefixLength(std::string_view cidr) -> std::optional<int> {
    size_t pos = cidr.find('/');
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    auto prefixStr = cidr.substr(pos + 1);
    int prefixLength = 0;

    try {
        auto result =
            std::from_chars(prefixStr.data(),
                            prefixStr.data() + prefixStr.size(), prefixLength);

        if (result.ec != std::errc() || prefixLength < 0 ||
            prefixLength > 128) {
            LOG_F(ERROR, "Invalid IPv6 CIDR prefix length: %.*s",
                  static_cast<int>(prefixStr.size()), prefixStr.data());
            return std::nullopt;
        }

        return prefixLength;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 CIDR prefix parsing: {}", e.what());
        return std::nullopt;
    }
}

auto IPv6::parseCIDR(std::string_view cidr) -> bool {
    try {
        size_t pos = cidr.find('/');
        if (pos == std::string::npos) {
            return parse(cidr);
        }

        auto ipAddr = cidr.substr(0, pos);
        auto prefixLengthOpt = getPrefixLength(cidr);

        if (!prefixLengthOpt.has_value()) {
            return false;
        }

        int prefixLength = prefixLengthOpt.value();

        if (!parse(ipAddr)) {
            return false;
        }

        // Apply network mask based on prefix length
        // This would need to be implemented for IPv6

        addressStr = std::string(ipAddr) + "/" + std::to_string(prefixLength);
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 CIDR parsing: {}", e.what());
        return false;
    }
}

void IPv6::printAddressType() const { LOG_F(INFO, "Address type: IPv6"); }

auto IPv6::isInRange(std::string_view start, std::string_view end) -> bool {
    try {
        auto startIp = ipToArray(start);
        auto endIp = ipToArray(end);

        // Check if the range is valid
        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            if (startIp[i] > endIp[i]) {
                throw AddressRangeError("Start IPv6 is greater than end IPv6");
            }
        }

        // Check if the address is in range
        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            if (ipSegments[i] < startIp[i])
                return false;
            if (ipSegments[i] > endIp[i])
                return false;
            if (ipSegments[i] != startIp[i])
                break;
        }

        return true;
    } catch (const InvalidAddressFormat& e) {
        LOG_F(ERROR, "Invalid address in IPv6 range check: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 range check: {}", e.what());
        return false;
    }
}

auto IPv6::toBinary() const -> std::string {
    try {
        std::string binaryStr;
        binaryStr.reserve(IPV6_SEGMENT_COUNT * IPV6_SEGMENT_BIT_LENGTH);

        // Use multiple threads for larger conversions
        if (std::thread::hardware_concurrency() > 1) {
            std::vector<std::future<std::string>> futures;

            for (int i = 0; i < IPV6_SEGMENT_COUNT; i += 2) {
                futures.push_back(std::async(std::launch::async, [this, i]() {
                    std::string result;
                    result.reserve(IPV6_SEGMENT_BIT_LENGTH * 2);
                    for (int j = 0; j < 2 && i + j < IPV6_SEGMENT_COUNT; ++j) {
                        result += std::bitset<IPV6_SEGMENT_BIT_LENGTH>(
                                      ipSegments[i + j])
                                      .to_string();
                    }
                    return result;
                }));
            }

            for (auto& future : futures) {
                binaryStr += future.get();
            }
        } else {
            // Single-threaded fallback
            for (uint16_t segment : ipSegments) {
                binaryStr +=
                    std::bitset<IPV6_SEGMENT_BIT_LENGTH>(segment).to_string();
            }
        }

        return binaryStr;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 binary conversion: {}", e.what());
        return "";
    }
}

auto IPv6::toHex() const -> std::string {
    try {
        std::stringstream ss;
        for (size_t i = 0; i < ipSegments.size(); ++i) {
            ss << std::hex << std::setfill('0') << std::setw(4)
               << ipSegments[i];
            if (i < ipSegments.size() - 1) {
                ss << ":";
            }
        }
        return ss.str();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 hex conversion: {}", e.what());
        return "";
    }
}

auto IPv6::isEqual(const Address& other) const -> bool {
    try {
        if (other.getType() != "IPv6") {
            return false;
        }

        const IPv6* ipv6Other = dynamic_cast<const IPv6*>(&other);
        if (!ipv6Other) {
            LOG_F(ERROR, "Dynamic cast failed in IPv6 isEqual");
            return false;
        }

        return std::equal(ipSegments.begin(), ipSegments.end(),
                          ipv6Other->ipSegments.begin());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 equality check: {}", e.what());
        return false;
    }
}

auto IPv6::getType() const -> std::string_view { return "IPv6"; }

auto IPv6::getNetworkAddress(std::string_view mask) const -> std::string {
    try {
        // Parse the mask as an IPv6 address
        auto maskSegments = ipToArray(mask);
        std::array<uint16_t, IPV6_SEGMENT_COUNT> networkSegments{};

        // Apply the mask to get the network address
        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            networkSegments[i] = ipSegments[i] & maskSegments[i];
        }

        return arrayToIp(networkSegments);
    } catch (const InvalidAddressFormat& e) {
        LOG_F(ERROR, "Invalid mask in IPv6 network address: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 network address calculation: {}",
              e.what());
        return "";
    }
}

auto IPv6::getBroadcastAddress(std::string_view mask) const -> std::string {
    try {
        // Parse the mask as an IPv6 address
        auto maskSegments = ipToArray(mask);
        std::array<uint16_t, IPV6_SEGMENT_COUNT> broadcastSegments{};

        // Apply the inverse mask to get the broadcast address
        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            broadcastSegments[i] = ipSegments[i] | (~maskSegments[i] & 0xFFFF);
        }

        return arrayToIp(broadcastSegments);
    } catch (const InvalidAddressFormat& e) {
        LOG_F(ERROR, "Invalid mask in IPv6 broadcast address: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 broadcast address calculation: {}",
              e.what());
        return "";
    }
}

auto IPv6::isSameSubnet(const Address& other, std::string_view mask) const
    -> bool {
    try {
        if (other.getType() != "IPv6") {
            return false;
        }

        const IPv6* ipv6Other = dynamic_cast<const IPv6*>(&other);
        if (!ipv6Other) {
            LOG_F(ERROR, "Dynamic cast failed in IPv6 isSameSubnet");
            return false;
        }

        // Parse the mask as an IPv6 address
        auto maskSegments = ipToArray(mask);

        // Compare network addresses
        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            if ((ipSegments[i] & maskSegments[i]) !=
                (ipv6Other->ipSegments[i] & maskSegments[i])) {
                return false;
            }
        }

        return true;
    } catch (const InvalidAddressFormat& e) {
        LOG_F(ERROR, "Invalid mask in IPv6 subnet check: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 subnet check: {}", e.what());
        return false;
    }
}

auto IPv6::ipToArray(std::string_view ipAddr) const -> std::array<uint16_t, 8> {
    try {
        std::array<uint16_t, IPV6_SEGMENT_COUNT> segments{};
        std::array<uint8_t, 16> addrBuf{};

        if (inet_pton(AF_INET6, std::string(ipAddr).c_str(), addrBuf.data()) !=
            1) {
            throw InvalidAddressFormat(std::string(ipAddr));
        }

        for (int i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            segments[i] = (addrBuf[i * 2] << 8) | addrBuf[i * 2 + 1];
        }

        return segments;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 to array conversion: {}", e.what());
        throw InvalidAddressFormat(std::string(ipAddr));
    }
}

auto IPv6::arrayToIp(const std::array<uint16_t, 8>& segments) const
    -> std::string {
    try {
        std::array<uint8_t, 16> addrBuf{};

        for (int i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            addrBuf[i * 2] = segments[i] >> 8;
            addrBuf[i * 2 + 1] = segments[i] & BYTE_MASK;
        }

        char buffer[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, addrBuf.data(), buffer, INET6_ADDRSTRLEN) ==
            nullptr) {
            LOG_F(ERROR, "Failed to convert IPv6 array to string");
            return "";
        }

        return std::string(buffer);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 array to string conversion: {}",
              e.what());
        return "";
    }
}

// UnixDomain Implementation

auto UnixDomain::isValidPath(std::string_view path) -> bool {
#ifdef _WIN32
    // Windows命名管道路径格式检查
    static const std::regex namedPipeRegex(R"(\\\\\.\\pipe\\[^\\/:*?"<>|]+)");
    // 检查是否是Windows命名管道格式
    if (std::regex_match(path.begin(), path.end(), namedPipeRegex)) {
        return true;
    }
    // 普通路径检查
    return !path.empty() && path.length() < MAX_PATH;
#else
    return !path.empty() && path.length() < UNIX_DOMAIN_PATH_MAX_LENGTH;
#endif
}

UnixDomain::UnixDomain(std::string_view path) {
    if (!parse(path)) {
        throw InvalidAddressFormat(std::string(path));
    }
}

auto UnixDomain::parse(std::string_view path) -> bool {
    try {
        if (!isValidPath(path)) {
#ifdef _WIN32
            LOG_F(ERROR, "Invalid Named Pipe or Unix domain socket path: {}",
                  std::string(path).c_str());
#else
            LOG_F(ERROR, "Invalid Unix domain socket path: {}",
                  std::string(path).c_str());
#endif
            return false;
        }

        addressStr = std::string(path);
        return true;
    } catch (const std::exception& e) {
#ifdef _WIN32
        LOG_F(
            ERROR,
            "Exception while parsing Named Pipe or Unix domain socket path: {}",
            e.what());
#else
        LOG_F(ERROR, "Exception while parsing Unix domain socket path: {}",
              e.what());
#endif
        return false;
    }
}

void UnixDomain::printAddressType() const {
#ifdef _WIN32
    LOG_F(INFO, "Address type: Windows Named Pipe or Unix Domain Socket");
#else
    LOG_F(INFO, "Address type: Unix Domain Socket");
#endif
}

auto UnixDomain::isInRange(std::string_view start, std::string_view end)
    -> bool {
    try {
        // For Unix domain sockets, we'll consider lexicographical ordering of
        // paths This is different from IP ranges but provides a consistent
        // behavior

        if (start.empty() || end.empty()) {
            LOG_F(ERROR,
                  "Empty path provided for Unix domain socket range check");
            throw InvalidAddressFormat("Empty path in range check");
        }

        // Check if the range is valid (start <= end lexicographically)
        if (start > end) {
            throw AddressRangeError(
                "Start path is lexicographically greater than end path");
        }

        // Check if the current path is in the lexicographical range
        return addressStr >= std::string(start) &&
               addressStr <= std::string(end);
    } catch (const AddressRangeError& e) {
        LOG_F(ERROR, "Invalid range in Unix domain socket range check: {}",
              e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket range check: {}",
              e.what());
        return false;
    }
}

auto UnixDomain::toBinary() const -> std::string {
    try {
        std::string binaryStr;
        binaryStr.reserve(addressStr.length() * 8);

        for (char c : addressStr) {
            binaryStr += std::bitset<8>(c).to_string();
        }

        return binaryStr;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket binary conversion: {}",
              e.what());
        return "";
    }
}

auto UnixDomain::toHex() const -> std::string {
    try {
        std::stringstream ss;
        for (char c : addressStr) {
            ss << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<int>(static_cast<unsigned char>(c));
        }
        return ss.str();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket hex conversion: {}",
              e.what());
        return "";
    }
}

auto UnixDomain::isEqual(const Address& other) const -> bool {
    try {
        if (other.getType() != "UnixDomain") {
            return false;
        }

        return addressStr == other.getAddress();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket equality check: {}",
              e.what());
        return false;
    }
}

auto UnixDomain::getType() const -> std::string_view { return "UnixDomain"; }

auto UnixDomain::getNetworkAddress([[maybe_unused]] std::string_view mask) const
    -> std::string {
    // Unix域套接字没有网络地址的概念，但我们可以返回路径的目录部分作为"网络"
    try {
        std::string path = addressStr;
        size_t lastSeparator = path.find_last_of("/\\");

        if (lastSeparator != std::string::npos) {
            return path.substr(0, lastSeparator);
        }

#ifdef _WIN32
        LOG_F(WARNING,
              "getNetworkAddress operation not fully applicable for Named "
              "Pipes or Unix domain sockets");
#else
        LOG_F(WARNING,
              "getNetworkAddress operation not fully applicable for Unix "
              "domain sockets");
#endif
        return path;  // 如果没有分隔符，返回整个路径
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket getNetworkAddress: {}",
              e.what());
        return "";
    }
}

auto UnixDomain::getBroadcastAddress(
    [[maybe_unused]] std::string_view mask) const -> std::string {
    // Unix域套接字没有广播地址的概念，但我们可以返回目录下的通配符路径
    try {
        std::string path = addressStr;
        size_t lastSeparator = path.find_last_of("/\\");

        if (lastSeparator != std::string::npos) {
            return path.substr(0, lastSeparator + 1) + "*";
        }

#ifdef _WIN32
        LOG_F(WARNING,
              "getBroadcastAddress operation not fully applicable for Named "
              "Pipes or Unix domain sockets");
#else
        LOG_F(WARNING,
              "getBroadcastAddress operation not fully applicable for Unix "
              "domain sockets");
#endif
        return path + ".*";  // 如果没有分隔符，在当前路径添加通配符
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket getBroadcastAddress: {}",
              e.what());
        return "";
    }
}

auto UnixDomain::isSameSubnet(const Address& other,
                              [[maybe_unused]] std::string_view mask) const
    -> bool {
    // 对于Unix域套接字，我们可以检查两个路径是否在同一目录
    try {
        if (other.getType() != "UnixDomain") {
            return false;
        }

        std::string thisPath = addressStr;
        std::string otherPath = std::string(other.getAddress());

        // 获取各自的目录部分
        size_t thisLastSeparator = thisPath.find_last_of("/\\");
        size_t otherLastSeparator = otherPath.find_last_of("/\\");

        // 如果两者都有分隔符，比较目录部分
        if (thisLastSeparator != std::string::npos &&
            otherLastSeparator != std::string::npos) {
            std::string thisDir = thisPath.substr(0, thisLastSeparator);
            std::string otherDir = otherPath.substr(0, otherLastSeparator);
            return thisDir == otherDir;
        }

        // 如果两者都没有分隔符，就看是否在同一工作目录
        if (thisLastSeparator == std::string::npos &&
            otherLastSeparator == std::string::npos) {
            return true;
        }

#ifdef _WIN32
        LOG_F(WARNING,
              "isSameSubnet operation not fully applicable for Named Pipes or "
              "Unix domain sockets");
#else
        LOG_F(WARNING,
              "isSameSubnet operation not fully applicable for Unix domain "
              "sockets");
#endif
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket isSameSubnet: {}",
              e.what());
        return false;
    }
}

}  // namespace atom::web