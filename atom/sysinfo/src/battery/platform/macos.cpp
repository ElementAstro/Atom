#ifdef __APPLE__

#include "macos.hpp"

#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <CoreFoundation/CoreFoundation.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace atom::system::battery::macos {

template <typename CFType>
struct CFDeleter {
    void operator()(CFType ref) {
        if (ref)
            CFRelease(ref);
    }
};

template <typename CFType>
using CFUniquePtr = std::unique_ptr<std::remove_pointer_t<CFType>, CFDeleter<CFType>>;

auto MacOSBatteryProvider::getStringValue(void* dict, const char* key) -> std::string {
    auto cfDict = static_cast<CFDictionaryRef>(dict);
    auto cfKey = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    CFUniquePtr<CFStringRef> keyPtr(cfKey);
    
    if (auto value = static_cast<CFStringRef>(CFDictionaryGetValue(cfDict, cfKey))) {
        char buffer[256];
        if (CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
            return buffer;
        }
    }
    return "";
}

auto MacOSBatteryProvider::getIntValue(void* dict, const char* key) -> int {
    auto cfDict = static_cast<CFDictionaryRef>(dict);
    auto cfKey = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    CFUniquePtr<CFStringRef> keyPtr(cfKey);
    
    if (auto value = static_cast<CFNumberRef>(CFDictionaryGetValue(cfDict, cfKey))) {
        int intVal;
        if (CFNumberGetValue(value, kCFNumberIntType, &intVal)) {
            return intVal;
        }
    }
    return 0;
}

auto MacOSBatteryProvider::getFloatValue(void* dict, const char* key, float scale) -> float {
    auto cfDict = static_cast<CFDictionaryRef>(dict);
    auto cfKey = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    CFUniquePtr<CFStringRef> keyPtr(cfKey);
    
    if (auto value = static_cast<CFNumberRef>(CFDictionaryGetValue(cfDict, cfKey))) {
        double doubleVal;
        if (CFNumberGetValue(value, kCFNumberDoubleType, &doubleVal)) {
            return static_cast<float>(doubleVal * scale);
        }
    }
    return 0.0f;
}

auto MacOSBatteryProvider::getBoolValue(void* dict, const char* key) -> bool {
    auto cfDict = static_cast<CFDictionaryRef>(dict);
    auto cfKey = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    CFUniquePtr<CFStringRef> keyPtr(cfKey);
    
    if (auto value = static_cast<CFBooleanRef>(CFDictionaryGetValue(cfDict, cfKey))) {
        return CFBooleanGetValue(value);
    }
    return false;
}

auto MacOSBatteryProvider::convertPowerState(const std::string& state, bool isCharging) -> PowerState {
    if (isCharging) {
        return PowerState::CHARGING;
    } else if (state == "AC Power") {
        return PowerState::FULL;
    } else if (state == "Battery Power") {
        return PowerState::DISCHARGING;
    } else {
        return PowerState::UNKNOWN;
    }
}

auto MacOSBatteryProvider::convertChemistry(const std::string& type) -> BatteryChemistry {
    if (type.find("Lithium") != std::string::npos) {
        if (type.find("Polymer") != std::string::npos) {
            return BatteryChemistry::LITHIUM_POLYMER;
        } else {
            return BatteryChemistry::LITHIUM_ION;
        }
    }
    return BatteryChemistry::UNKNOWN;
}

