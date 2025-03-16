#ifdef __linux__

#include <optional>
#include <string>
#include <vector>

#include "voltage.hpp"

namespace atom::system {

/**
 * @brief LinuxVoltageMonitor class implementation for voltage monitoring on
 * Linux systems.
 *
 * This class provides methods to retrieve voltage and power source information
 * from the Linux operating system using the /sys/class/power_supply interface.
 */
class LinuxVoltageMonitor : public VoltageMonitor {
public:
    /**
     * @brief Default constructor for the LinuxVoltageMonitor.
     */
    LinuxVoltageMonitor() = default;

    /**
     * @brief Default virtual destructor for proper cleanup.
     */
    ~LinuxVoltageMonitor() override = default;

    /**
     * @brief Gets the input voltage in volts (V).
     * @return An optional double representing the input voltage, or
     * std::nullopt if not available.
     * @override
     */
    std::optional<double> getInputVoltage() const override;

    /**
     * @brief Gets the battery voltage in volts (V).
     * @return An optional double representing the battery voltage, or
     * std::nullopt if not available.
     * @override
     */
    std::optional<double> getBatteryVoltage() const override;

    /**
     * @brief Gets information about all available power sources.
     * @return A vector of PowerSourceInfo structures, each representing a power
     * source.
     * @override
     */
    std::vector<PowerSourceInfo> getAllPowerSources() const override;

    /**
     * @brief Gets the name of the platform the monitor is running on.
     * @return A string representing the platform name (e.g., "Linux").
     * @override
     */
    std::string getPlatformName() const override;

private:
    /**
     * @brief The path to the power supply directory in the Linux filesystem.
     */
    static constexpr const char* POWER_SUPPLY_PATH = "/sys/class/power_supply";

    /**
     * @brief Reads a specific attribute from a power supply device.
     * @param device The name of the power supply device (e.g., "BAT0", "ACAD").
     * @param attribute The name of the attribute to read (e.g., "voltage_now",
     * "current_now").
     * @return An optional string containing the attribute value, or
     * std::nullopt if the attribute could not be read.
     */
    std::optional<std::string> readPowerSupplyAttribute(
        const std::string& device, const std::string& attribute) const;

    /**
     * @brief Converts a value from microvolts (uV) to volts (V).
     * @param microvolts The value in microvolts as a string.
     * @return The equivalent value in volts as a double.
     */
    static double microvoltsToVolts(const std::string& microvolts);

    /**
     * @brief Converts a value from microamperes (uA) to amperes (A).
     * @param microamps The value in microamperes as a string.
     * @return The equivalent value in amperes as a double.
     */
    static double microampsToAmps(const std::string& microamps);
};

}  // namespace atom::system

#endif  // __linux__