#ifndef ATOM_WEB_ADDRESS_IPV4_HPP
#define ATOM_WEB_ADDRESS_IPV4_HPP

#include <cstdint>
#include <optional>
#include "address.hpp"


namespace atom::web {

/**
 * @class IPv4
 * @brief A class representing an IPv4 address.
 */
class IPv4 : public Address {
public:
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
};

}  // namespace atom::web

#endif  // ATOM_WEB_ADDRESS_IPV4_HPP
