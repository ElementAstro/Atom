/**
 * @file platform_features.cpp
 * @brief Platform-specific serial port features demonstration
 *
 * This example demonstrates platform-specific functionality including:
 * - Platform detection and conditional compilation
 * - Windows-specific features (COM port handling, device enumeration)
 * - Unix/Linux-specific features (device nodes, permissions, udev)
 * - macOS-specific features (IOKit integration)
 * - Cross-platform compatibility strategies
 * - Platform-specific error handling and diagnostics
 *
 * @author Atom Serial Examples
 * @date 2024
 */

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "atom/serial/core/scanner.hpp"
#include "atom/serial/core/serial_port.hpp"

#ifdef _WIN32
#include <devguid.h>
#include <setupapi.h>
#include <windows.h>
#pragma comment(lib, "setupapi.lib")
#elif defined(__unix__) || defined(__APPLE__)
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <libudev.h>
#endif
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#endif
#endif

using namespace atom::serial;

/**
 * @brief Detects the current platform
 */
std::string detectPlatform() {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__unix__)
    return "Unix";
#else
    return "Unknown";
#endif
}

/**
 * @brief Demonstrates Windows-specific features
 */
void demonstrateWindowsFeatures() {
#ifdef _WIN32
    std::cout << "\n=== Windows-Specific Features ===\n";

    // 1. COM port enumeration using Windows API
    std::cout << "1. Windows COM Port Enumeration:\n";

    // Query registry for COM ports
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     TEXT("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0, KEY_READ,
                     &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        TCHAR valueName[256];
        TCHAR valueData[256];
        DWORD valueNameSize, valueDataSize;

        std::cout << "  Registry COM ports:\n";
        while (true) {
            valueNameSize = sizeof(valueName);
            valueDataSize = sizeof(valueData);

            LONG result =
                RegEnumValue(hKey, index++, valueName, &valueNameSize, NULL,
                             NULL, (LPBYTE)valueData, &valueDataSize);

            if (result != ERROR_SUCCESS)
                break;

            std::wcout << L"    " << valueName << L" -> " << valueData << L"\n";
        }

        RegCloseKey(hKey);
    } else {
        std::cout << "  Could not access COM port registry\n";
    }

    // 2. Device Manager information
    std::cout << "\n2. Device Manager Serial Devices:\n";

    HDEVINFO deviceInfoSet =
        SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (deviceInfoSet != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA deviceInfoData;
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0;
             SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
            TCHAR deviceDescription[256];
            if (SetupDiGetDeviceRegistryProperty(
                    deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC, NULL,
                    (PBYTE)deviceDescription, sizeof(deviceDescription),
                    NULL)) {
                std::wcout << L"  Device " << i << L": " << deviceDescription
                           << L"\n";
            }
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
    }

    // 3. Windows-specific port naming
    std::cout << "\n3. Windows Port Naming Conventions:\n";
    std::cout << "  - COM1-COM9: Standard naming\n";
    std::cout << "  - \\\\.\\COM10+: Extended naming for ports > 9\n";
    std::cout << "  - USB Serial: Usually COM3 and higher\n";
    std::cout << "  - Bluetooth: Virtual COM ports\n";

#else
    std::cout << "\n=== Windows-Specific Features ===\n";
    std::cout << "Not running on Windows - features not available\n";
#endif
}

/**
 * @brief Demonstrates Unix/Linux-specific features
 */
