/**
 * @file battery.hpp
 * @brief System battery information functionality (compatibility header)
 *
 * This file serves as a compatibility header that includes the reorganized
 * battery system. It maintains backward compatibility with existing code that
 * includes this header.
 *
 * @deprecated This header location is deprecated. Please use
 * "atom/sysinfo/interfaces/battery.hpp" instead.
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_BATTERY_COMPAT_HPP
#define ATOM_SYSTEM_MODULE_BATTERY_COMPAT_HPP

// Forward to the new location
#include "hardware/battery.hpp"

#endif  // ATOM_SYSTEM_MODULE_BATTERY_COMPAT_HPP
