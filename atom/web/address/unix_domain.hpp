#ifndef ATOM_WEB_ADDRESS_UNIX_DOMAIN_HPP
#define ATOM_WEB_ADDRESS_UNIX_DOMAIN_HPP

#include "address.hpp"

namespace atom::web {

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

    /**
     * @brief Parses the Unix domain socket address from a string.
     * @param path The string containing the Unix domain socket path.
     * @return True if the parsing was successful, false otherwise.
     * @override
     */
    auto parse(std::string_view path) -> bool override;

    /**
     * @brief Prints the type of the address (Unix Domain).
     * @override
     */
    void printAddressType() const override;

    /**
     * @brief Checks if the Unix domain socket address is within a specified
     * range.
     * @param start The start of the range (not applicable for Unix domain
     * sockets).
     * @param end The end of the range (not applicable for Unix domain sockets).
     * @return Always returns false as ranges are not applicable for Unix domain
     * sockets.
     * @override
     */
    auto isInRange(std::string_view start, std::string_view end)
        -> bool override;

    /**
     * @brief Converts the Unix domain socket address to its binary
     * representation.
     * @return The binary representation of the address as a string.
     * @override
     */
    [[nodiscard]] auto toBinary() const -> std::string override;

    /**
     * @brief Checks if this Unix domain socket address is equal to another
     * Address object.
     * @param other The other Address object to compare with.
     * @return True if the addresses are equal, false otherwise.
     * @override
     */
    [[nodiscard]] auto isEqual(const Address& other) const -> bool override;

    /**
     * @brief Gets the type of the address (Unix Domain).
     * @return A string_view representing the address type ("unixdomain").
     * @override
     */
    [[nodiscard]] auto getType() const -> std::string_view override;

    /**
     * @brief Gets the network address given a subnet mask.
     * @param mask The subnet mask (not applicable for Unix domain sockets).
     * @return An empty string as network addresses are not applicable for Unix
     * domain sockets.
     * @override
     */
    [[nodiscard]] auto getNetworkAddress(std::string_view mask) const
        -> std::string override;

    /**
     * @brief Gets the broadcast address given a subnet mask.
     * @param mask The subnet mask (not applicable for Unix domain sockets).
     * @return An empty string as broadcast addresses are not applicable for
     * Unix domain sockets.
     * @override
     */
    [[nodiscard]] auto getBroadcastAddress(std::string_view mask) const
        -> std::string override;

    /**
     * @brief Checks if two addresses are in the same subnet.
     * @param other The other address to compare with.
     * @param mask The subnet mask (not applicable for Unix domain sockets).
     * @return Always returns false as subnets are not applicable for Unix
     * domain sockets.
     * @override
     */
    [[nodiscard]] auto isSameSubnet(const Address& other,
                                    std::string_view mask) const
        -> bool override;

    /**
     * @brief Converts the Unix domain socket address to its hexadecimal
     * representation.
     * @return The hexadecimal representation of the address as a string.
     * @override
     */
    [[nodiscard]] auto toHex() const -> std::string override;

    /**
     * @brief Validates a Unix domain socket path.
     * @param path The path to validate.
     * @return True if the path is valid, false otherwise.
     */
    [[nodiscard]] static auto isValidPath(std::string_view path) -> bool;
};

}  // namespace atom::web

#endif  // ATOM_WEB_ADDRESS_UNIX_DOMAIN_HPP
