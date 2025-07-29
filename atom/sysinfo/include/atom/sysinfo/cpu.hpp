#ifndef ATOM_SYSINFO_CPU_HPP_COMPAT
#define ATOM_SYSINFO_CPU_HPP_COMPAT

/**
 * @file cpu.hpp
 * @brief Backward compatibility header for CPU module
 *
 * This header provides backward compatibility for existing code that includes
 * the CPU module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 *
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/cpu/cpu.hpp"
 */

// Include the new modular implementation
#include "../../src/cpu/cpu.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "atom/macro.hpp"

namespace atom::system {

// Import all CPU types and functions from the new namespace
using namespace sysinfo::cpu;

// Legacy function aliases (if any specific ones are needed)
// Most functions should already be compatible through the using declaration above

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_CPU_HPP
#define ATOM_SYSTEM_MODULE_CPU_HPP
#endif

#endif  // ATOM_SYSINFO_CPU_HPP_COMPAT
