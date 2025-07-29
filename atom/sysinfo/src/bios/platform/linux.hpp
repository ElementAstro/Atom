#ifndef ATOM_SYSINFO_BIOS_LINUX_HPP
#define ATOM_SYSINFO_BIOS_LINUX_HPP

#include "common.hpp"

#ifdef __linux__
#include <unistd.h>
#include <array>
#include <memory>

namespace atom::system {

/**
 * @brief Linux-specific BIOS implementation
 */
class LinuxBiosImplementation : public BiosImplementation {
public:
    LinuxBiosImplementation() = default;
    ~LinuxBiosImplementation() override = default;

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
    std::string executeCommand(const std::string& command) const;
    std::vector<std::string> executeCommandLines(const std::string& command) const;
    bool hasRootPrivileges() const;
    std::string parseDmidecodeField(const std::string& output, const std::string& field) const;
    bool isEfiVariableAvailable(const std::string& variable) const;
    std::string readEfiVariable(const std::string& variable) const;
    bool writeEfiVariable(const std::string& variable, const std::string& value);
};

}  // namespace atom::system

#endif  // __linux__
#endif  // ATOM_SYSINFO_BIOS_LINUX_HPP
