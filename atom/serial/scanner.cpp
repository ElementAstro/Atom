#include "scanner.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <regex>

#include "spdlog/spdlog.h"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <setupapi.h>
// clang-format on
#else
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <fstream>
#include <libudev.h>
#include <termios.h>
#endif
#endif

namespace {
template <typename T>
inline atom::serial::SerialPortScanner::Result<T> make_error_result(
    const std::string& message, int code = 0) {
    return atom::serial::SerialPortScanner::ErrorInfo{message, code};
}

inline void to_lower_inplace(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
}

inline bool contains_case_insensitive(std::string_view haystack,
                                      std::string_view needle) {
    auto it =
        std::search(haystack.begin(), haystack.end(), needle.begin(),
                    needle.end(), [](char ch1, char ch2) {
                        return std::tolower(static_cast<unsigned char>(ch1)) ==
                               std::tolower(static_cast<unsigned char>(ch2));
                    });
    return it != haystack.end();
}

#ifdef _WIN32
inline bool is_virtual_port(const std::string& port_name) {
    return port_name.find("CNCA") == 0 || port_name.find("VCOM") == 0 ||
           port_name.find("VPCOM") == 0;
}
#else
inline bool is_virtual_port(const std::string& device_path) {
    return device_path.find("/dev/ptmx") == 0 ||
           device_path.find("/dev/pts") == 0 ||
           device_path.find("/dev/ttyS") == 0;
}
#endif

}  // anonymous namespace

namespace atom {
namespace serial {

namespace {
// Constants for performance optimization
constexpr size_t INITIAL_PORT_VECTOR_SIZE = 16;
constexpr std::chrono::milliseconds DEFAULT_CACHE_CLEANUP_INTERVAL{
    300000};  // 5 minutes
constexpr size_t MAX_CONSECUTIVE_ERRORS = 5;

// Helper function to format duration in microseconds
std::string format_duration(std::chrono::microseconds duration) {
    auto us = duration.count();
    if (us < 1000) {
        return std::to_string(us) + "μs";
    } else if (us < 1000000) {
        return std::to_string(us / 1000) + "ms";
    } else {
        return std::to_string(us / 1000000) + "s";
    }
}

// Helper function to parse hex string to uint16_t
uint16_t parse_hex(const std::string& hex_str) {
    try {
        return static_cast<uint16_t>(std::stoul(hex_str, nullptr, 16));
    } catch (const std::exception&) {
        return 0;
    }
}

// Extract VID/PID from various hardware ID formats
std::pair<uint16_t, uint16_t> extract_vid_pid(const std::string& hardware_id) {
    // Common patterns: USB\\VID_1A86&PID_7523, VID_1A86&PID_7523, etc.
    std::regex vid_pid_regex(R"(VID_([0-9A-Fa-f]{4}).*?PID_([0-9A-Fa-f]{4}))",
                             std::regex_constants::icase);
    std::smatch match;

    if (std::regex_search(hardware_id, match, vid_pid_regex) &&
        match.size() >= 3) {
        return {parse_hex(match[1].str()), parse_hex(match[2].str())};
    }

    return {0, 0};
}

}  // anonymous namespace

SerialPortScanner::SerialPortScanner() noexcept {
    initialize_ch340_identifiers();
    last_scan_time_.store(std::chrono::steady_clock::time_point{});
    last_port_refresh_ = std::chrono::steady_clock::now();
    last_error_time_ = std::chrono::steady_clock::now();

    spdlog::info("SerialPortScanner initialized with default configuration");
}

SerialPortScanner::SerialPortScanner(const ScannerConfig& config) noexcept
    : config_(config) {
    initialize_ch340_identifiers();
    last_scan_time_.store(std::chrono::steady_clock::time_point{});
    last_port_refresh_ = std::chrono::steady_clock::now();
    last_error_time_ = std::chrono::steady_clock::now();

    if (!config_.is_valid()) {
        spdlog::warn(
            "Invalid scanner configuration provided, using default values");
        config_ = ScannerConfig{};
    }

    if (config_.enable_background_monitoring) {
        start_worker_threads();
    }
    spdlog::info("SerialPortScanner initialized with custom configuration");
    if (config_.enable_debug_logging) {
        spdlog::info(
            "Debug logging enabled, scan timeout: {} ms, cache TTL: {} ms",
            config_.scan_timeout.count(), config_.cache_ttl.count());
    }
}

void SerialPortScanner::set_config(const ScannerConfig& config) noexcept {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (!config.is_valid()) {
        spdlog::warn("Attempted to set invalid configuration, ignoring");
        return;
    }

    bool monitoring_changed = (config_.enable_background_monitoring !=
                               config.enable_background_monitoring);
    config_ = config;

    if (monitoring_changed) {
        if (config_.enable_background_monitoring &&
            !monitoring_active_.load()) {
            start_worker_threads();
        } else if (!config_.enable_background_monitoring &&
                   monitoring_active_.load()) {
            stop_monitoring();
            stop_worker_threads();
        }
    }

    spdlog::info("Scanner configuration updated");
}

ScannerConfig SerialPortScanner::get_config() const noexcept {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return config_;
}

void SerialPortScanner::initialize_ch340_identifiers() noexcept {
    // Initialize the CH340 device identifiers map
    ch340_identifiers[0x1A86] = {{0x7523, "CH340"},  {0x5523, "CH341A"},
                                 {0x7522, "CH340K"}, {0x7521, "CH340E"},
                                 {0x7520, "CH340B"}, {0x7519, "CH340C"},
                                 {0x7518, "CH340T"}, {0x7516, "CH340X"},
                                 {0x7515, "CH340N"}, {0x55D4, "CH341T"}};

    // QinHeng Electronics alternative VID
    ch340_identifiers[0x4348] = {{0x5523, "CH341A"}, {0x5584, "CH340H"}};

    spdlog::info("Initialized CH340 device identifiers with {} vendor IDs",
                 ch340_identifiers.size());
}

void SerialPortScanner::start_worker_threads() {
    shutdown_workers_.store(false);
    size_t thread_count =
        std::min(config_.max_concurrent_scans,
                 static_cast<size_t>(std::thread::hardware_concurrency()));

    worker_threads_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        worker_threads_.emplace_back([this]() {
            while (!shutdown_workers_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // Worker thread logic can be expanded for async operations
            }
        });
    }

