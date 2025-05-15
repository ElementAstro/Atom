/*
 * battery.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Battery

**************************************************/

#include "atom/sysinfo/battery.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <winnt.h>
#include <conio.h>
#include <setupapi.h>
#include <devguid.h>
#include <powersetting.h>
// clang-format on
#elif defined(__APPLE__)
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#elif defined(__linux__)
#include <csignal>
#include <cstdio>
#endif

#include "atom/log/loguru.hpp"

namespace atom::system {
auto getBatteryInfo() -> BatteryInfo {
    LOG_F(INFO, "Starting getBatteryInfo function");
    BatteryInfo info;

#ifdef _WIN32
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus) != 0) {
        LOG_F(INFO, "Successfully retrieved power status");
        info.isBatteryPresent = powerStatus.BatteryFlag != 128;
        info.isCharging =
            powerStatus.BatteryFlag == 8 || powerStatus.ACLineStatus == 1;
        info.batteryLifePercent =
            static_cast<float>(powerStatus.BatteryLifePercent);
        info.batteryLifeTime =
            powerStatus.BatteryLifeTime == 0xFFFFFFFF
                ? 0.0
                : static_cast<float>(powerStatus.BatteryLifeTime);
        info.batteryFullLifeTime =
            powerStatus.BatteryFullLifeTime == 0xFFFFFFFF
                ? 0.0
                : static_cast<float>(powerStatus.BatteryFullLifeTime);
        LOG_F(INFO,
              "Battery Present: %d, Charging: %d, Battery Life Percent: %.2f, "
              "Battery Life Time: %.2f, Battery Full Life Time: %.2f",
              info.isBatteryPresent, info.isCharging, info.batteryLifePercent,
              info.batteryLifeTime, info.batteryFullLifeTime);
    } else {
        LOG_F(ERROR, "Failed to get system power status");
    }
#elif defined(__APPLE__)
    CFTypeRef powerSourcesInfo = IOPSCopyPowerSourcesInfo();
    CFArrayRef powerSources = IOPSCopyPowerSourcesList(powerSourcesInfo);

    CFIndex count = CFArrayGetCount(powerSources);
    if (count > 0) {
        CFDictionaryRef powerSource = CFArrayGetValueAtIndex(powerSources, 0);

        CFBooleanRef isCharging =
            (CFBooleanRef)CFDictionaryGetValue(powerSource, kIOPSIsChargingKey);
        if (isCharging != nullptr) {
            info.isCharging = CFBooleanGetValue(isCharging);
        }

        CFNumberRef capacity = (CFNumberRef)CFDictionaryGetValue(
            powerSource, kIOPSCurrentCapacityKey);
        if (capacity != nullptr) {
            SInt32 value;
            CFNumberGetValue(capacity, kCFNumberSInt32Type, &value);
            info.batteryLifePercent = static_cast<float>(value);
        }

        CFNumberRef timeToEmpty =
            (CFNumberRef)CFDictionaryGetValue(powerSource, kIOPSTimeToEmptyKey);
        if (timeToEmpty != nullptr) {
            SInt32 value;
            CFNumberGetValue(timeToEmpty, kCFNumberSInt32Type, &value);
            info.batteryLifeTime = static_cast<float>(value) / 60.0f;
        }

        CFNumberRef capacityMax =
            (CFNumberRef)CFDictionaryGetValue(powerSource, kIOPSMaxCapacityKey);
        if (capacityMax != nullptr) {
            SInt32 value;
            CFNumberGetValue(capacityMax, kCFNumberSInt32Type, &value);
            info.batteryFullLifeTime = static_cast<float>(value);
        }

        LOG_F(INFO,
              "Battery Info - Charging: %d, Battery Life Percent: %.2f, "
              "Battery Life Time: %.2f, Battery Full Life Time: %.2f",
              info.isCharging, info.batteryLifePercent, info.batteryLifeTime,
              info.batteryFullLifeTime);
    } else {
        LOG_F(WARNING, "No power sources found");
    }

    CFRelease(powerSources);
    CFRelease(powerSourcesInfo);
