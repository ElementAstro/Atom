#include "scanner.hpp"

#ifdef _WIN32
#include <setupapi.h>
#include <windows.h>
#include <regex>
#else
#include <libudev.h>
#include <algorithm>
#endif

#include <thread>

#include "atom/error/exception.hpp"

namespace {
// 错误处理辅助函数
template <typename T>
inline atom::serial::SerialPortScanner::Result<T> make_error_result(
    const std::string& message, int code = 0) {
    return atom::serial::SerialPortScanner::ErrorInfo{message, code};
}

// 字符串转小写辅助函数 - 减少内存分配
inline void to_lower_inplace(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
}

// 字符串比较辅助函数 - 不区分大小写
inline bool contains_case_insensitive(std::string_view haystack,
                                     std::string_view needle) {
    auto it = std::search(
        haystack.begin(), haystack.end(), needle.begin(), needle.end(),
        [](char ch1, char ch2) {
            return std::tolower(static_cast<unsigned char>(ch1)) ==
                   std::tolower(static_cast<unsigned char>(ch2));
        });
    return it != haystack.end();
}

// 检查是否为虚拟端口
#ifdef _WIN32
inline bool is_virtual_port(const std::string& port_name) {
    return port_name.find("CNCA") == 0 || 
           port_name.find("VCOM") == 0 || 
           port_name.find("VPCOM") == 0;
}
#else
inline bool is_virtual_port(const std::string& device_path) {
    return device_path.find("/dev/ptmx") == 0 || 
           device_path.find("/dev/pts") == 0 || 
           device_path.find("/dev/ttyS") == 0;
}
#endif

} // anonymous namespace

namespace atom {
namespace serial {

SerialPortScanner::SerialPortScanner() noexcept {
    ch340_identifiers[0x1a86] = {
        {0x7523, "CH340"},   // Product ID for CH340
        {0x5523, "CH341"},   // Product ID for CH341
        {0x7522, "CH340K"},  // Product ID for CH340K
        {0x5512, "CH341A"},  // Product ID for CH341A
        {0x55D5, "CH343"},   // Product ID for CH343
        {0x55D4, "CH9102"}   // Product ID for CH9102 (Compatible chip)
    };

    // Add QinHeng Electronics vendor ID (another common manufacturer of CH340
    // chips)
    ch340_identifiers[0x4348] = {{0x5523, "CH341 (QinHeng)"},
                                 {0x7523, "CH340 (QinHeng)"}};
}

SerialPortScanner::SerialPortScanner(const ScannerConfig& config) noexcept
    : SerialPortScanner() {
    config_ = config;
}

void SerialPortScanner::set_config(const ScannerConfig& config) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

SerialPortScanner::ScannerConfig SerialPortScanner::get_config()
    const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

std::pair<bool, std::string> SerialPortScanner::is_ch340_device(
    uint16_t vid, uint16_t pid, std::string_view description) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Don't perform detection if disabled in config
    if (!config_.detect_ch340) {
        return {false, ""};
    }

    // Exact match if description contains "USB-SERIAL CH340" - 避免不必要的转换
    if (contains_case_insensitive(description, "USB-SERIAL CH340")) {
        return {true, "USB-SERIAL CH340(Exact Match)"};
    }

    // Check if VID exists in our CH340 identifiers - 使用更高效的查找
    auto vid_it = ch340_identifiers.find(vid);
    if (vid_it != ch340_identifiers.end()) {
        // Check if PID exists in the CH340 identifiers for the given VID
        auto& pid_map = vid_it->second;
        auto pid_it = pid_map.find(pid);
        if (pid_it != pid_map.end()) {
            return {true, pid_it->second};
        }
    }

    // Check custom device detectors
    for (const auto& [_, detector] : device_detectors_) {
        try {
            auto [detected, model] = detector(vid, pid, description);
            if (detected) {
                return {true, model};
            }
        } catch (...) {
            // 忽略自定义检测器异常，继续下一个检测
            continue;
        }
    }

