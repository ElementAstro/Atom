#include "voltage.hpp"

#include <format>

#ifdef _WIN32
#include "windows_voltage.hpp"
#elif defined(__APPLE__)
#include "macos_voltage.hpp"
#else
#include "voltage_linux.hpp"
#endif

namespace atom::system {

std::string powerSourceTypeToString(PowerSourceType type) {
    switch (type) {
        case PowerSourceType::AC:
            return "AC Power";
        case PowerSourceType::Battery:
            return "Battery";
        case PowerSourceType::USB:
            return "USB";
        case PowerSourceType::Unknown:
            return "Unknown";
        default:
            return "Undefined";
    }
}

std::string PowerSourceInfo::toString() const {
    std::string result =
        std::format("Name: {}, Type: {}", name, powerSourceTypeToString(type));

    if (voltage) {
        result += std::format(", Voltage: {:.2f}V", *voltage);
    }

    if (current) {
        result += std::format(", Current: {:.2f}A", *current);
    }

    if (chargePercent) {
        result += std::format(", Charge: {}%", *chargePercent);
    }

    if (isCharging) {
        result += std::format(", Status: {}",
                              *isCharging ? "Charging" : "Not Charging");
    }

    return result;
}

std::unique_ptr<VoltageMonitor> VoltageMonitor::create() {
#ifdef _WIN32
    return std::make_unique<WindowsVoltageMonitor>();
#elif defined(__APPLE__)
    return std::make_unique<MacOSVoltageMonitor>();
#elif defined(__linux__)
    return std::make_unique<LinuxVoltageMonitor>();
#else
    throw std::runtime_error("Unsupported platform");
#endif
}

}  // namespace atom::system