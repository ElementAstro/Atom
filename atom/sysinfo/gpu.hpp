/**
 * @file gpu.hpp
 * @brief GPU information functionality (compatibility header)
 *
 * This file serves as a compatibility header that includes the modular GPU
 * system. It maintains backward compatibility with existing code that includes
 * this header while providing access to the enhanced GPU functionality.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_GPU_COMPAT_HPP
#define ATOM_SYSTEM_MODULE_GPU_COMPAT_HPP

// Include the new modular GPU system
#include "src/gpu/gpu.hpp"

// For backward compatibility, also include common utilities
#include "src/gpu/common.hpp"

// Legacy namespace alias for backward compatibility
namespace atom::system {

// Legacy MonitorInfo structure is now defined in gpu/gpu.hpp
// All functions are now defined in gpu/gpu.hpp

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_GPU_COMPAT_HPP
