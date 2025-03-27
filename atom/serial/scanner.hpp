#ifndef ATOM_SERIAL_SCANNER_HPP
#define ATOM_SERIAL_SCANNER_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <devguid.h>
#include <setupapi.h>
#include <windows.h>
#include <regex>
#pragma comment(lib, "setupapi.lib")
#else
#include <fcntl.h>
#include <libudev.h>
#include <unistd.h>
#endif

/**
 * @brief A class to scan and retrieve information about serial ports.
 *
 * This class provides methods to list available serial ports, get detailed
 * information about a specific port, and identify CH340 series devices.  It
 * supports both Windows and Linux platforms.
 */
class SerialPortScanner {
public:
    /**
     * @brief Structure to hold basic information about a serial port.
     */
    struct PortInfo {
        std::string device;       ///< The device name (e.g., COM1 on Windows or
                                  ///< /dev/ttyUSB0 on Linux).
        std::string description;  ///< A description of the port.
        bool is_ch340{
            false};  ///< Flag indicating whether the device is a CH340.
        std::string ch340_model;  ///< The CH340 model (if applicable).
    };

    /**
     * @brief Structure to hold detailed information about a serial port.
     */
    struct PortDetails {
        std::string device_name;  ///< The device name.
        std::string description;  ///< A description of the port.
        std::string hardware_id;  ///< The hardware ID of the port.
        std::string vid;  ///< The Vendor ID (VID) in hexadecimal format.
        std::string pid;  ///< The Product ID (PID) in hexadecimal format.
        std::string serial_number;  ///< The serial number of the device.
        std::string location;       ///< The location of the device.
        std::string manufacturer;   ///< The manufacturer of the device.
        std::string product;        ///< The product name.
        std::string interface;      ///< The interface type.
        bool is_ch340{
            false};  ///< Flag indicating whether the device is a CH340.
        std::string ch340_model;  ///< The CH340 model (if applicable).
        std::string
            recommended_baud_rates;  ///< Recommended baud rates for the port.
        std::string notes;           ///< Additional notes about the port.
    };

    /**
     * @brief Default constructor for the SerialPortScanner class.
     *
     * Initializes the internal CH340 identifier map.
     */
    SerialPortScanner();

    /**
     * @brief Checks if a given serial port is a CH340 series device.
     *
     * This function checks the Vendor ID (VID), Product ID (PID), and device
     * description against a known list of CH340 devices.
     *
     * @param vid The Vendor ID of the device.
     * @param pid The Product ID of the device.
     * @param description The description of the device.
     * @return A pair containing a boolean indicating whether the device is a
     * CH340 and the CH340 model (if applicable).
     */
    std::pair<bool, std::string> is_ch340_device(
        uint16_t vid, uint16_t pid, const std::string& description) const;

    /**
     * @brief Lists all available serial ports.
     *
     * This function scans the system for available serial ports and returns a
     * list of `PortInfo` structures containing basic information about each
     * port.
     *
     * @param highlight_ch340 If true, CH340 devices will be marked in the
     * returned list. Defaults to true.
     * @return A vector of `PortInfo` structures, each representing a serial
     * port.
     */
    std::vector<PortInfo> list_available_ports(bool highlight_ch340 = true);

    /**
     * @brief Retrieves detailed information about a specific serial port.
     *
     * This function retrieves detailed information about the specified serial
     * port, such as hardware ID, Vendor ID, Product ID, serial number, and
     * other properties.
     *
     * @param port_name The name of the serial port to retrieve details for
     * (e.g., "COM1" or "/dev/ttyUSB0").
     * @return An optional `PortDetails` object containing the detailed
     * information.  If the port is not found, an empty optional is returned.
     */
    std::optional<PortDetails> get_port_details(const std::string& port_name);

private:
    /**
     * @brief A map to store known CH340 device identifiers.
     *
     * The outer map uses the Vendor ID as the key, and the inner map uses the
     * Product ID as the key. The value of the inner map is the CH340 model
     * string.
     */
    std::map<uint16_t, std::map<uint16_t, std::string>> ch340_identifiers;
};

#endif
