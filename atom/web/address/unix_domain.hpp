#ifndef ATOM_WEB_ADDRESS_UNIX_DOMAIN_HPP
#define ATOM_WEB_ADDRESS_UNIX_DOMAIN_HPP

#include <compare>
#include <filesystem>
#include "address.hpp"

namespace atom::web {

/**
 * @class UnixDomain
 * @brief A class representing a Unix domain socket address.
 */
class UnixDomain : public Address {
public:
#ifdef _WIN32
    static constexpr size_t MAX_PATH_LENGTH = 260;  // MAX_PATH
#else
    static constexpr size_t MAX_PATH_LENGTH = 108;  // Unix socket path limit
#endif

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
     *
     * For Unix domain sockets, this performs lexicographical comparison of
     * paths.
     *
     * @param start The start of the range.
     * @param end The end of the range.
     * @return True if the path is within the lexicographical range, false
     * otherwise.
     * @override
     */
    auto isInRange(std::string_view start,
                   std::string_view end) -> bool override;

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
     * @return A string_view representing the address type ("UnixDomain").
     * @override
     */
    [[nodiscard]] auto getType() const -> std::string_view override;

    /**
     * @brief Gets the network address given a subnet mask.
     *
     * For Unix domain sockets, returns the directory portion of the path.
     *
     * @param mask The subnet mask (not applicable for Unix domain sockets).
     * @return The directory portion of the path.
     * @override
     */
    [[nodiscard]] auto getNetworkAddress(std::string_view mask) const
        -> std::string override;

    /**
     * @brief Gets the broadcast address given a subnet mask.
     *
     * For Unix domain sockets, returns a wildcard pattern in the same
     * directory.
     *
     * @param mask The subnet mask (not applicable for Unix domain sockets).
     * @return A wildcard pattern in the directory.
     * @override
     */
    [[nodiscard]] auto getBroadcastAddress(std::string_view mask) const
        -> std::string override;

    /**
     * @brief Checks if two addresses are in the same subnet.
     *
     * For Unix domain sockets, checks if paths are in the same directory.
     *
     * @param other The other address to compare with.
     * @param mask The subnet mask (not applicable for Unix domain sockets).
     * @return True if paths are in the same directory, false otherwise.
     * @override
     */
    [[nodiscard]] auto isSameSubnet(
        const Address& other, std::string_view mask) const -> bool override;

    /**
     * @brief Converts the Unix domain socket address to its hexadecimal
     * representation.
     * @return The hexadecimal representation of the address as a string.
     * @override
     */
    [[nodiscard]] auto toHex() const -> std::string override;

    /**
     * @brief Three-way comparison for Unix domain socket addresses.
     * @param other The other address to compare.
     * @return Comparison result.
     */
    [[nodiscard]] auto operator<=>(const Address& other) const
        -> std::partial_ordering override;

    /**
     * @brief Get the path as a filesystem path object.
     * @return The path as std::filesystem::path.
     */
    [[nodiscard]] auto getPath() const -> std::filesystem::path {
        return std::filesystem::path(addressStr);
    }

    /**
     * @brief Check if the socket file exists.
     * @return True if the socket file exists.
     */
    [[nodiscard]] auto exists() const -> bool;

    /**
     * @brief Check if this is an abstract socket (Linux-specific).
     * @return True if abstract socket (path starts with null byte).
     */
    [[nodiscard]] auto isAbstract() const -> bool;

    /**
     * @brief Get the filename portion of the path.
     * @return The filename.
     */
    [[nodiscard]] auto getFilename() const -> std::string;

    /**
     * @brief Get the parent directory of the socket path.
     * @return The parent directory path.
     */
    [[nodiscard]] auto getParentPath() const -> std::string;

    /**
     * @brief Validates a Unix domain socket path.
     * @param path The path to validate.
     * @return True if the path is valid, false otherwise.
     */
    [[nodiscard]] static auto isValidPath(std::string_view path) -> bool;

private:
    /**
     * @brief Fast path validation without regex.
     * @param path The path to validate.
     * @return True if the path is valid, false otherwise.
     */
    [[nodiscard]] static auto fastIsValidPath(std::string_view path) -> bool;

    /**
     * @brief Gets the directory portion of a path.
     * @param path The full path.
     * @return The directory portion, or empty string if no directory.
     */
    [[nodiscard]] static auto getDirectoryPath(std::string_view path)
        -> std::string;

    /**
     * @brief Create a Unix domain socket address from a filesystem path.
     * @param path The filesystem path.
     * @return UnixDomain address.
     */
    [[nodiscard]] static auto fromPath(const std::filesystem::path& path)
        -> UnixDomain;

#ifndef _WIN32
    /**
     * @brief Create an abstract socket address (Linux-specific).
     * @param name The abstract socket name (without leading null byte).
     * @return UnixDomain address.
     */
    [[nodiscard]] static auto createAbstract(std::string_view name)
        -> UnixDomain;
#endif
};

}  // namespace atom::web

#endif  // ATOM_WEB_ADDRESS_UNIX_DOMAIN_HPP
