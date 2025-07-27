/**
 * @file battery_manager_demo.cpp
 * @brief Battery manager demonstration
 * 
 * This example demonstrates the advanced features of BatteryManager
 * including alerts, statistics, and data recording.
 */

#include "atom/sysinfo/battery/battery.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

using namespace atom::system::battery;

// Global flag for signal handling
std::atomic<bool> g_running{true};

// Signal handler for clean shutdown
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

// Alert callback function
void alertCallback(AlertType alertType, const BatteryInfo& info) {
    std::cout << "\n*** BATTERY ALERT ***" << std::endl;
    
    switch (alertType) {
        case AlertType::LOW_BATTERY:
            std::cout << "LOW BATTERY: " << std::fixed << std::setprecision(1)
                      << info.batteryLifePercent << "%" << std::endl;
            break;
        case AlertType::CRITICAL_BATTERY:
            std::cout << "CRITICAL BATTERY: " << std::fixed << std::setprecision(1)
                      << info.batteryLifePercent << "%" << std::endl;
            break;
        case AlertType::HIGH_TEMPERATURE:
            std::cout << "HIGH TEMPERATURE: " << std::fixed << std::setprecision(1)
                      << info.temperature << "°C" << std::endl;
            break;
        case AlertType::LOW_BATTERY_HEALTH:
            std::cout << "LOW BATTERY HEALTH: " << std::fixed << std::setprecision(1)
                      << info.getBatteryHealth() << "%" << std::endl;
            break;
        case AlertType::HIGH_CYCLE_COUNT:
            std::cout << "HIGH CYCLE COUNT: " << info.cycleCounts << " cycles" << std::endl;
            break;
        case AlertType::BATTERY_REMOVED:
            std::cout << "BATTERY REMOVED" << std::endl;
            break;
        case AlertType::BATTERY_INSERTED:
            std::cout << "BATTERY INSERTED" << std::endl;
            break;
        case AlertType::CHARGING_STARTED:
            std::cout << "CHARGING STARTED" << std::endl;
            break;
        case AlertType::CHARGING_STOPPED:
            std::cout << "CHARGING STOPPED" << std::endl;
            break;
        case AlertType::FULL_CHARGE_REACHED:
            std::cout << "FULL CHARGE REACHED" << std::endl;
            break;
    }
    
    std::cout << "*********************\n" << std::endl;
}

void printStats(const BatteryStats& stats) {
    std::cout << "\n=== Battery Statistics ===" << std::endl;
    std::cout << "Average Power Consumption: " << std::fixed << std::setprecision(2)
              << stats.averagePowerConsumption << " W" << std::endl;
    std::cout << "Total Energy Consumed: " << std::fixed << std::setprecision(2)
              << stats.totalEnergyConsumed << " Wh" << std::endl;
    std::cout << "Average Discharge Rate: " << std::fixed << std::setprecision(2)
              << stats.avgDischargeRate << " %/hour" << std::endl;
    std::cout << "Battery Level Range: " << std::fixed << std::setprecision(1)
              << stats.minBatteryLevel << "% - " << stats.maxBatteryLevel << "%" << std::endl;
    std::cout << "Temperature Range: " << std::fixed << std::setprecision(1)
              << stats.minTemperature << "°C - " << stats.maxTemperature << "°C" << std::endl;
    std::cout << "Voltage Range: " << std::fixed << std::setprecision(2)
              << stats.minVoltage << "V - " << stats.maxVoltage << "V" << std::endl;
    std::cout << "Cycle Count: " << stats.cycleCount << std::endl;
    std::cout << "Battery Health: " << std::fixed << std::setprecision(1)
              << stats.batteryHealth << "%" << std::endl;
    std::cout << "=========================" << std::endl;
}

int main() {
    // Set up signal handling
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "Battery Manager Demo" << std::endl;
    std::cout << "====================" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl << std::endl;
    
    // Check if battery is present
    auto batteryInfo = getBatteryInfo();
    if (!batteryInfo || !batteryInfo->isBatteryPresent) {
        std::cout << "No battery detected!" << std::endl;
        return 1;
    }
    
    // Get battery manager instance
    auto& manager = BatteryManager::getInstance();
    
    // Configure alert settings
    BatteryAlertSettings alertSettings;
    alertSettings.lowBatteryThreshold = 30.0f;      // Alert at 30%
    alertSettings.criticalBatteryThreshold = 10.0f; // Critical at 10%
    alertSettings.highTempThreshold = 40.0f;        // High temp at 40°C
    alertSettings.lowHealthThreshold = 70.0f;       // Low health at 70%
    alertSettings.enableTemperatureAlerts = true;
    alertSettings.enableHealthAlerts = true;
    alertSettings.enableCycleAlerts = true;
    
    manager.setAlertSettings(alertSettings);
    manager.setAlertCallback(alertCallback);
    
    // Start recording to a log file
    std::string logFile = "battery_log.csv";
    if (manager.startRecording(logFile)) {
        std::cout << "Started recording battery data to: " << logFile << std::endl;
    } else {
        std::cout << "Failed to start recording, continuing with memory-only mode" << std::endl;
        manager.startRecording();  // Memory-only recording
    }
    
    // Start monitoring with 5-second interval
    if (!manager.startMonitoring(5000)) {
        std::cout << "Failed to start battery monitoring!" << std::endl;
        return 1;
    }
    
    std::cout << "Battery manager started. Monitoring every 5 seconds." << std::endl;
    std::cout << "Alerts configured:" << std::endl;
    std::cout << "  - Low battery: " << alertSettings.lowBatteryThreshold << "%" << std::endl;
    std::cout << "  - Critical battery: " << alertSettings.criticalBatteryThreshold << "%" << std::endl;
    std::cout << "  - High temperature: " << alertSettings.highTempThreshold << "°C" << std::endl;
    std::cout << "  - Low health: " << alertSettings.lowHealthThreshold << "%" << std::endl;
    std::cout << std::endl;
    
    // Main loop - print statistics every 30 seconds
    auto lastStatsTime = std::chrono::steady_clock::now();
    const auto statsInterval = std::chrono::seconds(30);
    
    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        
        if (now - lastStatsTime >= statsInterval) {
            const auto& stats = manager.getStats();
            printStats(stats);
            
            // Print history summary
            auto history = manager.getHistory(10);  // Last 10 entries
            if (!history.empty()) {
                std::cout << "\nRecent history (" << history.size() << " entries):" << std::endl;
                for (const auto& entry : history) {
                    auto time = std::chrono::system_clock::to_time_t(entry.first);
                    std::cout << "  " << std::put_time(std::localtime(&time), "%H:%M:%S")
                              << " - " << std::fixed << std::setprecision(1)
                              << entry.second.batteryLifePercent << "%" << std::endl;
                }
            }
            
            lastStatsTime = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Stop monitoring and recording
    manager.stopMonitoring();
    manager.stopRecording();
    
    std::cout << "\nFinal statistics:" << std::endl;
    const auto& finalStats = manager.getStats();
    printStats(finalStats);
    
    std::cout << "Battery manager demo completed." << std::endl;
    
    return 0;
}
