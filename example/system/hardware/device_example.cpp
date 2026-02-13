/**
 * @file device_enumeration.cpp
 * @brief Comprehensive example demonstrating device discovery and enumeration
 *
 * This example showcases device enumeration capabilities including:
 * - USB device discovery and information
 * - Serial port enumeration and details
 * - Device hotplug detection and monitoring
 * - Device information parsing and display
 * - Cross-platform device management
 * - Device filtering and categorization
 *
 * @note Cross-platform compatibility: Windows, Linux, macOS
 * @note May require elevated privileges for full device access
 * @author Atom Framework
 * @date 2024
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include "atom/system/device.hpp"

using namespace atom::system;

/**
 * @brief Print a formatted section header
 */
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

/**
 * @brief Print device information in a formatted table
 */
void printDeviceInfo(const std::vector<DeviceInfo>& devices,
                     const std::string& deviceType) {
    if (devices.empty()) {
        std::cout << "No " << deviceType << " devices found." << std::endl;
        return;
    }

    std::cout << "Found " << devices.size() << " " << deviceType
              << " device(s):" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::setw(5) << "#" << " | " << std::setw(30) << "Description"
              << " | " << std::setw(35) << "Address/Identifier" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (size_t i = 0; i < devices.size(); i++) {
        const auto& device = devices[i];

        // Truncate long descriptions and addresses for display
        std::string description = device.description;
        std::string address = device.address;

        if (description.length() > 30) {
            description = description.substr(0, 27) + "...";
        }
        if (address.length() > 35) {
            address = address.substr(0, 32) + "...";
        }

        std::cout << std::setw(5) << (i + 1) << " | " << std::setw(30)
                  << description << " | " << std::setw(35) << address
                  << std::endl;
    }
    std::cout << std::string(80, '-') << std::endl;
}

/**
 * @brief Analyze and categorize USB devices
 */
void analyzeUsbDevices(const std::vector<DeviceInfo>& usbDevices) {
    if (usbDevices.empty()) {
        return;
    }

    std::cout << "\n--- USB Device Analysis ---" << std::endl;

    // Categorize devices by type (based on description keywords)
    std::map<std::string, std::vector<DeviceInfo>> categories;
    std::set<std::string> uniqueDescriptions;

    for (const auto& device : usbDevices) {
        std::string desc = device.description;
        std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);

        // Simple categorization based on keywords
        if (desc.find("hub") != std::string::npos) {
            categories["USB Hubs"].push_back(device);
        } else if (desc.find("mouse") != std::string::npos ||
                   desc.find("keyboard") != std::string::npos) {
            categories["Input Devices"].push_back(device);
        } else if (desc.find("camera") != std::string::npos ||
                   desc.find("webcam") != std::string::npos) {
            categories["Cameras"].push_back(device);
        } else if (desc.find("storage") != std::string::npos ||
                   desc.find("disk") != std::string::npos ||
                   desc.find("drive") != std::string::npos) {
            categories["Storage Devices"].push_back(device);
        } else if (desc.find("audio") != std::string::npos ||
                   desc.find("sound") != std::string::npos) {
            categories["Audio Devices"].push_back(device);
        } else if (desc.find("network") != std::string::npos ||
                   desc.find("ethernet") != std::string::npos ||
                   desc.find("wifi") != std::string::npos) {
            categories["Network Devices"].push_back(device);
        } else {
            categories["Other Devices"].push_back(device);
        }

        uniqueDescriptions.insert(device.description);
    }

    // Display categorized devices
    for (const auto& [category, devices] : categories) {
        if (!devices.empty()) {
            std::cout << "\n"
                      << category << " (" << devices.size()
                      << "):" << std::endl;
            for (size_t i = 0; i < devices.size(); i++) {
                std::cout << "  " << (i + 1) << ". " << devices[i].description
                          << std::endl;
                std::cout << "     Address: " << devices[i].address
                          << std::endl;
            }
        }
    }

    // Statistics
    std::cout << "\nUSB Device Statistics:" << std::endl;
    std::cout << "  Total devices: " << usbDevices.size() << std::endl;
    std::cout << "  Unique descriptions: " << uniqueDescriptions.size()
              << std::endl;
    std::cout << "  Device categories: " << categories.size() << std::endl;
}

