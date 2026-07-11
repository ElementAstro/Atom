/**
 * @file platform.hpp
 * @brief Backwards compatibility header for platform detection macros.
 *
 * This header provides backwards compatibility for existing code that includes
 * "atom/system/platform.hpp". The actual implementation has been moved to
 * "atom/system/core/platform.hpp" for better organization.
 *
 * @deprecated This header location is deprecated. Please use
 * "atom/system/core/platform.hpp" instead.
 */

#ifndef ATOM_SYSTEM_PLATFORM_COMPAT_HPP
#define ATOM_SYSTEM_PLATFORM_COMPAT_HPP

// Forward to the new location
#include "core/platform.hpp"

#endif  // ATOM_SYSTEM_PLATFORM_COMPAT_HPP
