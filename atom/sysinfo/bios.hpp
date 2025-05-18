#ifndef ATOM_SYSINFO_BIOS_HPP
#define ATOM_SYSINFO_BIOS_HPP

#include <chrono>
#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

struct BiosInfoData {
    std::string version;
    std::string manufacturer;
    std::string releaseDate;
    std::string serialNumber;
    std::string characteristics;
    bool isUpgradeable;
    std::chrono::system_clock::time_point lastUpdate;

    bool isValid() const;
    std::string toString() const;
} ATOM_ALIGNAS(128);

struct BiosHealthStatus {
    bool isHealthy;
    int biosAgeInDays;
    std::string lastCheckTime;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

struct BiosUpdateInfo {
    std::string currentVersion;
    std::string latestVersion;
    bool updateAvailable;
    std::string updateUrl;
    std::string releaseNotes;
};

class BiosInfo {
public:
    static BiosInfo& getInstance();
    const BiosInfoData& getBiosInfo(bool forceUpdate = false);
    bool refreshBiosInfo();

    BiosHealthStatus checkHealth() const;
    BiosUpdateInfo checkForUpdates() const;
    std::vector<std::string> getSMBIOSData() const;
    bool setSecureBoot(bool enable);
    bool setUEFIBoot(bool enable);
    bool backupBiosSettings(const std::string& filepath);
    bool restoreBiosSettings(const std::string& filepath);
    bool isSecureBootSupported();
    bool isUEFIBootSupported();

private:
    BiosInfo() = default;
    BiosInfoData fetchBiosInfo();

    std::string getManufacturerUpdateUrl() const;

    BiosInfoData cachedInfo;
    std::chrono::system_clock::time_point cacheTime;
    static constexpr auto CACHE_DURATION = std::chrono::minutes(5);
};

}  // namespace atom::system

#endif  // ATOM_SYSINFO_BIOS_HPP