void demonstrateUnixLinuxFeatures() {
#if defined(__unix__) || defined(__APPLE__)
    std::cout << "\n=== Unix/Linux-Specific Features ===\n";

    // 1. Device node enumeration
    std::cout << "1. Device Node Enumeration:\n";

    std::vector<std::string> devicePaths = {
        "/dev/ttyS*",    // Standard serial ports
        "/dev/ttyUSB*",  // USB serial adapters
        "/dev/ttyACM*",  // USB CDC ACM devices
        "/dev/cu.*",     // macOS calling units
        "/dev/tty.*"     // macOS terminal devices
    };

    for (const auto& pattern : devicePaths) {
        std::cout << "  Checking " << pattern << ":\n";

        // Simple check for common device nodes
        for (int i = 0; i < 10; ++i) {
            std::string devicePath;
            if (pattern.find("*") != std::string::npos) {
                devicePath =
                    pattern.substr(0, pattern.find("*")) + std::to_string(i);
            } else {
                devicePath = pattern + std::to_string(i);
            }

            struct stat statbuf;
            if (stat(devicePath.c_str(), &statbuf) == 0) {
                std::cout << "    Found: " << devicePath;

                // Check permissions
                if (access(devicePath.c_str(), R_OK | W_OK) == 0) {
                    std::cout << " (accessible)";
                } else {
                    std::cout << " (permission denied)";
                }
                std::cout << "\n";
            }
        }
    }

    // 2. Permission checking
    std::cout << "\n2. Permission Analysis:\n";

    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        std::cout << "  Current user: " << pw->pw_name << " (UID: " << uid
                  << ")\n";
    }

    // Check common groups that provide serial access
    std::vector<std::string> serialGroups = {"dialout", "uucp", "serial",
                                             "wheel"};
    std::cout << "  Serial access groups to check: ";
    for (const auto& group : serialGroups) {
        std::cout << group << " ";
    }
    std::cout << "\n";

    // 3. Symbolic link resolution
    std::cout << "\n3. Symbolic Link Resolution:\n";
    std::cout << "  Many Unix systems use symbolic links for device aliases\n";
    std::cout << "  Example: /dev/serial/by-id/ -> actual device nodes\n";

    if (std::filesystem::exists("/dev/serial/by-id/")) {
        std::cout << "  Found /dev/serial/by-id/ directory:\n";
        try {
            for (const auto& entry :
                 std::filesystem::directory_iterator("/dev/serial/by-id/")) {
                if (entry.is_symlink()) {
                    auto target = std::filesystem::read_symlink(entry.path());
                    std::cout << "    " << entry.path().filename().string()
                              << " -> " << target.string() << "\n";
                }
            }
        } catch (const std::exception& e) {
            std::cout << "    Error reading directory: " << e.what() << "\n";
        }
    }

#ifdef __linux__
    // 4. Linux-specific udev information
    std::cout << "\n4. Linux udev Information:\n";

    struct udev* udev_ctx = udev_new();
    if (udev_ctx) {
        std::cout << "  udev context created successfully\n";
        std::cout << "  udev can provide detailed device information\n";
        std::cout << "  Use 'udevadm info' command for device details\n";
        udev_unref(udev_ctx);
    } else {
        std::cout << "  Could not create udev context\n";
    }
#endif

#else
    std::cout << "\n=== Unix/Linux-Specific Features ===\n";
    std::cout << "Not running on Unix/Linux - features not available\n";
#endif
}

/**
 * @brief Demonstrates macOS-specific features
 */
void demonstrateMacOSFeatures() {
#ifdef __APPLE__
    std::cout << "\n=== macOS-Specific Features ===\n";

    // 1. IOKit serial port enumeration
    std::cout << "1. IOKit Serial Port Enumeration:\n";

    CFMutableDictionaryRef matchingDict =
        IOServiceMatching(kIOSerialBSDServiceValue);
    if (matchingDict) {
        io_iterator_t iterator;
        kern_return_t result = IOServiceGetMatchingServices(
            kIOMasterPortDefault, matchingDict, &iterator);

        if (result == KERN_SUCCESS) {
            io_object_t service;
            while ((service = IOIteratorNext(iterator))) {
                CFStringRef deviceNameRef =
                    (CFStringRef)IORegistryEntryCreateCFProperty(
                        service, CFSTR(kIOTTYDeviceKey), kCFAllocatorDefault,
                        0);

                if (deviceNameRef) {
                    char deviceName[256];
                    CFStringGetCString(deviceNameRef, deviceName,
                                       sizeof(deviceName),
                                       kCFStringEncodingUTF8);
                    std::cout << "  Found IOKit device: " << deviceName << "\n";
                    CFRelease(deviceNameRef);
                }

                IOObjectRelease(service);
            }
            IOObjectRelease(iterator);
        } else {
            std::cout << "  IOServiceGetMatchingServices failed\n";
        }
    }

    // 2. macOS device naming conventions
    std::cout << "\n2. macOS Device Naming:\n";
    std::cout << "  - /dev/cu.* : Calling units (outgoing connections)\n";
    std::cout << "  - /dev/tty.* : Terminal devices (incoming connections)\n";
    std::cout << "  - USB devices: /dev/cu.usbserial-*\n";
    std::cout << "  - Bluetooth: /dev/cu.*-Bluetooth-*\n";

    // 3. macOS-specific considerations
    std::cout << "\n3. macOS Considerations:\n";
    std::cout << "  - Use cu.* devices for most applications\n";
    std::cout << "  - tty.* devices may block waiting for carrier detect\n";
    std::cout << "  - System Integrity Protection may affect access\n";
    std::cout << "  - Gatekeeper may require code signing for device access\n";

#else
    std::cout << "\n=== macOS-Specific Features ===\n";
    std::cout << "Not running on macOS - features not available\n";
#endif
}

