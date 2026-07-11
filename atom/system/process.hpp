/**
 * @file process.hpp
 * @brief Backwards compatibility header for process management.
 *
 * @deprecated This header location is deprecated. Please use
 * "atom/system/process/process.hpp" instead.
 */

// NOTE: this compatibility shim must NOT reuse the ATOM_SYSTEM_PROCESS_HPP
// guard of the real header it forwards to — doing so would define the guard
// first and suppress the real header entirely.
#ifndef ATOM_SYSTEM_PROCESS_COMPAT_HPP
#define ATOM_SYSTEM_PROCESS_COMPAT_HPP

// Forward to the new location
#include "process/process.hpp"

#endif  // ATOM_SYSTEM_PROCESS_COMPAT_HPP
