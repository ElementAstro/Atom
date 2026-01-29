#ifndef ATOM_WEB_ADDRESS_ADDRESS_HPP
#define ATOM_WEB_ADDRESS_ADDRESS_HPP

#include <compare>    // For three-way comparison (C++20)
#include <format>     // For std::format (C++20)
#include <memory>     // For smart pointers
#include <stdexcept>  // For custom exceptions
#include <string>
#include <string_view>  // For efficient string passing

namespace atom::web {

// Custom exceptions for better error handling
class AddressException : public std::runtime_error {
public:
    explicit AddressException(const std::string& message)
        : std::runtime_error(message) {}

    explicit AddressException(std::string_view message)
        : std::runtime_error(std::string(message)) {}
};

class InvalidAddressFormat : public AddressException {
public:
    explicit InvalidAddressFormat(const std::string& message)
        : AddressException("Invalid address format: " + message) {}

    explicit InvalidAddressFormat(std::string_view message)
        : AddressException(std::format("Invalid address format: {}", message)) {
    }
};

class AddressRangeError : public AddressException {
public:
    explicit AddressRangeError(const std::string& message)
        : AddressException("Address range error: " + message) {}

    explicit AddressRangeError(std::string_view message)
        : AddressException(std::format("Address range error: {}", message)) {}
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
     * @brief Three-way comparison operator (C++20).
     * @param other The other address to compare with.
     * @return Comparison result.
     */
    [[nodiscard]] virtual auto operator<=>(const Address& other) const
        -> std::partial_ordering {
        if (getType() != other.getType()) {
            return std::partial_ordering::unordered;
        }
        return addressStr <=> std::string(other.getAddress());
    }

    /**
     * @brief Equality operator.
     * @param other The other address to compare with.
     * @return True if addresses are equal.
     */
    [[nodiscard]] virtual auto operator==(const Address& other) const -> bool {
        return isEqual(other);
    }

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
     * @brief Formats the address for output.
     * @return Formatted string representation.
     */
    [[nodiscard]] virtual auto toString() const -> std::string {
        return std::format("{}({})", std::string(getType()), addressStr);
    }

    /**
     * @brief Check if address is valid (non-empty).
     * @return True if address is valid.
     */
    [[nodiscard]] auto isValid() const noexcept -> bool {
        return !addressStr.empty();
    }

    /**
     * @brief Explicit bool conversion for validity check.
     */
    [[nodiscard]] explicit operator bool() const noexcept { return isValid(); }

    /**
     * @brief Creates an address object from a string.
     * @param addressStr The address string.
     * @return A unique_ptr to an Address object or nullptr if the address type
     * cannot be determined.
     */
    static auto createFromString(std::string_view addressStr)
        -> std::unique_ptr<Address>;
};

}  // namespace atom::web

#endif  // ATOM_WEB_ADDRESS_ADDRESS_HPP
