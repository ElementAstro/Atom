/**
 * @file battery_calibration.cpp
 * @brief Battery calibration example
 * 
 * This example demonstrates battery calibration functionality.
 */

#include "../battery.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

using namespace atom::system::battery;

// Global flag for signal handling
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", stopping calibration..." << std::endl;
    g_running = false;
    BatteryCalibrator::stopCalibration();
}

void printCalibrationData(const CalibrationData& data) {
    std::cout << "\n=== Calibration Data ===" << std::endl;
    std::cout << "Actual Capacity: " << std::fixed << std::setprecision(2)
              << data.actualCapacity << " Wh" << std::endl;
    std::cout << "Design Capacity: " << std::fixed << std::setprecision(2)
              << data.designCapacity << " Wh" << std::endl;
    std::cout << "Calibration Accuracy: " << std::fixed << std::setprecision(1)
              << data.calibrationAccuracy << "%" << std::endl;
    std::cout << "Calibration Cycles: " << data.calibrationCycles << std::endl;
    std::cout << "Needs Calibration: " << (data.needsCalibration ? "Yes" : "No") << std::endl;
    
    if (data.lastCalibration != std::chrono::system_clock::time_point{}) {
        auto time = std::chrono::system_clock::to_time_t(data.lastCalibration);
        std::cout << "Last Calibration: " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << std::endl;
    } else {
        std::cout << "Last Calibration: Never" << std::endl;
    }
    std::cout << "========================" << std::endl;
}

int main() {
    // Set up signal handling
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "Battery Calibration Example" << std::endl;
    std::cout << "===========================" << std::endl << std::endl;
    
    // Check if battery is present
    auto batteryInfo = getBatteryInfo();
    if (!batteryInfo || !batteryInfo->isBatteryPresent) {
        std::cout << "No battery detected!" << std::endl;
        return 1;
    }
    
    std::cout << "Battery detected:" << std::endl;
    std::cout << "  Level: " << std::fixed << std::setprecision(1) 
              << batteryInfo->batteryLifePercent << "%" << std::endl;
    std::cout << "  Charging: " << (batteryInfo->isCharging ? "Yes" : "No") << std::endl;
    
    if (batteryInfo->energyFull > 0) {
        std::cout << "  Energy Full: " << std::fixed << std::setprecision(2)
                  << batteryInfo->energyFull << " Wh" << std::endl;
    }
    
    if (batteryInfo->energyDesign > 0) {
        std::cout << "  Energy Design: " << std::fixed << std::setprecision(2)
                  << batteryInfo->energyDesign << " Wh" << std::endl;
        std::cout << "  Battery Health: " << std::fixed << std::setprecision(1)
                  << batteryInfo->getBatteryHealth() << "%" << std::endl;
    }
    
    std::cout << std::endl;
    
    // Show current calibration data
    auto calibData = BatteryCalibrator::getCalibrationData();
    printCalibrationData(calibData);
    
    // Check if calibration is needed
    bool needsCalibration = BatteryCalibrator::needsCalibration();
    std::cout << "\nCalibration needed: " << (needsCalibration ? "Yes" : "No") << std::endl;
    
    if (needsCalibration) {
        std::cout << "\nRecommendation: Your battery would benefit from calibration." << std::endl;
        std::cout << "This process involves a full discharge followed by a full charge." << std::endl;
    }
    
    // Check if already calibrating
    if (BatteryCalibrator::isCalibrating()) {
        std::cout << "\nCalibration is already in progress!" << std::endl;
        std::cout << "Progress: " << std::fixed << std::setprecision(1)
                  << BatteryCalibrator::getCalibrationProgress() << "%" << std::endl;
    } else {
        std::cout << "\nWould you like to start battery calibration? (y/n): ";
        char choice;
        std::cin >> choice;
        
        if (choice == 'y' || choice == 'Y') {
            std::cout << "\nStarting battery calibration..." << std::endl;
            std::cout << "WARNING: This process may take several hours!" << std::endl;
            std::cout << "The battery will be fully discharged and then fully charged." << std::endl;
            std::cout << "Press Ctrl+C to cancel at any time." << std::endl << std::endl;
            
            if (BatteryCalibrator::startCalibration()) {
                std::cout << "Calibration started successfully!" << std::endl;
                
                // Monitor calibration progress
                while (g_running && BatteryCalibrator::isCalibrating()) {
                    float progress = BatteryCalibrator::getCalibrationProgress();
                    
                    // Create a simple progress bar
                    int barWidth = 50;
                    int pos = static_cast<int>(barWidth * progress / 100.0f);
                    
                    std::cout << "\rProgress: [";
                    for (int i = 0; i < barWidth; ++i) {
                        if (i < pos) std::cout << "=";
                        else if (i == pos) std::cout << ">";
                        else std::cout << " ";
                    }
                    std::cout << "] " << std::fixed << std::setprecision(1) 
                              << progress << "%";
                    std::cout.flush();
                    
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                
                std::cout << std::endl;
                
                if (BatteryCalibrator::isCalibrating()) {
                    std::cout << "Calibration was cancelled." << std::endl;
                } else {
                    std::cout << "Calibration completed!" << std::endl;
                    
                    // Show updated calibration data
                    auto newCalibData = BatteryCalibrator::getCalibrationData();
                    printCalibrationData(newCalibData);
                    
                    // Show updated battery info
                    auto newBatteryInfo = getDetailedBatteryInfo();
                    if (auto* info = std::get_if<BatteryInfo>(&newBatteryInfo)) {
                        std::cout << "\nUpdated battery information:" << std::endl;
                        std::cout << "  Battery Health: " << std::fixed << std::setprecision(1)
                                  << info->getBatteryHealth() << "%" << std::endl;
                        
                        if (info->energyFull > 0 && calibData.actualCapacity > 0) {
                            float improvement = ((info->energyFull - calibData.actualCapacity) / 
                                               calibData.actualCapacity) * 100.0f;
                            if (improvement > 0) {
                                std::cout << "  Capacity improvement: +" << std::fixed << std::setprecision(1)
                                          << improvement << "%" << std::endl;
                            }
                        }
                    }
                }
            } else {
                std::cout << "Failed to start calibration!" << std::endl;
            }
        } else {
            std::cout << "Calibration cancelled by user." << std::endl;
        }
    }
    
    std::cout << "\nCalibration demo completed." << std::endl;
    
    return 0;
}
