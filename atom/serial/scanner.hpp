#ifndef ATOM_SERIAL_SCANNER_HPP
#define ATOM_SERIAL_SCANNER_HPP

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <devguid.h>
#include <setupapi.h>
#undef interface
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#endif
#else
#include <fcntl.h>
#include <libudev.h>
#include <unistd.h>
#endif

namespace atom {
namespace serial {

/**
 * @brief Exception type for serial port scanning errors
 */
class ScannerError : public std::runtime_error {
public:
    explicit ScannerError(const std::string& message)
        : std::runtime_error(message) {}
    explicit ScannerError(const char* message) : std::runtime_error(message) {}
};

/**
 * @brief A class to scan and retrieve information about serial ports.
 *
 * This class provides methods to list available serial ports, get detailed
 * information about a specific port, and identify CH340 series devices.
 * It supports both Windows and Linux platforms.
 *
 * Thread safety: The class is thread-safe for concurrent calls to const
 * methods. For non-const methods, external synchronization is required.
 *
 * @example
 * ```cpp
 * // List all available serial ports
 * atom::serial::SerialPortScanner scanner;
 * auto ports = scanner.list_available_ports();
 * for (const auto& port : ports) {
 *     std::cout << "Port: " << port.device << ", Description: " <<
 * port.description; if (port.is_ch340) { std::cout << " (CH340 device: " <<
 * port.ch340_model << ")";
 *     }
 *     std::cout << std::endl;
 * }
 *
 * // Get detailed information about a specific port
 * if (auto details = scanner.get_port_details("COM3")) {
 *     std::cout << "Device: " << details->device_name << std::endl;
 *     std::cout << "Manufacturer: " << details->manufacturer << std::endl;
 *     // ... other details
 * }
 * ```
 */
class SerialPortScanner {
public:
    /**
     * @brief Configuration options for the serial port scanner
     */
    struct ScannerConfig {
        bool detect_ch340{true};  ///< Enable detection of CH340 devices
        bool include_virtual_ports{
            true};  ///< Include virtual serial ports in scan results
        std::chrono::milliseconds timeout{
            1000};  ///< Timeout for device operations
    };

    /**
     * @brief Structure to hold basic information about a serial port.
     */
    struct PortInfo {
        std::string device;       ///< The device name (e.g., COM1 on Windows or
                                  ///< /dev/ttyUSB0 on Linux)
        std::string description;  ///< A description of the port
        bool is_ch340{
            false};  ///< Flag indicating whether the device is a CH340
        std::string ch340_model;  ///< The CH340 model (if applicable)
    };

    /**
     * @brief Structure to hold detailed information about a serial port.
     */
    struct PortDetails {
        std::string device_name;  ///< The device name
        std::string description;  ///< A description of the port
        std::string hardware_id;  ///< The hardware ID of the port
        std::string vid;          ///< The Vendor ID (VID) in hexadecimal format
        std::string pid;  ///< The Product ID (PID) in hexadecimal format
        std::string serial_number;  ///< The serial number of the device
        std::string location;       ///< The location of the device
        std::string manufacturer;   ///< The manufacturer of the device
        std::string product;        ///< The product name
        std::string interface;      ///< The interface type
        bool is_ch340{
            false};  ///< Flag indicating whether the device is a CH340
        std::string ch340_model;  ///< The CH340 model (if applicable)
        std::string
            recommended_baud_rates;  ///< Recommended baud rates for the port
        std::string notes;           ///< Additional notes about the port
    };

    /**
     * @brief Device identification information
     */
    struct DeviceId {
        uint16_t vid{0};  ///< Vendor ID
        uint16_t pid{0};  ///< Product ID
    };

    /**
     * @brief Error information that can be returned by various methods
     */
    struct ErrorInfo {
        std::string message;
        int error_code{0};
    };

    /**
     * @brief Result type that can contain either a value or an error
     * @tparam T The type of the value
     */
    template <typename T>
    using Result = std::variant<T, ErrorInfo>;

    /**
     * @brief Default constructor for the SerialPortScanner class
     *
     * Initializes the internal CH340 identifier map with default values.
     */
    SerialPortScanner() noexcept;

    /**
     * @brief Constructor with configuration options
     *
     * @param config Configuration options for the scanner
     */
    explicit SerialPortScanner(const ScannerConfig& config) noexcept;

    /**
     * @brief Destructor
     */
    ~SerialPortScanner() noexcept = default;