#elif defined(__linux__)
    std::ifstream batteryInfo("/sys/class/power_supply/BAT0/uevent");
    if (batteryInfo.is_open()) {
        LOG_F(INFO, "Opened battery info file");
        std::string line;
        while (std::getline(batteryInfo, line)) {
            if (line.find("POWER_SUPPLY_PRESENT") != std::string::npos) {
                info.isBatteryPresent = line.substr(line.find('=') + 1) == "1";
                LOG_F(INFO, "Battery Present: %d", info.isBatteryPresent);
            } else if (line.find("POWER_SUPPLY_STATUS") != std::string::npos) {
                std::string status = line.substr(line.find('=') + 1);
                info.isCharging = status == "Charging" || status == "Full";
                LOG_F(INFO, "Battery Charging: %d", info.isCharging);
            } else if (line.find("POWER_SUPPLY_CAPACITY") !=
                       std::string::npos) {
                info.batteryLifePercent =
                    std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Battery Life Percent: %.2f",
                      info.batteryLifePercent);
            } else if (line.find("POWER_SUPPLY_TIME_TO_EMPTY_MIN") !=
                       std::string::npos) {
                info.batteryLifeTime =
                    std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Battery Life Time: %.2f", info.batteryLifeTime);
            } else if (line.find("POWER_SUPPLY_TIME_TO_FULL_NOW") !=
                       std::string::npos) {
                info.batteryFullLifeTime =
                    std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Battery Full Life Time: %.2f",
                      info.batteryFullLifeTime);
            } else if (line.find("POWER_SUPPLY_ENERGY_NOW") !=
                       std::string::npos) {
                info.energyNow = std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Energy Now: %.2f", info.energyNow);
            } else if (line.find("POWER_SUPPLY_ENERGY_FULL_DESIGN") !=
                       std::string::npos) {
                info.energyDesign = std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Energy Design: %.2f", info.energyDesign);
            } else if (line.find("POWER_SUPPLY_VOLTAGE_NOW") !=
                       std::string::npos) {
                info.voltageNow =
                    std::stof(line.substr(line.find('=') + 1)) / 1000000.0f;
                LOG_F(INFO, "Voltage Now: %.2f", info.voltageNow);
            } else if (line.find("POWER_SUPPLY_CURRENT_NOW") !=
                       std::string::npos) {
                info.currentNow =
                    std::stof(line.substr(line.find('=') + 1)) / 1000000.0f;
                LOG_F(INFO, "Current Now: %.2f", info.currentNow);
            }
        }
        batteryInfo.close();
    } else {
        LOG_F(ERROR, "Failed to open battery info file");
    }
#endif
    LOG_F(INFO, "Finished getBatteryInfo function");
    return info;
}

auto getDetailedBatteryInfo() -> BatteryInfo {
    BatteryInfo info = getBatteryInfo();

#ifdef _WIN32
    HDEVINFO hdev = SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY, 0, 0,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev != INVALID_HANDLE_VALUE) {
        // 获取电池详细信息的Windows实现
        // ...
    }
#elif defined(__linux__)
    std::ifstream batteryInfo("/sys/class/power_supply/BAT0/uevent");
    if (batteryInfo.is_open()) {
        std::string line;
        while (std::getline(batteryInfo, line)) {
            if (line.find("POWER_SUPPLY_CYCLE_COUNT") != std::string::npos) {
                info.cycleCounts = std::stoi(line.substr(line.find('=') + 1));
            } else if (line.find("POWER_SUPPLY_TEMP") != std::string::npos) {
                info.temperature =
                    std::stof(line.substr(line.find('=') + 1)) / 10.0f;
            } else if (line.find("POWER_SUPPLY_MANUFACTURER") !=
                       std::string::npos) {
                info.manufacturer = line.substr(line.find('=') + 1);
            } else if (line.find("POWER_SUPPLY_MODEL_NAME") !=
                       std::string::npos) {
                info.model = line.substr(line.find('=') + 1);
            } else if (line.find("POWER_SUPPLY_SERIAL_NUMBER") !=
                       std::string::npos) {
                info.serialNumber = line.substr(line.find('=') + 1);
            }
        }
        batteryInfo.close();
    }
#endif
    return info;
}

