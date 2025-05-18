#include "ipv4.hpp"

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
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>

#include "atom/log/loguru.hpp"

namespace atom::web {
constexpr int IPV4_BIT_LENGTH = 32;

// 初始化Windows套接字库
#ifdef _WIN32
namespace {
struct WinsockInitializer {
    WinsockInitializer() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("Failed to initialize Winsock");
        }
    }

    ~WinsockInitializer() { WSACleanup(); }
};
// 全局对象确保WSA初始化
static WinsockInitializer winsockInit;
}  // namespace
#endif

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
            LOG_F(ERROR, "Invalid IPv4 address format: {}", address);
            return false;
        }

        if (inet_pton(AF_INET, std::string(address).c_str(), &ipValue) != 1) {
            LOG_F(ERROR, "IPv4 address parsing failed: {}", address);
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
            LOG_F(ERROR, "Invalid CIDR prefix length: {}", prefixStr);
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
            LOG_F(ERROR, "Invalid CIDR notation: {}", cidr);
            return false;
        }

        auto ipAddr = cidr.substr(0, pos);
        auto prefixLengthOpt = getPrefixLength(cidr);

        if (!prefixLengthOpt.has_value()) {
            LOG_F(ERROR, "Invalid CIDR prefix: {}", cidr);
            return false;
        }

        int prefixLength = prefixLengthOpt.value();

        if (!parse(ipAddr)) {
            LOG_F(ERROR, "Invalid IP address in CIDR: {}", cidr);
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
            throw AddressRangeError("Invalid range: start IP > end IP");
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
        // SIMD implementation would go here
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
            throw std::runtime_error("Failed to convert integer to IP");
        }

        return std::string(buffer);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in integer to IP conversion: {}", e.what());
        return "";
    }
}

}  // namespace atom::web
