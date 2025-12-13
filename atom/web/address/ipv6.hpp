#ifndef ATOM_WEB_ADDRESS_IPV6_HPP
#define ATOM_WEB_ADDRESS_IPV6_HPP

#include <array>
#include <compare>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include "address.hpp"

namespace atom::web {

/**
 * @brief IPv6 address parsing error codes
 */
enum class IPv6Error {
    Success = 0,
    InvalidFormat,
    InvalidSegment,
    InvalidPrefix,
    TooManySegments,
    InvalidCompression
};

/**
 * @brief Convert IPv6Error to string
 */
[[nodiscard]] constexpr auto ipv6ErrorToString(IPv6Error error) noexcept
    -> std::string_view {
    switch (error) {
        case IPv6Error::Success:
            return "Success";
        case IPv6Error::InvalidFormat:
            return "Invalid format";
        case IPv6Error::InvalidSegment:
            return "Invalid segment value";
        case IPv6Error::InvalidPrefix:
            return "Invalid prefix length";
        case IPv6Error::TooManySegments:
            return "Too many segments";
        case IPv6Error::InvalidCompression:
            return "Invalid compression";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Constexpr validation of IPv6 segment
 */
[[nodiscard]] constexpr auto isValidSegment(int value) noexcept -> bool {
    return value >= 0 && value <= 0xFFFF;
}

/**
 * @brief Constexpr validation of IPv6 prefix length
 */
[[nodiscard]] constexpr auto isValidIPv6Prefix(int prefix) noexcept -> bool {
    return prefix >= 0 && prefix <= 128;
}

/**
 * @class IPv6
 * @brief A class representing an IPv6 address.
 */
class IPv6 : public Address {
public:
    static constexpr size_t SEGMENT_COUNT = 8;
    static constexpr size_t SEGMENT_BIT_LENGTH = 16;
    static constexpr size_t BIT_LENGTH = 128;
    static constexpr int MAX_PREFIX_LENGTH = 128;

    IPv6() = default;

    /**
     * @brief Constructs an IPv6 address from a string.
     * @param address The IPv6 address as a string.
     * @throws InvalidAddressFormat if the address format is invalid.
     */
    explicit IPv6(std::string_view address);

    /**
     * @brief Parses the IPv6 address string.
     * @param address The IPv6 address string to parse.
     * @return True if the address is successfully parsed, false otherwise.
     * @throws InvalidAddressFormat if the address format is invalid.
     */
    auto parse(std::string_view address) -> bool override;

    void printAddressType() const override;
    auto isInRange(std::string_view start,
                   std::string_view end) -> bool override;
    [[nodiscard]] auto toBinary() const -> std::string override;
    [[nodiscard]] auto isEqual(const Address& other) const -> bool override;
    [[nodiscard]] auto getType() const -> std::string_view override;
    [[nodiscard]] auto getNetworkAddress(std::string_view mask) const
        -> std::string override;
    [[nodiscard]] auto getBroadcastAddress(std::string_view mask) const
        -> std::string override;
    [[nodiscard]] auto isSameSubnet(
        const Address& other, std::string_view mask) const -> bool override;
    [[nodiscard]] auto toHex() const -> std::string override;

    /**
     * @brief Three-way comparison for IPv6 addresses.
     * @param other The other address to compare.
     * @return Comparison result.
     */
    [[nodiscard]] auto operator<=>(const Address& other) const
        -> std::partial_ordering override;

    /**
     * @brief Get the IP address segments.
     * @return Array of 8 16-bit segments.
     */
    [[nodiscard]] auto getSegments() const noexcept
        -> const std::array<uint16_t, SEGMENT_COUNT>& {
        return ipSegments;
    }

    /**
     * @brief Check if this is a loopback address (::1).
     * @return True if loopback.
     */
    [[nodiscard]] auto isLoopback() const -> bool;

    /**
     * @brief Check if this is a link-local address (fe80::/10).
     * @return True if link-local.
     */
    [[nodiscard]] auto isLinkLocal() const -> bool;

    /**
     * @brief Check if this is a multicast address (ff00::/8).
     * @return True if multicast.
     */
    [[nodiscard]] auto isMulticast() const -> bool;

    /**
     * @brief Check if this is a unique local address (fc00::/7).
     * @return True if unique local.
     */
    [[nodiscard]] auto isUniqueLocal() const -> bool;

    /**
     * @brief Check if this is an IPv4-mapped IPv6 address (::ffff:x.x.x.x).
     * @return True if IPv4-mapped.
     */
    [[nodiscard]] auto isIPv4Mapped() const -> bool;

    /**
     * @brief Get the embedded IPv4 address if this is an IPv4-mapped address.
     * @return The IPv4 address string, or empty if not IPv4-mapped.
     */
    [[nodiscard]] auto getEmbeddedIPv4() const -> std::optional<std::string>;

    /**
     * @brief Parses an IPv6 address in CIDR notation.
     * @param cidr The CIDR notation string.
     * @return True if the CIDR notation is successfully parsed, false
     * otherwise.
     * @throws InvalidAddressFormat if the CIDR format is invalid.
     */
    auto parseCIDR(std::string_view cidr) -> bool;

    /**
     * @brief Gets the prefix length from a CIDR notation.
     * @param cidr The CIDR notation string.
     * @return The prefix length or std::nullopt if invalid.
     */
    [[nodiscard]] static auto getPrefixLength(std::string_view cidr)
        -> std::optional<int>;

    /**
     * @brief Validates an IPv6 address string.
     * @param address The IPv6 address string.
     * @return True if the address is valid, false otherwise.
     */
    [[nodiscard]] static auto isValidIPv6(std::string_view address) -> bool;

    /**
     * @brief Create IPv6 from segments.
     * @param segments Span of 8 16-bit segments.
     * @return IPv6 address.
     */
    [[nodiscard]] static auto fromSegments(
        std::span<const uint16_t, SEGMENT_COUNT> segments) -> IPv6;

    /**
     * @brief Create an IPv4-mapped IPv6 address.
     * @param ipv4Str The IPv4 address string.
     * @return IPv6 address in ::ffff:x.x.x.x format.
     */
    [[nodiscard]] static auto fromIPv4Mapped(std::string_view ipv4Str) -> IPv6;

private:
    std::array<uint16_t, 8> ipSegments{};

    /**
     * @brief Converts an IP address string to an array of segments.
     * @param ipAddr The IP address string.
     * @return The IP address as an array of segments.
     * @throws InvalidAddressFormat if the address format is invalid.
     */
    [[nodiscard]] auto ipToArray(std::string_view ipAddr) const
        -> std::array<uint16_t, 8>;

    /**
     * @brief Converts an array of segments to an IP address string.
     * @param segments The IP address segments.
     * @return The IP address string.
     */
    [[nodiscard]] auto arrayToIp(const std::array<uint16_t, 8>& segments) const
        -> std::string;

    /**
     * @brief Applies a prefix length mask to the IPv6 address segments.
     * @param prefixLength The prefix length (0-128).
     */
    void applyPrefixMask(int prefixLength);

    /**
     * @brief Try to parse an IPv6 address string.
     * @param address The address string.
     * @return Expected containing IPv6 or error.
     */
    [[nodiscard]] static auto tryParse(std::string_view address)
        -> std::expected<IPv6, IPv6Error>;

    /**
     * @brief Try to parse CIDR notation.
     * @param cidr The CIDR string.
     * @return Expected containing (IPv6, prefix) or error.
     */
    [[nodiscard]] static auto tryParseCIDR(std::string_view cidr)
        -> std::expected<std::pair<IPv6, int>, IPv6Error>;

    /**
     * @brief Check if this is an unspecified address (::).
     * @return True if unspecified.
     */
    [[nodiscard]] auto isUnspecified() const noexcept -> bool;

    /**
     * @brief Check if this is a global unicast address.
     * @return True if global unicast.
     */
    [[nodiscard]] auto isGlobalUnicast() const -> bool;

    /**
     * @brief Get the compressed (canonical) string representation.
     * @return Compressed IPv6 string.
     */
    [[nodiscard]] auto toCompressed() const -> std::string;

    /**
     * @brief Get the expanded (full) string representation.
     * @return Expanded IPv6 string with all segments.
     */
    [[nodiscard]] auto toExpanded() const -> std::string;

    /**
     * @brief Get subnet mask from prefix length.
     * @param prefix Prefix length (0-128).
     * @return Subnet mask as IPv6.
     */
    [[nodiscard]] static auto prefixToMask(int prefix) -> IPv6;
};

}  // namespace atom::web

// std::formatter specialization for IPv6
template <>
struct std::formatter<atom::web::IPv6> : std::formatter<std::string> {
    auto format(const atom::web::IPv6& ip, std::format_context& ctx) const {
        return std::formatter<std::string>::format(std::string(ip.getAddress()),
                                                   ctx);
    }
};

#endif  // ATOM_WEB_ADDRESS_IPV6_HPP
