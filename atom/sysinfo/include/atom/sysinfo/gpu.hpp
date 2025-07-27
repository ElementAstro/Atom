#ifndef ATOM_SYSINFO_GPU_HPP_COMPAT
#define ATOM_SYSINFO_GPU_HPP_COMPAT

/**
 * @file gpu.hpp
 * @brief Backward compatibility header for GPU module
 * 
 * This header provides backward compatibility for existing code that includes
 * the GPU module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 * 
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/gpu/gpu.hpp"
 */

// Include the new modular implementation
#include "../../src/gpu/gpu.hpp"

namespace atom::system {

// Import all GPU types and functions from the new namespace
using namespace sysinfo::gpu;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_GPU_HPP
#define ATOM_SYSTEM_MODULE_GPU_HPP
#endif

#endif  // ATOM_SYSINFO_GPU_HPP_COMPAT