    spdlog::info("Started {} worker threads", thread_count);
}

void SerialPortScanner::stop_worker_threads() {
    shutdown_workers_.store(true);
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    spdlog::info("Stopped all worker threads");
}

void SerialPortScanner::monitor_thread_func() {
    spdlog::info("Port monitoring thread started");

    std::unordered_set<std::string> previous_ports;
    auto last_scan = std::chrono::steady_clock::now();

    // Initial scan to establish baseline
    if (auto result = list_available_ports(false);
        std::holds_alternative<std::vector<PortInfo>>(result)) {
        for (const auto& port : std::get<std::vector<PortInfo>>(result)) {
            previous_ports.insert(port.device);
        }
    }

    while (monitoring_active_.load()) {
        std::unique_lock<std::mutex> lock(monitor_mutex_);

        if (monitor_cv_.wait_for(lock, config_.monitor_interval, [this] {
                return !monitoring_active_.load();
            })) {
            break;  // Monitoring was stopped
        }

        if (!monitoring_active_.load())
            break;

        try {
            // Perform port scan
            auto scan_start = std::chrono::steady_clock::now();
            auto result = list_available_ports(false);

            if (std::holds_alternative<std::vector<PortInfo>>(result)) {
                std::unordered_set<std::string> current_ports;
                for (const auto& port :
                     std::get<std::vector<PortInfo>>(result)) {
                    current_ports.insert(port.device);
                }

                // Detect added ports
                for (const auto& port : current_ports) {
                    if (previous_ports.find(port) == previous_ports.end()) {
                        PortEvent event{PortEvent::Type::ADDED, port, "",
                                        std::chrono::steady_clock::now(),
                                        "Port detected during monitoring"};
                        if (monitor_callback_) {
                            monitor_callback_(event);
                        }
                        spdlog::info("Port added: {}", port);
                    }
                }

                // Detect removed ports
                for (const auto& port : previous_ports) {
                    if (current_ports.find(port) == current_ports.end()) {
                        PortEvent event{PortEvent::Type::REMOVED, port, "",
                                        std::chrono::steady_clock::now(),
                                        "Port removed during monitoring"};
                        if (monitor_callback_) {
                            monitor_callback_(event);
                        }
                        spdlog::info("Port removed: {}", port);
                    }
                }

                previous_ports = std::move(current_ports);
                consecutive_errors_.store(0);
            } else {
                // Handle scan error
                const auto& error = std::get<ErrorInfo>(result);
                PortEvent event{PortEvent::Type::ERROR_TYPE, "", "",
                                std::chrono::steady_clock::now(),
                                error.message};
                if (monitor_callback_) {
                    monitor_callback_(event);
                }

                uint32_t errors = consecutive_errors_.fetch_add(1) + 1;
                spdlog::warn(
                    "Port monitoring scan failed ({} consecutive errors): {}",
                    errors, error.message);

                if (errors >= MAX_CONSECUTIVE_ERRORS &&
                    config_.enable_error_recovery) {
                    spdlog::error(
                        "Too many consecutive monitoring errors, attempting "
                        "recovery");
                    recover_from_error();
                }
            }

            auto scan_duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - scan_start);

            if (config_.enable_performance_logging) {
                spdlog::info("Monitor scan completed in {}",
                             format_duration(scan_duration));
            }

        } catch (const std::exception& e) {
            spdlog::error("Exception in monitoring thread: {}", e.what());
            handle_error("monitoring", e);
        }
    }

    spdlog::info("Port monitoring thread stopped");
}