auto MacOSBatteryProvider::getBatteryInfo() -> std::optional<BatteryInfo> {
    spdlog::debug("Getting macOS battery info via IOPowerSources");
    
    CFUniquePtr<CFTypeRef> powerSourcesInfo(IOPSCopyPowerSourcesInfo());
    if (!powerSourcesInfo) {
        spdlog::error("Failed to copy power sources info");
        return std::nullopt;
    }

    CFUniquePtr<CFArrayRef> powerSources(IOPSCopyPowerSourcesList(powerSourcesInfo.get()));
    if (!powerSources) {
        spdlog::error("Failed to copy power sources list");
        return std::nullopt;
    }

    CFIndex count = CFArrayGetCount(powerSources.get());
    if (count == 0) {
        spdlog::warn("No power sources found");
        return std::nullopt;
    }

    // Get the first power source (usually the internal battery)
    CFDictionaryRef powerSource = static_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(powerSources.get(), 0));

    BatteryInfo info;
    
    // Check if battery is present
    info.isBatteryPresent = getBoolValue(powerSource, kIOPSIsPresentKey);
    if (!info.isBatteryPresent) {
        spdlog::debug("Battery marked as not present");
        return std::nullopt;
    }

    // Get charging state
    info.isCharging = getBoolValue(powerSource, kIOPSIsChargingKey);

    // Get capacity percentage
    info.batteryLifePercent = static_cast<float>(getIntValue(powerSource, kIOPSCurrentCapacityKey));

    // Get time to empty
    int timeToEmpty = getIntValue(powerSource, kIOPSTimeToEmptyKey);
    if (timeToEmpty > 0) {
        info.batteryLifeTime = static_cast<float>(timeToEmpty);
    }

    // Get time to full charge
    int timeToFull = getIntValue(powerSource, kIOPSTimeToFullChargeKey);
    if (timeToFull > 0) {
        info.batteryFullLifeTime = static_cast<float>(timeToFull);
    }

    // Determine power state
    std::string powerSourceState = getStringValue(powerSource, kIOPSPowerSourceStateKey);
    info.powerState = convertPowerState(powerSourceState, info.isCharging);

    // Set timestamp
    info.lastUpdated = std::chrono::system_clock::now();

    spdlog::debug("Battery info - charging: {}, level: {:.2f}%, time: {:.2f}min",
                  info.isCharging, info.batteryLifePercent, info.batteryLifeTime);

    return info;
}

auto MacOSBatteryProvider::getDetailedBatteryInfo() -> BatteryResult {
    auto basicInfo = getBatteryInfo();
    if (!basicInfo) {
        return BatteryError::READ_ERROR;
    }

    BatteryInfo info = std::move(*basicInfo);

    CFUniquePtr<CFTypeRef> powerSourcesInfo(IOPSCopyPowerSourcesInfo());
    if (!powerSourcesInfo) {
        return BatteryError::READ_ERROR;
    }

    CFUniquePtr<CFArrayRef> powerSources(IOPSCopyPowerSourcesList(powerSourcesInfo.get()));
    if (!powerSources || CFArrayGetCount(powerSources.get()) == 0) {
        return BatteryError::NOT_PRESENT;
    }

    CFDictionaryRef psDesc = static_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(powerSources.get(), 0));

    // Get manufacturer
    info.manufacturer = getStringValue(psDesc, kIOPSManufacturerKey);

    // Get model name
    info.model = getStringValue(psDesc, kIOPSDeviceNameKey);

    // Get serial number
    info.serialNumber = getStringValue(psDesc, kIOPSSerialNumberKey);

    // Get cycle count
    info.cycleCounts = getIntValue(psDesc, kIOPSCycleCountKey);

    // Get temperature (in 0.01°C units)
    info.temperature = getFloatValue(psDesc, kIOPSTemperatureKey, 0.01f);

    // Get voltage (in mV)
    info.voltageNow = getFloatValue(psDesc, kIOPSVoltageKey, 0.001f);

    // Get current (in mA)
    info.currentNow = getFloatValue(psDesc, kIOPSAmperageKey, 0.001f);

    // Calculate power
    if (info.voltageNow > 0 && info.currentNow > 0) {
        info.powerNow = info.voltageNow * std::abs(info.currentNow);
    }

    // Get max capacity
    int maxCapacity = getIntValue(psDesc, kIOPSMaxCapacityKey);
    int currentCapacity = getIntValue(psDesc, kIOPSCurrentCapacityKey);
    
    if (maxCapacity > 0 && currentCapacity > 0) {
        // Estimate energy values based on capacity and voltage
        if (info.voltageNow > 0) {
            info.energyFull = (maxCapacity * info.voltageNow) / 1000.0f;  // Convert to Wh
            info.energyNow = (currentCapacity * info.voltageNow) / 1000.0f;
        }
    }

    // Get design capacity
    int designCapacity = getIntValue(psDesc, kIOPSDesignCapacityKey);
    if (designCapacity > 0 && info.voltageNow > 0) {
        info.energyDesign = (designCapacity * info.voltageNow) / 1000.0f;
    }

    // Get battery type/technology
    std::string batteryType = getStringValue(psDesc, kIOPSTypeKey);
    if (!batteryType.empty()) {
        info.technology = batteryType;
        info.chemistry = convertChemistry(batteryType);
    }

    return info;
}

