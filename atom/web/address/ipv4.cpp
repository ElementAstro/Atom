#include "ipv4.hpp"

#ifdef _WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <array>
#include <bitset>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>


#include <spdlog/spdlog.h>

namespace atom::web {

namespace {
constexpr int IPV4_BIT_LENGTH = 32;
constexpr size_t IPV4_OCTET_COUNT = 4;
constexpr uint8_t IPV4_MAX_OCTET = 255;

#ifdef _WIN32
struct WinsockInitializer {
    WinsockInitializer() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("Failed to initialize Winsock");
        }
    }

    ~WinsockInitializer() { WSACleanup(); }
};

static WinsockInitializer winsockInit;
#endif

/**
 * @brief Fast IPv4 validation without regex
 */
auto fastIsValidIPv4(std::string_view address) -> bool {
    if (address.empty() || address.length() > 15) {
        return false;
    }

    std::array<uint32_t, IPV4_OCTET_COUNT> octets{};
    size_t octetIndex = 0;
    size_t start = 0;

    for (size_t i = 0; i <= address.length(); ++i) {
        if (i == address.length() || address[i] == '.') {
            if (octetIndex >= IPV4_OCTET_COUNT || start == i) {
                return false;
            }

            auto octetStr = address.substr(start, i - start);
            if (octetStr.empty() || octetStr.length() > 3) {
                return false;
            }

            if (octetStr.length() > 1 && octetStr[0] == '0') {
                return false;
            }

            uint32_t value = 0;
            auto result = std::from_chars(
                octetStr.data(), octetStr.data() + octetStr.size(), value);

            if (result.ec != std::errc{} || value > IPV4_MAX_OCTET) {
                return false;
            }

            octets[octetIndex++] = value;
            start = i + 1;
        } else if (!std::isdigit(address[i])) {
            return false;
        }
    }

    return octetIndex == IPV4_OCTET_COUNT;
}
}  // namespace

auto IPv4::isValidIPv4(std::string_view address) -> bool {
    return fastIsValidIPv4(address);
}

IPv4::IPv4(std::string_view address) {
    if (!parse(address)) {
        throw InvalidAddressFormat(std::string(address));
    }
}

auto IPv4::parse(std::string_view address) -> bool {
    try {
        if (!isValidIPv4(address)) {
            spdlog::error("Invalid IPv4 address format: {}", address);
            return false;
        }

        std::string addressStr(address);
        uint32_t tempValue = 0;

        if (inet_pton(AF_INET, addressStr.c_str(), &tempValue) != 1) {
            spdlog::error("IPv4 address parsing failed: {}", address);
            return false;
        }

        ipValue = tempValue;
        this->addressStr = std::move(addressStr);
        spdlog::trace("Successfully parsed IPv4 address: {}", address);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception while parsing IPv4 address '{}': {}", address,
                      e.what());
        return false;
    }
}

auto IPv4::getPrefixLength(std::string_view cidr) -> std::optional<int> {
    size_t pos = cidr.find('/');
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }

    auto prefixStr = cidr.substr(pos + 1);
    if (prefixStr.empty()) {
        return std::nullopt;
    }

    int prefixLength = 0;
    auto result = std::from_chars(
        prefixStr.data(), prefixStr.data() + prefixStr.size(), prefixLength);

    if (result.ec != std::errc{} || prefixLength < 0 ||
        prefixLength > IPV4_BIT_LENGTH) {
        spdlog::error("Invalid CIDR prefix length: {}", prefixStr);
        return std::nullopt;
    }

    return prefixLength;
}

auto IPv4::parseCIDR(std::string_view cidr) -> bool {
    try {
        size_t pos = cidr.find('/');
        if (pos == std::string_view::npos) {
            spdlog::error("Invalid CIDR notation: {}", cidr);
            return false;
        }

        auto ipAddr = cidr.substr(0, pos);
        auto prefixLengthOpt = getPrefixLength(cidr);

        if (!prefixLengthOpt.has_value()) {
            spdlog::error("Invalid CIDR prefix: {}", cidr);
            return false;
        }

        int prefixLength = prefixLengthOpt.value();

        if (!parse(ipAddr)) {
            spdlog::error("Invalid IP address in CIDR: {}", cidr);
            return false;
        }

        uint32_t mask =
            (prefixLength == 0) ? 0 : (~0U << (IPV4_BIT_LENGTH - prefixLength));
        ipValue &= htonl(mask);

        addressStr = integerToIp(ipValue) + "/" + std::to_string(prefixLength);
        spdlog::debug("Successfully parsed CIDR: {}", cidr);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in CIDR parsing '{}': {}", cidr, e.what());
        return false;
    }
}

void IPv4::printAddressType() const { spdlog::info("Address type: IPv4"); }