void SerialPortScanner::update_statistics(
    const std::chrono::microseconds& scan_time, bool success,
    size_t port_count) const noexcept {
    stats_.total_scans.fetch_add(1);
    if (success) {
        stats_.successful_scans.fetch_add(1);
        stats_.ports_found.fetch_add(port_count);
    } else {
        stats_.failed_scans.fetch_add(1);
        stats_.scan_errors.fetch_add(1);
    }

    auto time_us = static_cast<uint64_t>(scan_time.count());
    stats_.total_scan_time.fetch_add(time_us);
    stats_.last_scan_time.store(time_us);

    // Update min/max scan times
    uint64_t current_min = stats_.min_scan_time.load();
    while (time_us < current_min &&
           !stats_.min_scan_time.compare_exchange_weak(current_min, time_us)) {
        // Keep trying until successful or time_us is no longer smaller
    }

    uint64_t current_max = stats_.max_scan_time.load();
    while (time_us > current_max &&
           !stats_.max_scan_time.compare_exchange_weak(current_max, time_us)) {
        // Keep trying until successful or time_us is no longer larger
    }
}

bool SerialPortScanner::is_cache_valid(
    const std::string& port_name) const noexcept {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = port_cache_.find(port_name);
    if (it == port_cache_.end()) {
        return false;
    }

    return !it->second.is_expired(config_.cache_ttl);
}

void SerialPortScanner::update_cache(const std::string& port_name,
                                     bool available,
                                     const std::string& error) const noexcept {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto& entry = port_cache_[port_name];
    entry.timestamp = std::chrono::steady_clock::now();
    entry.port_path = port_name;
    entry.is_available = available;
    entry.last_error = error;
    entry.access_count++;

    // Cleanup cache if it's getting too large
    if (port_cache_.size() > config_.max_cache_size) {
        cleanup_cache();
    }
}

void SerialPortScanner::cleanup_cache() const noexcept {
    // Remove oldest entries when cache is full
    if (port_cache_.size() <= config_.max_cache_size) {
        return;
    }

    std::vector<std::pair<std::chrono::steady_clock::time_point, std::string>>
        entries;
    entries.reserve(port_cache_.size());

    for (const auto& [name, entry] : port_cache_) {
        entries.emplace_back(entry.timestamp, name);
    }

    // Sort by timestamp (oldest first)
    std::sort(entries.begin(), entries.end());

    // Remove oldest entries to get back to reasonable size
    size_t to_remove = port_cache_.size() - (config_.max_cache_size * 3 / 4);
    for (size_t i = 0; i < to_remove && i < entries.size(); ++i) {
        port_cache_.erase(entries[i].second);
    }
    spdlog::info("Cleaned up {} cache entries, {} entries remaining", to_remove,
                 port_cache_.size());
}

void SerialPortScanner::log_performance_stats() const noexcept {
    auto stats = get_statistics();

    spdlog::info("Performance Statistics:");
    spdlog::info("  Total scans: {}", stats.total_scans.load());
    spdlog::info("  Success rate: {:.2f}%", stats.get_success_rate());
    spdlog::info("  Average scan time: {}",
                 format_duration(std::chrono::microseconds(
                     static_cast<uint64_t>(stats.get_average_scan_time()))));
    spdlog::info("  Cache hit rate: {:.2f}%", stats.get_cache_hit_rate());
    spdlog::info("  Total ports found: {}", stats.ports_found.load());
    spdlog::info("  CH340 devices found: {}", stats.ch340_devices_found.load());
}

std::string SerialPortScanner::format_error(
    const std::string& operation, const std::string& details) const noexcept {
    std::ostringstream oss;
    oss << "SerialPortScanner::" << operation << " failed: " << details;
    return oss.str();
}

void SerialPortScanner::handle_error(const std::string& context,
                                     const std::exception& e) const noexcept {
    last_error_ = ErrorInfo(e.what(), 0, context);
    // last_error_time_ = std::chrono::steady_clock::now();
    // consecutive_errors_.fetch_add(1);

    spdlog::error("Error in {}: {}", context, e.what());
}

void SerialPortScanner::recover_from_error() noexcept {
    spdlog::info("Attempting error recovery...");

    // Clear cache to force fresh data
    refresh_cache();

    // Reset consecutive error counter
    consecutive_errors_.store(0);

    // Small delay before resuming operations
    std::this_thread::sleep_for(config_.retry_interval);

    spdlog::info("Error recovery completed");
}

std::pair<bool, std::string> SerialPortScanner::is_ch340_device(
    uint16_t vid, uint16_t pid, std::string_view description) const noexcept {
    // Check against known CH340 identifiers
    auto vid_it = ch340_identifiers.find(vid);
    if (vid_it != ch340_identifiers.end()) {
        auto pid_it = vid_it->second.find(pid);
        if (pid_it != vid_it->second.end()) {
            return {true, pid_it->second};
        }
    }

    // Fallback: check description for CH340/CH341 strings
    std::string desc_lower(description);
    std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(),
                   ::tolower);

    if (desc_lower.find("ch340") != std::string::npos) {
        return {true, "CH340 (detected by description)"};
    }
    if (desc_lower.find("ch341") != std::string::npos) {
        return {true, "CH341 (detected by description)"};
    }

    // Check custom detectors
    for (const auto& [name, detector] : device_detectors_) {
        try {
            auto result = detector(vid, pid, description);
            if (result.first) {
                return result;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Custom detector '{}' threw exception: {}", name,
                         e.what());
        }
    }

    return {false, ""};
}