// 电池监控实现
static std::atomic<bool> g_monitoring{false};
static std::thread g_monitorThread;

void BatteryMonitor::startMonitoring(BatteryCallback callback,
                                     unsigned int interval_ms) {
    if (g_monitoring.exchange(true)) {
        return;  // 已经在监控中
    }

    g_monitorThread = std::thread([callback, interval_ms]() {
        BatteryInfo lastInfo;
        while (g_monitoring) {
            BatteryInfo currentInfo = getDetailedBatteryInfo();
            if (currentInfo != lastInfo) {
                callback(currentInfo);
                lastInfo = currentInfo;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    });
}

void BatteryMonitor::stopMonitoring() {
    if (g_monitoring.exchange(false)) {
        if (g_monitorThread.joinable()) {
            g_monitorThread.join();
        }
    }
}

class BatteryManagerImpl {
private:
    BatteryAlertSettings alertSettings;
    BatteryManager::AlertCallback alertCallback;
    std::atomic<bool> isRecording{false};
    std::ofstream logFile;
    std::mutex dataMutex;
    std::deque<std::pair<std::chrono::system_clock::time_point, BatteryInfo>>
        historyData;
    BatteryStats currentStats;

    void checkAlerts(const BatteryInfo& info) {
        if (!alertCallback)
            return;

        if (info.batteryLifePercent <= alertSettings.criticalBatteryThreshold) {
            alertCallback("CRITICAL_BATTERY", info);
        } else if (info.batteryLifePercent <=
                   alertSettings.lowBatteryThreshold) {
            alertCallback("LOW_BATTERY", info);
        }

        if (info.temperature >= alertSettings.highTempThreshold) {
            alertCallback("HIGH_TEMPERATURE", info);
        }

        if (info.getBatteryHealth() <= alertSettings.lowHealthThreshold) {
            alertCallback("LOW_BATTERY_HEALTH", info);
        }
    }

    void updateStats(const BatteryInfo& info) {
        std::lock_guard<std::mutex> lock(dataMutex);

        // Update min/max values
        currentStats.minBatteryLevel =
            std::min(currentStats.minBatteryLevel, info.batteryLifePercent);
        currentStats.maxBatteryLevel =
            std::max(currentStats.maxBatteryLevel, info.batteryLifePercent);

        currentStats.minTemperature =
            std::min(currentStats.minTemperature, info.temperature);
        currentStats.maxTemperature =
            std::max(currentStats.maxTemperature, info.temperature);

        currentStats.minVoltage =
            std::min(currentStats.minVoltage, info.voltageNow);
        currentStats.maxVoltage =
            std::max(currentStats.maxVoltage, info.voltageNow);

        // Update discharge rate calculations
        if (!historyData.empty() &&
            historyData.back().second.batteryLifePercent >
                info.batteryLifePercent) {
            auto timeDiff =
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - historyData.back().first)
                    .count();
            if (timeDiff > 0) {
                float dischargePct =
                    historyData.back().second.batteryLifePercent -
                    info.batteryLifePercent;
                float ratePerHour = (dischargePct / timeDiff) * 3600.0f;

                if (currentStats.avgDischargeRate < 0) {
                    currentStats.avgDischargeRate = ratePerHour;
                } else {
                    currentStats.avgDischargeRate =
                        (currentStats.avgDischargeRate * 0.9f) +
                        (ratePerHour * 0.1f);
                }
            }
        }

        // Update cycle count and health
        currentStats.cycleCount = info.cycleCounts;
        currentStats.batteryHealth = info.getBatteryHealth();
    }

    void recordData(const BatteryInfo& info) {
        if (!isRecording)
            return;

        auto now = std::chrono::system_clock::now();
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            historyData.emplace_back(now, info);

            // Keep last 24 hours of data
            while (historyData.size() >
                   8640) {  // 24h * 60min * 60sec / 10sec interval
                historyData.pop_front();
            }
        }

        if (logFile.is_open()) {
            logFile << std::format("{},{},{},{},{},{}\n",
                                   std::chrono::system_clock::to_time_t(now),
                                   info.batteryLifePercent, info.temperature,
                                   info.voltageNow, info.currentNow,
                                   info.getBatteryHealth());
        }
    }
};

