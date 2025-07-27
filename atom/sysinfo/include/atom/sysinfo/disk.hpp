#ifndef ATOM_SYSINFO_DISK_HPP_COMPAT
#define ATOM_SYSINFO_DISK_HPP_COMPAT

/**
 * @file disk.hpp
 * @brief Backward compatibility header for disk module
 * 
 * This header provides backward compatibility for existing code that includes
 * the disk module. It forwards to the new modular implementation and
 * provides namespace aliases for seamless migration.
 * 
 * @deprecated This compatibility header will be removed in a future version.
 * Please update your code to use the new include paths:
 * #include "atom/sysinfo/src/disk/disk.hpp"
 */

// Include the new modular implementation
#include "../../src/disk/disk.hpp"

// Include all disk submodule headers for backward compatibility
#include "../../src/disk/components/disk_device.hpp"
#include "../../src/disk/components/disk_info.hpp"
#include "../../src/disk/components/disk_monitor.hpp"
#include "../../src/disk/components/disk_security.hpp"
#include "../../src/disk/components/disk_analytics.hpp"
#include "../../src/disk/components/disk_performance.hpp"
#include "../../src/disk/common/disk_types.hpp"
#include "../../src/disk/common/disk_util.hpp"

namespace atom::system {

// Import all disk types and functions from the new namespace
using namespace sysinfo::disk;

}  // namespace atom::system

// Legacy include guard for existing code
#ifndef ATOM_SYSTEM_MODULE_DISK_HPP
#define ATOM_SYSTEM_MODULE_DISK_HPP
#endif

#endif  // ATOM_SYSINFO_DISK_HPP_COMPAT
