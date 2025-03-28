/*
 * utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-17

Description: Network Utils

**************************************************/

#ifndef ATOM_WEB_UTILS_HPP
#define ATOM_WEB_UTILS_HPP

#include <chrono>
#include <concepts>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <netdb.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#endif

namespace atom::web {

// C++20 concept to ensure a type is a valid port number
template <typename T>
concept PortNumber = std::integral<T> && requires(T port) {
    { port >= 0 && port <= 65535 } -> std::same_as<bool>;
};

/**
 * @brief Initialize networking subsystem (Windows-specific)
 * 初始化网络子系统（Windows特定）
 *
 * @return true if initialization succeeded, false otherwise
 * @throws std::runtime_error if initialization fails with a specific error
 * message
 */
auto initializeWindowsSocketAPI() -> bool;

/**
 * @brief Check if a port is in use.
 * 检查端口是否正在使用。
 *
 * This function checks if a port is in use by attempting to bind a socket to
 * the port. If the socket can be bound, the port is not in use.
 * 该函数通过尝试将套接字绑定到端口来检查端口是否正在使用。如果套接字可以绑定，则端口未被使用。
 *
 * @param port The port number to check. 要检查的端口号。
 * @return `true` if the port is in use, `false` otherwise.
 * 如果端口正在使用，则返回`true`，否则返回`false`。
 *
 * @code
 * if (atom::web::isPortInUse(8080)) {
 *     std::cout << "Port 8080 is in use." << std::endl;
 * } else {
 *     std::cout << "Port 8080 is available." << std::endl;
 * }
 * @endcode
 *
 * @throws std::invalid_argument if port is outside valid range
 * @throws std::runtime_error if socket operations fail
 */
auto isPortInUse(PortNumber auto port) -> bool;

/**
 * @brief Check if there is any program running on the specified port and kill
 * it if found. 检查指定端口上是否有程序正在运行，如果找到则终止该程序。
 *
 * This function checks if there is any program running on the specified port by
 * querying the system. If a program is found, it will be terminated.
 * 该函数通过查询系统检查指定端口上是否有程序正在运行。如果找到程序，将终止它。
 *
 * @param port The port number to check. 要检查的端口号。
 * @return `true` if a program was found and terminated, `false` otherwise.
 * 如果找到并终止了程序，则返回`true`；否则返回`false`。
 *
 * @code
 * if (atom::web::checkAndKillProgramOnPort(8080)) {
 *     std::cout << "Program on port 8080 was terminated." << std::endl;
 * } else {
 *     std::cout << "No program running on port 8080." << std::endl;
 * }
 * @endcode
 *
 * @throws std::invalid_argument if port is outside valid range
 * @throws std::runtime_error if socket operations fail
 * @throws std::system_error if process termination fails
 */
auto checkAndKillProgramOnPort(PortNumber auto port) -> bool;

/**
 * @brief Get the process ID of the program running on a specific port
 * 获取在特定端口上运行的程序的进程ID
 *
 * @param port The port number to check
 * @return std::optional<int> The process ID if found, empty optional otherwise
 * @throws std::invalid_argument if port is outside valid range
 * @throws std::runtime_error if command execution fails
 */
auto getProcessIDOnPort(PortNumber auto port) -> std::optional<int>;

/**
 * @brief Asynchronously check if a port is in use
 * 异步检查端口是否在使用中
 *
 * @param port The port number to check
 * @return std::future<bool> Future result of the check
 */
auto isPortInUseAsync(PortNumber auto port) -> std::future<bool>;

/**
 * @brief Scan a specific port on a given host to check if it's open
 * 扫描指定主机上的特定端口，检查是否开放
 *
 * @param host The hostname or IP address to scan
 * @param port The port number to scan
 * @param timeout The maximum time to wait for a connection (default: 2 seconds)
 * @return true if the port is open, false otherwise
 */
auto scanPort(const std::string& host, uint16_t port,
              std::chrono::milliseconds timeout =
                  std::chrono::milliseconds(2000)) -> bool;

/**
 * @brief Scan a range of ports on a given host to find open ones
 * 扫描指定主机上的端口范围，查找开放的端口
 *
 * @param host The hostname or IP address to scan
 * @param startPort The beginning of the port range to scan
 * @param endPort The end of the port range to scan
 * @param timeout The maximum time to wait for each connection attempt
 * @return std::vector<uint16_t> List of open ports
 */
auto scanPortRange(
    const std::string& host, uint16_t startPort, uint16_t endPort,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    -> std::vector<uint16_t>;

/**
 * @brief Asynchronously scan a range of ports on a given host
 * 异步扫描指定主机上的端口范围
 *
 * @param host The hostname or IP address to scan
 * @param startPort The beginning of the port range to scan
 * @param endPort The end of the port range to scan
 * @param timeout The maximum time to wait for each connection attempt
 * @return std::future<std::vector<uint16_t>> Future result containing list of
 * open ports
 */
auto scanPortRangeAsync(
    const std::string& host, uint16_t startPort, uint16_t endPort,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    -> std::future<std::vector<uint16_t>>;

/**
 * @brief Get IP addresses for a given hostname through DNS resolution
 * 通过DNS解析获取指定主机名的IP地址
 *
 * @param hostname The hostname to resolve
 * @return std::vector<std::string> List of IP addresses
 */
auto getIPAddresses(const std::string& hostname) -> std::vector<std::string>;

/**
 * @brief Get all local IP addresses of the machine
 * 获取本机的所有本地IP地址
 *
 * @return std::vector<std::string> List of local IP addresses
 */
auto getLocalIPAddresses() -> std::vector<std::string>;

/**
 * @brief Check if the device has active internet connectivity
 * 检查设备是否有活跃的互联网连接
 *
 * @return true if internet is available, false otherwise
 */
auto checkInternetConnectivity() -> bool;

/**
 * @brief Dump address information from source to destination.
 * 将地址信息从源转储到目标。
 *
 * This function copies address information from the source to the destination.
 * 该函数将地址信息从源复制到目标。
 *
 * @param dst Destination address information. 目标地址信息。
 * @param src Source address information. 源地址信息。
 * @return `0` on success, `-1` on failure. 成功返回`0`，失败返回`-1`。
 *
 * @throws std::invalid_argument if src is nullptr
 * @throws std::runtime_error if memory allocation fails
 *
 * @code
 * struct addrinfo* src = ...;
 * std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> dst(nullptr,
 * freeaddrinfo); if (atom::web::dumpAddrInfo(&dst, src) == 0) { std::cout <<
 * "Address information dumped successfully." << std::endl; } else { std::cout
 * << "Failed to dump address information." << std::endl;
 * }
 * @endcode
 */
auto dumpAddrInfo(
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>& dst,
    const struct addrinfo* src) -> int;

/**
 * @brief Convert address information to string.
 * 将地址信息转换为字符串。
 *
 * This function converts address information to a string representation.
 * 该函数将地址信息转换为字符串表示。
 *
 * @param addrInfo Address information. 地址信息。
 * @param jsonFormat If `true`, output in JSON format.
 * 如果为`true`，则以JSON格式输出。
 * @return String representation of address information. 地址信息的字符串表示。
 *
 * @code
 * struct addrinfo* addrInfo = ...;
 * std::string addrStr = atom::web::addrInfoToString(addrInfo, true);
 * std::cout << addrStr << std::endl;
 * @endcode
 *
 * @throws std::invalid_argument if addrInfo is nullptr
 */
auto addrInfoToString(const struct addrinfo* addrInfo, bool jsonFormat = false)
    -> std::string;

/**
 * @brief Get address information for a given hostname and service.
 * 获取给定主机名和服务的地址信息。
 *
 * This function retrieves address information for a given hostname and service.
 * 该函数检索给定主机名和服务的地址信息。
 *
 * @param hostname The hostname to resolve. 要解析的主机名。
 * @param service The service to resolve. 要解析的服务。
 * @return Smart pointer to the address information. 地址信息的智能指针。
 *
 * @throws std::runtime_error if getaddrinfo fails
 * @throws std::invalid_argument if hostname or service is empty
 *
 * @code
 * try {
 *     auto addrInfo = atom::web::getAddrInfo("www.google.com", "http");
 *     std::cout << "Address information retrieved successfully." << std::endl;
 *     // No need to manually free, handled by smart pointer
 * } catch (const std::exception& e) {
 *     std::cout << "Error: " << e.what() << std::endl;
 * }
 * @endcode
 */
auto getAddrInfo(const std::string& hostname, const std::string& service)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>;

/**
 * @brief Compare two address information structures.
 * 比较两个地址信息结构。
 *
 * This function compares two address information structures for equality.
 * 该函数比较两个地址信息结构是否相等。
 *
 * @param addrInfo1 First address information structure. 第一个地址信息结构。
 * @param addrInfo2 Second address information structure. 第二个地址信息结构。
 * @return `true` if the structures are equal, `false` otherwise.
 * 如果结构相等，则返回`true`，否则返回`false`。
 *
 * @throws std::invalid_argument if either addrInfo1 or addrInfo2 is nullptr
 *
 * @code
 * struct addrinfo* addrInfo1 = ...;
 * struct addrinfo* addrInfo2 = ...;
 * if (atom::web::compareAddrInfo(addrInfo1, addrInfo2)) {
 *     std::cout << "Address information structures are equal." << std::endl;
 * } else {
 *     std::cout << "Address information structures are not equal." <<
 * std::endl;
 * }
 * @endcode
 */
auto compareAddrInfo(const struct addrinfo* addrInfo1,
                     const struct addrinfo* addrInfo2) -> bool;

/**
 * @brief Filter address information by family.
 * 按家庭过滤地址信息。
 *
 * This function filters address information by the specified family.
 * 该函数按指定的家庭过滤地址信息。
 *
 * @param addrInfo Address information to filter. 要过滤的地址信息。
 * @param family The family to filter by (e.g., AF_INET).
 * 要过滤的家庭（例如，AF_INET）。
 * @return Filtered address information (smart pointer).
 * 过滤后的地址信息（智能指针）。
 *
 * @throws std::invalid_argument if addrInfo is nullptr
 *
 * @code
 * try {
 *     auto addrInfo = atom::web::getAddrInfo("www.google.com", "http");
 *     auto filtered = atom::web::filterAddrInfo(addrInfo.get(), AF_INET);
 *     if (filtered) {
 *         std::cout << "Filtered address information retrieved successfully."
 * << std::endl; } else { std::cout << "No address information matched the
 * filter." << std::endl;
 *     }
 * } catch (const std::exception& e) {
 *     std::cout << "Error: " << e.what() << std::endl;
 * }
 * @endcode
 */
auto filterAddrInfo(const struct addrinfo* addrInfo, int family)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>;

/**
 * @brief Sort address information by family.
 * 按家庭排序地址信息。
 *
 * This function sorts address information by family.
 * 该函数按家庭排序地址信息。
 *
 * @param addrInfo Address information to sort. 要排序的地址信息。
 * @return Sorted address information (smart pointer).
 * 排序后的地址信息（智能指针）。
 *
 * @throws std::invalid_argument if addrInfo is nullptr
 *
 * @code
 * try {
 *     auto addrInfo = atom::web::getAddrInfo("www.google.com", "http");
 *     auto sorted = atom::web::sortAddrInfo(addrInfo.get());
 *     if (sorted) {
 *         std::cout << "Sorted address information retrieved successfully." <<
 * std::endl; } else { std::cout << "Failed to sort address information." <<
 * std::endl;
 *     }
 * } catch (const std::exception& e) {
 *     std::cout << "Error: " << e.what() << std::endl;
 * }
 * @endcode
 */
auto sortAddrInfo(const struct addrinfo* addrInfo)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_HPP