// TODO: Implement the rest of the BatteryManager methods
bool PowerPlanManager::setPowerPlan(PowerPlan plan) {
#ifdef _WIN32
    GUID planGuid;
    switch (plan) {
        case PowerPlan::BALANCED:
            break;
        case PowerPlan::PERFORMANCE:
            break;
        case PowerPlan::POWER_SAVER:
            // 设置节能模式
            planGuid = GUID_MIN_POWER_SAVINGS;
            break;
        default:
            LOG_F(ERROR, "Invalid power plan specified");
            return false;
    }

    DWORD result = PowerSetActiveScheme(NULL, &planGuid);
    if (result != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to set power plan: error code %lu", result);
        return false;
    }
    LOG_F(INFO, "Power plan successfully changed");
    return true;
#elif defined(__linux__)
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
        default:
            LOG_F(ERROR, "Invalid power plan specified");
            return false;
    }

    int result = std::system(cmd.c_str());
    if (result != 0) {
        LOG_F(ERROR, "Failed to set power plan: command returned %d", result);
        return false;
    }
    LOG_F(INFO, "Power plan successfully changed");
    return true;
#elif defined(__APPLE__)
    LOG_F(WARNING, "Power plan management not implemented for macOS");
    return false;
#else
    LOG_F(WARNING, "Power plan management not implemented for this platform");
    return false;
#endif
}

PowerPlan PowerPlanManager::getCurrentPowerPlan() {
#ifdef _WIN32
    HMODULE hPowrProf = LoadLibraryA("powrprof.dll");
    if (!hPowrProf) {
        LOG_F(ERROR, "Failed to load powrprof.dll");
        return PowerPlan::BALANCED;
    }

    typedef DWORD(WINAPI * PFN_PowerGetActiveScheme)(HKEY, GUID**);
    auto pGetActiveScheme = (PFN_PowerGetActiveScheme)(GetProcAddress(
        hPowrProf, "PowerGetActiveScheme"));

    if (!pGetActiveScheme) {
        LOG_F(ERROR, "Failed to get PowerGetActiveScheme function");
        FreeLibrary(hPowrProf);
        return PowerPlan::BALANCED;
    }

    GUID* pActiveSchemeGuid = nullptr;
    if (pGetActiveScheme(NULL, &pActiveSchemeGuid) == ERROR_SUCCESS) {
        PowerPlan result = PowerPlan::BALANCED;

        if (IsEqualGUID(*pActiveSchemeGuid, GUID_MAX_POWER_SAVINGS)) {
            LOG_F(INFO, "Current power plan: Power Saver");
            result = PowerPlan::POWER_SAVER;
        } else if (IsEqualGUID(*pActiveSchemeGuid,
                               GUID_TYPICAL_POWER_SAVINGS)) {
            LOG_F(INFO, "Current power plan: Balanced");
            result = PowerPlan::BALANCED;
        } else if (IsEqualGUID(*pActiveSchemeGuid, GUID_MIN_POWER_SAVINGS)) {
            LOG_F(INFO, "Current power plan: Performance");
            result = PowerPlan::PERFORMANCE;
        }

        if (pActiveSchemeGuid) {
            HeapFree(GetProcessHeap(), 0, pActiveSchemeGuid);
        }
        FreeLibrary(hPowrProf);
        return result;
    }

    FreeLibrary(hPowrProf);
#endif
    return PowerPlan::BALANCED;
}

std::vector<std::string> PowerPlanManager::getAvailablePowerPlans() {
    std::vector<std::string> plans;
#ifdef _WIN32
    plans = {"Balanced", "High performance", "Power saver"};
#elif defined(__linux__)
    FILE* fp = popen("powerprofilesctl list", "r");
    if (fp) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            std::string line(buffer);
            if (!line.empty()) {
                plans.push_back(line);
            }
        }
        pclose(fp);
    }
    if (plans.empty()) {
        plans = {"balanced", "performance", "power-saver"};
    }
#elif defined(__APPLE__)
    plans = {"Default"};
#else
    plans = {"Default"};
#endif
    return plans;
}

}  // namespace atom::system