/**
 * @brief Demonstrates cross-platform compatibility strategies
 */
void demonstrateCrossPlatformStrategies() {
    std::cout << "\n=== Cross-Platform Compatibility Strategies ===\n";

    // 1. Platform-agnostic port discovery
    std::cout << "1. Platform-Agnostic Port Discovery:\n";

    try {
        SerialPortScanner scanner;
        auto result = scanner.list_available_ports();

        if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(
                result)) {
            const auto& ports =
                std::get<std::vector<SerialPortScanner::PortInfo>>(result);
            std::cout << "  Scanner found " << ports.size() << " ports:\n";

            for (const auto& port : ports) {
                std::cout << "    " << port.device << " - " << port.description
                          << "\n";

                // Platform-specific information
                std::string platform = detectPlatform();
                if (platform == "Windows" && port.device.find("COM") == 0) {
                    std::cout << "      Windows COM port detected\n";
                } else if ((platform == "Linux" || platform == "Unix") &&
                           (port.device.find("/dev/ttyUSB") == 0 ||
                            port.device.find("/dev/ttyACM") == 0)) {
                    std::cout << "      Linux USB serial device detected\n";
                } else if (platform == "macOS" &&
                           port.device.find("/dev/cu.") == 0) {
                    std::cout << "      macOS calling unit detected\n";
                }
            }
        }
    } catch (const SerialException& e) {
        std::cout << "  Scanner error: " << e.what() << "\n";
    }

    // 2. Configuration recommendations by platform
    std::cout << "\n2. Platform-Specific Configuration Recommendations:\n";

    std::string platform = detectPlatform();
    std::cout << "  Current platform: " << platform << "\n";

    if (platform == "Windows") {
        std::cout << "  Windows recommendations:\n";
        std::cout << "    - Use standard COM port names (COM1, COM2, etc.)\n";
        std::cout << "    - For COM10+, use \\\\.\\COM10 format\n";
        std::cout << "    - Check Device Manager for port availability\n";
        std::cout << "    - Consider Windows-specific timeout behavior\n";
    } else if (platform == "Linux" || platform == "Unix") {
        std::cout << "  Linux/Unix recommendations:\n";
        std::cout << "    - Check user permissions (dialout group)\n";
        std::cout << "    - Use full device paths (/dev/ttyUSB0)\n";
        std::cout << "    - Consider udev rules for consistent naming\n";
        std::cout << "    - Handle device node permissions properly\n";
    } else if (platform == "macOS") {
        std::cout << "  macOS recommendations:\n";
        std::cout << "    - Prefer /dev/cu.* over /dev/tty.*\n";
        std::cout << "    - Handle System Integrity Protection\n";
        std::cout << "    - Consider code signing requirements\n";
        std::cout << "    - Use IOKit for advanced device information\n";
    }

    // 3. Error handling differences
    std::cout << "\n3. Platform-Specific Error Handling:\n";
    std::cout << "  Different platforms may report different error codes\n";
    std::cout
        << "  Use SerialException hierarchy for portable error handling\n";
    std::cout
        << "  Platform-specific diagnostics available in implementation\n";
}

/**
 * @brief Main function demonstrating platform-specific features
 */
int main() {
    std::cout << "=== Platform-Specific Serial Port Features Example ===\n";

    std::string platform = detectPlatform();
    std::cout << "Detected platform: " << platform << "\n";
    std::cout << "This example demonstrates platform-specific functionality.\n";

    // Demonstrate features for each platform
    demonstrateWindowsFeatures();
    demonstrateUnixLinuxFeatures();
    demonstrateMacOSFeatures();
    demonstrateCrossPlatformStrategies();

    std::cout << "\n=== Platform Development Guidelines ===\n";
    std::cout
        << "1. Use the SerialPortScanner for cross-platform port discovery\n";
    std::cout << "2. Handle platform-specific naming conventions\n";
    std::cout << "3. Consider permission requirements on Unix-like systems\n";
    std::cout << "4. Test on all target platforms\n";
    std::cout << "5. Use conditional compilation for platform-specific code\n";
    std::cout << "6. Provide fallbacks for unsupported features\n";
    std::cout << "7. Document platform-specific requirements\n";

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
