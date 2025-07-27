/**
 * @file adaptive_power.cpp
 * @brief Adaptive power management example
 */

#include "atom/sysinfo/battery/battery.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

using namespace atom::system::battery;

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "Adaptive Power Management Example" << std::endl;
    std::cout << "=================================" << std::endl << std::endl;
    
    // Show available profiles
    auto profiles = AdaptivePowerManager::getAvailableProfiles();
    std::cout << "Available optimization profiles:" << std::endl;
    for (const auto& profile : profiles) {
        std::cout << "  - " << profile << std::endl;
    }
    std::cout << std::endl;
    
    // Set initial profile
    AdaptivePowerManager::setOptimizationProfile("balanced");
    std::cout << "Set optimization profile to: " 
              << AdaptivePowerManager::getCurrentProfile() << std::endl;
    
    // Enable adaptive power management
    if (AdaptivePowerManager::enableAdaptivePower()) {
        std::cout << "Adaptive power management enabled" << std::endl;
        std::cout << "The system will automatically adjust power settings based on battery state" << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl << std::endl;
        
        // Monitor for a while
        while (g_running) {
            auto batteryInfo = getBatteryInfo();
            if (batteryInfo) {
                std::cout << "Battery: " << std::fixed << std::setprecision(1)
                          << batteryInfo->batteryLifePercent << "% | ";
                std::cout << "Charging: " << (batteryInfo->isCharging ? "Yes" : "No") << " | ";
                
                auto currentPlan = PowerPlanManager::getCurrentPowerPlan();
                if (currentPlan) {
                    std::cout << "Power Plan: ";
                    switch (*currentPlan) {
                        case PowerPlan::POWER_SAVER: std::cout << "Power Saver"; break;
                        case PowerPlan::BALANCED: std::cout << "Balanced"; break;
                        case PowerPlan::PERFORMANCE: std::cout << "Performance"; break;
                        default: std::cout << "Other"; break;
                    }
                }
                std::cout << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        AdaptivePowerManager::disableAdaptivePower();
        std::cout << "Adaptive power management disabled" << std::endl;
    } else {
        std::cout << "Failed to enable adaptive power management" << std::endl;
    }
    
    return 0;
}
