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
#include <future>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/log/loguru.hpp"

namespace atom::web {
constexpr int IPV6_SEGMENT_COUNT = 8;
constexpr int IPV6_SEGMENT_BIT_LENGTH = 16;
constexpr uint32_t BYTE_MASK = 0xFF;

auto IPv6::isValidIPv6(std::string_view address) -> bool {
    // Basic validation - more comprehensive validation is done by inet_pton
    if (address.empty() || address.length() > 39) {
        return false;
    }

    // Count colons - IPv6 has between 2 and 8 segments (1-7 colons)
    int colonCount = 0;
    for (char c : address) {
        if (c == ':') {
            colonCount++;
        }
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
            LOG_F(ERROR, "IPv6 address parsing failed: {}", address);
            return false;
        }

        addressStr = std::string(address);

        // Convert to segments
        for (int i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            ipSegments[i] = (static_cast<uint16_t>(addrBuf[i * 2]) << 8) |
                           addrBuf[i * 2 + 1];
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
            LOG_F(ERROR, "Invalid IPv6 CIDR prefix length: {}", prefixStr);
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
            LOG_F(ERROR, "Invalid IPv6 CIDR notation: {}", cidr);
            return false;
        }

        auto ipAddr = cidr.substr(0, pos);
        auto prefixLengthOpt = getPrefixLength(cidr);

        if (!prefixLengthOpt.has_value()) {
            LOG_F(ERROR, "Invalid IPv6 CIDR prefix: {}", cidr);
            return false;
        }

        int prefixLength = prefixLengthOpt.value();

        if (!parse(ipAddr)) {
            LOG_F(ERROR, "Invalid IP address in IPv6 CIDR: {}", cidr);
            return false;
        }

        // Apply network mask based on prefix length
        // Implementation for IPv6 prefix mask application
        // This would need proper bit manipulation across the segments

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
                throw AddressRangeError("Invalid range: start IP > end IP");
            } else if (startIp[i] < endIp[i]) {
                break;
            }
        }

        // Check if the address is in range
        for (size_t i = 0; i < IPV6_SEGMENT_COUNT; ++i) {
            if (ipSegments[i] < startIp[i] || ipSegments[i] > endIp[i]) {
                return false;
            } else if (ipSegments[i] != startIp[i] || ipSegments[i] != endIp[i]) {
                // If we're different and within range, we're good
                break;
            }
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
            // Implementation could leverage multiple threads
            // For now, using single-threaded approach
            for (const auto& segment : ipSegments) {
                std::bitset<IPV6_SEGMENT_BIT_LENGTH> bits(segment);
                binaryStr += bits.to_string();
            }
        } else {
            for (const auto& segment : ipSegments) {
                std::bitset<IPV6_SEGMENT_BIT_LENGTH> bits(segment);
                binaryStr += bits.to_string();
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
            if (i > 0) ss << ":";
            ss << std::hex << std::setfill('0') << std::setw(4) << ipSegments[i];
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
            segments[i] = (static_cast<uint16_t>(addrBuf[i * 2]) << 8) |
                         addrBuf[i * 2 + 1];
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
            addrBuf[i * 2] = static_cast<uint8_t>(segments[i] >> 8);
            addrBuf[i * 2 + 1] = static_cast<uint8_t>(segments[i] & 0xFF);
        }

        char buffer[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, addrBuf.data(), buffer, INET6_ADDRSTRLEN) ==
            nullptr) {
            throw std::runtime_error("Failed to convert IPv6 array to string");
        }

        return std::string(buffer);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in IPv6 array to string conversion: {}",
              e.what());
        return "";
    }
}

}  // namespace atom::web