/**
 * @brief Analyze and categorize serial ports
 */
void analyzeSerialPorts(const std::vector<DeviceInfo>& serialPorts) {
    if (serialPorts.empty()) {
        return;
    }

    std::cout << "\n--- Serial Port Analysis ---" << std::endl;

    // Categorize serial ports by type
    std::map<std::string, std::vector<DeviceInfo>> categories;

    for (const auto& port : serialPorts) {
        std::string desc = port.description;
        std::string addr = port.address;
        std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);
        std::transform(addr.begin(), addr.end(), addr.begin(), ::tolower);

        // Categorization based on common patterns
        if (desc.find("usb") != std::string::npos ||
            addr.find("usb") != std::string::npos) {
            categories["USB Serial Ports"].push_back(port);
        } else if (desc.find("bluetooth") != std::string::npos ||
                   desc.find("bt") != std::string::npos) {
            categories["Bluetooth Serial Ports"].push_back(port);
        } else if (desc.find("com") != std::string::npos ||
                   addr.find("com") != std::string::npos) {
            categories["COM Ports"].push_back(port);
        } else if (desc.find("tty") != std::string::npos ||
                   addr.find("tty") != std::string::npos) {
            categories["TTY Devices"].push_back(port);
        } else {
            categories["Other Serial Devices"].push_back(port);
        }
    }

    // Display categorized ports
    for (const auto& [category, ports] : categories) {
        if (!ports.empty()) {
            std::cout << "\n"
                      << category << " (" << ports.size() << "):" << std::endl;
            for (size_t i = 0; i < ports.size(); i++) {
                std::cout << "  " << (i + 1) << ". " << ports[i].address
                          << std::endl;
                if (!ports[i].description.empty()) {
                    std::cout << "     Description: " << ports[i].description
                              << std::endl;
                }
            }
        }
    }

    // Statistics
    std::cout << "\nSerial Port Statistics:" << std::endl;
    std::cout << "  Total ports: " << serialPorts.size() << std::endl;
    std::cout << "  Port categories: " << categories.size() << std::endl;
}

