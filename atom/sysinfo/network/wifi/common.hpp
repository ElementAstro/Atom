/*
 * common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Common WiFi Definitions

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_COMMON_HPP
#define ATOM_SYSTEM_MODULE_WIFI_COMMON_HPP

#include <string>
#include <vector>

// Platform-specific includes
#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <iptypes.h>
#include <wlanapi.h>
#include <ws2tcpip.h>
#include <pdh.h>
#include <icmpapi.h>
#undef max
#undef min
// clang-format on
#if !defined(__MINGW32__) && !defined(__MINGW64__)
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "pdh.lib")
#endif
#elif defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/CaptiveNetwork.h>
#endif

#include "atom/log/loguru.hpp"

// Define common types for cross-platform use
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
using IF_ADDRS = PIP_ADAPTER_ADDRESSES;
using IF_ADDRS_UNICAST = PIP_ADAPTER_UNICAST_ADDRESS;
#else
using IF_ADDRS = struct ifaddrs*;
using IF_ADDRS_UNICAST = struct ifaddrs*;
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

namespace atom::system {

// Free addresses helper function
void freeAddresses(IF_ADDRS addrs);

// Get addresses helper function
auto getAddresses(int family, IF_ADDRS* addrs) -> int;

// Helper function to measure ping latency
float measurePing(const std::string& host, int timeout);

// Template function to get IP addresses by family
template <typename AddressType>
auto getIPAddresses(int addressFamily) -> std::vector<std::string>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_WIFI_COMMON_HPP
