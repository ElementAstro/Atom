#ifndef ATOM_SYSINFO_MEMORY_HPP_COMPAT
#define ATOM_SYSINFO_MEMORY_HPP_COMPAT

/**
 * @file memory.hpp
 * @brief Backward compatibility header for memory module
 *
 * This header provides backward compatibility for existing code that includes
 * the memory module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 *
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/memory/memory.hpp"
 */

// Include the new modular implementation
#include "../../src/memory/memory.hpp"

namespace atom::system {

// Import all memory types and functions from the new namespace
using namespace sysinfo::memory;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_MEMORY_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_HPP
#endif

#endif  // ATOM_SYSINFO_MEMORY_HPP_COMPAT