auto MacOSBatteryProvider::getAllBatteries() -> MultiBatteryInfo {
    MultiBatteryInfo result;
    
    CFUniquePtr<CFTypeRef> powerSourcesInfo(IOPSCopyPowerSourcesInfo());
    if (!powerSourcesInfo) {
        return result;
    }

    CFUniquePtr<CFArrayRef> powerSources(IOPSCopyPowerSourcesList(powerSourcesInfo.get()));
    if (!powerSources) {
        return result;
    }

    CFIndex count = CFArrayGetCount(powerSources.get());
    
    for (CFIndex i = 0; i < count; ++i) {
        CFDictionaryRef powerSource = static_cast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(powerSources.get(), i));
        
        // Check if this is a battery (not AC adapter)
        std::string type = getStringValue(powerSource, kIOPSTypeKey);
        if (type != "InternalBattery" && type != "Battery") {
            continue;
        }
        
        // Check if battery is present
        bool isPresent = getBoolValue(powerSource, kIOPSIsPresentKey);
        if (!isPresent) {
            continue;
        }
        
        BatteryInfo info;
        info.isBatteryPresent = true;
        info.isCharging = getBoolValue(powerSource, kIOPSIsChargingKey);
        info.batteryLifePercent = static_cast<float>(getIntValue(powerSource, kIOPSCurrentCapacityKey));
        
        // Get detailed information
        info.manufacturer = getStringValue(powerSource, kIOPSManufacturerKey);
        info.model = getStringValue(powerSource, kIOPSDeviceNameKey);
        info.serialNumber = getStringValue(powerSource, kIOPSSerialNumberKey);
        info.cycleCounts = getIntValue(powerSource, kIOPSCycleCountKey);
        info.temperature = getFloatValue(powerSource, kIOPSTemperatureKey, 0.01f);
        info.voltageNow = getFloatValue(powerSource, kIOPSVoltageKey, 0.001f);
        info.currentNow = getFloatValue(powerSource, kIOPSAmperageKey, 0.001f);
        
        // Calculate power
        if (info.voltageNow > 0 && info.currentNow > 0) {
            info.powerNow = info.voltageNow * std::abs(info.currentNow);
        }
        
        // Set timestamp
        info.lastUpdated = std::chrono::system_clock::now();
        
        result.batteries.push_back(info);
        result.activeBatteryCount++;
        
        // Add to totals
        result.totalEnergyRemaining += info.energyNow;
        result.totalCapacity += info.energyFull;
    }
    
    // Set combined information
    if (!result.batteries.empty()) {
        result.combined = result.batteries[0];  // Use first battery as base
        
        // Calculate average battery percentage
        if (result.batteries.size() > 1) {
            float totalPercent = 0.0f;
            for (const auto& battery : result.batteries) {
                totalPercent += battery.batteryLifePercent;
            }
            result.combined.batteryLifePercent = totalPercent / result.batteries.size();
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

auto MacOSPowerManager::getCurrentPowerPlan() -> std::optional<PowerPlan> {
    spdlog::debug("macOS uses automatic power management; reporting as Balanced");
    return PowerPlan::BALANCED;
}

auto MacOSPowerManager::getAvailablePowerPlans() -> std::vector<std::string> {
    return {"Automatic"};
}

auto MacOSPowerManager::setPowerPlan(PowerPlan plan) -> std::optional<bool> {
    spdlog::warn("Direct power plan setting not standard on macOS");
    return std::nullopt;
}

}  // namespace atom::system::battery::macos

#endif  // __APPLE__
