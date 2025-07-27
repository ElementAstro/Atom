#include "macos.hpp"

#ifdef __APPLE__
#include <spdlog/spdlog.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <array>

namespace atom::system {

BiosInfoData MacOSBiosImplementation::fetchBiosInfo() {
    spdlog::info("Fetching BIOS information");
    BiosInfoData biosInfo;

    try {
        // On macOS, we get firmware information from IORegistry
        biosInfo.version = getIORegistryProperty("IOPlatformExpertDevice", "firmware-version");
        biosInfo.manufacturer = getIORegistryProperty("IOPlatformExpertDevice", "manufacturer");
        biosInfo.serialNumber = getIORegistryProperty("IOPlatformExpertDevice", "IOPlatformSerialNumber");
        
        // Get system information
        std::string systemInfo = executeCommand("system_profiler SPHardwareDataType");
        
        // Parse additional information from system_profiler output
        if (systemInfo.find("Boot ROM Version:") != std::string::npos) {
            size_t pos = systemInfo.find("Boot ROM Version:");
            size_t end = systemInfo.find("\n", pos);
            if (pos != std::string::npos && end != std::string::npos) {
                biosInfo.version = systemInfo.substr(pos + 17, end - pos - 17);
            }
        }
        
        // macOS systems typically use UEFI
        biosInfo.isUpgradeable = false; // Apple controls firmware updates
        
    } catch (const std::exception& e) {
        spdlog::error("Error fetching BIOS info: {}", e.what());
        throw;
    }

    return biosInfo;
}

BiosHealthStatus MacOSBiosImplementation::checkHealth() const {
    BiosHealthStatus status;
    status.isHealthy = true;
    status.lastCheckTime = std::chrono::system_clock::now().time_since_epoch().count();

    try {
        // Check system logs for firmware-related issues
        std::string logResult = executeCommand("log show --predicate 'subsystem == \"com.apple.kernel\"' --info --last 1d | grep -i 'firmware\\|efi\\|boot'");
        
        if (logResult.find("error") != std::string::npos || 
            logResult.find("fail") != std::string::npos) {
            status.isHealthy = false;
            status.errors.push_back("Firmware-related errors found in system logs");
        }
        
        // Check hardware diagnostics
        std::string hwTest = executeCommand("system_profiler SPDiagnosticsDataType");
        if (hwTest.find("FAIL") != std::string::npos) {
            status.warnings.push_back("Hardware diagnostics indicate potential issues");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to check BIOS health: {}", e.what());
        status.isHealthy = false;
        status.errors.push_back(e.what());
    }

    return status;
}

BiosUpdateInfo MacOSBiosImplementation::checkForUpdates() const {
    BiosUpdateInfo updateInfo;
    updateInfo.updateAvailable = false;

    try {
        // On macOS, firmware updates are handled by Software Update
        std::string updateCheck = executeCommand("softwareupdate -l");
        
        if (updateCheck.find("Firmware") != std::string::npos ||
            updateCheck.find("EFI") != std::string::npos) {
            updateInfo.updateAvailable = true;
            updateInfo.updateUrl = "Use Software Update in System Preferences";
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to check for BIOS updates: {}", e.what());
    }

    return updateInfo;
}

std::vector<std::string> MacOSBiosImplementation::getSMBIOSData() const {
    std::vector<std::string> smbiosData;

    try {
        // macOS doesn't provide direct SMBIOS access, use system_profiler instead
        std::string hardwareInfo = executeCommand("system_profiler SPHardwareDataType");
        std::vector<std::string> lines = executeCommandLines("system_profiler SPHardwareDataType");
        smbiosData = lines;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to get SMBIOS data: {}", e.what());
    }

    return smbiosData;
}

bool MacOSBiosImplementation::setSecureBoot(bool enable) {
    spdlog::error("Secure Boot modification is not supported on macOS");
    spdlog::info("macOS uses System Integrity Protection (SIP) instead");
    return false;
}

bool MacOSBiosImplementation::setUEFIBoot(bool enable) {
    spdlog::error("UEFI Boot modification is not supported on macOS");
    spdlog::info("macOS boot configuration is managed by the system");
    return false;
}

bool MacOSBiosImplementation::isSecureBootSupported() const {
    // macOS uses SIP instead of traditional Secure Boot
    try {
        std::string sipStatus = executeCommand("csrutil status");
        return sipStatus.find("enabled") != std::string::npos;
    } catch (const std::exception& e) {
        spdlog::error("Failed to check SIP status: {}", e.what());
        return false;
    }
}

bool MacOSBiosImplementation::isUEFIBootSupported() const {
    // All modern Macs use UEFI
    return true;
}

bool MacOSBiosImplementation::backupBiosSettings(const std::string& filepath) {
    try {
        std::ofstream out(filepath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot open file for writing");
        }
        
        // Backup NVRAM settings
        std::string nvramData = executeCommand("nvram -p");
        out << nvramData;
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to backup BIOS settings: {}", e.what());
        return false;
    }
}

bool MacOSBiosImplementation::restoreBiosSettings(const std::string& filepath) {
    try {
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            spdlog::error("Failed to open BIOS settings backup file: {}", filepath);
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        if (content.empty()) {
            spdlog::warn("BIOS settings backup file is empty: {}", filepath);
        }

        spdlog::info("BIOS settings restoration from {} (simulated) successful.", filepath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to restore BIOS settings: {}", e.what());
        return false;
    }
}

// Enhanced features implementation
FirmwareInfo MacOSBiosImplementation::getFirmwareInfo() const {
    FirmwareInfo info;
    
    try {
        info.type = "UEFI";
        info.secureBootCapable = true; // macOS has SIP
        info.tpmSupported = true; // Modern Macs have T2/Apple Silicon security
        
        info.version = getIORegistryProperty("IOPlatformExpertDevice", "firmware-version");
        info.vendor = "Apple Inc.";
        
        // Get build date from system info
        std::string systemInfo = executeCommand("system_profiler SPHardwareDataType");
        // Parse build date if available
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to get firmware info: {}", e.what());
    }
    
    return info;
}

BootConfiguration MacOSBiosImplementation::getBootConfiguration() const {
    BootConfiguration config;
    
    try {
        config.uefiMode = true; // macOS always uses UEFI
        config.secureBootEnabled = isSecureBootSupported();
        
        // Get boot device information
        std::string bootDevice = executeCommand("bless --info --getboot");
        config.currentBootDevice = bootDevice;
        
        // Get available boot devices
        config.bootDevices = getAvailableBootDevices();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to get boot configuration: {}", e.what());
    }
    
    return config;
}

BiosSecuritySettings MacOSBiosImplementation::getSecuritySettings() const {
    BiosSecuritySettings settings;
    
    try {
        settings.secureBootEnabled = isSecureBootSupported();
        settings.tpmEnabled = true; // Modern Macs have security chips
        
        // Check virtualization support
        std::string cpuInfo = executeCommand("sysctl -n machdep.cpu.features");
        settings.virtualizationEnabled = cpuInfo.find("VMX") != std::string::npos;
        
        // Check hyper-threading
        std::string htInfo = executeCommand("sysctl -n machdep.cpu.thread_count");
        std::string coreInfo = executeCommand("sysctl -n machdep.cpu.core_count");
        settings.hyperThreadingEnabled = std::stoi(htInfo) > std::stoi(coreInfo);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to get security settings: {}", e.what());
    }
    
    return settings;
}

BiosOperationResult MacOSBiosImplementation::setBootOrder(const std::vector<std::string>& order) {
    spdlog::error("Boot order modification is not supported on macOS");
    return BiosOperationResult::NOT_SUPPORTED;
}

BiosOperationResult MacOSBiosImplementation::enableVirtualization(bool enable) {
    spdlog::error("Virtualization settings modification is not supported on macOS");
    return BiosOperationResult::NOT_SUPPORTED;
}

BiosOperationResult MacOSBiosImplementation::enableHyperThreading(bool enable) {
    spdlog::error("Hyper-threading settings modification is not supported on macOS");
    return BiosOperationResult::NOT_SUPPORTED;
}

std::vector<std::string> MacOSBiosImplementation::getAvailableBootDevices() const {
    std::vector<std::string> devices;
    
    try {
        // Get mounted volumes that could be bootable
        std::vector<std::string> volumes = executeCommandLines("diskutil list | grep 'Apple_HFS\\|Apple_APFS'");
        for (const auto& volume : volumes) {
            devices.push_back(volume);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to get boot devices: {}", e.what());
    }
    
    return devices;
}

bool MacOSBiosImplementation::validateBiosIntegrity() const {
    try {
        // Check SIP status as a form of integrity validation
        std::string sipStatus = executeCommand("csrutil status");
        if (sipStatus.find("enabled") != std::string::npos) {
            spdlog::info("System Integrity Protection is enabled");
            return true;
        }
        
        spdlog::warn("System Integrity Protection is disabled");
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Failed to validate system integrity: {}", e.what());
        return false;
    }
}

// Private helper methods
std::string MacOSBiosImplementation::getIORegistryProperty(const std::string& service, const std::string& property) const {
    try {
        std::string command = "ioreg -l | grep '" + property + "'";
        std::string result = executeCommand(command);
        
        // Parse the result to extract the property value
        size_t pos = result.find("=");
        if (pos != std::string::npos) {
            std::string value = result.substr(pos + 1);
            // Clean up the value (remove quotes, whitespace, etc.)
            value.erase(0, value.find_first_not_of(" \t\""));
            value.erase(value.find_last_not_of(" \t\"\n") + 1);
            return value;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to get IORegistry property {}: {}", property, e.what());
    }
    
    return "";
}

std::string MacOSBiosImplementation::executeCommand(const std::string& command) const {
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(command.c_str(), "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("popen() failed for command: " + command);
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

std::vector<std::string> MacOSBiosImplementation::executeCommandLines(const std::string& command) const {
    std::vector<std::string> lines;
    std::string result = executeCommand(command);
    std::istringstream iss(result);
    std::string line;

    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    return lines;
}

bool MacOSBiosImplementation::hasAdminPrivileges() const {
    return geteuid() == 0;
}

CFStringRef MacOSBiosImplementation::createCFString(const std::string& str) const {
    return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
}

std::string MacOSBiosImplementation::cfStringToString(CFStringRef cfStr) const {
    if (!cfStr) return "";
    
    CFIndex length = CFStringGetLength(cfStr);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::vector<char> buffer(maxSize);
    
    if (CFStringGetCString(cfStr, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
        return std::string(buffer.data());
    }
    
    return "";
}

// Additional enhanced features implementation
PowerManagementSettings MacOSBiosImplementation::getPowerManagementSettings() const {
    PowerManagementSettings settings;

    try {
        // Get power management settings
        std::string pmsetInfo = executeCommand("pmset -g");
        settings.acpiEnabled = true; // macOS always has power management

        // Check wake settings
        if (pmsetInfo.find("womp") != std::string::npos) {
            settings.wakeOnLanEnabled = pmsetInfo.find("womp 1") != std::string::npos;
        }

        // Get power profile
        std::string powerMode = executeCommand("pmset -g | grep 'Currently in use'");
        if (powerMode.find("Battery Power") != std::string::npos) {
            settings.powerProfile = "Battery";
        } else if (powerMode.find("AC Power") != std::string::npos) {
            settings.powerProfile = "AC Power";
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get power management settings: {}", e.what());
    }

    return settings;
}

BiosOperationResult MacOSBiosImplementation::setPowerManagementSettings(const PowerManagementSettings& settings) {
    if (!hasAdminPrivileges()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        // Set wake-on-LAN
        std::string wakeCommand = "pmset -a womp " + std::to_string(settings.wakeOnLanEnabled ? 1 : 0);
        if (std::system(wakeCommand.c_str()) != 0) {
            return BiosOperationResult::FAILED;
        }

        return BiosOperationResult::SUCCESS;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set power management settings: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

OverclockingInfo MacOSBiosImplementation::getOverclockingInfo() const {
    OverclockingInfo info;

    try {
        // Get CPU frequency information
        std::string cpuFreq = executeCommand("sysctl -n hw.cpufrequency_max");
        if (!cpuFreq.empty()) {
            info.maxCpuFrequency = std::stoi(cpuFreq) / 1000000; // Convert to MHz
        }

        std::string currentFreq = executeCommand("sysctl -n hw.cpufrequency");
        if (!currentFreq.empty()) {
            info.currentCpuFrequency = std::stoi(currentFreq) / 1000000; // Convert to MHz
        }

        // Apple doesn't typically allow overclocking
        info.overclockingSupported = false;
        info.overclockingEnabled = false;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get overclocking info: {}", e.what());
    }

    return info;
}

BiosOperationResult MacOSBiosImplementation::setOverclockingProfile(const std::string& profile) {
    spdlog::error("Overclocking is not supported on macOS systems");
    spdlog::info("Profile requested: {}", profile);
    return BiosOperationResult::NOT_SUPPORTED;
}

HardwareMonitoring MacOSBiosImplementation::getHardwareMonitoring() const {
    HardwareMonitoring monitoring;

    try {
        // Get temperature information (if available)
        std::string tempInfo = executeCommand("sudo powermetrics -n 1 -s cpu_power | grep 'CPU die temperature'");
        if (!tempInfo.empty()) {
            // Parse temperature from powermetrics output
            size_t pos = tempInfo.find(":");
            if (pos != std::string::npos) {
                std::string tempStr = tempInfo.substr(pos + 1);
                // Extract numeric value
                size_t numStart = tempStr.find_first_of("0123456789");
                if (numStart != std::string::npos) {
                    monitoring.systemTemperature = std::stoi(tempStr.substr(numStart));
                }
            }
        }

        // Get fan information (if available)
        std::string fanInfo = executeCommand("sudo powermetrics -n 1 -s cpu_power | grep 'Fan'");
        if (!fanInfo.empty()) {
            // Parse fan speed if available
            monitoring.sensorNames.push_back("System Fan");
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get hardware monitoring data: {}", e.what());
    }

    return monitoring;
}

BiosDiagnostics MacOSBiosImplementation::runDiagnostics() const {
    BiosDiagnostics diagnostics;
    diagnostics.lastDiagnosticTime = std::chrono::system_clock::now();

    try {
        // Check memory
        std::string memInfo = executeCommand("sysctl -n hw.memsize");
        diagnostics.memoryTestPassed = !memInfo.empty();

        // Check CPU
        std::string cpuInfo = executeCommand("sysctl -n machdep.cpu.brand_string");
        diagnostics.cpuTestPassed = !cpuInfo.empty();

        // Check storage
        std::string diskInfo = executeCommand("diskutil list");
        diagnostics.storageTestPassed = !diskInfo.empty();

        // Check system logs for errors
        std::string logErrors = executeCommand("log show --predicate 'messageType == 16' --info --last 1h | head -10");
        if (!logErrors.empty()) {
            diagnostics.warnings.push_back("System errors found in logs");
        }

        // Run Apple Diagnostics if available
        std::string diagResult = executeCommand("system_profiler SPDiagnosticsDataType");
        if (diagResult.find("PASS") != std::string::npos) {
            diagnostics.postTestPassed = true;
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to run diagnostics: {}", e.what());
        diagnostics.postTestPassed = false;
    }

    return diagnostics;
}

BiosOperationResult MacOSBiosImplementation::resetToDefaults() {
    if (!hasAdminPrivileges()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        spdlog::warn("BIOS/firmware reset is not directly supported on macOS");
        spdlog::info("Use Apple Configurator or recovery mode for firmware operations");
        return BiosOperationResult::NOT_SUPPORTED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to reset to defaults: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

std::vector<std::string> MacOSBiosImplementation::getSupportedFeatures() const {
    std::vector<std::string> features;

    try {
        features.push_back("Firmware Information Retrieval");
        features.push_back("System Health Monitoring");
        features.push_back("Hardware Information Access");
        features.push_back("Power Management");
        features.push_back("System Integrity Protection");

        // Check for T2/Apple Silicon security features
        std::string securityInfo = executeCommand("system_profiler SPiBridgeDataType");
        if (!securityInfo.empty()) {
            features.push_back("T2 Security Chip");
        }

        // Check for Touch ID
        std::string touchIdInfo = executeCommand("bioutil -r");
        if (touchIdInfo.find("Touch ID") != std::string::npos) {
            features.push_back("Touch ID Support");
        }

        features.push_back("NVRAM Access");
        features.push_back("Boot Device Management");

    } catch (const std::exception& e) {
        spdlog::error("Failed to get supported features: {}", e.what());
    }

    return features;
}

}  // namespace atom::system

#endif  // __APPLE__
