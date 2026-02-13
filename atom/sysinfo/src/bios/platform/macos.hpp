#ifndef ATOM_SYSINFO_BIOS_MACOS_HPP
#define ATOM_SYSINFO_BIOS_MACOS_HPP

#include "common.hpp"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <memory>

namespace atom::system {

/**
 * @brief macOS-specific BIOS implementation
 */
class MacOSBiosImplementation : public BiosImplementation {
public:
    MacOSBiosImplementation() = default;
    ~MacOSBiosImplementation() override = default;

    BiosInfoData fetchBiosInfo() override;
    BiosHealthStatus checkHealth() const override;
    BiosUpdateInfo checkForUpdates() const override;
    std::vector<std::string> getSMBIOSData() const override;
    bool setSecureBoot(bool enable) override;
    bool setUEFIBoot(bool enable) override;
    bool backupBiosSettings(const std::string& filepath) override;
    bool restoreBiosSettings(const std::string& filepath) override;
    bool isSecureBootSupported() const override;
    bool isUEFIBootSupported() const override;

    // Enhanced features
    FirmwareInfo getFirmwareInfo() const override;
    BootConfiguration getBootConfiguration() const override;
    BiosSecuritySettings getSecuritySettings() const override;
    BiosOperationResult setBootOrder(const std::vector<std::string>& order) override;
    BiosOperationResult enableVirtualization(bool enable) override;
    BiosOperationResult enableHyperThreading(bool enable) override;
    std::vector<std::string> getAvailableBootDevices() const override;
    bool validateBiosIntegrity() const override;

    // Additional enhanced features
    PowerManagementSettings getPowerManagementSettings() const override;
    BiosOperationResult setPowerManagementSettings(const PowerManagementSettings& settings) override;
    OverclockingInfo getOverclockingInfo() const override;
    BiosOperationResult setOverclockingProfile(const std::string& profile) override;
    HardwareMonitoring getHardwareMonitoring() const override;
    BiosDiagnostics runDiagnostics() const override;
    BiosOperationResult resetToDefaults() override;
    std::vector<std::string> getSupportedFeatures() const override;

private:
    std::string getIORegistryProperty(const std::string& service, const std::string& property) const;
    std::string executeCommand(const std::string& command) const;
    std::vector<std::string> executeCommandLines(const std::string& command) const;
    bool hasAdminPrivileges() const;
    CFStringRef createCFString(const std::string& str) const;
    std::string cfStringToString(CFStringRef cfStr) const;
};

}  // namespace atom::system

#endif  // __APPLE__
#endif  // ATOM_SYSINFO_BIOS_MACOS_HPP
