#ifndef ATOM_SYSTEM_DEVICE_HPP
#define ATOM_SYSTEM_DEVICE_HPP

#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Structure to hold device information
 */
struct DeviceInfo {
    std::string description;  ///< Device description or name
    std::string address;      ///< Device address or identifier
} ATOM_ALIGNAS(64);

/**
 * @brief Enumerate all USB devices in the system
 * @return Vector of USB device information
 */
auto enumerateUsbDevices() -> std::vector<DeviceInfo>;

/**
 * @brief Enumerate all serial ports in the system
 * @return Vector of serial port information
 */
auto enumerateSerialPorts() -> std::vector<DeviceInfo>;

/**
 * @brief Enumerate all Bluetooth devices in the system
 * @return Vector of Bluetooth device information
 */
auto enumerateBluetoothDevices() -> std::vector<DeviceInfo>;

}  // namespace atom::system

#endif