#ifdef _WIN32

SerialPortScanner::Result<std::vector<SerialPortScanner::PortInfo>>
SerialPortScanner::list_available_ports(bool highlight_ch340) {
    auto scan_start = std::chrono::steady_clock::now();

    try {
        std::vector<PortInfo> ports;
        ports.reserve(INITIAL_PORT_VECTOR_SIZE);

        HDEVINFO device_info_set =
            SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (device_info_set == INVALID_HANDLE_VALUE) {
            auto error_msg = format_error(
                "list_available_ports", "Failed to get device information set");
            spdlog::error(error_msg);
            return ErrorInfo(error_msg, GetLastError(),
                             "Windows SetupDiGetClassDevs");
        }

        SP_DEVINFO_DATA device_info_data{};
        device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD device_index = 0; SetupDiEnumDeviceInfo(
                 device_info_set, device_index, &device_info_data);
             ++device_index) {
            PortInfo port_info;
            char buffer[1024] = {0};
            DWORD buffer_size = sizeof(buffer);

            // Get device description
            if (SetupDiGetDeviceRegistryPropertyA(
                    device_info_set, &device_info_data, SPDRP_DEVICEDESC,
                    nullptr, reinterpret_cast<PBYTE>(buffer), buffer_size,
                    &buffer_size)) {
                port_info.description = std::string(buffer);
            }

            // Get hardware ID
            buffer_size = sizeof(buffer);
            if (SetupDiGetDeviceRegistryPropertyA(
                    device_info_set, &device_info_data, SPDRP_HARDWAREID,
                    nullptr, reinterpret_cast<PBYTE>(buffer), buffer_size,
                    &buffer_size)) {
                port_info.hardware_id = std::string(buffer);
                auto [vid, pid] = extract_vid_pid(port_info.hardware_id);
                port_info.vendor_id = std::to_string(vid);
                port_info.product_id = std::to_string(pid);

                if (highlight_ch340) {
                    auto [is_ch340_val, model] = is_ch340_device(
                        vid, pid, port_info.description);  // Renamed is_ch340
                                                           // to avoid conflict
                    port_info.is_ch340 = is_ch340_val;
                    port_info.ch340_model = model;
                    if (is_ch340_val) {
                        stats_.ch340_devices_found.fetch_add(1);
                    }
                }
            }

            // Get manufacturer
            buffer_size = sizeof(buffer);
            if (SetupDiGetDeviceRegistryPropertyA(
                    device_info_set, &device_info_data, SPDRP_MFG, nullptr,
                    reinterpret_cast<PBYTE>(buffer), buffer_size,
                    &buffer_size)) {
                port_info.manufacturer = std::string(buffer);
            }

            // Get friendly name to extract COM port
            buffer_size = sizeof(buffer);
            if (SetupDiGetDeviceRegistryPropertyA(
                    device_info_set, &device_info_data, SPDRP_FRIENDLYNAME,
                    nullptr, reinterpret_cast<PBYTE>(buffer), buffer_size,
                    &buffer_size)) {
                std::string friendly_name(buffer);
                std::regex com_regex(R"(COM(\d+))");
                std::smatch match;
                if (std::regex_search(friendly_name, match, com_regex)) {
                    port_info.device = "COM" + match[1].str();
                }
            }

            if (!port_info.device.empty()) {
                port_info.last_seen = std::chrono::steady_clock::now();
                port_info.scan_count = 1;

                // Check if port is actually available
                HANDLE handle = CreateFileA(
                    port_info.device.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                port_info.is_available = (handle != INVALID_HANDLE_VALUE);
                if (handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(handle);
                }

                // Update cache
                update_cache(port_info.device, port_info.is_available);

                ports.push_back(std::move(port_info));
            }
        }

        SetupDiDestroyDeviceInfoList(device_info_set);

        auto scan_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - scan_start);

        update_statistics(scan_duration, true, ports.size());
        last_scan_time_.store(std::chrono::steady_clock::now());

        if (config_.enable_performance_logging) {
            spdlog::info("Port scan completed: found {} ports in {}",
                         ports.size(), format_duration(scan_duration));
        }

        return ports;

    } catch (const std::exception& e) {
        auto scan_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - scan_start);
        update_statistics(scan_duration, false, 0);
        handle_error("list_available_ports", e);
        return ErrorInfo(format_error("list_available_ports", e.what()), 0,
                         "exception");
    }
}

#else  // Linux implementation

