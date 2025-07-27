#include "common.hpp"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace atom::system::battery {

auto BatteryInfo::operator==(const BatteryInfo& other) const -> bool {
    return isBatteryPresent == other.isBatteryPresent &&
           isCharging == other.isCharging &&
           batteryLifePercent == other.batteryLifePercent &&
           batteryLifeTime == other.batteryLifeTime &&
           batteryFullLifeTime == other.batteryFullLifeTime &&
           energyNow == other.energyNow && 
           energyFull == other.energyFull &&
           energyDesign == other.energyDesign &&
           voltageNow == other.voltageNow && 
           currentNow == other.currentNow &&
           temperature == other.temperature &&
           cycleCounts == other.cycleCounts &&
           manufacturer == other.manufacturer && 
           model == other.model &&
           serialNumber == other.serialNumber;
}

auto BatteryInfo::operator!=(const BatteryInfo& other) const -> bool {
    return !(*this == other);
}

auto BatteryInfo::getBatteryHealth() const -> float {
    if (energyDesign > 0) {
        return (energyFull / energyDesign) * 100.0f;
    }
    return 0.0f;
}

auto BatteryInfo::getEstimatedTimeRemaining() const -> float {
    if (powerState == PowerState::DISCHARGING && powerNow > 0) {
        return energyNow / powerNow;
    }
    
    if (currentNow > 0 && !isCharging) {
        return (energyNow / (voltageNow * currentNow));
    }
    
    return batteryLifeTime / 60.0f;
}

auto BatteryInfo::getPowerConsumption() const -> float {
    if (voltageNow > 0 && currentNow > 0) {
        return voltageNow * std::abs(currentNow);
    }
    return powerNow;
}

auto BatteryInfo::isCritical() const -> bool {
    return batteryLifePercent <= 5.0f && !isCharging;
}

auto BatteryInfo::getBatteryAge() const -> float {
    // Estimate battery age based on cycle count
    // Most lithium batteries are rated for 300-500 cycles
    constexpr float RATED_CYCLES = 500.0f;
    
    if (cycleCounts <= 0) {
        return 0.0f;
    }
    
    return std::min(100.0f, (cycleCounts / RATED_CYCLES) * 100.0f);
}

auto MultiBatteryInfo::getPrimaryBattery() const -> const BatteryInfo* {
    if (batteries.empty()) {
        return nullptr;
    }
    
    // Return the first battery by default
    if (batteries.size() == 1) {
        return &batteries[0];
    }
    
    // Find the battery with the highest energy capacity
    auto it = std::max_element(batteries.begin(), batteries.end(),
        [](const BatteryInfo& a, const BatteryInfo& b) {
            return a.energyFull < b.energyFull;
        });
    
    return &(*it);
}

}  // namespace atom::system::battery
