/*
 * common.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Common WiFi Implementations

**************************************************/

#include "common.hpp"

namespace atom::system {

void freeAddresses(IF_ADDRS addrs) {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    HeapFree(GetProcessHeap(), 0, addrs);
#else
    freeifaddrs(addrs);
#endif
}

auto getAddresses(int family, IF_ADDRS* addrs) -> int {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    DWORD rv = 0;
    ULONG bufLen =
        15000;  // recommended size from Windows API docs to avoid error
    ULONG iter = 0;
    do {
        *addrs = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, bufLen);
        if (*addrs == nullptr) {
            LOG_F(ERROR, "HeapAlloc failed");
            return -1;
        }

        rv = GetAdaptersAddresses(family, GAA_FLAG_INCLUDE_PREFIX, NULL, *addrs,
                                  &bufLen);
        if (rv == ERROR_BUFFER_OVERFLOW) {
            freeAddresses(*addrs);
            *addrs = nullptr;
            bufLen = bufLen * 2;  // Double buffer length for the next attempt
        } else {
            break;
        }
        iter++;
    } while ((rv == ERROR_BUFFER_OVERFLOW) && (iter < 3));
    if (rv != NO_ERROR) {
        LOG_F(ERROR, "GetAdaptersAddresses failed");
        return -1;
    }
    return 0;
#else
    (void)family;
    return getifaddrs(addrs);
#endif
}

// Note: The platform-specific implementations of measurePing
// are in their respective platform files

} // namespace atom::system