    // 最后才做大小写不敏感查找（性能较低）
    if (contains_case_insensitive(description, "ch340")) {
        return {true, "CH340 Series(From Description)"};
    }

    // Not a CH340 device if none of the above checks passed
    return {false, ""};
}

bool SerialPortScanner::register_device_detector(
    const std::string& detector_name,
    std::function<std::pair<bool, std::string>(uint16_t, uint16_t,
                                               std::string_view)>
        detector) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if detector with this name already exists
    if (device_detectors_.find(detector_name) != device_detectors_.end()) {
        return false;
    }

    device_detectors_[detector_name] = std::move(detector);
    return true;
}

SerialPortScanner::Result<std::vector<SerialPortScanner::PortInfo>>
SerialPortScanner::list_available_ports(bool highlight_ch340) {
    try {
        std::vector<PortInfo> result;

#ifdef _WIN32
        // Windows平台实现
        // 改进资源管理，使用RAII模式
        struct SetupDiDeleter {
            void operator()(HDEVINFO hDevInfo) const {
                if (hDevInfo != INVALID_HANDLE_VALUE) {
                    SetupDiDestroyDeviceInfoList(hDevInfo);
                }
            }
        };
        
        HDEVINFO device_info_set =
            SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (device_info_set == INVALID_HANDLE_VALUE) {
            return ErrorInfo{"Failed to get device information set",
                             static_cast<int>(GetLastError())};
        }

        // 使用RAII对象管理设备信息集
        std::unique_ptr<void, SetupDiDeleter> device_guard(device_info_set);

        SP_DEVINFO_DATA device_info_data{};
        device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

        // 预先为结果分配一些空间，减少重新分配
        result.reserve(8); // 假设通常有最多8个端口
        char buffer[256];  // 重用缓冲区

        for (DWORD i = 0;
             SetupDiEnumDeviceInfo(device_info_set, i, &device_info_data);
             i++) {
            std::string port_name;
            std::string description;
            DWORD property_type;
            DWORD required_size;
            uint16_t vid = 0, pid = 0;

            // Get friendly name (description)
            if (SetupDiGetDeviceRegistryProperty(
                    device_info_set, &device_info_data, SPDRP_FRIENDLYNAME,
                    &property_type, reinterpret_cast<PBYTE>(buffer),
                    sizeof(buffer), &required_size)) {
                description.assign(buffer); // 优化：直接分配，避免不必要的复制
            }

            // 获取硬件ID以提取VID和PID
            if (SetupDiGetDeviceRegistryProperty(
                    device_info_set, &device_info_data, SPDRP_HARDWAREID,
                    &property_type, reinterpret_cast<PBYTE>(buffer),
                    sizeof(buffer), &required_size)) {
                std::string hw_id(buffer); // 必须用std::string，因为std::regex_search需要它
                // 编译一次正则表达式并重用
                static const std::regex vid_regex("VID_(\\w{4})");
                static const std::regex pid_regex("PID_(\\w{4})");
                std::smatch match;

                if (std::regex_search(hw_id, match, vid_regex) &&
                    match.size() > 1) {
                    vid = std::stoi(match[1].str(), nullptr, 16);
                }

                if (std::regex_search(hw_id, match, pid_regex) &&
                    match.size() > 1) {
                    pid = std::stoi(match[1].str(), nullptr, 16);
                }
            }

            HKEY key =
                SetupDiOpenDevRegKey(device_info_set, &device_info_data,
                                     DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

            if (key) {
                DWORD type;
                DWORD size = sizeof(buffer);

                if (RegQueryValueEx(key, "PortName", nullptr, &type,
                                    reinterpret_cast<LPBYTE>(buffer),
                                    &size) == ERROR_SUCCESS) {
                    port_name = buffer;
                }

                RegCloseKey(key);
            }

            if (!port_name.empty()) {
                // Skip virtual ports if configured to do so
                if (!config_.include_virtual_ports) {
                    // Simple heuristic: most common virtual port prefixes
                    if (is_virtual_port(port_name)) {
                        continue;
                    }
                }

                auto [is_ch340, model] = is_ch340_device(vid, pid, description);

                PortInfo port_info{
                    .device = port_name,
                    .description = description,
                    .is_ch340 = highlight_ch340 ? is_ch340 : false,
                    .ch340_model = highlight_ch340 ? model : ""};

                result.push_back(port_info);
            }
        }
        
        // 不需要手动调用 SetupDiDestroyDeviceInfoList，
        // 因为 device_guard unique_ptr 的自定义删除器会处理
#else
        // Linux平台实现
        // 使用 RAII 更优雅地管理 udev 资源
        struct UdevDeleter {
            void operator()(struct udev* u) const {
                if (u) udev_unref(u);
            }
        };
        
        struct UdevEnumerateDeleter {
            void operator()(struct udev_enumerate* e) const {
                if (e) udev_enumerate_unref(e);
            }
        };
        
        struct UdevDeviceDeleter {
            void operator()(struct udev_device* d) const {
                if (d) udev_device_unref(d);
            }
        };
        
        struct udev* udev = udev_new();
        if (!udev) {
            return ErrorInfo{"Failed to create udev context", errno};
        }
        
        // 使用自定义删除器的智能指针实现 RAII
        std::unique_ptr<struct udev, UdevDeleter> udev_guard(udev);
        
        struct udev_enumerate* enumerate = udev_enumerate_new(udev);
        if (!enumerate) {
            return ErrorInfo{"Failed to create udev enumeration", errno};
        }
        
        std::unique_ptr<struct udev_enumerate, UdevEnumerateDeleter> enumerate_guard(enumerate);

        udev_enumerate_add_match_subsystem(enumerate, "tty");
        udev_enumerate_scan_devices(enumerate);

        struct udev_list_entry* devices =
            udev_enumerate_get_list_entry(enumerate);
        struct udev_list_entry* dev_list_entry;

        udev_list_entry_foreach(dev_list_entry, devices) {
            const char* path = udev_list_entry_get_name(dev_list_entry);
            struct udev_device* dev = udev_device_new_from_syspath(udev, path);

            // RAII for device cleanup
            std::unique_ptr<struct udev_device, UdevDeviceDeleter> dev_guard(dev);

            const char* dev_path = udev_device_get_devnode(dev);
            if (dev_path) {
                std::string device = dev_path;
                std::string description;
                uint16_t vid = 0, pid = 0;

                // Skip virtual ports if configured to do so
                if (!config_.include_virtual_ports) {
                    // Common virtual port prefixes in Linux
                    if (is_virtual_port(device)) {
                        continue;
                    }
                }

                // Get USB device information
                struct udev_device* usb_dev = nullptr;
                usb_dev = udev_device_get_parent_with_subsystem_devtype(
                    dev, "usb", "usb_device");

                if (usb_dev) {
                    const char* vendor =
                        udev_device_get_sysattr_value(usb_dev, "idVendor");
                    const char* product =
                        udev_device_get_sysattr_value(usb_dev, "idProduct");
                    const char* manufacturer =
                        udev_device_get_sysattr_value(usb_dev, "manufacturer");
                    const char* product_name =
                        udev_device_get_sysattr_value(usb_dev, "product");

                    if (vendor)
                        vid = std::stoi(vendor, nullptr, 16);
                    if (product)
                        pid = std::stoi(product, nullptr, 16);

                    if (manufacturer && product_name) {
                        description = std::string(manufacturer) + " " +
                                      std::string(product_name);
                    } else if (product_name) {
                        description = product_name;
                    } else if (manufacturer) {
                        description = manufacturer;
                    } else {
                        description = "Unknown USB device";
                    }
                } else {
                    description = "Serial Device";
                }

                auto [is_ch340, model] = is_ch340_device(vid, pid, description);

                PortInfo port_info{
                    .device = device,
                    .description = description,
                    .is_ch340 = highlight_ch340 ? is_ch340 : false,
                    .ch340_model = highlight_ch340 ? model : ""};

                result.push_back(port_info);
            }
        }
#endif
        return result;
    } catch (const std::exception& e) {
        return ErrorInfo{std::string("Error listing serial ports: ") + e.what(),
                         0};
    }
}

void SerialPortScanner::list_available_ports_async(
    std::function<void(Result<std::vector<PortInfo>>)> callback,
    bool highlight_ch340) {
    // Launch async operation in a new thread
    std::thread([this, callback = std::move(callback), highlight_ch340]() {
        auto result = this->list_available_ports(highlight_ch340);
        callback(std::move(result));
    }).detach();
}

SerialPortScanner::Result<std::optional<SerialPortScanner::PortDetails>>
SerialPortScanner::get_port_details(std::string_view port_name) {
    try {
        Result<std::vector<PortInfo>> ports_result = list_available_ports();

        // Handle error case
        if (std::holds_alternative<ErrorInfo>(ports_result)) {
            return std::get<ErrorInfo>(ports_result);
        }

        auto& ports = std::get<std::vector<PortInfo>>(ports_result);

        auto it = std::find_if(ports.begin(), ports.end(),
                               [&port_name](const PortInfo& info) {
                                   return info.device == port_name;
                               });

        if (it == ports.end()) {
            return std::optional<PortDetails>{};  // Return empty optional
        }

        PortDetails details;

        // Fill basic information
        details.device_name = it->device;
        details.description = it->description;
        details.is_ch340 = it->is_ch340;
        details.ch340_model = it->ch340_model;

        // Fill platform-specific details
#ifdef _WIN32
        auto win_details = get_port_details_win(port_name);
        if (win_details) {
            details = *win_details;
        }
#else
        auto linux_details = get_port_details_linux(port_name);
        if (linux_details) {
            details = *linux_details;
        }
#endif

        // Provide suggested configuration for CH340 devices
        if (details.is_ch340) {
            details.recommended_baud_rates = "9600, 115200";
            details.notes =
                "CH340 devices typically work best with standard baud rates. "
                "Some systems may require dedicated drivers.";
        }

        return details;
    } catch (const std::exception& e) {
        return ErrorInfo{std::string("Error getting port details: ") + e.what(),
                         0};
    }
}

#ifdef _WIN32
std::optional<SerialPortScanner::PortDetails>
SerialPortScanner::get_port_details_win(std::string_view port_name) {
    try {
        PortDetails details;

        HDEVINFO device_info_set =
            SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (device_info_set == INVALID_HANDLE_VALUE) {
            throw ScannerError("Failed to get device information set");
        }

        // 修正 cleanup_guard 的构造
        struct DeviceInfoSetDeleter {
            void operator()(HDEVINFO handle) {
                if (handle != INVALID_HANDLE_VALUE) {
                    SetupDiDestroyDeviceInfoList(handle);
                }
            }
        };

        std::unique_ptr<void, DeviceInfoSetDeleter> cleanup_guard(
            device_info_set);

        SP_DEVINFO_DATA device_info_data{};
        device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

        bool found = false;
        for (DWORD i = 0;
             SetupDiEnumDeviceInfo(device_info_set, i, &device_info_data) &&
             !found;
             i++) {
            char buffer[256];
            DWORD property_type;
            DWORD required_size;

            HKEY key =
                SetupDiOpenDevRegKey(device_info_set, &device_info_data,
                                     DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
            if (key) {
                DWORD type;
                DWORD size = sizeof(buffer);

                if (RegQueryValueEx(key, "PortName", nullptr, &type,
                                    reinterpret_cast<LPBYTE>(buffer),
                                    &size) == ERROR_SUCCESS) {
                    std::string current_port = buffer;
                    if (current_port == port_name) {
                        found = true;

                        details.device_name = current_port;

                        // Get friendly name
                        if (SetupDiGetDeviceRegistryProperty(
                                device_info_set, &device_info_data,
                                SPDRP_FRIENDLYNAME, &property_type,
                                reinterpret_cast<PBYTE>(buffer), sizeof(buffer),
                                &required_size)) {
                            details.description = buffer;
                        }

                        // Get hardware ID
                        if (SetupDiGetDeviceRegistryProperty(
                                device_info_set, &device_info_data,
                                SPDRP_HARDWAREID, &property_type,
                                reinterpret_cast<PBYTE>(buffer), sizeof(buffer),
                                &required_size)) {
                            details.hardware_id = buffer;

                            // Extract VID and PID from hardware ID
                            std::regex vid_regex("VID_(\\w{4})");
                            std::regex pid_regex("PID_(\\w{4})");
                            std::smatch match;

                            if (std::regex_search(details.hardware_id, match,
                                                  vid_regex) &&
                                match.size() > 1) {
                                details.vid = match[1].str();

                                // Check if it's a CH340 device
                                uint16_t vid_val =
                                    std::stoi(details.vid, nullptr, 16);
                                uint16_t pid_val = 0;

                                if (std::regex_search(details.hardware_id,
                                                      match, pid_regex) &&
                                    match.size() > 1) {
                                    details.pid = match[1].str();
                                    pid_val =
                                        std::stoi(details.pid, nullptr, 16);
                                }

                                auto [is_ch340, model] = is_ch340_device(
                                    vid_val, pid_val, details.description);
                                details.is_ch340 = is_ch340;
                                details.ch340_model = model;
                            }
                        }

                        // Get manufacturer
                        if (SetupDiGetDeviceRegistryProperty(
                                device_info_set, &device_info_data, SPDRP_MFG,
                                &property_type, reinterpret_cast<PBYTE>(buffer),
                                sizeof(buffer), &required_size)) {
                            details.manufacturer = buffer;
                        }

                        // Get location information
                        if (SetupDiGetDeviceRegistryProperty(
                                device_info_set, &device_info_data,
                                SPDRP_LOCATION_INFORMATION, &property_type,
                                reinterpret_cast<PBYTE>(buffer), sizeof(buffer),
                                &required_size)) {
                            details.location = buffer;
                        }

                        // Get device description
                        if (SetupDiGetDeviceRegistryProperty(
                                device_info_set, &device_info_data,
                                SPDRP_DEVICEDESC, &property_type,
                                reinterpret_cast<PBYTE>(buffer), sizeof(buffer),
                                &required_size)) {
                            details.product = buffer;
                        }
                    }
                }

                RegCloseKey(key);
            }
        }

        if (!found) {
            return std::nullopt;
        }

        // 正确处理 release() 返回值
        [[maybe_unused]] auto released_handle = cleanup_guard.release();

        return details;
    } catch (const ScannerError& e) {
        throw;
    } catch (const std::exception& e) {
        throw ScannerError(std::string("Error in get_port_details_win: ") +
                           e.what());
    }
}

// 修改未使用参数
void SerialPortScanner::fill_details_win(
    [[maybe_unused]] PortDetails& details) {
    // 未实现的函数,使用 [[maybe_unused]] 标记参数
}
#else
std::optional<SerialPortScanner::PortDetails>
SerialPortScanner::get_port_details_linux(std::string_view port_name) {
    try {
        PortDetails details;

        // 使用前面定义的删除器类型
        struct UdevDeleter {
            void operator()(struct udev* u) const {
                if (u) udev_unref(u);
            }
        };
        
        struct UdevEnumerateDeleter {
            void operator()(struct udev_enumerate* e) const {
                if (e) udev_enumerate_unref(e);
            }
        };
        
        struct UdevDeviceDeleter {
            void operator()(struct udev_device* d) const {
                if (d) udev_device_unref(d);
            }
        };

        struct udev* udev = udev_new();
        if (!udev) {
            throw ScannerError("Failed to create udev context");
        }

        // 使用统一的智能指针风格进行资源管理
        std::unique_ptr<struct udev, UdevDeleter> udev_guard(udev);

        struct udev_enumerate* enumerate = udev_enumerate_new(udev);
        if (!enumerate) {
            throw ScannerError("Failed to create udev enumeration");
        }

        std::unique_ptr<struct udev_enumerate, UdevEnumerateDeleter> enumerate_guard(enumerate);

        udev_enumerate_add_match_subsystem(enumerate, "tty");
        udev_enumerate_scan_devices(enumerate);

        struct udev_list_entry* devices =
            udev_enumerate_get_list_entry(enumerate);
        struct udev_list_entry* dev_list_entry;

        bool found = false;
        udev_list_entry_foreach(dev_list_entry, devices) {
            const char* path = udev_list_entry_get_name(dev_list_entry);
            struct udev_device* dev = udev_device_new_from_syspath(udev, path);

            // RAII for device cleanup
            std::unique_ptr<struct udev_device, UdevDeviceDeleter> dev_guard(dev);

            const char* dev_path = udev_device_get_devnode(dev);
            if (dev_path && std::string(dev_path) == port_name) {
                found = true;
                details.device_name = dev_path;

                // Get USB device information
                struct udev_device* usb_dev = nullptr;
                usb_dev = udev_device_get_parent_with_subsystem_devtype(
                    dev, "usb", "usb_device");

                if (usb_dev) {
                    const char* vendor =
                        udev_device_get_sysattr_value(usb_dev, "idVendor");
                    const char* product =
                        udev_device_get_sysattr_value(usb_dev, "idProduct");
                    const char* manufacturer =
                        udev_device_get_sysattr_value(usb_dev, "manufacturer");
                    const char* product_name =
                        udev_device_get_sysattr_value(usb_dev, "product");
                    const char* serial =
                        udev_device_get_sysattr_value(usb_dev, "serial");

                    if (vendor) {
                        details.vid = vendor;
                    }
                    if (product) {
                        details.pid = product;
                    }
                    if (manufacturer) {
                        details.manufacturer = manufacturer;
                    }
                    if (product_name) {
                        details.product = product_name;
                        details.description = details.product;
                        if (!details.manufacturer.empty()) {
                            details.description = details.manufacturer + " " +
                                                  details.description;
                        }
                    }
                    if (serial) {
                        details.serial_number = serial;
                    }

                    details.hardware_id = udev_device_get_syspath(usb_dev);

                    // Check if it's a CH340 device
                    if (!details.vid.empty() && !details.pid.empty()) {
                        uint16_t vid_val = std::stoi(details.vid, nullptr, 16);
                        uint16_t pid_val = std::stoi(details.pid, nullptr, 16);
                        auto [is_ch340, model] = is_ch340_device(
                            vid_val, pid_val, details.description);
                        details.is_ch340 = is_ch340;
                        details.ch340_model = model;
                    }
                }

                // Additional device properties from sysfs
                const char* driver = udev_device_get_driver(dev);
                if (driver) {
                    details.interface = driver;
                }

                break;
            }
        }

        if (!found) {
            return std::nullopt;
        }

        return details;
    } catch (const ScannerError& e) {
        throw;
    } catch (const std::exception& e) {
        throw ScannerError(std::string("Error in get_port_details_linux: ") +
                           e.what());
    }
}

void SerialPortScanner::fill_details_linux(PortDetails& details) {
    // This is a placeholder for future implementation
    // Would add additional Linux-specific port details
}
#endif

}  // namespace serial
}  // namespace atom
