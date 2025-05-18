#include "unix_domain.hpp"

#ifdef _WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <bitset>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>

#include "atom/log/loguru.hpp"

namespace atom::web {

#ifdef _WIN32
[[maybe_unused]] constexpr int UNIX_DOMAIN_PATH_MAX_LENGTH = MAX_PATH;
#else
constexpr int UNIX_DOMAIN_PATH_MAX_LENGTH = 108;
#endif

auto UnixDomain::isValidPath(std::string_view path) -> bool {
#ifdef _WIN32
    // Windows命名管道路径格式检查
    static const std::regex namedPipeRegex(R"(\\\\\.\\pipe\\[^\\/:*?"<>|]+)");
    // 检查是否是Windows命名管道格式
    if (std::regex_match(path.begin(), path.end(), namedPipeRegex)) {
        return true;
    }
    // 普通路径检查
    return !path.empty() && path.length() < MAX_PATH;
#else
    return !path.empty() && path.length() < UNIX_DOMAIN_PATH_MAX_LENGTH;
#endif
}

UnixDomain::UnixDomain(std::string_view path) {
    if (!parse(path)) {
        throw InvalidAddressFormat(std::string(path));
    }
}

auto UnixDomain::parse(std::string_view path) -> bool {
    try {
        if (!isValidPath(path)) {
            LOG_F(ERROR, "Invalid Unix domain socket path: {}", path);
            return false;
        }

        addressStr = std::string(path);
        return true;
    } catch (const std::exception& e) {
#ifdef _WIN32
        LOG_F(ERROR, "Exception parsing Windows Named Pipe path: {}", e.what());
#else
        LOG_F(ERROR, "Exception parsing Unix domain socket path: {}", e.what());
#endif
        return false;
    }
}

void UnixDomain::printAddressType() const {
#ifdef _WIN32
    LOG_F(INFO, "Address type: Windows Named Pipe or Unix Domain Socket");
#else
    LOG_F(INFO, "Address type: Unix Domain Socket");
#endif
}

auto UnixDomain::isInRange(std::string_view start, std::string_view end)
    -> bool {
    try {
        // For Unix domain sockets, we'll consider lexicographical ordering of
        // paths This is different from IP ranges but provides a consistent
        // behavior

        if (start.empty() || end.empty()) {
            throw AddressRangeError("Empty range boundaries");
        }

        // Check if the range is valid (start <= end lexicographically)
        if (start > end) {
            throw AddressRangeError("Invalid range: start path > end path");
        }

        // Check if the current path is in the lexicographical range
        return (addressStr >= start) && (addressStr <= end);
    } catch (const AddressRangeError& e) {
        LOG_F(ERROR, "Range error for Unix domain socket: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket range check: {}", e.what());
        return false;
    }
}

auto UnixDomain::toBinary() const -> std::string {
    try {
        std::string binaryStr;
        for (unsigned char c : addressStr) {
            std::bitset<8> bits(c);
            binaryStr += bits.to_string();
        }
        return binaryStr;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket binary conversion: {}", e.what());
        return "";
    }
}

auto UnixDomain::toHex() const -> std::string {
    try {
        std::stringstream ss;
        ss << std::hex;
        for (unsigned char c : addressStr) {
            ss << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
        return ss.str();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket hex conversion: {}", e.what());
        return "";
    }
}

auto UnixDomain::isEqual(const Address& other) const -> bool {
    try {
        if (other.getType() != "UnixDomain") {
            return false;
        }

        const UnixDomain* unixDomainOther = dynamic_cast<const UnixDomain*>(&other);
        if (!unixDomainOther) {
            return false;
        }

        return addressStr == unixDomainOther->addressStr;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket equality check: {}", e.what());
        return false;
    }
}

auto UnixDomain::getType() const -> std::string_view { return "UnixDomain"; }

auto UnixDomain::getNetworkAddress([[maybe_unused]] std::string_view mask) const
    -> std::string {
    // Unix域套接字没有网络地址的概念，但我们可以返回路径的目录部分作为"网络"
    try {
        size_t pos = addressStr.find_last_of("/\\");
        if (pos == std::string::npos) {
            return "";
        }
        return addressStr.substr(0, pos + 1);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket network address calculation: {}", e.what());
        return "";
    }
}

auto UnixDomain::getBroadcastAddress(
    [[maybe_unused]] std::string_view mask) const -> std::string {
    // Unix域套接字没有广播地址的概念，但我们可以返回目录下的通配符路径
    try {
        size_t pos = addressStr.find_last_of("/\\");
        if (pos == std::string::npos) {
            return addressStr + "/*";
        }
        return addressStr.substr(0, pos + 1) + "*";
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket broadcast address calculation: {}", e.what());
        return "";
    }
}

auto UnixDomain::isSameSubnet(const Address& other,
                              [[maybe_unused]] std::string_view mask) const
    -> bool {
    // 对于Unix域套接字，我们可以检查两个路径是否在同一目录
    try {
        if (other.getType() != "UnixDomain") {
            return false;
        }

        const UnixDomain* unixDomainOther = dynamic_cast<const UnixDomain*>(&other);
        if (!unixDomainOther) {
            return false;
        }

        // 获取两个路径的目录部分
        size_t pos1 = addressStr.find_last_of("/\\");
        size_t pos2 = unixDomainOther->addressStr.find_last_of("/\\");

        // 如果任一路径没有目录部分，则不在同一子网
        if (pos1 == std::string::npos || pos2 == std::string::npos) {
            return false;
        }

        return addressStr.substr(0, pos1) == unixDomainOther->addressStr.substr(0, pos2);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in Unix domain socket subnet check: {}", e.what());
        return false;
    }
}

}  // namespace atom::web
