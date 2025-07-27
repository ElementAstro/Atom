#ifndef ATOM_SYSINFO_WIFI_HPP_COMPAT
#define ATOM_SYSINFO_WIFI_HPP_COMPAT

/**
 * @file wifi.hpp
 * @brief Backward compatibility header for WiFi module
 * 
 * This header provides backward compatibility for existing code that includes
 * the WiFi module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 * 
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/wifi/wifi.hpp"
 */

// Include the new modular implementation
#include "../../src/wifi/wifi.hpp"

namespace atom::system {

// Import all WiFi types and functions from the new namespace
using namespace sysinfo::wifi;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_WIFI_HPP
#define ATOM_SYSTEM_MODULE_WIFI_HPP
#endif

#endif  // ATOM_SYSINFO_WIFI_HPP_COMPAT
