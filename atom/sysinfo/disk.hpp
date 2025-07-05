/*
 * disk.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_DISK_HPP
#define ATOM_SYSTEM_MODULE_DISK_HPP

/**
 * @brief Disk module for system information
 *
 * This module provides functionality for retrieving disk information,
 * monitoring disk events, and managing disk security.
 */

// Include all disk submodule headers
#include "atom/sysinfo/disk/disk_device.hpp"
#include "atom/sysinfo/disk/disk_info.hpp"
#include "atom/sysinfo/disk/disk_monitor.hpp"
#include "atom/sysinfo/disk/disk_security.hpp"
#include "atom/sysinfo/disk/disk_types.hpp"
#include "atom/sysinfo/disk/disk_util.hpp"

#endif  // ATOM_SYSTEM_MODULE_DISK_HPP