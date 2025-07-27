#ifndef ATOM_SYSINFO_BIOS_WINDOWS_HPP
#define ATOM_SYSINFO_BIOS_WINDOWS_HPP

#include "common.hpp"

#ifdef _WIN32
#include <comdef.h>
#include <sysinfoapi.h>
#include <wbemidl.h>
#include <winbase.h>
#include <windows.h>
#include <memory>

namespace atom::system {

/**
 * @brief RAII COM initializer
 */
class ComInitializer {
public:
    explicit ComInitializer(COINIT coinit = COINIT_MULTITHREADED);
    ~ComInitializer();

    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;

    void release();

private:
    bool initialized_;
};

/**
 * @brief Smart pointer for COM interfaces
 */
template <typename T>
class ComPtr {
public:
    ComPtr() : ptr_(nullptr) {}
    explicit ComPtr(T* ptr) : ptr_(ptr) {}
    ~ComPtr();

    ComPtr(ComPtr&& other) noexcept;
    ComPtr& operator=(ComPtr&& other) noexcept;

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* get() const { return ptr_; }
    T** getAddressOf() { return &ptr_; }
    T* operator->() const { return ptr_; }
    T* release();

private:
    T* ptr_;
};

/**
 * @brief Windows-specific BIOS implementation
 */
class WindowsBiosImplementation : public BiosImplementation {
public:
    WindowsBiosImplementation() = default;
    ~WindowsBiosImplementation() override = default;

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
    std::string getManufacturerUpdateUrl() const;
    bool isElevated() const;
    std::string executeWMIQuery(const std::wstring& query, const std::wstring& property) const;
    std::vector<std::string> executeWMIQueryMultiple(const std::wstring& query, const std::wstring& property) const;
};

}  // namespace atom::system

#endif  // _WIN32
#endif  // ATOM_SYSINFO_BIOS_WINDOWS_HPP
