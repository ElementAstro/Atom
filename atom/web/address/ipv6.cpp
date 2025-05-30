#include "ipv6.hpp"

#ifdef _WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <bitset>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>

namespace atom::web {

namespace {
constexpr int IPV6_SEGMENT_COUNT = 8;
constexpr int IPV6_SEGMENT_BIT_LENGTH = 16;
constexpr int IPV6_MAX_PREFIX_LENGTH = 128;
constexpr size_t IPV6_MAX_STRING_LENGTH = 39;
constexpr uint16_t SEGMENT_MASK = 0xFFFF;

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
 * @brief Fast IPv6 validation without complex parsing
 */
auto fastIsValidIPv6(std::string_view address) -> bool {
    if (address.empty() || address.length() > IPV6_MAX_STRING_LENGTH) {
        return false;
    }

    int colonCount = 0;
    int doubleColonCount = 0;
    size_t consecutiveColons = 0;

    for (size_t i = 0; i < address.length(); ++i) {
        char c = address[i];

        if (c == ':') {
            colonCount++;
            consecutiveColons++;

            if (consecutiveColons > 2) {
                return false;
            }

            if (consecutiveColons == 2) {
                doubleColonCount++;
                if (doubleColonCount > 1) {
                    return false;
                }
            }
        } else {
            consecutiveColons = 0;

            if (!std::isxdigit(c)) {
                return false;
            }
        }
    }

    return colonCount >= 2 && colonCount <= 7 && doubleColonCount <= 1;
}

/**
 * @brief Compare two IPv6 address arrays
 */
auto compareArrays(const std::array<uint16_t, 8>& a,
                   const std::array<uint16_t, 8>& b) -> int {
    for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
        if (a[i] < b[i])
            return -1;
        if (a[i] > b[i])
            return 1;
    }
    return 0;
}
}  // namespace

auto IPv6::isValidIPv6(std::string_view address) -> bool {
    return fastIsValidIPv6(address);
}

IPv6::IPv6(std::string_view address) {
    if (!parse(address)) {
        throw InvalidAddressFormat(std::string(address));
    }
}

auto IPv6::parse(std::string_view address) -> bool {
    try {
        if (!isValidIPv6(address)) {
            spdlog::error("Invalid IPv6 address format: {}", address);
            return false;
        }

        std::array<uint8_t, 16> addrBuf{};
        std::string addressStr(address);

        if (inet_pton(AF_INET6, addressStr.c_str(), addrBuf.data()) != 1) {
            spdlog::error("IPv6 address parsing failed: {}", address);
            return false;
        }

        this->addressStr = std::move(addressStr);

        for (int i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            ipSegments[i] = (static_cast<uint16_t>(addrBuf[i * 2]) << 8) |
                            addrBuf[i * 2 + 1];
        }

        spdlog::trace("Successfully parsed IPv6 address: {}", address);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception while parsing IPv6 address '{}': {}", address,
                      e.what());
        return false;
    }
}

auto IPv6::getPrefixLength(std::string_view cidr) -> std::optional<int> {
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
        prefixLength > IPV6_MAX_PREFIX_LENGTH) {
        spdlog::error("Invalid IPv6 CIDR prefix length: {}", prefixStr);
        return std::nullopt;
    }

    return prefixLength;
}

void IPv6::applyPrefixMask(int prefixLength) {
    if (prefixLength < 0 || prefixLength > IPV6_MAX_PREFIX_LENGTH) {
        return;
    }

    int fullSegments = prefixLength / IPV6_SEGMENT_BIT_LENGTH;
    int remainingBits = prefixLength % IPV6_SEGMENT_BIT_LENGTH;

    for (int i = fullSegments; i < IPV6_SEGMENT_COUNT; ++i) {
        if (i == fullSegments && remainingBits > 0) {
            uint16_t mask = static_cast<uint16_t>(~0)
                            << (IPV6_SEGMENT_BIT_LENGTH - remainingBits);
            ipSegments[i] &= mask;
        } else {
            ipSegments[i] = 0;
        }
    }
}

auto IPv6::parseCIDR(std::string_view cidr) -> bool {
    try {
        size_t pos = cidr.find('/');
        if (pos == std::string_view::npos) {
            spdlog::error("Invalid IPv6 CIDR notation: {}", cidr);
            return false;
        }

        auto ipAddr = cidr.substr(0, pos);
        auto prefixLengthOpt = getPrefixLength(cidr);

        if (!prefixLengthOpt.has_value()) {
            spdlog::error("Invalid IPv6 CIDR prefix: {}", cidr);
            return false;
        }

        int prefixLength = prefixLengthOpt.value();

        if (!parse(ipAddr)) {
            spdlog::error("Invalid IP address in IPv6 CIDR: {}", cidr);
            return false;
        }

        applyPrefixMask(prefixLength);

        addressStr = arrayToIp(ipSegments) + "/" + std::to_string(prefixLength);
        spdlog::debug("Successfully parsed IPv6 CIDR: {}", cidr);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 CIDR parsing '{}': {}", cidr,
                      e.what());
        return false;
    }
}

void IPv6::printAddressType() const { spdlog::info("Address type: IPv6"); }

auto IPv6::isInRange(std::string_view start, std::string_view end) -> bool {
    try {
        auto startIp = ipToArray(start);
        auto endIp = ipToArray(end);

        if (compareArrays(startIp, endIp) > 0) {
            throw AddressRangeError("Invalid range: start IP > end IP");
        }

        bool inRange = compareArrays(ipSegments, startIp) >= 0 &&
                       compareArrays(ipSegments, endIp) <= 0;

        spdlog::trace("IPv6 range check: {} in [{}, {}] = {}", addressStr,
                      start, end, inRange);
        return inRange;

    } catch (const InvalidAddressFormat& e) {
        spdlog::error("Invalid address in IPv6 range check: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 range check: {}", e.what());
        return false;
    }
}

