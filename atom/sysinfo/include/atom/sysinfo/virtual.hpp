#ifndef ATOM_SYSINFO_VIRTUAL_HPP_COMPAT
#define ATOM_SYSINFO_VIRTUAL_HPP_COMPAT

/**
 * @file virtual.hpp
 * @brief Backward compatibility header for virtualization module
 *
 * This header provides backward compatibility for existing code that includes
 * the virtualization module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 *
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/virtual/virtual.hpp"
 */

// Include the new modular implementation
#include "../../src/virtual/virtual.hpp"

namespace atom::system {

// Import all virtualization types and functions from the new namespace
using namespace sysinfo::virtual_env;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_VIRTUAL_HPP
#define ATOM_SYSTEM_MODULE_VIRTUAL_HPP
#endif

#endif  // ATOM_SYSINFO_VIRTUAL_HPP_COMPAT
