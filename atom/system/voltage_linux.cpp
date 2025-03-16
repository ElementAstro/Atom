#ifdef __linux__

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "voltage_linux.hpp"

namespace fs = std::filesystem;

namespace atom::system {

// 将微伏特转换为伏特
double LinuxVoltageMonitor::microvoltsToVolts(const std::string& microvolts) {
    try {
        return std::stod(microvolts) / 1000000.0;
    } catch (...) {
        return 0.0;
    }
}

// 将微安培转换为安培
double LinuxVoltageMonitor::microampsToAmps(const std::string& microamps) {
    try {
        return std::stod(microamps) / 1000000.0;
    } catch (...) {
        return 0.0;
    }
}

std::optional<double> LinuxVoltageMonitor::getInputVoltage() const {
    // 尝试查找交流电源适配器
    for (const auto& entry : fs::directory_iterator(POWER_SUPPLY_PATH)) {
        std::string path = entry.path().filename().string();
        auto type = readPowerSupplyAttribute(path, "type");

        if (type && (*type == "Mains" || *type == "USB" ||
                     path.find("AC") != std::string::npos)) {
            auto voltage = readPowerSupplyAttribute(path, "voltage_now");
            if (voltage) {
                return microvoltsToVolts(*voltage);
            }

            // 如果没有voltage_now，尝试其他可能的属性
            voltage = readPowerSupplyAttribute(path, "voltage_boot");
            if (voltage) {
                return microvoltsToVolts(*voltage);
            }
        }
    }
    return std::nullopt;
}

std::optional<double> LinuxVoltageMonitor::getBatteryVoltage() const {
    // 查找电池设备
    for (const auto& entry : fs::directory_iterator(POWER_SUPPLY_PATH)) {
        std::string path = entry.path().filename().string();
        auto type = readPowerSupplyAttribute(path, "type");

        if (type && *type == "Battery") {
            auto voltage = readPowerSupplyAttribute(path, "voltage_now");
            if (voltage) {
                return microvoltsToVolts(*voltage);
            }
        }
    }
    return std::nullopt;
}

std::vector<PowerSourceInfo> LinuxVoltageMonitor::getAllPowerSources() const {
    std::vector<PowerSourceInfo> sources;

    try {
        if (!fs::exists(POWER_SUPPLY_PATH)) {
            return sources;
        }

        for (const auto& entry : fs::directory_iterator(POWER_SUPPLY_PATH)) {
            std::string deviceName = entry.path().filename().string();
            auto typeAttr = readPowerSupplyAttribute(deviceName, "type");

            if (!typeAttr)
                continue;

            PowerSourceInfo info;
            info.name = deviceName;

            // 设置电源类型
            if (*typeAttr == "Mains") {
                info.type = PowerSourceType::AC;
            } else if (*typeAttr == "Battery") {
                info.type = PowerSourceType::Battery;
            } else if (*typeAttr == "USB") {
                info.type = PowerSourceType::USB;
            } else {
                info.type = PowerSourceType::Unknown;
            }

            // 读取电压
            auto voltage = readPowerSupplyAttribute(deviceName, "voltage_now");
            if (voltage) {
                info.voltage = microvoltsToVolts(*voltage);
            }

            // 读取电流
            auto current = readPowerSupplyAttribute(deviceName, "current_now");
            if (current) {
                info.current = microampsToAmps(*current);
            }

            // 读取电池百分比
            if (info.type == PowerSourceType::Battery) {
                auto capacity =
                    readPowerSupplyAttribute(deviceName, "capacity");
                if (capacity) {
                    try {
                        info.chargePercent = std::stoi(*capacity);
                    } catch (...) {
                    }
                }

                // 读取充电状态
                auto status = readPowerSupplyAttribute(deviceName, "status");
                if (status) {
                    info.isCharging = (*status == "Charging");
                }
            }

            sources.push_back(info);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading power sources: " << e.what() << std::endl;
    }

    return sources;
}

std::string LinuxVoltageMonitor::getPlatformName() const { return "Linux"; }

std::optional<std::string> LinuxVoltageMonitor::readPowerSupplyAttribute(
    const std::string& device, const std::string& attribute) const {
    std::string path =
        std::string(POWER_SUPPLY_PATH) + "/" + device + "/" + attribute;

    try {
        if (!fs::exists(path)) {
            return std::nullopt;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        std::string value;
        std::getline(file, value);
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

inline std::unique_ptr<VoltageMonitor> VoltageMonitor::create() {
    return std::make_unique<LinuxVoltageMonitor>();
}

}  // namespace atom::system

#endif  // __linux__