#ifndef ATOM_SYSINFO_PRINTER_HPP_COMPAT
#define ATOM_SYSINFO_PRINTER_HPP_COMPAT

/**
 * @file sysinfo_printer.hpp
 * @brief Backward compatibility header for system info printer module
 * 
 * This header provides backward compatibility for existing code that includes
 * the system info printer module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 * 
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/printer/printer.hpp"
 * 
 * @note The module has been renamed from 'sysinfo_printer' to 'printer' for clarity.
 */

// Include the new modular implementation (renamed from sysinfo_printer to printer)
#include "../../src/printer/printer.hpp"

namespace atom::system {

// Import all printer types and functions from the new namespace
using namespace sysinfo::printer;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_SYSINFO_PRINTER_HPP
#define ATOM_SYSTEM_MODULE_SYSINFO_PRINTER_HPP
#endif

#endif  // ATOM_SYSINFO_PRINTER_HPP_COMPAT
