/**
 * @file convert.hpp
 * @brief Backwards compatibility header for conversion utilities.
 *
 * @deprecated This header location is deprecated. Please use
 * "atom/utils/conversion/convert.hpp" instead.
 */

// NOTE: must NOT reuse the ATOM_UTILS_CONVERT_HPP guard of the real header it
// forwards to — defining it first would suppress the real header entirely.
#ifndef ATOM_UTILS_CONVERT_COMPAT_HPP
#define ATOM_UTILS_CONVERT_COMPAT_HPP

// Forward to the new location
#include "conversion/convert.hpp"

#endif  // ATOM_UTILS_CONVERT_COMPAT_HPP
