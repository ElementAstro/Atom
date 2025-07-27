#ifndef ATOM_SYSINFO_OS_HPP_COMPAT
#define ATOM_SYSINFO_OS_HPP_COMPAT

/**
 * @file os.hpp
 * @brief Backward compatibility header for OS module
 * 
 * This header provides backward compatibility for existing code that includes
 * the OS module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 * 
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/os/os.hpp"
 */

// Include the new modular implementation
#include "../../src/os/os.hpp"

namespace atom::system {

// Import all OS types and functions from the new namespace
using namespace sysinfo::os;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_OS_HPP
#define ATOM_SYSTEM_MODULE_OS_HPP
#endif

#endif  // ATOM_SYSINFO_OS_HPP_COMPAT