    /**
     * @brief Copy constructor (deleted)
     */
    SerialPortScanner(const SerialPortScanner&) = delete;

    /**
     * @brief Copy assignment operator (deleted)
     */
    SerialPortScanner& operator=(const SerialPortScanner&) = delete;

    /**
     * @brief Set configuration options for the scanner
     *
     * @param config The new configuration to apply
     */
    void set_config(const ScannerConfig& config) noexcept;

    /**
     * @brief Get the current configuration options
     *
     * @return The current configuration
     */
    [[nodiscard]] ScannerConfig get_config() const noexcept;

    /**
     * @brief Checks if a given serial port is a CH340 series device
     *
     * This function checks the Vendor ID (VID), Product ID (PID), and device
     * description against a known list of CH340 devices.
     *
     * @param vid The Vendor ID of the device
     * @param pid The Product ID of the device
     * @param description The description of the device
     * @return A pair containing a boolean indicating whether the device is a
     *         CH340 and the CH340 model (if applicable)
     */
    [[nodiscard]] std::pair<bool, std::string> is_ch340_device(
        uint16_t vid, uint16_t pid,
        std::string_view description) const noexcept;

    /**
     * @brief Lists all available serial ports
     *
     * This function scans the system for available serial ports and returns a
     * list of `PortInfo` structures containing basic information about each
     * port.
     *
     * @param highlight_ch340 If true, CH340 devices will be marked in the
     * returned list
     * @return A Result containing either a vector of `PortInfo` structures or
     * an ErrorInfo
     */
    [[nodiscard]] Result<std::vector<PortInfo>> list_available_ports(
        bool highlight_ch340 = true);

    /**
     * @brief Retrieves detailed information about a specific serial port
     *
     * This function retrieves detailed information about the specified serial
     * port, such as hardware ID, Vendor ID, Product ID, serial number, and
     * other properties.
     *
     * @param port_name The name of the serial port to retrieve details for
     * (e.g., "COM1" or "/dev/ttyUSB0")
     * @return A Result containing either an optional PortDetails (which may be
     * empty if port not found) or an ErrorInfo
     */
    [[nodiscard]] Result<std::optional<PortDetails>> get_port_details(
        std::string_view port_name);

    /**
     * @brief Asynchronously list all available serial ports
     *
     * @param callback Function to call when port scan is complete
     * @param highlight_ch340 Whether to highlight CH340 devices
     */
    void list_available_ports_async(
        std::function<void(Result<std::vector<PortInfo>>)> callback,
        bool highlight_ch340 = true);

    /**
     * @brief Register a custom device detector
     *
     * Allows adding custom device type detection beyond the built-in CH340
     * detection.
     *
     * @param detector_name Name of the detector for reference
     * @param detector Function that takes VID, PID and description, returns
     * pair of {detected, model_name}
     * @return true if detector was registered successfully, false if name
     * already exists
     */
    bool register_device_detector(const std::string& detector_name,
                                  std::function<std::pair<bool, std::string>(
                                      uint16_t, uint16_t, std::string_view)>
                                      detector);

private:
    /**
     * @brief A map to store known CH340 device identifiers
     *
     * The outer map uses the Vendor ID as the key, and the inner map uses the
     * Product ID as the key. The value of the inner map is the CH340 model
     * string.
     */
    std::unordered_map<uint16_t, std::unordered_map<uint16_t, std::string>>
        ch340_identifiers;

    /**
     * @brief Configuration options for the scanner
     */
    ScannerConfig config_{};

    /**
     * @brief Custom device detectors
     */
    std::unordered_map<std::string, std::function<std::pair<bool, std::string>(
                                        uint16_t, uint16_t, std::string_view)>>
        device_detectors_;

    /**
     * @brief Mutex for thread safety
     */
    mutable std::mutex mutex_;

#ifdef _WIN32
    /**
     * @brief Internal implementation of get_port_details for Windows
     */
    std::optional<PortDetails> get_port_details_win(std::string_view port_name);

    /**
     * @brief Fill additional details for a Windows port
     */
    void fill_details_win(PortDetails& details);
#else
    /**
     * @brief Internal implementation of get_port_details for Linux
     */
    std::optional<PortDetails> get_port_details_linux(
        std::string_view port_name);

    /**
     * @brief Fill additional details for a Linux port
     */
    void fill_details_linux(PortDetails& details);
#endif
};

}  // namespace serial
}  // namespace atom

#endif  // ATOM_SERIAL_SCANNER_HPP
