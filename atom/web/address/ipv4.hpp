#ifndef ATOM_WEB_ADDRESS_IPV4_HPP
#define ATOM_WEB_ADDRESS_IPV4_HPP

#include <array>
#include <compare>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include "address.hpp"
#include "atom/type/expected.hpp"

namespace atom::web {

/**
 * @brief IPv4 address parsing error codes
 */
enum class IPv4Error {
    Success = 0,
    InvalidFormat,
    InvalidOctet,
    InvalidPrefix,
    OutOfRange
};

/**
 * @brief Convert IPv4Error to string
 */
[[nodiscard]] constexpr auto ipv4ErrorToString(IPv4Error error) noexcept
    -> std::string_view {
    switch (error) {
        case IPv4Error::Success:
            return "Success";
        case IPv4Error::InvalidFormat:
            return "Invalid format";
        case IPv4Error::InvalidOctet:
            return "Invalid octet value";
        case IPv4Error::InvalidPrefix:
            return "Invalid prefix length";
        case IPv4Error::OutOfRange:
            return "Value out of range";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Constexpr validation of IPv4 octet
 */
[[nodiscard]] constexpr auto isValidOctet(int value) noexcept -> bool {
    return value >= 0 && value <= 255;
}

/**
 * @brief Constexpr validation of IPv4 prefix length
 */
[[nodiscard]] constexpr auto isValidPrefix(int prefix) noexcept -> bool {
    return prefix >= 0 && prefix <= 32;
}

/**
 * @class IPv4
 * @brief A class representing an IPv4 address.
 */
class IPv4 : public Address {
public:
    static constexpr size_t OCTET_COUNT = 4;
    static constexpr size_t BIT_LENGTH = 32;
    static constexpr uint8_t MAX_OCTET_VALUE = 255;
    static constexpr int MAX_PREFIX_LENGTH = 32;

    IPv4() = default;

    /**
     * @brief Constructs an IPv4 address from a string.
     * @param address The IPv4 address as a string.
     * @throws InvalidAddressFormat if the address format is invalid.
     */
    explicit IPv4(std::string_view address);

    /**
     * @brief Parses the IPv4 address string.
     * @param address The IPv4 address string to parse.
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
     * @brief Three-way comparison for IPv4 addresses.
     * @param other The other address to compare.
     * @return Comparison result.
     */
    [[nodiscard]] auto operator<=>(const Address& other) const
        -> std::partial_ordering override;

    /**
     * @brief Get the raw IP value as uint32_t.
     * @return The IP address as a 32-bit unsigned integer in network byte
     * order.
     */
    [[nodiscard]] auto getRawValue() const noexcept -> uint32_t {
        return ipValue;
    }

    /**
     * @brief Get the IP address as an array of octets.
     * @return Array of 4 octets.
     */
    [[nodiscard]] auto getOctets() const -> std::array<uint8_t, OCTET_COUNT>;

    /**
     * @brief Check if this is a private IP address.
     * @return True if private (10.x.x.x, 172.16-31.x.x, 192.168.x.x).
     */
    [[nodiscard]] auto isPrivate() const -> bool;

    /**
     * @brief Check if this is a loopback address.
     * @return True if loopback (127.x.x.x).
     */
    [[nodiscard]] auto isLoopback() const -> bool;

    /**
     * @brief Check if this is a multicast address.
     * @return True if multicast (224.0.0.0 - 239.255.255.255).
     */
    [[nodiscard]] auto isMulticast() const -> bool;

    /**
     * @brief Check if this is a link-local address.
     * @return True if link-local (169.254.x.x).
     */
    [[nodiscard]] auto isLinkLocal() const -> bool;

    /**
     * @brief Parses an IPv4 address in CIDR notation.
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

private:
    uint32_t ipValue{0};

    /**
     * @brief Converts an IP address string to an integer.
     * @param ipAddr The IP address string.
     * @return The IP address as an integer.
     * @throws InvalidAddressFormat if the address format is invalid.
     */
    [[nodiscard]] auto ipToInteger(std::string_view ipAddr) const -> uint32_t;

    /**
     * @brief Converts an integer to an IP address string.
     * @param ipAddr The IP address as an integer.
     * @return The IP address string.
     */
    [[nodiscard]] auto integerToIp(uint32_t ipAddr) const -> std::string;

    /**
     * @brief Validates an IPv4 address string.
     * @param address The IPv4 address string.
     * @return True if the address is valid, false otherwise.
     */
    [[nodiscard]] static auto isValidIPv4(std::string_view address) -> bool;

    /**
     * @brief Create IPv4 from octets.
     * @param octets Span of 4 octets.
     * @return IPv4 address.
     */
    [[nodiscard]] static auto fromOctets(
        std::span<const uint8_t, OCTET_COUNT> octets) -> IPv4;

    /**
     * @brief Create IPv4 from individual octets.
     * @param a First octet.
     * @param b Second octet.
     * @param c Third octet.
     * @param d Fourth octet.
     * @return IPv4 address.
     */
    [[nodiscard]] static auto fromOctets(uint8_t a, uint8_t b, uint8_t c,
                                         uint8_t d) -> IPv4;

    /**
     * @brief Try to parse an IPv4 address string.
     * @param address The address string.
     * @return Expected containing IPv4 or error.
     */
    [[nodiscard]] static auto tryParse(std::string_view address)
        -> atom::type::expected<IPv4, IPv4Error>;

    /**
     * @brief Try to parse CIDR notation.
     * @param cidr The CIDR string.
     * @return Expected containing (IPv4, prefix) or error.
     */
    [[nodiscard]] static auto tryParseCIDR(std::string_view cidr)
        -> atom::type::expected<std::pair<IPv4, int>, IPv4Error>;

    /**
     * @brief Check if this is a broadcast address.
     * @return True if broadcast (255.255.255.255).
     */
    [[nodiscard]] auto isBroadcast() const noexcept -> bool;

    /**
     * @brief Check if this is an unspecified address.
     * @return True if unspecified (0.0.0.0).
     */
    [[nodiscard]] auto isUnspecified() const noexcept -> bool;

    /**
     * @brief Get subnet mask from prefix length.
     * @param prefix Prefix length (0-32).
     * @return Subnet mask as IPv4.
     */
    [[nodiscard]] static auto prefixToMask(int prefix) -> IPv4;

    /**
     * @brief Get prefix length from subnet mask.
     * @param mask Subnet mask.
     * @return Prefix length or nullopt if invalid mask.
     */
    [[nodiscard]] static auto maskToPrefix(const IPv4& mask)
        -> std::optional<int>;

    /**
     * @brief Apply subnet mask to get network address.
     * @param mask Subnet mask.
     * @return Network address.
     */
    [[nodiscard]] auto applyMask(const IPv4& mask) const -> IPv4;

    /**
     * @brief Get the next IP address.
     * @return Next IP address or nullopt if overflow.
     */
    [[nodiscard]] auto next() const -> std::optional<IPv4>;

    /**
     * @brief Get the previous IP address.
     * @return Previous IP address or nullopt if underflow.
     */
    [[nodiscard]] auto prev() const -> std::optional<IPv4>;
};

}  // namespace atom::web

// std::formatter specialization for IPv4
template <>
struct std::formatter<atom::web::IPv4> : std::formatter<std::string> {
    auto format(const atom::web::IPv4& ip, std::format_context& ctx) const {
        return std::formatter<std::string>::format(std::string(ip.getAddress()),
                                                   ctx);
    }
};

#endif  // ATOM_WEB_ADDRESS_IPV4_HPP
