#ifndef ATOM_WEB_ADDRESS_HPP
#define ATOM_WEB_ADDRESS_HPP

#include <array>  // For fixed-size arrays
#include <cstdint>
#include <memory>     // For smart pointers
#include <optional>   // For nullable return types
#include <stdexcept>  // For custom exceptions
#include <string>
#include <string_view>  // For efficient string passing

namespace atom::web {

// Custom exceptions for better error handling
class AddressException : public std::runtime_error {
public:
    explicit AddressException(const std::string& message)
        : std::runtime_error(message) {}
};

class InvalidAddressFormat : public AddressException {
public:
    explicit InvalidAddressFormat(const std::string& message)
        : AddressException("Invalid address format: " + message) {}
};

class AddressRangeError : public AddressException {
public:
    explicit AddressRangeError(const std::string& message)
        : AddressException("Address range error: " + message) {}
};

/**
 * @class Address
 * @brief A base class representing a generic network address.
 */
class Address {
protected:
    std::string addressStr;  ///< Stores the address as a string.

public:
    Address() = default;
    virtual ~Address() = default;
    Address(const Address& other) = default;
    Address& operator=(const Address& other) = default;
    Address(Address&& other) noexcept = default;
    Address& operator=(Address&& other) noexcept = default;

    /**
     * @brief Parses the address string.
     * @param address The address string to parse.
     * @return True if the address is successfully parsed, false otherwise.
     * @throws InvalidAddressFormat if the address format is invalid.
     */
    virtual auto parse(std::string_view address) -> bool = 0;

    /**
     * @brief Prints the address type.
     */
    virtual void printAddressType() const = 0;

    /**
     * @brief Checks if the address is within the specified range.
     * @param start The start address of the range.
     * @param end The end address of the range.
     * @return True if the address is within the range, false otherwise.
     * @throws AddressRangeError if the range is invalid.
     */
    virtual auto isInRange(std::string_view start,
                           std::string_view end) -> bool = 0;

    /**
     * @brief Converts the address to its binary representation.
     * @return The binary representation of the address as a string.
     */
    [[nodiscard]] virtual auto toBinary() const -> std::string = 0;

    /**
     * @brief Gets the address string.
     * @return The address as a string.
     */
    [[nodiscard]] auto getAddress() const -> std::string_view {
        return addressStr;
    }

    /**
     * @brief Checks if two addresses are equal.
     * @param other The other address to compare with.
     * @return True if the addresses are equal, false otherwise.
     */
    [[nodiscard]] virtual auto isEqual(const Address& other) const -> bool = 0;

    /**
     * @brief Gets the address type.
     * @return The address type as a string.
     */
    [[nodiscard]] virtual auto getType() const -> std::string_view = 0;

    /**
     * @brief Gets the network address given a subnet mask.
     * @param mask The subnet mask.
     * @return The network address as a string.
     * @throws InvalidAddressFormat if the mask format is invalid.
     */
    [[nodiscard]] virtual auto getNetworkAddress(std::string_view mask) const
        -> std::string = 0;

    /**
     * @brief Gets the broadcast address given a subnet mask.
     * @param mask The subnet mask.
     * @return The broadcast address as a string.
     * @throws InvalidAddressFormat if the mask format is invalid.
     */
    [[nodiscard]] virtual auto getBroadcastAddress(std::string_view mask) const
        -> std::string = 0;

    /**
     * @brief Checks if two addresses are in the same subnet.
     * @param other The other address to compare with.
     * @param mask The subnet mask.
     * @return True if the addresses are in the same subnet, false otherwise.
     * @throws InvalidAddressFormat if the mask format is invalid.
     */
    [[nodiscard]] virtual auto isSameSubnet(
        const Address& other, std::string_view mask) const -> bool = 0;

    /**
     * @brief Converts the address to its hexadecimal representation.
     * @return The hexadecimal representation of the address as a string.
     */
    [[nodiscard]] virtual auto toHex() const -> std::string = 0;

    /**
     * @brief Creates an address object from a string.
     * @param addressStr The address string.
     * @return A unique_ptr to an Address object or nullptr if the address type
     * cannot be determined.
     */
    static auto createFromString(std::string_view addressStr)
        -> std::unique_ptr<Address>;
};

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
    uint32_t ipValue{0};  ///< Stores the IP address as an integer.

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
    std::array<uint16_t, 8> ipSegments{};  ///< Stores the IP address segments.

    /**
     * @brief Converts an IP address string to a vector of segments.
     * @param ipAddr The IP address string.
     * @return The IP address as a vector of segments.
     * @throws InvalidAddressFormat if the address format is invalid.
     */
    [[nodiscard]] auto ipToArray(std::string_view ipAddr) const
        -> std::array<uint16_t, 8>;

    /**
     * @brief Converts a vector of segments to an IP address string.
     * @param segments The IP address segments.
     * @return The IP address string.
     */
    [[nodiscard]] auto arrayToIp(const std::array<uint16_t, 8>& segments) const
        -> std::string;
};

/**
 * @class UnixDomain
 * @brief A class representing a Unix domain socket address.
 */
class UnixDomain : public Address {
public:
    UnixDomain() = default;

    /**
     * @brief Constructs a Unix domain socket address from a path.
     * @param path The Unix domain socket path.
     * @throws InvalidAddressFormat if the path format is invalid.
     */
    explicit UnixDomain(std::string_view path);

    auto parse(std::string_view path) -> bool override;
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
     * @brief Validates a Unix domain socket path.
     * @param path The path to validate.
     * @return True if the path is valid, false otherwise.
     */
    [[nodiscard]] static auto isValidPath(std::string_view path) -> bool;
};

}  // namespace atom::web

#endif  // ATOM_WEB_ADDRESS_HPP