SerialPortScanner::Result<std::vector<SerialPortScanner::PortInfo>>
SerialPortScanner::list_available_ports(bool highlight_ch340) {
    auto scan_start = std::chrono::steady_clock::now();

    try {
        std::vector<PortInfo> ports;
        ports.reserve(INITIAL_PORT_VECTOR_SIZE);

        // Use udev for device enumeration
        struct udev* udev_context = udev_new();
        if (!udev_context) {
            auto error_msg = format_error("list_available_ports",
                                          "Failed to create udev context");
            spdlog::error(error_msg);
            return ErrorInfo(error_msg, errno, "udev_new");
        }

        struct udev_enumerate* enumerate = udev_enumerate_new(udev_context);
        if (!enumerate) {
            udev_unref(udev_context);
            auto error_msg = format_error("list_available_ports",
                                          "Failed to create udev enumerator");
            spdlog::error(error_msg);
            return ErrorInfo(error_msg, errno, "udev_enumerate_new");
        }

        // Filter for tty devices
        udev_enumerate_add_match_subsystem(enumerate, "tty");
        udev_enumerate_scan_devices(enumerate);

        struct udev_list_entry* device_list =
            udev_enumerate_get_list_entry(enumerate);
        struct udev_list_entry* entry;

        udev_list_entry_foreach(entry, device_list) {
            const char* path = udev_list_entry_get_name(entry);
            struct udev_device* device =
                udev_device_new_from_syspath(udev_context, path);

            if (!device)
                continue;

            const char* devnode = udev_device_get_devnode(device);
            if (!devnode) {
                udev_device_unref(device);
                continue;
            }

            PortInfo port_info;
            port_info.device = devnode;

            // Get device properties
            if (const char* desc =
                    udev_device_get_property_value(device, "ID_MODEL")) {
                port_info.description = desc;
            }

            if (const char* vid_str =
                    udev_device_get_property_value(device, "ID_VENDOR_ID")) {
                port_info.vendor_id = vid_str;
            }

            if (const char* pid_str =
                    udev_device_get_property_value(device, "ID_MODEL_ID")) {
                port_info.product_id = pid_str;
            }

            if (const char* serial =
                    udev_device_get_property_value(device, "ID_SERIAL_SHORT")) {
                port_info.serial_number = serial;
            }

            if (const char* mfg =
                    udev_device_get_property_value(device, "ID_VENDOR")) {
                port_info.manufacturer = mfg;
            }

            // Check if port is available
            int fd = open(devnode, O_RDWR | O_NOCTTY | O_NONBLOCK);
            port_info.is_available = (fd >= 0);
            if (fd >= 0) {
                close(fd);
            }

            if (highlight_ch340 && !port_info.vendor_id.empty() &&
                !port_info.product_id.empty()) {
                uint16_t vid = parse_hex(port_info.vendor_id);
                uint16_t pid = parse_hex(port_info.product_id);
                auto [is_ch340_val, model] = is_ch340_device(
                    vid, pid, port_info.description);  // Renamed is_ch340
                port_info.is_ch340 = is_ch340_val;
                port_info.ch340_model = model;
                if (is_ch340_val) {
                    stats_.ch340_devices_found.fetch_add(1);
                }
            }

            port_info.last_seen = std::chrono::steady_clock::now();
            port_info.scan_count = 1;

            // Update cache
            update_cache(port_info.device, port_info.is_available);

            ports.push_back(std::move(port_info));
            udev_device_unref(device);
        }

        udev_enumerate_unref(enumerate);
        udev_unref(udev_context);

        auto scan_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - scan_start);

        update_statistics(scan_duration, true, ports.size());
        last_scan_time_.store(std::chrono::steady_clock::now());

        if (config_.enable_performance_logging) {
            spdlog::info("Port scan completed: found {} ports in {}",
                         ports.size(), format_duration(scan_duration));
        }

        return ports;

    } catch (const std::exception& e) {
        auto scan_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - scan_start);
        update_statistics(scan_duration, false, 0);
        handle_error("list_available_ports", e);
        return ErrorInfo(format_error("list_available_ports", e.what()), 0,
                         "exception");
    }
}

#endif

// Common implementation for all platforms

SerialPortScanner::Result<std::optional<SerialPortScanner::PortDetails>>
SerialPortScanner::get_port_details(std::string_view port_name) {
    if (port_name.empty()) {
        return ErrorInfo("Port name cannot be empty", 0, "get_port_details");
    }

    try {
        std::optional<PortDetails> details;

#ifdef _WIN32
        details = get_port_details_win(port_name);
#else
        details = get_port_details_linux(port_name);
#endif

        if (details && config_.enable_debug_logging) {
            spdlog::info("Retrieved details for port: {}", port_name);
        }
        return details;
    } catch (const std::exception& e) {
        handle_error("get_port_details", e);
        return ErrorInfo(format_error("get_port_details", e.what()), 0,
                         "exception");
    }
}

