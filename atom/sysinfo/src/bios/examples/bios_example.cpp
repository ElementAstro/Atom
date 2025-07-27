/**
 * @file bios_example.cpp
 * @brief Example demonstrating the enhanced BIOS module functionality
 *
 * This example shows how to use the new modular BIOS implementation
 * with enhanced features like power management, overclocking info,
 * hardware monitoring, and diagnostics.
 */

#include "../bios.hpp"
#include <iostream>
#include <iomanip>

using namespace atom::system;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << " " << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

void demonstrateBasicBiosInfo() {
    printSeparator("Basic BIOS Information");

    try {
        auto& biosInfo = BiosInfo::getInstance();
        const auto& info = biosInfo.getBiosInfo();

        std::cout << "BIOS Version: " << info.version << std::endl;
        std::cout << "Manufacturer: " << info.manufacturer << std::endl;
        std::cout << "Release Date: " << info.releaseDate << std::endl;
        std::cout << "Serial Number: " << info.serialNumber << std::endl;
        std::cout << "Upgradeable: " << (info.isUpgradeable ? "Yes" : "No") << std::endl;

        std::cout << "\nSupported Features:" << std::endl;
        auto features = biosInfo.getSupportedFeatures();
        for (const auto& feature : features) {
            std::cout << "  - " << feature << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void demonstrateFirmwareInfo() {
    printSeparator("Firmware Information");

    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto firmwareInfo = biosInfo.getFirmwareInfo();

        std::cout << "Firmware Type: " << firmwareInfo.type << std::endl;
        std::cout << "Firmware Version: " << firmwareInfo.version << std::endl;
        std::cout << "Vendor: " << firmwareInfo.vendor << std::endl;
        std::cout << "Build Date: " << firmwareInfo.buildDate << std::endl;
        std::cout << "Secure Boot Capable: " << (firmwareInfo.secureBootCapable ? "Yes" : "No") << std::endl;
        std::cout << "TPM Supported: " << (firmwareInfo.tpmSupported ? "Yes" : "No") << std::endl;

        if (!firmwareInfo.supportedFeatures.empty()) {
            std::cout << "\nSupported Firmware Features:" << std::endl;
            for (const auto& feature : firmwareInfo.supportedFeatures) {
                std::cout << "  - " << feature << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void demonstrateBootConfiguration() {
    printSeparator("Boot Configuration");

    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto bootConfig = biosInfo.getBootConfiguration();

        std::cout << "UEFI Mode: " << (bootConfig.uefiMode ? "Yes" : "No") << std::endl;
        std::cout << "Secure Boot Enabled: " << (bootConfig.secureBootEnabled ? "Yes" : "No") << std::endl;
        std::cout << "Fast Boot Enabled: " << (bootConfig.fastBootEnabled ? "Yes" : "No") << std::endl;
        std::cout << "Current Boot Device: " << bootConfig.currentBootDevice << std::endl;

        if (!bootConfig.bootOrder.empty()) {
            std::cout << "\nBoot Order:" << std::endl;
            for (size_t i = 0; i < bootConfig.bootOrder.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << bootConfig.bootOrder[i] << std::endl;
            }
        }

        if (!bootConfig.bootDevices.empty()) {
            std::cout << "\nAvailable Boot Devices:" << std::endl;
            for (const auto& device : bootConfig.bootDevices) {
                std::cout << "  - " << device << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void demonstratePowerManagement() {
    printSeparator("Power Management Settings");

    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto powerSettings = biosInfo.getPowerManagementSettings();

        std::cout << "ACPI Enabled: " << (powerSettings.acpiEnabled ? "Yes" : "No") << std::endl;
        std::cout << "Wake on LAN: " << (powerSettings.wakeOnLanEnabled ? "Yes" : "No") << std::endl;
        std::cout << "Wake on Keyboard: " << (powerSettings.wakeOnKeyboardEnabled ? "Yes" : "No") << std::endl;
        std::cout << "Wake on Mouse: " << (powerSettings.wakeOnMouseEnabled ? "Yes" : "No") << std::endl;
        std::cout << "Power Profile: " << powerSettings.powerProfile << std::endl;

        if (powerSettings.cpuPowerLimit > 0) {
            std::cout << "CPU Power Limit: " << powerSettings.cpuPowerLimit << " watts" << std::endl;
        }

        if (!powerSettings.supportedSleepStates.empty()) {
            std::cout << "\nSupported Sleep States:" << std::endl;
            for (const auto& state : powerSettings.supportedSleepStates) {
                std::cout << "  - " << state << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void demonstrateOverclockingInfo() {
    printSeparator("Overclocking Information");

    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto overclockInfo = biosInfo.getOverclockingInfo();

        std::cout << "Overclocking Supported: " << (overclockInfo.overclockingSupported ? "Yes" : "No") << std::endl;
        std::cout << "Overclocking Enabled: " << (overclockInfo.overclockingEnabled ? "Yes" : "No") << std::endl;

        if (overclockInfo.baseCpuFrequency > 0) {
            std::cout << "Base CPU Frequency: " << BiosUtils::mhzToGhz(overclockInfo.baseCpuFrequency)
                      << " GHz (" << overclockInfo.baseCpuFrequency << " MHz)" << std::endl;
        }

        if (overclockInfo.currentCpuFrequency > 0) {
            std::cout << "Current CPU Frequency: " << BiosUtils::mhzToGhz(overclockInfo.currentCpuFrequency)
                      << " GHz (" << overclockInfo.currentCpuFrequency << " MHz)" << std::endl;
        }

        if (overclockInfo.maxCpuFrequency > 0) {
            std::cout << "Max CPU Frequency: " << BiosUtils::mhzToGhz(overclockInfo.maxCpuFrequency)
                      << " GHz (" << overclockInfo.maxCpuFrequency << " MHz)" << std::endl;
        }

        if (overclockInfo.currentMemoryFrequency > 0) {
            std::cout << "Current Memory Frequency: " << overclockInfo.currentMemoryFrequency << " MHz" << std::endl;
        }

        if (!overclockInfo.availableProfiles.empty()) {
            std::cout << "\nAvailable Overclocking Profiles:" << std::endl;
            for (const auto& profile : overclockInfo.availableProfiles) {
                std::cout << "  - " << profile << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void demonstrateHardwareMonitoring() {
    printSeparator("Hardware Monitoring");

    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto monitoring = biosInfo.getHardwareMonitoring();

        if (monitoring.systemTemperature > 0) {
            std::cout << "System Temperature: " << monitoring.systemTemperature << "°C ("
                      << BiosUtils::celsiusToFahrenheit(monitoring.systemTemperature) << "°F)" << std::endl;
        }

        std::cout << "Thermal Throttling: " << (monitoring.thermalThrottling ? "Active" : "Inactive") << std::endl;

        if (!monitoring.cpuTemperatures.empty()) {
            std::cout << "\nCPU Temperatures:" << std::endl;
            for (size_t i = 0; i < monitoring.cpuTemperatures.size(); ++i) {
                std::cout << "  Core " << i << ": " << monitoring.cpuTemperatures[i] << "°C ("
                          << BiosUtils::celsiusToFahrenheit(monitoring.cpuTemperatures[i]) << "°F)" << std::endl;
            }
        }

        if (!monitoring.fanSpeeds.empty()) {
            std::cout << "\nFan Speeds:" << std::endl;
            for (size_t i = 0; i < monitoring.fanSpeeds.size(); ++i) {
                std::cout << "  Fan " << i << ": " << monitoring.fanSpeeds[i] << " RPM" << std::endl;
            }
        }

        if (!monitoring.voltages.empty()) {
            std::cout << "\nVoltages:" << std::endl;
            for (size_t i = 0; i < monitoring.voltages.size(); ++i) {
                std::cout << "  Rail " << i << ": " << std::fixed << std::setprecision(2)
                          << monitoring.voltages[i] << " V" << std::endl;
            }
        }

        if (!monitoring.sensorNames.empty()) {
            std::cout << "\nAvailable Sensors:" << std::endl;
            for (const auto& sensor : monitoring.sensorNames) {
                std::cout << "  - " << sensor << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void demonstrateDiagnostics() {
    printSeparator("BIOS Diagnostics");

    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto diagnostics = biosInfo.runDiagnostics();

        std::cout << "POST Test: " << (diagnostics.postTestPassed ? "PASS" : "FAIL") << std::endl;
        std::cout << "Memory Test: " << (diagnostics.memoryTestPassed ? "PASS" : "FAIL") << std::endl;
        std::cout << "CPU Test: " << (diagnostics.cpuTestPassed ? "PASS" : "FAIL") << std::endl;
        std::cout << "Storage Test: " << (diagnostics.storageTestPassed ? "PASS" : "FAIL") << std::endl;

        if (diagnostics.diagnosticCode != 0) {
            std::cout << "Diagnostic Code: " << diagnostics.diagnosticCode << std::endl;
        }

        if (!diagnostics.failedComponents.empty()) {
            std::cout << "\nFailed Components:" << std::endl;
            for (const auto& component : diagnostics.failedComponents) {
                std::cout << "  - " << component << std::endl;
            }
        }

        if (!diagnostics.warnings.empty()) {
            std::cout << "\nWarnings:" << std::endl;
            for (const auto& warning : diagnostics.warnings) {
                std::cout << "  - " << warning << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void demonstrateHealthCheck() {
    printSeparator("BIOS Health Check");

    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto health = biosInfo.checkHealth();

        std::cout << "Overall Health: " << (health.isHealthy ? "HEALTHY" : "UNHEALTHY") << std::endl;

        if (health.biosAgeInDays > 0) {
            std::cout << "BIOS Age: " << health.biosAgeInDays << " days" << std::endl;

            if (health.biosAgeInDays > 365) {
                std::cout << "  (Consider checking for BIOS updates)" << std::endl;
            }
        }

        if (!health.warnings.empty()) {
            std::cout << "\nWarnings:" << std::endl;
            for (const auto& warning : health.warnings) {
                std::cout << "  - " << warning << std::endl;
            }
        }

        if (!health.errors.empty()) {
            std::cout << "\nErrors:" << std::endl;
            for (const auto& error : health.errors) {
                std::cout << "  - " << error << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "Enhanced BIOS Module Demonstration" << std::endl;
    std::cout << "==================================" << std::endl;

    try {
        demonstrateBasicBiosInfo();
        demonstrateFirmwareInfo();
        demonstrateBootConfiguration();
        demonstratePowerManagement();
        demonstrateOverclockingInfo();
        demonstrateHardwareMonitoring();
        demonstrateDiagnostics();
        demonstrateHealthCheck();

        printSeparator("Utility Functions Demo");

        // Demonstrate utility functions
        std::cout << "Temperature conversion: 25°C = " << BiosUtils::celsiusToFahrenheit(25.0) << "°F" << std::endl;
        std::cout << "Frequency conversion: 3200 MHz = " << BiosUtils::mhzToGhz(3200) << " GHz" << std::endl;
        std::cout << "Boot time formatting: " << BiosUtils::formatBootTime(15500) << std::endl;
        std::cout << "BIOS version validation: " << (BiosUtils::isValidBiosVersion("1.2.3") ? "Valid" : "Invalid") << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nDemo completed successfully!" << std::endl;
    return 0;
}
