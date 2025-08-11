#ifndef ATOM_SYSTEM_VOLTAGE_HPP
#define ATOM_SYSTEM_VOLTAGE_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace atom::system {

/**
 * @brief Enumeration representing the type of power source.
 */
enum class PowerSourceType {
    /**
     * @brief AC power source (e.g., wall outlet).
     */
    AC,
    /**
     * @brief Battery power source.
     */
    Battery,
    /**
     * @brief USB power source.
     */
    USB,
    /**
     * @brief Unknown power source type.
     */
    Unknown
};

/**
 * @brief Structure containing information about a power source.
 */
struct PowerSourceInfo {
    /**
     * @brief The name of the power source (e.g., "Battery 1", "AC Adapter").
     */
    std::string name;
    /**
     * @brief The type of power source.
     */
    PowerSourceType type;
    /**
     * @brief The voltage of the power source in volts (V), if available.
     */
    std::optional<double> voltage;
    /**
     * @brief The current of the power source in amperes (A), if available.
     */
    std::optional<double> current;
    /**
     * @brief The charge percentage of the power source (e.g., for batteries),
     * if available.
     */
    std::optional<int> chargePercent;
    /**
     * @brief A boolean indicating whether the power source is currently
     * charging (e.g., for batteries), if available.
     */
    std::optional<bool> isCharging;

    /**
     * @brief Returns a string representation of the PowerSourceInfo.
     * @return A string containing the power source information.
     */
    std::string toString() const;
};

/**
 * @brief Abstract base class for voltage monitors.
 *
 * Provides an interface for retrieving voltage and power source information
 * from the underlying system.
 */
class VoltageMonitor {
public:
    /**
     * @brief Virtual destructor to ensure proper cleanup of derived classes.
     */
    virtual ~VoltageMonitor() = default;

    /**
     * @brief Gets the input voltage in volts (V).
     * @return An optional double representing the input voltage, or
     * std::nullopt if not available.
     */
    virtual std::optional<double> getInputVoltage() const = 0;

    /**
     * @brief Gets the battery voltage in volts (V).
     * @return An optional double representing the battery voltage, or
     * std::nullopt if not available.
     */
    virtual std::optional<double> getBatteryVoltage() const = 0;

    /**
     * @brief Gets information about all available power sources.
     * @return A vector of PowerSourceInfo structures, each representing a power
     * source.
     */
    virtual std::vector<PowerSourceInfo> getAllPowerSources() const = 0;

    /**
     * @brief Gets the name of the platform the monitor is running on.
     * @return A string representing the platform name (e.g., "Windows",
     * "MacOS", "Linux").
     */
    virtual std::string getPlatformName() const = 0;

    /**
     * @brief Creates a platform-specific VoltageMonitor implementation.
     * @return A unique pointer to a VoltageMonitor instance.
     */
    static std::unique_ptr<VoltageMonitor> create();
};

/**
 * @brief Platform-specific implementation class for Windows (forward
 * declaration).
 */
#ifdef _WIN32
class WindowsVoltageMonitor;
#elif defined(__APPLE__)
/**
 * @brief Platform-specific implementation class for MacOS (forward
 * declaration).
 */
class MacOSVoltageMonitor;
#elif defined(__linux__)
/**
 * @brief Platform-specific implementation class for Linux (forward
 * declaration).
 */
class LinuxVoltageMonitor;
#endif

/**
 * @brief Converts a PowerSourceType enum value to a string representation.
 * @param type The PowerSourceType enum value to convert.
 * @return A string representing the power source type.
 */
std::string powerSourceTypeToString(PowerSourceType type);

}  // namespace atom::system

#endif  // ATOM_SYSTEM_VOLTAGE_HPP
