#ifndef ATOM_WEB_ADDRESS_IPV6_HPP
#define ATOM_WEB_ADDRESS_IPV6_HPP

#include <array>
#include <cstdint>
#include <optional>
#include "address.hpp"


namespace atom::web {

/**
 * @class IPv6
 * @brief A class representing an IPv6 address.
 */
class IPv6 : public Address {
public:
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
    auto isInRange(std::string_view start, std::string_view end)
        -> bool override;
    [[nodiscard]] auto toBinary() const -> std::string override;
    [[nodiscard]] auto isEqual(const Address& other) const -> bool override;
    [[nodiscard]] auto getType() const -> std::string_view override;
    [[nodiscard]] auto getNetworkAddress(std::string_view mask) const
        -> std::string override;
    [[nodiscard]] auto getBroadcastAddress(std::string_view mask) const
        -> std::string override;
    [[nodiscard]] auto isSameSubnet(const Address& other,
                                    std::string_view mask) const
        -> bool override;
    [[nodiscard]] auto toHex() const -> std::string override;

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
};

}  // namespace atom::web

#endif  // ATOM_WEB_ADDRESS_IPV6_HPP