void SerialPortScanner::list_available_ports_async(
    std::function<void(Result<std::vector<PortInfo>>)> callback,
    bool highlight_ch340) {
    if (!callback) {
        spdlog::warn("Async port scan called with null callback");
        return;
    }

    // Launch async task
    std::thread([this, callback = std::move(callback), highlight_ch340]() {
        try {
            auto result = list_available_ports(highlight_ch340);
            callback(result);
        } catch (const std::exception& e) {
            callback(
                ErrorInfo(format_error("list_available_ports_async", e.what()),
                          0, "async_exception"));
        }
    }).detach();
}

bool SerialPortScanner::register_device_detector(
    const std::string& detector_name,
    std::function<std::pair<bool, std::string>(uint16_t, uint16_t,
                                               std::string_view)>
        detector) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (device_detectors_.find(detector_name) != device_detectors_.end()) {
        spdlog::warn("Device detector '{}' already exists", detector_name);
        return false;
    }

    device_detectors_[detector_name] = std::move(detector);
    spdlog::info("Registered custom device detector: {}", detector_name);
    return true;
}

bool SerialPortScanner::start_monitoring(
    std::function<void(const PortEvent&)> callback) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (monitoring_active_.load()) {
        spdlog::warn("Port monitoring is already active");
        return false;
    }

    monitor_callback_ = std::move(callback);
    monitoring_active_.store(true);

    monitor_thread_ = std::make_unique<std::thread>(
        &SerialPortScanner::monitor_thread_func, this);

    spdlog::info("Started port monitoring");
    return true;
}

void SerialPortScanner::stop_monitoring() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (!monitoring_active_.load()) {
        return;
    }

    monitoring_active_.store(false);
    monitor_cv_.notify_all();

    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    monitor_thread_.reset();
    monitor_callback_ = nullptr;

    spdlog::info("Stopped port monitoring");
}

bool SerialPortScanner::is_monitoring() const noexcept {
    return monitoring_active_.load();
}

ScannerStats SerialPortScanner::get_statistics() const noexcept {
    return stats_;  // Atomic members are copied safely
}

void SerialPortScanner::reset_statistics() noexcept {
    stats_.reset();
    spdlog::info("Scanner statistics reset");
}

SerialPortScanner::Result<bool> SerialPortScanner::is_port_available(
    std::string_view port_name) {
    if (port_name.empty()) {
        return ErrorInfo("Port name cannot be empty", 0, "is_port_available");
    }

    std::string port_str(port_name);

    // Check cache first
    if (is_cache_valid(port_str)) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = port_cache_.find(port_str);
        if (it != port_cache_.end()) {
            stats_.cache_hits.fetch_add(1);
            return it->second.is_available;
        }
    }

    stats_.cache_misses.fetch_add(1);

    try {
        bool available = false;

#ifdef _WIN32
        HANDLE handle =
            CreateFileA(port_str.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        available = (handle != INVALID_HANDLE_VALUE);
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
#else
        int fd = open(port_str.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        available = (fd >= 0);
        if (fd >= 0) {
            close(fd);
        }
#endif

        update_cache(port_str, available);
        return available;

    } catch (const std::exception& e) {
        handle_error("is_port_available", e);
        return ErrorInfo(format_error("is_port_available", e.what()), 0,
                         "exception");
    }
}

void SerialPortScanner::refresh_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    port_cache_.clear();
    known_ports_.clear();
    last_port_refresh_ = std::chrono::steady_clock::now();
    spdlog::info("Port cache refreshed");
}

std::string SerialPortScanner::get_cache_info() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    std::ostringstream oss;
    oss << "Cache Statistics:\n";
    oss << "  Total entries: " << port_cache_.size() << "\n";
    oss << "  Max capacity: " << config_.max_cache_size << "\n";
    // Use fmt::format for consistent formatting with spdlog if this string is
    // logged via spdlog For direct string return, this is fine.
    oss << "  Hit rate: " << std::fixed << std::setprecision(2)
        << stats_.get_cache_hit_rate() << "%\n";
    oss << "  Cache hits: " << stats_.cache_hits.load() << "\n";
    oss << "  Cache misses: " << stats_.cache_misses.load() << "\n";

    auto now = std::chrono::steady_clock::now();
    size_t expired_count = 0;
    for (const auto& [name, entry] : port_cache_) {
        if (entry.is_expired(config_.cache_ttl)) {
            expired_count++;
        }
    }
    oss << "  Expired entries: " << expired_count;

    return oss.str();
}

