#ifdef __linux__

#include "linux.hpp"

#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

namespace atom::system::battery::linux {

namespace fs = std::filesystem;

auto LinuxBatteryProvider::readSysfsValue(const std::string& path) -> std::optional<std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::string value;
    std::getline(file, value);
    return value;
}

template<typename T>
auto LinuxBatteryProvider::readSysfsNumeric(const std::string& path) -> std::optional<T> {
    auto strValue = readSysfsValue(path);
    if (!strValue) {
        return std::nullopt;
    }

    try {
        if constexpr (std::is_same_v<T, float>) {
            return std::stof(*strValue);
        } else if constexpr (std::is_same_v<T, int>) {
            return std::stoi(*strValue);
        } else if constexpr (std::is_same_v<T, long>) {
            return std::stol(*strValue);
        } else {
            return static_cast<T>(std::stol(*strValue));
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to convert sysfs value '{}' to numeric: {}", *strValue, e.what());
        return std::nullopt;
    }
}

auto LinuxBatteryProvider::getBatteryPaths() -> std::vector<std::string> {
    std::vector<std::string> paths;

    try {
        for (const auto& entry : fs::directory_iterator("/sys/class/power_supply/")) {
            std::string path = entry.path().string();
            std::string typeFile = path + "/type";

            auto type = readSysfsValue(typeFile);
            if (type && *type == "Battery") {
                paths.push_back(path);
            }
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error when scanning battery paths: {}", e.what());
    }

    return paths;
}

auto LinuxBatteryProvider::convertPowerState(const std::string& status) -> PowerState {
    if (status == "Charging") {
        return PowerState::CHARGING;
    } else if (status == "Discharging") {
        return PowerState::DISCHARGING;
    } else if (status == "Full") {
        return PowerState::FULL;
    } else if (status == "Not charging") {
        return PowerState::NOT_CHARGING;
    } else {
        return PowerState::UNKNOWN;
    }
}

auto LinuxBatteryProvider::convertChemistry(const std::string& technology) -> BatteryChemistry {
    if (technology == "Li-ion" || technology == "Li-Ion") {
        return BatteryChemistry::LITHIUM_ION;
    } else if (technology == "Li-poly" || technology == "Li-Polymer") {
        return BatteryChemistry::LITHIUM_POLYMER;
    } else if (technology == "NiMH") {
        return BatteryChemistry::NICKEL_METAL_HYDRIDE;
    } else if (technology == "NiCd") {
        return BatteryChemistry::NICKEL_CADMIUM;
    } else if (technology == "Lead-acid" || technology == "VRLA") {
        return BatteryChemistry::LEAD_ACID;
    } else {
        return BatteryChemistry::UNKNOWN;
    }
}

auto LinuxBatteryProvider::getBatteryInfo() -> std::optional<BatteryInfo> {
    spdlog::debug("Getting Linux battery info via sysfs");

    auto batteryPaths = getBatteryPaths();
    if (batteryPaths.empty()) {
        spdlog::warn("No battery found in sysfs");
        return std::nullopt;
    }

    // Use the first battery found
    std::string batteryPath = batteryPaths[0];
    BatteryInfo info;

    // Check if battery is present
    auto presentOpt = readSysfsNumeric<int>(batteryPath + "/present");
    if (!presentOpt || *presentOpt == 0) {
        spdlog::debug("Battery marked as not present");
        return std::nullopt;
    }

    info.isBatteryPresent = true;

    // Get battery status
    auto statusOpt = readSysfsValue(batteryPath + "/status");
    if (statusOpt) {
        info.isCharging = (*statusOpt == "Charging" || *statusOpt == "Full");
        info.powerState = convertPowerState(*statusOpt);
    }

    // Get capacity percentage
    auto capacityOpt = readSysfsNumeric<float>(batteryPath + "/capacity");
    if (capacityOpt) {
        info.batteryLifePercent = *capacityOpt;
    }

    // Get energy values
    auto energyNowOpt = readSysfsNumeric<float>(batteryPath + "/energy_now");
    if (energyNowOpt) {
        info.energyNow = *energyNowOpt / 1000000.0f;  // Convert to Wh
    } else {
        // Try charge_now as alternative
        auto chargeNowOpt = readSysfsNumeric<float>(batteryPath + "/charge_now");
        auto voltageNowOpt = readSysfsNumeric<float>(batteryPath + "/voltage_now");
        if (chargeNowOpt && voltageNowOpt) {
            info.energyNow = (*chargeNowOpt * *voltageNowOpt) / 1000000000000.0f;  // Convert to Wh
        }
    }

    // Get voltage
    auto voltageNowOpt = readSysfsNumeric<float>(batteryPath + "/voltage_now");
    if (voltageNowOpt) {
        info.voltageNow = *voltageNowOpt / 1000000.0f;  // Convert to V
    }

    // Get current
    auto currentNowOpt = readSysfsNumeric<float>(batteryPath + "/current_now");
    if (currentNowOpt) {
        info.currentNow = *currentNowOpt / 1000000.0f;  // Convert to A
    }

    // Calculate power
    if (info.voltageNow > 0 && info.currentNow > 0) {
        info.powerNow = info.voltageNow * info.currentNow;
    }

    // Set timestamp
    info.lastUpdated = std::chrono::system_clock::now();

    spdlog::debug("Battery level: {:.2f}%, charging: {}",
                 info.batteryLifePercent, info.isCharging);

    return info;
}

auto LinuxBatteryProvider::getDetailedBatteryInfo() -> BatteryResult {
    auto basicInfo = getBatteryInfo();
    if (!basicInfo) {
        return BatteryError::READ_ERROR;
    }

    BatteryInfo info = std::move(*basicInfo);
    auto batteryPaths = getBatteryPaths();

    if (batteryPaths.empty()) {
        return info;  // Return basic info if no battery path found
    }

    std::string batteryPath = batteryPaths[0];

    // Get energy values
    auto energyFullOpt = readSysfsNumeric<float>(batteryPath + "/energy_full");
    if (energyFullOpt) {
        info.energyFull = *energyFullOpt / 1000000.0f;  // Convert to Wh
    }

    auto energyDesignOpt = readSysfsNumeric<float>(batteryPath + "/energy_full_design");
    if (energyDesignOpt) {
        info.energyDesign = *energyDesignOpt / 1000000.0f;  // Convert to Wh
    }

    // Get cycle count
    auto cycleCountOpt = readSysfsNumeric<int>(batteryPath + "/cycle_count");
    if (cycleCountOpt) {
        info.cycleCounts = *cycleCountOpt;
    }

    // Get temperature
    auto tempOpt = readSysfsNumeric<float>(batteryPath + "/temp");
    if (tempOpt) {
        info.temperature = *tempOpt / 10.0f;  // Convert to °C
    }

    // Get manufacturer
    auto manufacturerOpt = readSysfsValue(batteryPath + "/manufacturer");
    if (manufacturerOpt) {
        info.manufacturer = *manufacturerOpt;
    }

    // Get model name
    auto modelNameOpt = readSysfsValue(batteryPath + "/model_name");
    if (modelNameOpt) {
        info.model = *modelNameOpt;
    }

    // Get serial number
    auto serialNumberOpt = readSysfsValue(batteryPath + "/serial_number");
    if (serialNumberOpt) {
        info.serialNumber = *serialNumberOpt;
    }

    // Get technology
    auto technologyOpt = readSysfsValue(batteryPath + "/technology");
    if (technologyOpt) {
        info.technology = *technologyOpt;
        info.chemistry = convertChemistry(*technologyOpt);
    }

    return info;
}

auto LinuxBatteryProvider::getAllBatteries() -> MultiBatteryInfo {
    MultiBatteryInfo result;
    auto batteryPaths = getBatteryPaths();

    float totalEnergy = 0.0f;
    float totalEnergyFull = 0.0f;

    for (const auto& path : batteryPaths) {
        // Check if battery is present
        auto presentOpt = readSysfsNumeric<int>(path + "/present");
        if (!presentOpt || *presentOpt == 0) {
            continue;
        }

        BatteryInfo info;
        info.isBatteryPresent = true;

        // Get battery status
        auto statusOpt = readSysfsValue(path + "/status");
        if (statusOpt) {
            info.isCharging = (*statusOpt == "Charging" || *statusOpt == "Full");
            info.powerState = convertPowerState(*statusOpt);
        }

        // Get capacity percentage
        auto capacityOpt = readSysfsNumeric<float>(path + "/capacity");
        if (capacityOpt) {
            info.batteryLifePercent = *capacityOpt;
        }

        // Get energy values
        auto energyNowOpt = readSysfsNumeric<float>(path + "/energy_now");
        if (energyNowOpt) {
            info.energyNow = *energyNowOpt / 1000000.0f;
            totalEnergy += info.energyNow;
        }

        auto energyFullOpt = readSysfsNumeric<float>(path + "/energy_full");
        if (energyFullOpt) {
            info.energyFull = *energyFullOpt / 1000000.0f;
            totalEnergyFull += info.energyFull;
        }

        // Add more detailed info
        auto detailedResult = getDetailedBatteryInfo();
        if (auto* detailedInfo = std::get_if<BatteryInfo>(&detailedResult)) {
            info = *detailedInfo;
        }

        result.batteries.push_back(info);
        result.activeBatteryCount++;
    }

    // Calculate combined stats
    if (!result.batteries.empty()) {
        result.totalCapacity = totalEnergyFull;
        result.totalEnergyRemaining = totalEnergy;

        // Set combined info from first battery for now
        result.combined = result.batteries[0];

        // Calculate average battery percentage
        if (totalEnergyFull > 0) {
            result.combined.batteryLifePercent = (totalEnergy / totalEnergyFull) * 100.0f;
        } else {
            float avgPercent = 0.0f;
            for (const auto& battery : result.batteries) {
                avgPercent += battery.batteryLifePercent;
            }
            result.combined.batteryLifePercent = avgPercent / result.batteries.size();
        }

        // Determine combined charging state
        bool anyCharging = false;
        for (const auto& battery : result.batteries) {
            if (battery.isCharging) {
                anyCharging = true;
                break;
            }
        }
        result.combined.isCharging = anyCharging;
    }

    return result;
}

auto LinuxPowerManager::setPowerPlan(PowerPlan plan) -> std::optional<bool> {
    std::string cmd;
    switch (plan) {
        case PowerPlan::BALANCED:
            cmd = "powerprofilesctl set balanced";
            break;
        case PowerPlan::PERFORMANCE:
            cmd = "powerprofilesctl set performance";
            break;
        case PowerPlan::POWER_SAVER:
            cmd = "powerprofilesctl set power-saver";
            break;
        case PowerPlan::CUSTOM:
            spdlog::error("Custom power plans not settable via powerprofilesctl enum");
            return std::nullopt;
        default:
            spdlog::error("Invalid power plan specified for Linux");
            return std::nullopt;
    }

    spdlog::info("Setting Linux power profile: {}", cmd);
    int result = std::system(cmd.c_str());
    if (WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        spdlog::info("Linux power profile successfully changed");
        return true;
    } else {
        spdlog::error("Failed to set Linux power profile: exit status {}", WEXITSTATUS(result));
        return false;
    }
}

auto LinuxPowerManager::getCurrentPowerPlan() -> std::optional<PowerPlan> {
    std::string cmd = "powerprofilesctl get";
    std::string currentProfile;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        spdlog::error("Failed to run 'powerprofilesctl get'");
        return std::nullopt;
    }

    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        currentProfile = buffer;
        currentProfile.erase(currentProfile.find_last_not_of(" \n\r\t") + 1);
    }
    pclose(pipe);

    if (currentProfile.empty()) {
        spdlog::warn("Could not determine current power profile");
        return std::nullopt;
    }

    spdlog::debug("Current Linux power profile: {}", currentProfile);
    if (currentProfile == "power-saver") {
        return PowerPlan::POWER_SAVER;
    } else if (currentProfile == "balanced") {
        return PowerPlan::BALANCED;
    } else if (currentProfile == "performance") {
        return PowerPlan::PERFORMANCE;
    } else {
        return PowerPlan::CUSTOM;
    }
}

auto LinuxPowerManager::getAvailablePowerPlans() -> std::vector<std::string> {
    std::vector<std::string> plans;
    std::string cmd = "powerprofilesctl list";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        spdlog::error("Failed to run 'powerprofilesctl list'");
        plans = {"balanced", "performance", "power-saver"};
        return plans;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line = buffer;
        size_t nameStart = line.find_first_not_of(" *");
        size_t nameEnd = line.find_last_of(":");
        if (nameStart != std::string::npos && nameEnd != std::string::npos &&
            nameStart < nameEnd) {
            plans.push_back(line.substr(nameStart, nameEnd - nameStart));
        }
    }
    pclose(pipe);

    if (plans.empty()) {
        spdlog::warn("Failed to parse powerprofilesctl output, using defaults");
        plans = {"balanced", "performance", "power-saver"};
    }

    return plans;
}

}  // namespace atom::system::battery::linux

#endif  // __linux__