int main() {
    try {
        std::cout << "=== Device Enumeration and Discovery Example ==="
                  << std::endl;
        std::cout
            << "Demonstrating comprehensive device discovery capabilities\n"
            << std::endl;

        // 1. USB Device Enumeration
        printSection("USB Device Enumeration");

        std::cout << "Scanning for USB devices..." << std::endl;
        auto usbDevices = enumerateUsbDevices();
        printDeviceInfo(usbDevices, "USB");

        if (!usbDevices.empty()) {
            analyzeUsbDevices(usbDevices);
        }

        // 2. Serial Port Enumeration
        printSection("Serial Port Enumeration");

        std::cout << "Scanning for serial ports..." << std::endl;
        auto serialPorts = enumerateSerialPorts();
        printDeviceInfo(serialPorts, "serial port");

        if (!serialPorts.empty()) {
            analyzeSerialPorts(serialPorts);
        }

        // 3. Device Information Details
        printSection("Detailed Device Information");

        if (!usbDevices.empty()) {
            std::cout << "Detailed USB Device Information:" << std::endl;
            for (size_t i = 0; i < std::min(usbDevices.size(), size_t(3));
                 i++) {
                const auto& device = usbDevices[i];
                std::cout << "\nDevice " << (i + 1) << ":" << std::endl;
                std::cout << "  Description: " << device.description
                          << std::endl;
                std::cout << "  Address: " << device.address << std::endl;
                std::cout << "  Description Length: "
                          << device.description.length() << " characters"
                          << std::endl;
                std::cout << "  Address Length: " << device.address.length()
                          << " characters" << std::endl;
            }

            if (usbDevices.size() > 3) {
                std::cout << "\n... and " << (usbDevices.size() - 3)
                          << " more USB devices" << std::endl;
            }
        }

        if (!serialPorts.empty()) {
            std::cout << "\nDetailed Serial Port Information:" << std::endl;
            for (size_t i = 0; i < std::min(serialPorts.size(), size_t(3));
                 i++) {
                const auto& port = serialPorts[i];
                std::cout << "\nPort " << (i + 1) << ":" << std::endl;
                std::cout << "  Address: " << port.address << std::endl;
                std::cout << "  Description: " << port.description << std::endl;

                // Try to determine port type
                std::string portType = "Unknown";
                std::string addr = port.address;
                std::transform(addr.begin(), addr.end(), addr.begin(),
                               ::tolower);

                if (addr.find("com") != std::string::npos) {
                    portType = "Windows COM Port";
                } else if (addr.find("ttyusb") != std::string::npos) {
                    portType = "Linux USB Serial";
                } else if (addr.find("ttyacm") != std::string::npos) {
                    portType = "Linux ACM Device";
                } else if (addr.find("ttys") != std::string::npos) {
                    portType = "Linux Serial Port";
                } else if (addr.find("cu.") != std::string::npos) {
                    portType = "macOS Serial Device";
                }

                std::cout << "  Detected Type: " << portType << std::endl;
            }

            if (serialPorts.size() > 3) {
                std::cout << "\n... and " << (serialPorts.size() - 3)
                          << " more serial ports" << std::endl;
            }
        }

        // 4. Summary and Recommendations
        printSection("Summary and Recommendations");

        std::cout << "Device Discovery Summary:" << std::endl;
        std::cout << "  USB Devices Found: " << usbDevices.size() << std::endl;
        std::cout << "  Serial Ports Found: " << serialPorts.size()
                  << std::endl;
        std::cout << "  Total Devices: "
                  << (usbDevices.size() + serialPorts.size()) << std::endl;

        if (usbDevices.empty() && serialPorts.empty()) {
            std::cout << "\nNo devices found. This could indicate:"
                      << std::endl;
            std::cout << "  - Insufficient permissions (try running as "
                         "administrator/root)"
                      << std::endl;
            std::cout << "  - No devices connected" << std::endl;
            std::cout << "  - Device enumeration not supported on this platform"
                      << std::endl;
            std::cout << "  - Required system libraries not available"
                      << std::endl;
        } else {
            std::cout << "\nRecommendations:" << std::endl;
            if (!usbDevices.empty()) {
                std::cout << "  - USB devices detected - consider USB device "
                             "management features"
                          << std::endl;
            }
            if (!serialPorts.empty()) {
                std::cout << "  - Serial ports available - suitable for serial "
                             "communication"
                          << std::endl;
            }
            std::cout << "  - Use device monitoring for hotplug detection"
                      << std::endl;
            std::cout
                << "  - Implement device filtering for specific device types"
                << std::endl;
        }

        std::cout << "\n=== Device Enumeration Complete ===" << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- USB device discovery and enumeration" << std::endl;
        std::cout << "- Serial port detection and analysis" << std::endl;
        std::cout << "- Device categorization and filtering" << std::endl;
        std::cout << "- Detailed device information extraction" << std::endl;
        std::cout << "- Cross-platform device management" << std::endl;
        std::cout << "- Device statistics and analysis" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Device enumeration error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr
            << "- Insufficient permissions (try running as administrator/root)"
            << std::endl;
        std::cerr << "- Required system libraries not available" << std::endl;
        std::cerr << "- Platform not supported" << std::endl;
        std::cerr << "- Hardware access restrictions" << std::endl;
        return 1;
    }

    return 0;
}
