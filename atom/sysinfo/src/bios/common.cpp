#include "common.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <memory>
#include <iomanip>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include "windows.hpp"
#elif __linux__
#include "linux.hpp"
#elif __APPLE__
#include "macos.hpp"
#endif

namespace atom::system {

bool BiosInfoData::isValid() const {
    return !version.empty() && !manufacturer.empty() && !releaseDate.empty();
}

std::string BiosInfoData::toString() const {
    std::stringstream ss;
    ss << "BIOS Information:\n"
       << "Version: " << version << "\n"
       << "Manufacturer: " << manufacturer << "\n"
       << "Release Date: " << releaseDate << "\n"
       << "Serial Number: " << serialNumber << "\n"
       << "Characteristics: " << characteristics << "\n"
       << "Upgradeable: " << (isUpgradeable ? "Yes" : "No");
    return ss.str();
}

std::unique_ptr<BiosImplementation> createBiosImplementation() {
#ifdef _WIN32
    return std::make_unique<WindowsBiosImplementation>();
#elif __linux__
    return std::make_unique<LinuxBiosImplementation>();
#elif __APPLE__
    return std::make_unique<MacOSBiosImplementation>();
#else
    spdlog::error("Unsupported platform for BIOS operations");
    return nullptr;
#endif
}

// BiosUtils implementation
namespace BiosUtils {

double celsiusToFahrenheit(double celsius) {
    return (celsius * 9.0 / 5.0) + 32.0;
}

double mhzToGhz(int mhz) {
    return static_cast<double>(mhz) / 1000.0;
}

std::string formatBootTime(int bootTimeMs) {
    if (bootTimeMs < 1000) {
        return std::to_string(bootTimeMs) + " ms";
    } else if (bootTimeMs < 60000) {
        return std::to_string(bootTimeMs / 1000.0) + " s";
    } else {
        int minutes = bootTimeMs / 60000;
        int seconds = (bootTimeMs % 60000) / 1000;
        return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    }
}

bool isValidBiosVersion(const std::string& version) {
    if (version.empty()) {
        return false;
    }

    // Basic validation - should contain at least one digit
    return std::any_of(version.begin(), version.end(), ::isdigit);
}

std::chrono::system_clock::time_point parseBiosDate(const std::string& dateString) {
    std::tm tm = {};
    std::istringstream ss(dateString);

    // Try different date formats
    if (ss >> std::get_time(&tm, "%m/%d/%Y")) {
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    ss.clear();
    ss.str(dateString);
    if (ss >> std::get_time(&tm, "%Y%m%d%H%M%S")) {
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    ss.clear();
    ss.str(dateString);
    if (ss >> std::get_time(&tm, "%d/%m/%Y")) {
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    // Return epoch if parsing fails
    return std::chrono::system_clock::time_point{};
}

int calculateBiosAge(const std::chrono::system_clock::time_point& biosDate) {
    auto now = std::chrono::system_clock::now();
    auto duration = now - biosDate;
    return std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;
}

}  // namespace BiosUtils

}  // namespace atom::system
