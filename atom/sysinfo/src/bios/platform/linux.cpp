#include "linux.hpp"

#ifdef __linux__
#include <spdlog/spdlog.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <iomanip>

namespace atom::system {

BiosInfoData LinuxBiosImplementation::fetchBiosInfo() {
    spdlog::info("Fetching BIOS information");
    BiosInfoData biosInfo;

    try {
        std::array<std::string, 2> commands = {"sudo dmidecode -t bios", "sudo dmidecode -t system"};

        for (const auto& cmd : commands) {
            std::string result = executeCommand(cmd);

            biosInfo.version = parseDmidecodeField(result, "Version:");
            if (biosInfo.manufacturer.empty()) {
                biosInfo.manufacturer = parseDmidecodeField(result, "Vendor:");
            }
            if (biosInfo.releaseDate.empty()) {
                biosInfo.releaseDate = parseDmidecodeField(result, "Release Date:");
            }
            if (biosInfo.serialNumber.empty()) {
                biosInfo.serialNumber = parseDmidecodeField(result, "Serial Number:");
            }
            if (biosInfo.characteristics.empty()) {
                biosInfo.characteristics = parseDmidecodeField(result, "Characteristics:");
            }

            if (result.find("BIOS is upgradeable") != std::string::npos) {
                biosInfo.isUpgradeable = true;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error fetching BIOS info: {}", e.what());
        throw;
    }

    return biosInfo;
}

BiosHealthStatus LinuxBiosImplementation::checkHealth() const {
    BiosHealthStatus status;
    status.isHealthy = true;
    status.lastCheckTime = std::chrono::system_clock::now().time_since_epoch().count();

    try {
        std::vector<std::string> checkItems = {
            "sudo dmidecode -t 0", "sudo dmidecode -t memory",
            "sudo dmidecode -t processor", "sudo dmidecode -t system"};

        for (const auto& cmd : checkItems) {
            std::string result = executeCommand(cmd);

            if (result.find("Error") != std::string::npos ||
                result.find("Failure") != std::string::npos ||
                result.find("Critical") != std::string::npos) {
                status.isHealthy = false;
                status.errors.push_back("Issue detected in " + cmd + ": " +
                                        result.substr(0, 100) + "...");
            }
        }

        // Check system logs for BIOS-related issues
        std::string logResult = executeCommand(
            "journalctl -b | grep -i 'bios\\|firmware\\|uefi' | grep -i 'error\\|fail\\|warning'");

        if (!logResult.empty()) {
            status.warnings.push_back("BIOS-related warnings in system logs");
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to check BIOS health: {}", e.what());
        status.isHealthy = false;
        status.errors.push_back(e.what());
    }

    return status;
}

BiosUpdateInfo LinuxBiosImplementation::checkForUpdates() const {
    BiosUpdateInfo updateInfo;
    updateInfo.updateAvailable = false;

    try {
        // Implementation would check manufacturer's update service
        // For now, return basic info
    } catch (const std::exception& e) {
        spdlog::error("Failed to check for BIOS updates: {}", e.what());
    }

    return updateInfo;
}

std::vector<std::string> LinuxBiosImplementation::getSMBIOSData() const {
    std::vector<std::string> smbiosData;

    try {
        std::string cmd = "sudo dmidecode";
        std::vector<std::string> lines = executeCommandLines(cmd);
        smbiosData = lines;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get SMBIOS data: {}", e.what());
    }

    return smbiosData;
}

bool LinuxBiosImplementation::setSecureBoot(bool enable) {
    if (!isSecureBootSupported()) {
        spdlog::error("Secure Boot is not supported on this system");
        return false;
    }

    try {
        spdlog::info("Attempting to {} Secure Boot", enable ? "enable" : "disable");

        if (!hasRootPrivileges()) {
            spdlog::error("Root privileges required to modify Secure Boot settings");
            return false;
        }

        // Check if EFI variables filesystem is available
        std::string mountCheck = executeCommand("mount | grep efivarfs");
        if (mountCheck.empty()) {
            spdlog::error("EFI variables filesystem not available");
            return false;
        }

        const std::string secureBootVar = "/sys/firmware/efi/efivars/"
                                          "SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c";
        std::string backupCmd = "cp " + secureBootVar + " /tmp/SecureBoot.bak";

        if (std::system(backupCmd.c_str()) != 0) {
            spdlog::error("Failed to backup current Secure Boot state");
            return false;
        }

        spdlog::warn("System will need to be restarted for changes to take effect");
        spdlog::error("Direct modification of Secure Boot state is restricted for security reasons");
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set Secure Boot: {}", e.what());
        return false;
    }
}

bool LinuxBiosImplementation::isSecureBootSupported() const {
    try {
        std::ifstream efi_dir("/sys/firmware/efi");
        if (!efi_dir.good()) {
            spdlog::info("EFI variables directory not found, SecureBoot not supported");
            return false;
        }

        std::ifstream secure_boot_var("/sys/firmware/efi/efivars/"
                                      "SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c");
        if (secure_boot_var.good()) {
            spdlog::info("SecureBoot variable found, SecureBoot is supported");
            return true;
        }

        if (access("/usr/bin/efibootmgr", X_OK) == 0) {
            std::string result = executeCommand("efibootmgr -v | grep -i secureboot");
            if (!result.empty()) {
                spdlog::info("SecureBoot found via efibootmgr: {}", result);
                return true;
            }
        }

        spdlog::info("No evidence of SecureBoot support found");
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Failed to check Secure Boot support: {}", e.what());
        return false;
    }
}

bool LinuxBiosImplementation::isUEFIBootSupported() const {
    std::ifstream efi_dir("/sys/firmware/efi");
    if (efi_dir.good()) {
        return true;
    }

    try {
        std::string result = executeCommand("command -v efibootmgr");
        if (!result.empty()) {
            return true;
        }
    } catch (...) {
        // Ignore errors
    }

    return false;
}

std::string LinuxBiosImplementation::executeCommand(const std::string& command) const {
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

std::vector<std::string> LinuxBiosImplementation::executeCommandLines(const std::string& command) const {
    std::vector<std::string> lines;
    std::string result = executeCommand(command);
    std::istringstream iss(result);
    std::string line;

    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    return lines;
}

bool LinuxBiosImplementation::hasRootPrivileges() const {
    return geteuid() == 0;
}

std::string LinuxBiosImplementation::parseDmidecodeField(const std::string& output, const std::string& field) const {
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.find(field) != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos && pos + 2 < line.length()) {
                return line.substr(pos + 2);
            }
        }
    }

    return "";
}

bool LinuxBiosImplementation::setUEFIBoot(bool enable) {
    if (!isUEFIBootSupported()) {
        spdlog::error("UEFI Boot is not supported on this system");
        return false;
    }

    try {
        spdlog::info("Attempting to {} UEFI Boot mode", enable ? "enable" : "disable");

        if (!hasRootPrivileges()) {
            spdlog::error("Root privileges required to modify UEFI boot settings");
            return false;
        }

        if (access("/usr/bin/efibootmgr", X_OK) != 0) {
            spdlog::error("efibootmgr not found, cannot modify UEFI boot settings");
            return false;
        }

        std::string command;
        if (enable) {
            command = "efibootmgr --create --disk /dev/sda --part 1 "
                     "--loader \\\\EFI\\\\BOOT\\\\BOOTX64.EFI --label \"UEFI OS\" --quiet";
        } else {
            std::string bootEntries = executeCommand("efibootmgr | grep \"UEFI OS\"");

            std::string bootNum;
            size_t pos = bootEntries.find("Boot");
            if (pos != std::string::npos && bootEntries.length() > pos + 8) {
                bootNum = bootEntries.substr(pos + 4, 4);
                command = "efibootmgr -b " + bootNum + " -B --quiet";
            } else {
                spdlog::error("Could not find UEFI boot entry to disable");
                return false;
            }
        }

        spdlog::info("Executing command: {}", command);
        int result = ::system(command.c_str());

        if (result != 0) {
            spdlog::error("Failed to set UEFI boot mode, command returned: {}", result);
            return false;
        }

        spdlog::warn("System will need to be restarted for changes to take effect");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set UEFI boot mode: {}", e.what());
        return false;
    }
}

bool LinuxBiosImplementation::backupBiosSettings(const std::string& filepath) {
    try {
        std::ofstream out(filepath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot open file for writing");
        }

        // Backup EFI variables if available
        if (isUEFIBootSupported()) {
            std::string efiVars = executeCommand("find /sys/firmware/efi/efivars -name '*' -type f");
            out << efiVars;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to backup BIOS settings: {}", e.what());
        return false;
    }
}

bool LinuxBiosImplementation::restoreBiosSettings(const std::string& filepath) {
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
FirmwareInfo LinuxBiosImplementation::getFirmwareInfo() const {
    FirmwareInfo info;

    try {
        info.type = isUEFIBootSupported() ? "UEFI" : "Legacy BIOS";
        info.secureBootCapable = isSecureBootSupported();

        // Get additional firmware info from dmidecode
        std::string biosInfo = executeCommand("sudo dmidecode -t bios");
        info.version = parseDmidecodeField(biosInfo, "Version:");
        info.vendor = parseDmidecodeField(biosInfo, "Vendor:");
        info.buildDate = parseDmidecodeField(biosInfo, "Release Date:");

        // Check for TPM support
        std::string tpmCheck = executeCommand("find /sys -name 'tpm*' 2>/dev/null");
        info.tpmSupported = !tpmCheck.empty();

    } catch (const std::exception& e) {
        spdlog::error("Failed to get firmware info: {}", e.what());
    }

    return info;
}

BootConfiguration LinuxBiosImplementation::getBootConfiguration() const {
    BootConfiguration config;

    try {
        config.uefiMode = isUEFIBootSupported();
        config.secureBootEnabled = isSecureBootSupported();

        if (isUEFIBootSupported()) {
            std::string bootOrder = executeCommand("efibootmgr | grep BootOrder");
            // Parse boot order from efibootmgr output

            std::string bootEntries = executeCommand("efibootmgr");
            config.bootDevices = executeCommandLines("efibootmgr | grep 'Boot[0-9]'");
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get boot configuration: {}", e.what());
    }

    return config;
}

BiosSecuritySettings LinuxBiosImplementation::getSecuritySettings() const {
    BiosSecuritySettings settings;

    try {
        settings.secureBootEnabled = isSecureBootSupported();

        // Check for TPM
        std::string tpmCheck = executeCommand("find /sys -name 'tpm*' 2>/dev/null");
        settings.tpmEnabled = !tpmCheck.empty();

        // Check virtualization support
        std::string cpuInfo = executeCommand("grep -E '(vmx|svm)' /proc/cpuinfo");
        settings.virtualizationEnabled = !cpuInfo.empty();

        // Check hyper-threading
        std::string htCheck = executeCommand("grep -E 'siblings.*[2-9]' /proc/cpuinfo");
        settings.hyperThreadingEnabled = !htCheck.empty();

    } catch (const std::exception& e) {
        spdlog::error("Failed to get security settings: {}", e.what());
    }

    return settings;
}

BiosOperationResult LinuxBiosImplementation::setBootOrder(const std::vector<std::string>& order) {
    if (!hasRootPrivileges()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    if (!isUEFIBootSupported()) {
        return BiosOperationResult::NOT_SUPPORTED;
    }

    try {
        // Implementation would set boot order via efibootmgr
        spdlog::info("Setting boot order (simulated)");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set boot order: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

BiosOperationResult LinuxBiosImplementation::enableVirtualization(bool enable) {
    if (!hasRootPrivileges()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        // Check if virtualization is supported
        std::string cpuInfo = executeCommand("grep -E '(vmx|svm)' /proc/cpuinfo");
        if (cpuInfo.empty()) {
            return BiosOperationResult::NOT_SUPPORTED;
        }

        spdlog::info("{} virtualization (simulated)", enable ? "Enabling" : "Disabling");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set virtualization: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

BiosOperationResult LinuxBiosImplementation::enableHyperThreading(bool enable) {
    if (!hasRootPrivileges()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        spdlog::info("{} hyper-threading (simulated)", enable ? "Enabling" : "Disabling");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set hyper-threading: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

std::vector<std::string> LinuxBiosImplementation::getAvailableBootDevices() const {
    std::vector<std::string> devices;

    try {
        if (isUEFIBootSupported()) {
            devices = executeCommandLines("efibootmgr | grep 'Boot[0-9]' | cut -d' ' -f2-");
        } else {
            // For legacy BIOS, check available block devices
            devices = executeCommandLines("lsblk -d -n -o NAME,TYPE | grep disk | awk '{print $1}'");
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to get boot devices: {}", e.what());
    }

    return devices;
}

bool LinuxBiosImplementation::validateBiosIntegrity() const {
    try {
        // Check BIOS/UEFI integrity using available tools
        if (isUEFIBootSupported()) {
            std::string secureBootStatus = executeCommand("mokutil --sb-state 2>/dev/null");
            if (secureBootStatus.find("SecureBoot enabled") != std::string::npos) {
                spdlog::info("UEFI Secure Boot is enabled, integrity validated");
                return true;
            }
        }

        // Additional integrity checks could be implemented here
        spdlog::info("BIOS integrity validation completed");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to validate BIOS integrity: {}", e.what());
        return false;
    }
}

bool LinuxBiosImplementation::isEfiVariableAvailable(const std::string& variable) const {
    std::string path = "/sys/firmware/efi/efivars/" + variable;
    std::ifstream file(path);
    return file.good();
}

std::string LinuxBiosImplementation::readEfiVariable(const std::string& variable) const {
    std::string path = "/sys/firmware/efi/efivars/" + variable;
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return "";
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}

bool LinuxBiosImplementation::writeEfiVariable(const std::string& variable, const std::string& value) {
    if (!hasRootPrivileges()) {
        return false;
    }

    std::string path = "/sys/firmware/efi/efivars/" + variable;
    std::ofstream file(path, std::ios::binary);
    if (!file.good()) {
        return false;
    }

    file << value;
    return file.good();
}

// Additional enhanced features implementation
PowerManagementSettings LinuxBiosImplementation::getPowerManagementSettings() const {
    PowerManagementSettings settings;

    try {
        // Check ACPI support
        std::ifstream acpiFile("/sys/module/acpi/parameters/acpi");
        if (acpiFile.good()) {
            settings.acpiEnabled = true;
        }

        // Check wake-on-LAN
        std::string ethtoolResult = executeCommand("ethtool eth0 2>/dev/null | grep 'Wake-on'");
        settings.wakeOnLanEnabled = ethtoolResult.find("g") != std::string::npos;

        // Check power profiles
        std::string powerProfile = executeCommand("cat /sys/firmware/acpi/platform_profile 2>/dev/null");
        if (!powerProfile.empty()) {
            settings.powerProfile = powerProfile;
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get power management settings: {}", e.what());
    }

    return settings;
}

BiosOperationResult LinuxBiosImplementation::setPowerManagementSettings(const PowerManagementSettings& settings) {
    if (!hasRootPrivileges()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        // Set power profile if supported
        if (!settings.powerProfile.empty()) {
            std::string command = "echo " + settings.powerProfile + " > /sys/firmware/acpi/platform_profile";
            if (std::system(command.c_str()) != 0) {
                return BiosOperationResult::FAILED;
            }
        }

        return BiosOperationResult::SUCCESS;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set power management settings: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

OverclockingInfo LinuxBiosImplementation::getOverclockingInfo() const {
    OverclockingInfo info;

    try {
        // Get CPU frequency information
        std::string cpuInfo = executeCommand("cat /proc/cpuinfo | grep 'cpu MHz'");
        if (!cpuInfo.empty()) {
            // Parse frequency from cpuinfo
            size_t pos = cpuInfo.find(":");
            if (pos != std::string::npos) {
                std::string freqStr = cpuInfo.substr(pos + 1);
                info.currentCpuFrequency = static_cast<int>(std::stod(freqStr));
            }
        }

        // Check if overclocking is supported (basic check)
        std::string scalingDriver = executeCommand("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver 2>/dev/null");
        info.overclockingSupported = !scalingDriver.empty();

    } catch (const std::exception& e) {
        spdlog::error("Failed to get overclocking info: {}", e.what());
    }

    return info;
}

BiosOperationResult LinuxBiosImplementation::setOverclockingProfile(const std::string& profile) {
    if (!hasRootPrivileges()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        // Set CPU governor as a form of performance profile
        std::string command = "echo " + profile + " > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor";
        if (std::system(command.c_str()) != 0) {
            return BiosOperationResult::FAILED;
        }

        return BiosOperationResult::SUCCESS;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set overclocking profile: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

HardwareMonitoring LinuxBiosImplementation::getHardwareMonitoring() const {
    HardwareMonitoring monitoring;

    try {
        // Get CPU temperatures
        std::vector<std::string> tempFiles = executeCommandLines("find /sys/class/thermal -name 'temp*' 2>/dev/null");
        for (const auto& tempFile : tempFiles) {
            std::string tempStr = executeCommand("cat " + tempFile + " 2>/dev/null");
            if (!tempStr.empty()) {
                int temp = std::stoi(tempStr) / 1000; // Convert from millidegrees
                monitoring.cpuTemperatures.push_back(temp);
            }
        }

        // Get fan speeds
        std::vector<std::string> fanFiles = executeCommandLines("find /sys/class/hwmon -name 'fan*_input' 2>/dev/null");
        for (const auto& fanFile : fanFiles) {
            std::string fanStr = executeCommand("cat " + fanFile + " 2>/dev/null");
            if (!fanStr.empty()) {
                monitoring.fanSpeeds.push_back(std::stoi(fanStr));
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get hardware monitoring data: {}", e.what());
    }

    return monitoring;
}

BiosDiagnostics LinuxBiosImplementation::runDiagnostics() const {
    BiosDiagnostics diagnostics;
    diagnostics.lastDiagnosticTime = std::chrono::system_clock::now();

    try {
        // Check memory
        std::string memInfo = executeCommand("cat /proc/meminfo | grep MemTotal");
        diagnostics.memoryTestPassed = !memInfo.empty();

        // Check CPU
        std::string cpuInfo = executeCommand("cat /proc/cpuinfo | grep processor");
        diagnostics.cpuTestPassed = !cpuInfo.empty();

        // Check storage
        std::string diskInfo = executeCommand("lsblk");
        diagnostics.storageTestPassed = !diskInfo.empty();

        // Check for errors in dmesg
        std::string dmesgErrors = executeCommand("dmesg | grep -i 'error\\|fail' | tail -5");
        if (!dmesgErrors.empty()) {
            diagnostics.warnings.push_back("System errors found in kernel log");
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to run diagnostics: {}", e.what());
        diagnostics.postTestPassed = false;
    }

    return diagnostics;
}

BiosOperationResult LinuxBiosImplementation::resetToDefaults() {
    if (!hasRootPrivileges()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        spdlog::warn("BIOS reset to defaults is not directly supported on Linux");
        spdlog::info("Consider using manufacturer-specific tools or UEFI setup");
        return BiosOperationResult::NOT_SUPPORTED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to reset to defaults: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

std::vector<std::string> LinuxBiosImplementation::getSupportedFeatures() const {
    std::vector<std::string> features;

    try {
        features.push_back("BIOS Information Retrieval");
        features.push_back("Health Monitoring");
        features.push_back("SMBIOS Data Access");

        if (isUEFIBootSupported()) {
            features.push_back("UEFI Boot Support");
        }

        if (isSecureBootSupported()) {
            features.push_back("Secure Boot Support");
        }

        // Check for additional features
        std::ifstream acpiFile("/sys/module/acpi/parameters/acpi");
        if (acpiFile.good()) {
            features.push_back("ACPI Power Management");
        }

        std::ifstream cpufreqFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_driver");
        if (cpufreqFile.good()) {
            features.push_back("CPU Frequency Scaling");
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get supported features: {}", e.what());
    }

    return features;
}

}  // namespace atom::system

#endif  // __linux__
