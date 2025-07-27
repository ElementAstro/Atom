/**
 * @file battery_monitoring.cpp
 * @brief Battery monitoring example
 *
 * This example demonstrates how to monitor battery status changes
 * using the BatteryMonitor class.
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

// Battery status callback
void batteryCallback(const BatteryInfo& info) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "] ";

    std::cout << "Battery: " << std::fixed << std::setprecision(1)
              << info.batteryLifePercent << "% | ";

    std::cout << "State: ";
    if (info.isCharging) {
        std::cout << "Charging | ";
    } else {
        std::cout << "Discharging | ";
    }

    if (info.temperature > -100) {
        std::cout << "Temp: " << std::fixed << std::setprecision(1)
                  << info.temperature << "°C | ";
    }

    if (info.voltageNow > 0) {
        std::cout << "Voltage: " << std::fixed << std::setprecision(2)
                  << info.voltageNow << "V | ";
    }

    if (info.currentNow != 0) {
        std::cout << "Current: " << std::fixed << std::setprecision(2)
                  << info.currentNow << "A | ";
    }

    float health = info.getBatteryHealth();
    if (health > 0) {
        std::cout << "Health: " << std::fixed << std::setprecision(1)
                  << health << "% | ";
    }

    float timeRemaining = info.getEstimatedTimeRemaining();
    if (timeRemaining > 0) {
        int hours = static_cast<int>(timeRemaining);
        int minutes = static_cast<int>((timeRemaining - hours) * 60);
        std::cout << "Time remaining: " << hours << "h " << minutes << "m";
    }

    std::cout << std::endl;
}

int main() {
    // Set up signal handling
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "Battery Monitoring Example" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl << std::endl;

    // Check if battery is present
    auto batteryInfo = getBatteryInfo();
    if (!batteryInfo || !batteryInfo->isBatteryPresent) {
        std::cout << "No battery detected!" << std::endl;
        return 1;
    }

    // Start monitoring with 2-second interval
    if (!BatteryMonitor::startMonitoring(batteryCallback, 2000)) {
        std::cout << "Failed to start battery monitoring!" << std::endl;
        return 1;
    }

    std::cout << "Battery monitoring started. Updates will appear every 2 seconds." << std::endl;
    std::cout << "Monitoring will continue until you press Ctrl+C." << std::endl << std::endl;

    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop monitoring
    BatteryMonitor::stopMonitoring();
    std::cout << "Battery monitoring stopped." << std::endl;

    return 0;
}
