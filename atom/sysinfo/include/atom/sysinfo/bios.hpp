#ifndef ATOM_SYSINFO_BIOS_HPP_COMPAT
#define ATOM_SYSINFO_BIOS_HPP_COMPAT

/**
 * @file bios.hpp
 * @brief Backward compatibility header for BIOS module
 * 
 * This header provides backward compatibility for existing code that includes
 * the BIOS module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 * 
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/bios/bios.hpp"
 */

// Include the new modular implementation
#include "../../src/bios/bios.hpp"

namespace atom::system {

// Import all BIOS types and functions from the new namespace
using namespace sysinfo::bios;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_BIOS_HPP
#define ATOM_SYSTEM_MODULE_BIOS_HPP
#endif

#endif  // ATOM_SYSINFO_BIOS_HPP_COMPAT
