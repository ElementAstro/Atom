#include "unix_domain.hpp"

#include <algorithm>
#include <bitset>
#include <iomanip>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>

namespace atom::web {

namespace {
#ifdef _WIN32
constexpr size_t UNIX_DOMAIN_PATH_MAX_LENGTH = 260;  // MAX_PATH
constexpr std::string_view NAMED_PIPE_PREFIX = "\\\\.\\pipe\\";
#else
constexpr size_t UNIX_DOMAIN_PATH_MAX_LENGTH = 108;
#endif

constexpr std::string_view INVALID_PATH_CHARS = "<>:\"|?*";

/**
 * @brief Check if character is valid for a Unix domain socket path
 */
auto isValidPathChar(char c) -> bool {
    if (c < 32 || c > 126) {
        return false;
    }

    return INVALID_PATH_CHARS.find(c) == std::string_view::npos;
}

#ifdef _WIN32
/**
 * @brief Validate Windows named pipe path format
 */
auto isValidNamedPipePath(std::string_view path) -> bool {
    if (!path.starts_with(NAMED_PIPE_PREFIX)) {
        return false;
    }

    auto pipeName = path.substr(NAMED_PIPE_PREFIX.length());
    if (pipeName.empty()) {
        return false;
    }

    return std::all_of(pipeName.begin(), pipeName.end(),
                       [](char c) { return isValidPathChar(c) && c != '\\'; });
}
#endif
}  // namespace

auto UnixDomain::fastIsValidPath(std::string_view path) -> bool {
    if (path.empty() || path.length() >= UNIX_DOMAIN_PATH_MAX_LENGTH) {
        return false;
    }

#ifdef _WIN32
    if (path.starts_with(NAMED_PIPE_PREFIX)) {
        return isValidNamedPipePath(path);
    }

    if (path.length() >= 3 && path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return std::all_of(path.begin(), path.end(), isValidPathChar);
    }

    return std::all_of(path.begin(), path.end(), isValidPathChar);
#else
    if (!path.starts_with('/')) {
        return false;
    }

    return std::all_of(path.begin(), path.end(), isValidPathChar);
#endif
}

auto UnixDomain::isValidPath(std::string_view path) -> bool {
    return fastIsValidPath(path);
}

auto UnixDomain::getDirectoryPath(std::string_view path) -> std::string {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string_view::npos) {
        return "";
    }
    return std::string(path.substr(0, pos + 1));
}

UnixDomain::UnixDomain(std::string_view path) {
    if (!parse(path)) {
        throw InvalidAddressFormat(std::string(path));
    }
}

auto UnixDomain::parse(std::string_view path) -> bool {
    try {
        if (!isValidPath(path)) {
            spdlog::error("Invalid Unix domain socket path: {}", path);
            return false;
        }

        addressStr = std::string(path);
        spdlog::trace("Successfully parsed Unix domain socket path: {}", path);
        return true;

    } catch (const std::exception& e) {
#ifdef _WIN32
        spdlog::error("Exception parsing Windows Named Pipe path '{}': {}",
                      path, e.what());
#else
        spdlog::error("Exception parsing Unix domain socket path '{}': {}",
                      path, e.what());
#endif
        return false;
    }
}

void UnixDomain::printAddressType() const {
#ifdef _WIN32
    spdlog::info("Address type: Windows Named Pipe or Unix Domain Socket");
#else
    spdlog::info("Address type: Unix Domain Socket");
#endif
}

auto UnixDomain::isInRange(std::string_view start, std::string_view end)
    -> bool {
    try {
        if (start.empty() || end.empty()) {
            throw AddressRangeError("Empty range boundaries");
        }

        if (start > end) {
            throw AddressRangeError("Invalid range: start path > end path");
        }

        bool inRange = (addressStr >= start) && (addressStr <= end);
        spdlog::trace("Unix domain socket range check: {} in [{}, {}] = {}",
                      addressStr, start, end, inRange);
        return inRange;

    } catch (const AddressRangeError& e) {
        spdlog::error("Range error for Unix domain socket: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception in Unix domain socket range check: {}",
                      e.what());
        return false;
    }
}

auto UnixDomain::toBinary() const -> std::string {
    try {
        std::string binaryStr;
        binaryStr.reserve(addressStr.length() * 8);

        for (unsigned char c : addressStr) {
            std::bitset<8> bits(c);
            binaryStr += bits.to_string();
        }

        return binaryStr;
    } catch (const std::exception& e) {
        spdlog::error("Exception in Unix domain socket binary conversion: {}",
                      e.what());
        return "";
    }
}

auto UnixDomain::toHex() const -> std::string {
    try {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        for (unsigned char c : addressStr) {
            oss << std::setw(2) << static_cast<unsigned int>(c);
        }

        return oss.str();
    } catch (const std::exception& e) {
        spdlog::error("Exception in Unix domain socket hex conversion: {}",
                      e.what());
        return "";
    }
}

auto UnixDomain::isEqual(const Address& other) const -> bool {
    try {
        if (other.getType() != "UnixDomain") {
            return false;
        }

        const auto* unixDomainOther = dynamic_cast<const UnixDomain*>(&other);
        if (!unixDomainOther) {
            return false;
        }

        return addressStr == unixDomainOther->addressStr;
    } catch (const std::exception& e) {
        spdlog::error("Exception in Unix domain socket equality check: {}",
                      e.what());
        return false;
    }
}

auto UnixDomain::getType() const -> std::string_view { return "UnixDomain"; }

auto UnixDomain::getNetworkAddress([[maybe_unused]] std::string_view mask) const
    -> std::string {
    try {
        std::string directory = getDirectoryPath(addressStr);
        spdlog::trace("Unix domain socket network address (directory): {}",
                      directory);
        return directory;
    } catch (const std::exception& e) {
        spdlog::error(
            "Exception in Unix domain socket network address calculation: {}",
            e.what());
        return "";
    }
}

auto UnixDomain::getBroadcastAddress(
    [[maybe_unused]] std::string_view mask) const -> std::string {
    try {
        std::string directory = getDirectoryPath(addressStr);
        std::string broadcast = directory.empty() ? "*" : directory + "*";
        spdlog::trace("Unix domain socket broadcast address (wildcard): {}",
                      broadcast);
        return broadcast;
    } catch (const std::exception& e) {
        spdlog::error(
            "Exception in Unix domain socket broadcast address calculation: {}",
            e.what());
        return "";
    }
}

auto UnixDomain::isSameSubnet(const Address& other,
                              [[maybe_unused]] std::string_view mask) const
    -> bool {
    try {
        if (other.getType() != "UnixDomain") {
            return false;
        }

        const auto* unixDomainOther = dynamic_cast<const UnixDomain*>(&other);
        if (!unixDomainOther) {
            return false;
        }

        std::string directory1 = getDirectoryPath(addressStr);
        std::string directory2 = getDirectoryPath(unixDomainOther->addressStr);

        if (directory1.empty() || directory2.empty()) {
            return false;
        }

        bool sameSubnet = directory1 == directory2;
        spdlog::trace(
            "Unix domain socket subnet check: {} and {} in same directory: {}",
            addressStr, unixDomainOther->addressStr, sameSubnet);
        return sameSubnet;

    } catch (const std::exception& e) {
        spdlog::error("Exception in Unix domain socket subnet check: {}",
                      e.what());
        return false;
    }
}

}  // namespace atom::web
