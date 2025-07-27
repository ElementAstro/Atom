/**
 * @file basic_battery_info.cpp
 * @brief Basic battery information example
 * 
 * This example demonstrates how to get basic battery information
 * using the new battery module.
 */

#include "atom/sysinfo/battery/battery.hpp"
#include <iostream>
#include <iomanip>

using namespace atom::system::battery;

void printBatteryInfo(const BatteryInfo& info) {
    std::cout << "=== Battery Information ===" << std::endl;
    std::cout << "Present: " << (info.isBatteryPresent ? "Yes" : "No") << std::endl;
    
    if (!info.isBatteryPresent) {
        std::cout << "No battery detected!" << std::endl;
        return;
    }
    
    std::cout << "Charging: " << (info.isCharging ? "Yes" : "No") << std::endl;
    std::cout << "Battery Level: " << std::fixed << std::setprecision(1) 
              << info.batteryLifePercent << "%" << std::endl;
    
    if (info.batteryLifeTime > 0) {
        std::cout << "Time Remaining: " << std::fixed << std::setprecision(1)
                  << info.batteryLifeTime / 60.0f << " hours" << std::endl;
    }
    
    if (info.energyNow > 0 && info.energyFull > 0) {
        std::cout << "Energy: " << std::fixed << std::setprecision(2)
                  << info.energyNow << " / " << info.energyFull << " Wh" << std::endl;
    }
    
    if (info.voltageNow > 0) {
        std::cout << "Voltage: " << std::fixed << std::setprecision(2)
                  << info.voltageNow << " V" << std::endl;
    }
    
    if (info.currentNow != 0) {
        std::cout << "Current: " << std::fixed << std::setprecision(2)
                  << info.currentNow << " A" << std::endl;
    }
    
    if (info.temperature > -100) {
        std::cout << "Temperature: " << std::fixed << std::setprecision(1)
                  << info.temperature << " °C" << std::endl;
    }
    
    if (info.cycleCounts > 0) {
        std::cout << "Cycle Count: " << info.cycleCounts << std::endl;
    }
    
    float health = info.getBatteryHealth();
    if (health > 0) {
        std::cout << "Battery Health: " << std::fixed << std::setprecision(1)
                  << health << "%" << std::endl;
    }
    
    if (!info.manufacturer.empty()) {
        std::cout << "Manufacturer: " << info.manufacturer << std::endl;
    }
    
    if (!info.model.empty()) {
        std::cout << "Model: " << info.model << std::endl;
    }
    
    if (!info.serialNumber.empty()) {
        std::cout << "Serial Number: " << info.serialNumber << std::endl;
    }
    
    std::cout << std::endl;
}

int main() {
    std::cout << "Battery Information Example" << std::endl;
    std::cout << "===========================" << std::endl << std::endl;
    
    // Get basic battery information
    std::cout << "Getting basic battery info..." << std::endl;
    auto basicInfo = getBatteryInfo();
    if (basicInfo) {
        printBatteryInfo(*basicInfo);
    } else {
        std::cout << "Failed to get basic battery information!" << std::endl;
    }
    
    // Get detailed battery information
    std::cout << "Getting detailed battery info..." << std::endl;
    auto detailedResult = getDetailedBatteryInfo();
    if (auto* detailedInfo = std::get_if<BatteryInfo>(&detailedResult)) {
        printBatteryInfo(*detailedInfo);
    } else {
        auto error = std::get<BatteryError>(detailedResult);
        std::cout << "Failed to get detailed battery information. Error: " 
                  << static_cast<int>(error) << std::endl;
    }
    
    // Get all batteries
    std::cout << "Getting all batteries..." << std::endl;
    auto allBatteries = getAllBatteries();
    if (!allBatteries.isEmpty()) {
        std::cout << "Found " << allBatteries.size() << " battery(ies)" << std::endl;
        std::cout << "Active batteries: " << allBatteries.activeBatteryCount << std::endl;
        std::cout << "Total capacity: " << std::fixed << std::setprecision(2)
                  << allBatteries.totalCapacity << " Wh" << std::endl;
        std::cout << "Total energy remaining: " << std::fixed << std::setprecision(2)
                  << allBatteries.totalEnergyRemaining << " Wh" << std::endl;
        
        std::cout << "\nCombined battery info:" << std::endl;
        printBatteryInfo(allBatteries.combined);
        
        std::cout << "Individual batteries:" << std::endl;
        for (size_t i = 0; i < allBatteries.batteries.size(); ++i) {
            std::cout << "Battery " << (i + 1) << ":" << std::endl;
            printBatteryInfo(allBatteries.batteries[i]);
        }
    } else {
        std::cout << "No batteries found!" << std::endl;
    }
    
    return 0;
}
