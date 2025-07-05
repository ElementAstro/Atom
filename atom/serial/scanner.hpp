#ifndef ATOM_SERIAL_SCANNER_HPP
#define ATOM_SERIAL_SCANNER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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

    template <typename... Args>
    explicit ScannerError(const std::string& format, Args&&... args)
        : std::runtime_error(format) {}
};

/**
 * @brief Performance statistics for the serial port scanner
 */
struct ScannerStats {
    std::atomic<uint64_t> total_scans{0};
    std::atomic<uint64_t> successful_scans{0};
    std::atomic<uint64_t> failed_scans{0};
    std::atomic<uint64_t> ports_found{0};
    std::atomic<uint64_t> ch340_devices_found{0};
    std::atomic<uint64_t> scan_errors{0};
    std::atomic<uint64_t> timeout_errors{0};
    std::atomic<uint64_t> permission_errors{0};

    // Timing statistics (in microseconds)
    std::atomic<uint64_t> min_scan_time{UINT64_MAX};
    std::atomic<uint64_t> max_scan_time{0};
    std::atomic<uint64_t> total_scan_time{0};
    std::atomic<uint64_t> last_scan_time{0};

    // Cache statistics
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};

    // Memory usage
    std::atomic<size_t> peak_memory_usage{0};
    std::atomic<size_t> current_memory_usage{0};

    // Copy constructor to handle atomic members
    ScannerStats(const ScannerStats& other) noexcept
        : total_scans(other.total_scans.load()),
          successful_scans(other.successful_scans.load()),
          failed_scans(other.failed_scans.load()),
          ports_found(other.ports_found.load()),
          ch340_devices_found(other.ch340_devices_found.load()),
          scan_errors(other.scan_errors.load()),
          timeout_errors(other.timeout_errors.load()),
          permission_errors(other.permission_errors.load()),
          min_scan_time(other.min_scan_time.load()),
          max_scan_time(other.max_scan_time.load()),
          total_scan_time(other.total_scan_time.load()),
          last_scan_time(other.last_scan_time.load()),
          cache_hits(other.cache_hits.load()),
          cache_misses(other.cache_misses.load()),
          peak_memory_usage(other.peak_memory_usage.load()),
          current_memory_usage(other.current_memory_usage.load()) {}

    // Default constructor
    ScannerStats() noexcept = default;

    void reset() noexcept {
        total_scans = 0;
        successful_scans = 0;
        failed_scans = 0;
        ports_found = 0;
        ch340_devices_found = 0;
        scan_errors = 0;
        timeout_errors = 0;
        permission_errors = 0;
        min_scan_time = UINT64_MAX;
        max_scan_time = 0;
        total_scan_time = 0;
        last_scan_time = 0;
        cache_hits = 0;
        cache_misses = 0;
        peak_memory_usage = 0;
        current_memory_usage = 0;
    }

    [[nodiscard]] double get_average_scan_time() const noexcept {
        auto total = total_scans.load();
        return total > 0 ? static_cast<double>(total_scan_time.load()) / total
                         : 0.0;
    }

    [[nodiscard]] double get_success_rate() const noexcept {
        auto total = total_scans.load();
        return total > 0 ? static_cast<double>(successful_scans.load()) /
                               total * 100.0
                         : 0.0;
    }

    [[nodiscard]] double get_cache_hit_rate() const noexcept {
        auto total = cache_hits.load() + cache_misses.load();
        return total > 0
                   ? static_cast<double>(cache_hits.load()) / total * 100.0
                   : 0.0;
    }
};

/**
 * @brief Cache entry for port information
 */
struct CacheEntry {
    std::chrono::steady_clock::time_point timestamp;
    std::string port_path;
    bool is_available;
    std::string last_error;
    uint32_t access_count{0};

    [[nodiscard]] bool is_expired(
        std::chrono::milliseconds ttl) const noexcept {
        return std::chrono::steady_clock::now() - timestamp > ttl;
    }
};

/**
 * @brief Enhanced configuration options for the serial port scanner
 */
struct ScannerConfig {
    // Detection options
    bool detect_ch340{true};  ///< Enable detection of CH340 devices
    bool include_virtual_ports{
        true};  ///< Include virtual serial ports in scan results
    bool enable_bluetooth_scan{
        false};                  ///< Enable Bluetooth serial port scanning
    bool enable_usb_scan{true};  ///< Enable USB serial port scanning

    // Performance options
    std::chrono::milliseconds scan_timeout{
        5000};  ///< Timeout for device operations
    std::chrono::milliseconds retry_interval{
        1000};  ///< Retry interval for failed operations
    std::chrono::milliseconds cache_ttl{30000};  ///< Cache time-to-live
    std::chrono::milliseconds monitor_interval{
        5000};                    ///< Port monitoring interval
    uint32_t max_retry_count{3};  ///< Maximum retry attempts
    size_t max_cache_size{1000};  ///< Maximum cache entries