SerialPortScanner::Result<bool> SerialPortScanner::validate_port(
    std::string_view port_name, std::chrono::milliseconds timeout) {
    if (port_name.empty()) {
        return ErrorInfo("Port name cannot be empty", 0, "validate_port");
    }

    try {
        std::string port_str(port_name);
        auto start_time = std::chrono::steady_clock::now();

#ifdef _WIN32
        HANDLE handle =
            CreateFileA(port_str.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        bool valid = (handle != INVALID_HANDLE_VALUE);
        if (valid) {
            // Perform basic validation operations
            DCB dcb = {};
            dcb.DCBlength = sizeof(DCB);
            valid = GetCommState(handle, &dcb) != 0;
            CloseHandle(handle);
        }
#else
        int fd = open(port_str.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        bool valid = (fd >= 0);

        if (valid) {
            // Perform basic validation operations
            struct termios tio;
            valid = (tcgetattr(fd, &tio) == 0);
            close(fd);
        }
#endif

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);

        if (duration > timeout) {
            return ErrorInfo("Port validation timed out", 0, "timeout");
        }

        return valid;

    } catch (const std::exception& e) {
        handle_error("validate_port", e);
        return ErrorInfo(format_error("validate_port", e.what()), 0,
                         "exception");
    }
}

std::chrono::steady_clock::time_point SerialPortScanner::get_last_scan_time()
    const noexcept {
    return last_scan_time_.load();
}

void SerialPortScanner::set_retry_strategy(
    uint32_t max_retries, std::chrono::milliseconds retry_delay) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    config_.max_retry_count = max_retries;
    config_.retry_interval = retry_delay;

    spdlog::info("Updated retry strategy: max_retries={}, delay={}ms",
                 max_retries, retry_delay.count());
}

std::optional<SerialPortScanner::ErrorInfo> SerialPortScanner::get_last_error()
    const noexcept {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return last_error_;
}

#ifdef _WIN32

std::optional<SerialPortScanner::PortDetails>
SerialPortScanner::get_port_details_win(std::string_view port_name) {
    // Implementation for Windows port details retrieval using registry and
    // SetupDi APIs
    PortDetails details;
    details.device_name = std::string(port_name);
    details.last_detected = std::chrono::steady_clock::now();

    try {
        // Get device information using SetupDi APIs
        HDEVINFO device_info_set =
            SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (device_info_set == INVALID_HANDLE_VALUE) {
            return std::nullopt;
        }

        SP_DEVINFO_DATA device_info_data{};
        device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD device_index = 0; SetupDiEnumDeviceInfo(
                 device_info_set, device_index, &device_info_data);
             ++device_index) {
            char buffer[1024] = {0};
            DWORD buffer_size = sizeof(buffer);

            // Get friendly name and check if it matches our port
            if (SetupDiGetDeviceRegistryPropertyA(
                    device_info_set, &device_info_data, SPDRP_FRIENDLYNAME,
                    nullptr, reinterpret_cast<PBYTE>(buffer), buffer_size,
                    &buffer_size)) {
                std::string friendly_name(buffer);
                if (friendly_name.find(port_name) != std::string::npos) {
                    details.friendly_name = friendly_name;

                    // Get additional properties
                    buffer_size = sizeof(buffer);
                    if (SetupDiGetDeviceRegistryPropertyA(
                            device_info_set, &device_info_data,
                            SPDRP_DEVICEDESC, nullptr,
                            reinterpret_cast<PBYTE>(buffer), buffer_size,
                            &buffer_size)) {
                        details.description = std::string(buffer);
                    }

                    buffer_size = sizeof(buffer);
                    if (SetupDiGetDeviceRegistryPropertyA(
                            device_info_set, &device_info_data,
                            SPDRP_HARDWAREID, nullptr,
                            reinterpret_cast<PBYTE>(buffer), buffer_size,
                            &buffer_size)) {
                        details.hardware_id = std::string(buffer);
                        auto [vid, pid] = extract_vid_pid(details.hardware_id);
                        details.vid = vid;
                        details.pid = pid;
                    }

                    buffer_size = sizeof(buffer);
                    if (SetupDiGetDeviceRegistryPropertyA(
                            device_info_set, &device_info_data, SPDRP_MFG,
                            nullptr, reinterpret_cast<PBYTE>(buffer),
                            buffer_size, &buffer_size)) {
                        details.manufacturer = std::string(buffer);
                    }

                    buffer_size = sizeof(buffer);
                    if (SetupDiGetDeviceRegistryPropertyA(
                            device_info_set, &device_info_data,
                            SPDRP_LOCATION_INFORMATION, nullptr,
                            reinterpret_cast<PBYTE>(buffer), buffer_size,
                            &buffer_size)) {
                        details.location = std::string(buffer);
                    }

                    // Test port availability and get baud rate info
                    HANDLE handle = CreateFileA(
                        port_name.data(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (handle != INVALID_HANDLE_VALUE) {
                        details.is_available = true;

                        DCB dcb = {};
                        dcb.DCBlength = sizeof(DCB);
                        if (GetCommState(handle, &dcb)) {
                            details.current_baud_rate = dcb.BaudRate;
                        }

                        COMMPROP comm_prop = {};
                        if (GetCommProperties(handle, &comm_prop)) {
                            details.max_baud_rate = comm_prop.dwMaxBaud;
                        }

                        CloseHandle(handle);
                    }

                    fill_details_win(details);
                    SetupDiDestroyDeviceInfoList(device_info_set);
                    return details;
                }
            }
        }

        SetupDiDestroyDeviceInfoList(device_info_set);

    } catch (const std::exception& e) {
        if (config_.enable_debug_logging) {
            spdlog::warn("Failed to get Windows port details for {}: {}",
                         port_name, e.what());
        }
    }

    return std::nullopt;
}

void SerialPortScanner::fill_details_win(PortDetails& details) {
    // Fill additional Windows-specific details using registry queries
    try {
        // Query additional registry information
        HKEY hkey;
        std::string reg_path = "HARDWARE\\DEVICEMAP\\SERIALCOMM";

        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, reg_path.c_str(), 0, KEY_READ,
                          &hkey) == ERROR_SUCCESS) {
            char value_name[256];
            char value_data[256];
            DWORD value_name_size = sizeof(value_name);
            DWORD value_data_size = sizeof(value_data);
            DWORD value_type;

            for (DWORD index = 0;
                 RegEnumValueA(hkey, index, value_name, &value_name_size,
                               nullptr, &value_type,
                               reinterpret_cast<LPBYTE>(value_data),
                               &value_data_size) == ERROR_SUCCESS;
                 ++index) {
                if (std::string(value_data) == details.device_name) {
                    details.registry_path = value_name;
                    break;
                }

                value_name_size = sizeof(value_name);
                value_data_size = sizeof(value_data);
            }

            RegCloseKey(hkey);
        }

        // Set additional Windows-specific properties
        details.driver_name =
            "Microsoft";  // Default, could be queried from driver info

    } catch (const std::exception& e) {
        if (config_.enable_debug_logging) {
            spdlog::warn("Failed to fill Windows details: {}", e.what());
        }
    }
}

