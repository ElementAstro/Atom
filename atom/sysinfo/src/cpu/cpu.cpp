/*
 * cpu.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - CPU Implementation

**************************************************/

#include "cpu.hpp"
#include "common.hpp"

#ifdef _WIN32
#include "platform/windows.cpp"
#elif defined(__APPLE__)
#include "platform/macos.cpp"
#elif defined(__linux__)
#include "platform/linux.cpp"
#elif defined(__FreeBSD__)
#include "platform/freebsd.cpp"
#else
#error "Unsupported platform"
#endif

namespace atom::sysinfo::cpu {

// Implementation will be provided by platform-specific files
// This file serves as the main entry point and includes the appropriate platform implementation

} // namespace atom::sysinfo::cpu