    // Threading options
    size_t max_concurrent_scans{4};  ///< Maximum concurrent scanning threads
    bool enable_background_monitoring{
        false};  ///< Enable background port monitoring

    // Logging options
    bool enable_debug_logging{false};  ///< Enable debug level logging
    bool enable_performance_logging{
        true};  ///< Enable performance metrics logging
    std::chrono::milliseconds log_stats_interval{
        60000};  ///< Statistics logging interval

    // Error handling options
    bool enable_error_recovery{true};  ///< Enable automatic error recovery
    bool fail_fast{false};             ///< Fail fast on first error

    // Validation
    [[nodiscard]] bool is_valid() const noexcept {
        return scan_timeout.count() > 0 && retry_interval.count() > 0 &&
               cache_ttl.count() > 0 && monitor_interval.count() > 0 &&
               max_retry_count > 0 && max_cache_size > 0 &&
               max_concurrent_scans > 0 && log_stats_interval.count() > 0;
    }
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
     * @brief Structure to hold basic information about a serial port.
     */
    struct PortInfo {
        std::string device;       ///< The device name (e.g., COM1 on Windows or
                                  ///< /dev/ttyUSB0 on Linux)
        std::string description;  ///< A description of the port
        std::string hardware_id;  ///< Hardware identifier
        std::string vendor_id;    ///< Vendor ID in hex format
        std::string product_id;   ///< Product ID in hex format
        std::string serial_number;  ///< Device serial number
        std::string manufacturer;   ///< Device manufacturer
        std::string location;       ///< Physical location/path
        bool is_available{false};   ///< Whether the port is currently available
        bool is_ch340{
            false};  ///< Flag indicating whether the device is a CH340
        bool is_virtual{false};    ///< Flag indicating if it's a virtual port
        bool is_bluetooth{false};  ///< Flag indicating if it's a Bluetooth port
        std::string ch340_model;   ///< The CH340 model (if applicable)
        std::chrono::steady_clock::time_point
            last_seen;  ///< Last time the port was detected

        // Performance and status information
        uint32_t scan_count{0};  ///< Number of times this port has been scanned
        std::chrono::microseconds last_scan_duration{
            0};                  ///< Duration of last scan
        std::string last_error;  ///< Last error encountered
    };

    /**
     * @brief Structure to hold detailed information about a serial port.
     */
    struct PortDetails {
        std::string device_name;    ///< The device name
        std::string description;    ///< A description of the port
        std::string hardware_id;    ///< The hardware ID of the port
        std::string registry_path;  ///< Windows registry path for the device
        std::string vid;  ///< The Vendor ID (VID) in hexadecimal format
        std::string pid;  ///< The Product ID (PID) in hexadecimal format
        std::string serial_number;   ///< The serial number of the device
        std::string location;        ///< The location of the device
        std::string manufacturer;    ///< The manufacturer of the device
        std::string product;         ///< The product name
        std::string interface;       ///< The interface type
        std::string driver_name;     ///< Driver name
        std::string driver_version;  ///< Driver version
        std::string friendly_name;   ///< User-friendly device name (Windows)
        bool is_ch340{
            false};  ///< Flag indicating whether the device is a CH340
        bool is_virtual{false};    ///< Flag indicating if it's a virtual port
        bool is_bluetooth{false};  ///< Flag indicating if it's a Bluetooth port
        bool is_available{false};  ///< Whether the port is currently available
        std::string ch340_model;   ///< The CH340 model (if applicable)
        std::string
            recommended_baud_rates;     ///< Recommended baud rates for the port
        std::string notes;              ///< Additional notes about the port
        uint32_t current_baud_rate{0};  ///< Current baud rate of the port
        uint32_t max_baud_rate{0};      ///< Maximum supported baud rate

        // Performance and diagnostic information
        std::chrono::steady_clock::time_point
            last_detected;  ///< Last detection time
        std::chrono::microseconds detection_time{
            0};                   ///< Time taken to detect this port
        uint32_t error_count{0};  ///< Number of errors encountered
        std::string last_error;   ///< Last error message
        std::chrono::steady_clock::time_point
            last_error_time;  ///< Time of last error
    };

    /**
     * @brief Device identification information
     */
    struct DeviceId {
        uint16_t vid{0};  ///< Vendor ID
        uint16_t pid{0};  ///< Product ID

        bool operator==(const DeviceId& other) const noexcept {
            return vid == other.vid && pid == other.pid;
        }
    };

    /**
     * @brief Port monitoring event information
     */
    struct PortEvent {
        enum class Type {
            ADDED,      ///< Port was added
            REMOVED,    ///< Port was removed
            CHANGED,    ///< Port properties changed
            ERROR_TYPE  ///< Error occurred
        };

        Type type;
        std::string port_name;
        std::string description;
        std::chrono::steady_clock::time_point timestamp;
        std::string details;
    };