#else  // Linux implementation

std::optional<SerialPortScanner::PortDetails>
SerialPortScanner::get_port_details_linux(std::string_view port_name) {
    PortDetails details;
    details.device_name = std::string(port_name);
    details.last_detected = std::chrono::steady_clock::now();

    try {
        struct udev* udev_context = udev_new();
        if (!udev_context) {
            return std::nullopt;
        }

        struct udev_device* device =
            udev_device_new_from_syspath(udev_context, port_name.data());
        if (!device) {
            udev_unref(udev_context);
            return std::nullopt;
        }

        // Get device properties
        if (const char* desc = udev_device_get_property_value(device, "ID_MODEL")) {
            details.description = desc;
        }

        if (const char* vid_str = udev_device_get_property_value(device, "ID_VENDOR_ID")) {
            details.vid = vid_str;
        }

        if (const char* pid_str = udev_device_get_property_value(device, "ID_MODEL_ID")) {
            details.pid = pid_str;
        }

        if (const char* serial = udev_device_get_property_value(device, "ID_SERIAL_SHORT")) {
            details.serial_number = serial;
        }

        if (const char* mfg = udev_device_get_property_value(device, "ID_VENDOR")) {
            details.manufacturer = mfg;
        }

        if (const char* driver = udev_device_get_property_value(device, "ID_USB_DRIVER")) {
            details.driver_name = driver;
        }

        if (const char* subsystem = udev_device_get_subsystem(device)) {
            details.interface = subsystem;
        }

        if (const char* syspath = udev_device_get_syspath(device)) {
            details.location = syspath;
        }

        // Test port availability
        int fd = open(port_name.data(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0) {
            details.is_available = true;
            close(fd);
        }

        fill_details_linux(details);

        udev_device_unref(device);
        udev_unref(udev_context);

        return details;

    } catch (const std::exception& e) {
        if (config_.enable_debug_logging) {
            spdlog::warn("Failed to get Linux port details for {}: {}", 
                         port_name, e.what());
        }
    }

    return std::nullopt;
}

void SerialPortScanner::fill_details_linux(PortDetails& details) {
    try {
        std::string sysfs_base = "/sys/class/tty/";
        std::string device_basename = details.device_name;

        size_t last_slash = device_basename.find_last_of('/');
        if (last_slash != std::string::npos) {
            device_basename = device_basename.substr(last_slash + 1);
        }

        std::string sysfs_path = sysfs_base + device_basename;

        auto read_sysfs_file = [](const std::string& path) -> std::string {
            std::ifstream file(path);
            if (file.is_open()) {
                std::string content;
                std::getline(file, content);
                return content;
            }
            return "";
        };

        std::string product = read_sysfs_file(sysfs_path + "/device/../../product");
        if (!product.empty()) {
            details.product = product;
        }

        std::string version = read_sysfs_file(sysfs_path + "/device/../../version");
        if (!version.empty()) {
            details.recommended_baud_rates = version;
        }

        if (details.device_name.find("ttyUSB") != std::string::npos ||
            details.device_name.find("ttyACM") != std::string::npos) {
            details.max_baud_rate = 921600;
        } else if (details.device_name.find("ttyS") != std::string::npos) {
            details.max_baud_rate = 115200;
        }

    } catch (const std::exception& e) {
        if (config_.enable_debug_logging) {
            spdlog::warn("Failed to fill Linux details: {}", e.what());
        }
    }
}

#endif

}  // namespace serial
}  // namespace atom
