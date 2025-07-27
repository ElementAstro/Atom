#ifndef ATOM_SYSINFO_WM_HPP_COMPAT
#define ATOM_SYSINFO_WM_HPP_COMPAT

/**
 * @file wm.hpp
 * @brief Backward compatibility header for window manager module
 *
 * This header provides backward compatibility for existing code that includes
 * the window manager module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 *
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/wm/wm.hpp"
 */

// Include the new modular implementation
#include "../../src/wm/wm.hpp"

namespace atom::system {

// Import all window manager types and functions from the new namespace
using namespace sysinfo::wm;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_WM_HPP
#define ATOM_SYSTEM_MODULE_WM_HPP
#endif

#endif  // ATOM_SYSINFO_WM_HPP_COMPAT