auto IPv6::toBinary() const -> std::string {
    try {
        std::string binaryStr;
        binaryStr.reserve(IPV6_SEGMENT_COUNT * IPV6_SEGMENT_BIT_LENGTH);

        for (const auto& segment : ipSegments) {
            std::bitset<IPV6_SEGMENT_BIT_LENGTH> bits(segment);
            binaryStr += bits.to_string();
        }

        return binaryStr;
    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 binary conversion: {}", e.what());
        return "";
    }
}

auto IPv6::toHex() const -> std::string {
    try {
        std::ostringstream oss;
        for (size_t i = 0; i < ipSegments.size(); ++i) {
            if (i > 0)
                oss << ":";
            oss << std::hex << std::setfill('0') << std::setw(4)
                << ipSegments[i];
        }
        return oss.str();
    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 hex conversion: {}", e.what());
        return "";
    }
}

auto IPv6::isEqual(const Address& other) const -> bool {
    try {
        if (other.getType() != "IPv6") {
            return false;
        }

        const auto* ipv6Other = dynamic_cast<const IPv6*>(&other);
        if (!ipv6Other) {
            return false;
        }

        return std::equal(ipSegments.begin(), ipSegments.end(),
                          ipv6Other->ipSegments.begin());
    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 equality check: {}", e.what());
        return false;
    }
}

auto IPv6::getType() const -> std::string_view { return "IPv6"; }

auto IPv6::getNetworkAddress(std::string_view mask) const -> std::string {
    try {
        auto maskSegments = ipToArray(mask);
        std::array<uint16_t, IPV6_SEGMENT_COUNT> networkSegments{};

        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            networkSegments[i] = ipSegments[i] & maskSegments[i];
        }

        std::string result = arrayToIp(networkSegments);
        spdlog::trace("IPv6 network address for {} with mask {}: {}",
                      addressStr, mask, result);
        return result;

    } catch (const InvalidAddressFormat& e) {
        spdlog::error("Invalid mask in IPv6 network address: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 network address calculation: {}",
                      e.what());
        return "";
    }
}

auto IPv6::getBroadcastAddress(std::string_view mask) const -> std::string {
    try {
        auto maskSegments = ipToArray(mask);
        std::array<uint16_t, IPV6_SEGMENT_COUNT> broadcastSegments{};

        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            broadcastSegments[i] =
                ipSegments[i] | (~maskSegments[i] & SEGMENT_MASK);
        }

        std::string result = arrayToIp(broadcastSegments);
        spdlog::trace("IPv6 broadcast address for {} with mask {}: {}",
                      addressStr, mask, result);
        return result;

    } catch (const InvalidAddressFormat& e) {
        spdlog::error("Invalid mask in IPv6 broadcast address: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 broadcast address calculation: {}",
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

        const auto* ipv6Other = dynamic_cast<const IPv6*>(&other);
        if (!ipv6Other) {
            return false;
        }

        auto maskSegments = ipToArray(mask);

        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            if ((ipSegments[i] & maskSegments[i]) !=
                (ipv6Other->ipSegments[i] & maskSegments[i])) {
                return false;
            }
        }

        bool sameSubnet = true;
        spdlog::trace("IPv6 subnet check: {} and {} with mask {}: {}",
                      addressStr, other.getAddress(), mask, sameSubnet);
        return sameSubnet;

    } catch (const InvalidAddressFormat& e) {
        spdlog::error("Invalid mask in IPv6 subnet check: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 subnet check: {}", e.what());
        return false;
    }
}

auto IPv6::ipToArray(std::string_view ipAddr) const -> std::array<uint16_t, 8> {
    try {
        if (!isValidIPv6(ipAddr)) {
            throw InvalidAddressFormat(std::string(ipAddr));
        }

        std::array<uint16_t, IPV6_SEGMENT_COUNT> segments{};
        std::array<uint8_t, 16> addrBuf{};
        std::string ipStr(ipAddr);

        if (inet_pton(AF_INET6, ipStr.c_str(), addrBuf.data()) != 1) {
            throw InvalidAddressFormat(std::string(ipAddr));
        }

        for (int i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            segments[i] = (static_cast<uint16_t>(addrBuf[i * 2]) << 8) |
                          addrBuf[i * 2 + 1];
        }

        return segments;

    } catch (const InvalidAddressFormat&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 to array conversion '{}': {}", ipAddr,
                      e.what());
        throw InvalidAddressFormat(std::string(ipAddr));
    }
}

auto IPv6::arrayToIp(const std::array<uint16_t, 8>& segments) const
    -> std::string {
    try {
        std::array<uint8_t, 16> addrBuf{};

        for (int i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            addrBuf[i * 2] = static_cast<uint8_t>(segments[i] >> 8);
            addrBuf[i * 2 + 1] = static_cast<uint8_t>(segments[i] & 0xFF);
        }

        std::array<char, INET6_ADDRSTRLEN> buffer{};
        if (inet_ntop(AF_INET6, addrBuf.data(), buffer.data(), buffer.size()) ==
            nullptr) {
            throw std::runtime_error("Failed to convert IPv6 array to string");
        }

        return std::string(buffer.data());

    } catch (const std::exception& e) {
        spdlog::error("Exception in IPv6 array to string conversion: {}",
                      e.what());
        return "";
    }
}

}  // namespace atom::web