auto IPv4::isInRange(std::string_view start, std::string_view end) -> bool {
    try {
        uint32_t startIp = ipToInteger(start);
        uint32_t endIp = ipToInteger(end);
        uint32_t currentIp = ntohl(ipValue);

        if (startIp > endIp) {
            throw AddressRangeError("Invalid range: start IP > end IP");
        }

        bool inRange = currentIp >= startIp && currentIp <= endIp;
        spdlog::trace("Range check: {} in [{}, {}] = {}", addressStr, start,
                      end, inRange);
        return inRange;

    } catch (const InvalidAddressFormat& e) {
        spdlog::error("Invalid address in range check: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in range check: {}", e.what());
        return false;
    }
}

auto IPv4::toBinary() const -> std::string {
    try {
        std::bitset<IPV4_BIT_LENGTH> bits(ntohl(ipValue));
        return bits.to_string();
    } catch (const std::exception& e) {
        spdlog::error("Exception in binary conversion: {}", e.what());
        return "";
    }
}

auto IPv4::toHex() const -> std::string {
    try {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(8) << ntohl(ipValue);
        return oss.str();
    } catch (const std::exception& e) {
        spdlog::error("Exception in hex conversion: {}", e.what());
        return "";
    }
}

auto IPv4::isEqual(const Address& other) const -> bool {
    try {
        if (other.getType() != "IPv4") {
            return false;
        }

        const auto* ipv4Other = dynamic_cast<const IPv4*>(&other);
        if (!ipv4Other) {
            return false;
        }

        return ipValue == ipv4Other->ipValue;
    } catch (const std::exception& e) {
        spdlog::error("Exception in equality check: {}", e.what());
        return false;
    }
}

auto IPv4::getType() const -> std::string_view { return "IPv4"; }

auto IPv4::getNetworkAddress(std::string_view mask) const -> std::string {
    try {
        uint32_t maskValue = ipToInteger(mask);
        uint32_t netAddr = ntohl(ipValue) & maskValue;
        std::string result = integerToIp(htonl(netAddr));
        spdlog::trace("Network address for {} with mask {}: {}", addressStr,
                      mask, result);
        return result;
    } catch (const InvalidAddressFormat& e) {
        spdlog::error("Invalid mask in network address: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in network address calculation: {}", e.what());
        return "";
    }
}

auto IPv4::getBroadcastAddress(std::string_view mask) const -> std::string {
    try {
        uint32_t maskValue = ipToInteger(mask);
        uint32_t broadcastAddr = (ntohl(ipValue) & maskValue) | (~maskValue);
        std::string result = integerToIp(htonl(broadcastAddr));
        spdlog::trace("Broadcast address for {} with mask {}: {}", addressStr,
                      mask, result);
        return result;
    } catch (const InvalidAddressFormat& e) {
        spdlog::error("Invalid mask in broadcast address: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in broadcast address calculation: {}",
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

        const auto* ipv4Other = dynamic_cast<const IPv4*>(&other);
        if (!ipv4Other) {
            return false;
        }

        uint32_t maskValue = ipToInteger(mask);
        uint32_t netAddr1 = ntohl(ipValue) & maskValue;
        uint32_t netAddr2 = ntohl(ipv4Other->ipValue) & maskValue;

        bool sameSubnet = netAddr1 == netAddr2;
        spdlog::trace("Subnet check: {} and {} with mask {}: {}", addressStr,
                      other.getAddress(), mask, sameSubnet);
        return sameSubnet;

    } catch (const InvalidAddressFormat& e) {
        spdlog::error("Invalid mask in subnet check: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in subnet check: {}", e.what());
        return false;
    }
}

auto IPv4::ipToInteger(std::string_view ipAddr) const -> uint32_t {
    try {
        if (!isValidIPv4(ipAddr)) {
            throw InvalidAddressFormat(std::string(ipAddr));
        }

        std::string ipStr(ipAddr);
        uint32_t result = 0;

        if (inet_pton(AF_INET, ipStr.c_str(), &result) != 1) {
            throw InvalidAddressFormat(std::string(ipAddr));
        }

        return ntohl(result);

    } catch (const InvalidAddressFormat&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in IP to integer conversion '{}': {}", ipAddr,
                      e.what());
        throw InvalidAddressFormat(std::string(ipAddr));
    }
}

auto IPv4::integerToIp(uint32_t ipAddr) const -> std::string {
    try {
        std::array<char, INET_ADDRSTRLEN> buffer{};
        struct in_addr addr{};
        addr.s_addr = ipAddr;

        if (inet_ntop(AF_INET, &addr, buffer.data(), buffer.size()) ==
            nullptr) {
            throw std::runtime_error("Failed to convert integer to IP");
        }

        return std::string(buffer.data());
    } catch (const std::exception& e) {
        spdlog::error("Exception in integer to IP conversion: {}", e.what());
        return "";
    }
}

}  // namespace atom::web
