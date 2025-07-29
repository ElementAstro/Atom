/**
 * @file power_plan_control.cpp
 * @brief Power plan control example
 * 
 * This example demonstrates how to control system power plans.
 */

#include "../battery.hpp"
#include <iostream>
#include <string>

using namespace atom::system::battery;

void printCurrentPowerPlan() {
    auto currentPlan = PowerPlanManager::getCurrentPowerPlan();
    if (currentPlan) {
        std::cout << "Current power plan: ";
        switch (*currentPlan) {
            case PowerPlan::BALANCED:
                std::cout << "Balanced";
                break;
            case PowerPlan::PERFORMANCE:
                std::cout << "Performance";
                break;
            case PowerPlan::POWER_SAVER:
                std::cout << "Power Saver";
                break;
            case PowerPlan::CUSTOM:
                std::cout << "Custom";
                break;
            case PowerPlan::ADAPTIVE:
                std::cout << "Adaptive";
                break;
            case PowerPlan::GAMING:
                std::cout << "Gaming";
                break;
            case PowerPlan::PRESENTATION:
                std::cout << "Presentation";
                break;
        }
        std::cout << std::endl;
    } else {
        std::cout << "Unable to determine current power plan" << std::endl;
    }
}

void printAvailablePowerPlans() {
    auto plans = PowerPlanManager::getAvailablePowerPlans();
    std::cout << "Available power plans:" << std::endl;
    for (size_t i = 0; i < plans.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << plans[i] << std::endl;
    }
}

bool setPowerPlan(PowerPlan plan) {
    std::cout << "Setting power plan to: ";
    switch (plan) {
        case PowerPlan::BALANCED:
            std::cout << "Balanced";
            break;
        case PowerPlan::PERFORMANCE:
            std::cout << "Performance";
            break;
        case PowerPlan::POWER_SAVER:
            std::cout << "Power Saver";
            break;
        default:
            std::cout << "Unknown";
            break;
    }
    std::cout << "... ";
    
    auto result = PowerPlanManager::setPowerPlan(plan);
    if (result && *result) {
        std::cout << "SUCCESS" << std::endl;
        return true;
    } else if (result && !*result) {
        std::cout << "FAILED" << std::endl;
        return false;
    } else {
        std::cout << "NOT SUPPORTED" << std::endl;
        return false;
    }
}

int main() {
    std::cout << "Power Plan Control Example" << std::endl;
    std::cout << "==========================" << std::endl << std::endl;
    
    // Show current power plan
    printCurrentPowerPlan();
    std::cout << std::endl;
    
    // Show available power plans
    printAvailablePowerPlans();
    std::cout << std::endl;
    
    // Get battery info to make intelligent decisions
    auto batteryInfo = getBatteryInfo();
    if (batteryInfo && batteryInfo->isBatteryPresent) {
        std::cout << "Battery detected:" << std::endl;
        std::cout << "  Level: " << batteryInfo->batteryLifePercent << "%" << std::endl;
        std::cout << "  Charging: " << (batteryInfo->isCharging ? "Yes" : "No") << std::endl;
        std::cout << std::endl;
        
        // Demonstrate automatic power plan selection based on battery state
        std::cout << "Demonstrating automatic power plan selection:" << std::endl;
        
        PowerPlan recommendedPlan;
        if (batteryInfo->batteryLifePercent <= 20.0f && !batteryInfo->isCharging) {
            recommendedPlan = PowerPlan::POWER_SAVER;
            std::cout << "Low battery detected - recommending Power Saver mode" << std::endl;
        } else if (batteryInfo->isCharging && batteryInfo->batteryLifePercent > 80.0f) {
            recommendedPlan = PowerPlan::PERFORMANCE;
            std::cout << "High battery and charging - recommending Performance mode" << std::endl;
        } else {
            recommendedPlan = PowerPlan::BALANCED;
            std::cout << "Normal conditions - recommending Balanced mode" << std::endl;
        }
        
        // Apply the recommended plan
        if (setPowerPlan(recommendedPlan)) {
            std::cout << "Power plan applied successfully!" << std::endl;
        }
        
    } else {
        std::cout << "No battery detected - demonstrating power plan switching anyway" << std::endl;
        
        // Demonstrate switching between different power plans
        std::cout << "\nDemonstrating power plan switching:" << std::endl;
        
        // Try to set to Performance
        if (setPowerPlan(PowerPlan::PERFORMANCE)) {
            std::cout << "Switched to Performance mode" << std::endl;
            printCurrentPowerPlan();
        }
        
        std::cout << std::endl;
        
        // Try to set to Power Saver
        if (setPowerPlan(PowerPlan::POWER_SAVER)) {
            std::cout << "Switched to Power Saver mode" << std::endl;
            printCurrentPowerPlan();
        }
        
        std::cout << std::endl;
        
        // Try to set back to Balanced
        if (setPowerPlan(PowerPlan::BALANCED)) {
            std::cout << "Switched back to Balanced mode" << std::endl;
            printCurrentPowerPlan();
        }
    }
    
    std::cout << std::endl;
    std::cout << "Power plan control demo completed." << std::endl;
    
    return 0;
}
