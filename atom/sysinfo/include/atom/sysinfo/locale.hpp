#ifndef ATOM_SYSINFO_LOCALE_HPP_COMPAT
#define ATOM_SYSINFO_LOCALE_HPP_COMPAT

/**
 * @file locale.hpp
 * @brief Backward compatibility header for locale module
 *
 * This header provides backward compatibility for existing code that includes
 * the locale module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 *
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/locale/locale.hpp"
 */

// Include the new modular implementation
#include "../../src/locale/locale.hpp"

namespace atom::system {

// Import all locale types and functions from the new namespace
using namespace sysinfo::locale;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_LOCALE_HPP
#define ATOM_SYSTEM_MODULE_LOCALE_HPP
#endif

#endif  // ATOM_SYSINFO_LOCALE_HPP_COMPAT