    /**
     * @brief Error information that can be returned by various methods
     */
    struct ErrorInfo {
        std::string message;
        int error_code{0};
        std::chrono::steady_clock::time_point timestamp;
        std::string context;

        ErrorInfo() = default;
        ErrorInfo(std::string msg, int code = 0, std::string ctx = "")
            : message(std::move(msg)),
              error_code(code),
              timestamp(std::chrono::steady_clock::now()),
              context(std::move(ctx)) {}
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

    /**
     * @brief Start background monitoring for port changes
     *
     * Starts a background thread that monitors for port addition/removal
     * events and calls the provided callback.
     *
     * @param callback Function to call when port events occur
     * @return true if monitoring started successfully, false otherwise
     */
    bool start_monitoring(std::function<void(const PortEvent&)> callback);

    /**
     * @brief Stop background monitoring
     */
    void stop_monitoring();

    /**
     * @brief Check if background monitoring is active
     *
     * @return true if monitoring is active, false otherwise
     */
    [[nodiscard]] bool is_monitoring() const noexcept;

    /**
     * @brief Get performance statistics
     *
     * @return Copy of current performance statistics
     */
    [[nodiscard]] ScannerStats get_statistics() const noexcept;

    /**
     * @brief Reset performance statistics
     */
    void reset_statistics() noexcept;

    /**
     * @brief Check if a specific port is available
     *
     * This is a lightweight check that may use cached data if available.
     *
     * @param port_name The name of the port to check
     * @return Result containing availability status or error
     */
    [[nodiscard]] Result<bool> is_port_available(std::string_view port_name);

    /**
     * @brief Force refresh of port cache
     *
     * Clears the cache and forces a fresh scan on next operation.
     */
    void refresh_cache();

    /**
     * @brief Get cache statistics
     *
     * @return String containing formatted cache statistics
     */
    [[nodiscard]] std::string get_cache_info() const;

    /**
     * @brief Validate port configuration
     *
     * Attempts to open and validate the specified port.
     *
     * @param port_name The name of the port to validate
     * @param timeout Maximum time to wait for validation
     * @return Result containing validation status or error
     */
    [[nodiscard]] Result<bool> validate_port(
        std::string_view port_name,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /**
     * @brief Get last scan timestamp
     *
     * @return Time point of last successful scan
     */
    [[nodiscard]] std::chrono::steady_clock::time_point get_last_scan_time()
        const noexcept;

    /**
     * @brief Set custom retry strategy
     *
     * @param max_retries Maximum number of retry attempts
     * @param retry_delay Delay between retry attempts
     */
    void set_retry_strategy(uint32_t max_retries,
                            std::chrono::milliseconds retry_delay);

    /**
     * @brief Get detailed error information for last operation
     *
     * @return ErrorInfo containing details about the last error
     */
    [[nodiscard]] std::optional<ErrorInfo> get_last_error() const noexcept;

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
    mutable std::shared_mutex mutex_;

    /**
     * @brief Performance statistics
     */
    mutable ScannerStats stats_;

    /**
     * @brief Port information cache
     */
    mutable std::unordered_map<std::string, CacheEntry> port_cache_;

    /**
     * @brief Cache mutex for thread-safe cache operations
     */
    mutable std::mutex cache_mutex_;

    /**
     * @brief Last scan timestamp
     */
    mutable std::atomic<std::chrono::steady_clock::time_point> last_scan_time_;

    /**
     * @brief Last error information
     */
    mutable std::optional<ErrorInfo> last_error_;

    /**
     * @brief Monitoring thread and related synchronization
     */
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> monitoring_active_{false};
    std::condition_variable monitor_cv_;
    std::mutex monitor_mutex_;
    std::function<void(const PortEvent&)> monitor_callback_;

    /**
     * @brief Thread pool for async operations
     */
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> shutdown_workers_{false};

    /**
     * @brief Known port cache for quick availability checks
     */
    mutable std::unordered_set<std::string> known_ports_;
    mutable std::chrono::steady_clock::time_point last_port_refresh_;

    /**
     * @brief Error recovery state
     */
    std::atomic<uint32_t> consecutive_errors_{0};
    std::chrono::steady_clock::time_point last_error_time_;

    /**
     * @brief Internal helper methods
     */
    void initialize_ch340_identifiers() noexcept;
    void start_worker_threads();
    void stop_worker_threads();
    void monitor_thread_func();
    void update_statistics(const std::chrono::microseconds& scan_time,
                           bool success, size_t port_count) const noexcept;
    bool is_cache_valid(const std::string& port_name) const noexcept;
    void update_cache(const std::string& port_name, bool available,
                      const std::string& error = {}) const noexcept;
    void cleanup_cache() const noexcept;
    void log_performance_stats() const noexcept;
    [[nodiscard]] std::string format_error(
        const std::string& operation,
        const std::string& details) const noexcept;
    void handle_error(const std::string& context,
                      const std::exception& e) const noexcept;
    void recover_from_error() noexcept;

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
