#ifndef ATOM_SYSINFO_BATTERY_HPP_COMPAT
#define ATOM_SYSINFO_BATTERY_HPP_COMPAT

/**
 * @file battery.hpp
 * @brief Backward compatibility header for battery module
 * 
 * This header provides backward compatibility for existing code that includes
 * the battery module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 * 
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/battery/battery.hpp"
 */

// Include the new modular implementation
#include "../../../battery/battery.hpp"

namespace atom::system {

// Import all types and functions from the battery namespace
// The battery module is already in atom::system::battery namespace
using namespace battery;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_BATTERY_HPP
#define ATOM_SYSTEM_MODULE_BATTERY_HPP
#endif

#endif  // ATOM_SYSINFO_BATTERY_HPP_COMPAT
