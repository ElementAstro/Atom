#ifndef ATOM_SYSINFO_SN_HPP_COMPAT
#define ATOM_SYSINFO_SN_HPP_COMPAT

/**
 * @file sn.hpp
 * @brief Backward compatibility header for serial number module
 * 
 * This header provides backward compatibility for existing code that includes
 * the serial number module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 * 
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/serial/sn.hpp"
 * 
 * @note The module has been renamed from 'sn' to 'serial' for clarity.
 */

// Include the new modular implementation (renamed from sn to serial)
#include "../../src/serial/sn.hpp"

namespace atom::system {

// Import all serial number types and functions from the new namespace
using namespace sysinfo::serial;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_SN_HPP
#define ATOM_SYSTEM_MODULE_SN_HPP
#endif

#endif  // ATOM_SYSINFO_SN_HPP_COMPAT
