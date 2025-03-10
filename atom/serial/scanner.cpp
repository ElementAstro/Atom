#include <algorithm>
#include <atomic>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <devguid.h>
#include <setupapi.h>
#include <windows.h>
#include <regex>
#pragma comment(lib, "setupapi.lib")
#else
#include <fcntl.h>
#include <libudev.h>
#include <unistd.h>
#include <filesystem>
#endif

class SerialPortScanner {
public:
    struct PortInfo {
        std::string device;       // 端口名称（如 COM1 或 /dev/ttyUSB0）
        std::string description;  // 端口描述
        bool is_ch340{false};     // 是否为CH340设备
        std::string ch340_model;  // CH340型号（如适用）
    };

    struct PortDetails {
        std::string device_name;             // 端口名称
        std::string description;             // 端口描述
        std::string hardware_id;             // 硬件ID
        std::string vid;                     // 厂商ID（十六进制）
        std::string pid;                     // 产品ID（十六进制）
        std::string serial_number;           // 序列号
        std::string location;                // 位置
        std::string manufacturer;            // 制造商
        std::string product;                 // 产品
        std::string interface;               // 接口
        bool is_ch340{false};                // 是否为CH340设备
        std::string ch340_model;             // CH340型号（如适用）
        std::string recommended_baud_rates;  // 推荐波特率
        std::string notes;                   // 注释
    };

    SerialPortScanner() {
        // 定义已知的CH340系列设备VID/PID对
        ch340_identifiers[0x1a86] = {
            {0x7523, "CH340"},   // CH340的产品ID
            {0x5523, "CH341"},   // CH341的产品ID
            {0x7522, "CH340K"},  // CH340K的产品ID
            {0x5512, "CH341A"},  // CH341A的产品ID
            {0x55D5, "CH343"},   // CH343的产品ID
            {0x55D4, "CH9102"}   // CH9102的产品ID（兼容芯片）
        };
    }

    /**
     * 检查给定的串口是否为CH340系列设备
     *
     * @param vid 厂商ID
     * @param pid 产品ID
     * @param description 设备描述
     * @return std::pair<bool, std::string>
     * 包含是否为CH340设备的布尔值和CH340型号（如适用）
     */
    std::pair<bool, std::string> is_ch340_device(
        uint16_t vid, uint16_t pid, const std::string& description) const {
        // 如果描述包含"USB-SERIAL CH340"，则为精确匹配
        if (description.find("USB-SERIAL CH340") != std::string::npos) {
            return {true, "USB-SERIAL CH340(Exact Match)"};
        }

        // 检查VID是否在我们的CH340标识符中
        if (ch340_identifiers.contains(vid)) {
            // 检查PID是否在给定VID的CH340标识符中
            const auto& pid_map = ch340_identifiers.at(vid);
            if (pid_map.contains(pid)) {
                return {true, pid_map.at(pid)};
            }
        }

        // 如果描述中包含"ch340"（不区分大小写），则认为是CH340设备
        std::string lower_desc = description;
        std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower_desc.find("ch340") != std::string::npos) {
            return {true, "CH340 Series(From Description)"};
        }

        // 如果以上检查都未通过，则不是CH340设备
        return {false, ""};
    }

    /**
     * 列出所有可用的串口，可选择突出显示CH340设备
     *
     * @param highlight_ch340 如果为true，将标记CH340设备
     * @return std::vector<PortInfo> 包含串口信息的向量
     */
    std::vector<PortInfo> list_available_ports(bool highlight_ch340 = true) {
        std::vector<PortInfo> result;

        try {
#ifdef _WIN32
            // Windows平台实现
            HDEVINFO device_info_set = SetupDiGetClassDevs(
                &GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

            if (device_info_set == INVALID_HANDLE_VALUE) {
                throw std::runtime_error(
                    "Failed to get device information set");
            }

            SP_DEVINFO_DATA device_info_data{};
            device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

            for (DWORD i = 0;
                 SetupDiEnumDeviceInfo(device_info_set, i, &device_info_data);
                 i++) {
                char buffer[256];
                std::string port_name;
                std::string description;
                DWORD property_type;
                DWORD required_size;
                uint16_t vid = 0, pid = 0;

                // 获取友好名称（描述）
                if (SetupDiGetDeviceRegistryProperty(
                        device_info_set, &device_info_data, SPDRP_FRIENDLYNAME,
                        &property_type, reinterpret_cast<PBYTE>(buffer),
                        sizeof(buffer), &required_size)) {
                    description = buffer;
                }

                // 获取硬件ID以提取VID和PID
                if (SetupDiGetDeviceRegistryProperty(
                        device_info_set, &device_info_data, SPDRP_HARDWAREID,
                        &property_type, reinterpret_cast<PBYTE>(buffer),
                        sizeof(buffer), &required_size)) {
                    std::string hw_id = buffer;
                    std::regex vid_regex("VID_(\\w{4})");
                    std::regex pid_regex("PID_(\\w{4})");
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

                // 获取端口名称（从注册表）
                HKEY key;
                if (SetupDiOpenDevRegKey(device_info_set, &device_info_data,
                                         DICS_FLAG_GLOBAL, 0, DIREG_DEV,
                                         KEY_READ)) {
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
                    auto [is_ch340, model] =
                        is_ch340_device(vid, pid, description);

                    PortInfo port_info{.device = port_name,
                                       .description = description,
                                       .is_ch340 = is_ch340,
                                       .ch340_model = model};

                    result.push_back(port_info);
                }
            }

            SetupDiDestroyDeviceInfoList(device_info_set);
#else
            // Linux平台实现
            struct udev* udev = udev_new();
            if (!udev) {
                throw std::runtime_error("Failed to create udev context");
            }

            struct udev_enumerate* enumerate = udev_enumerate_new(udev);
            udev_enumerate_add_match_subsystem(enumerate, "tty");
            udev_enumerate_scan_devices(enumerate);

            struct udev_list_entry* devices =
                udev_enumerate_get_list_entry(enumerate);
            struct udev_list_entry* dev_list_entry;

            udev_list_entry_foreach(dev_list_entry, devices) {
                const char* path = udev_list_entry_get_name(dev_list_entry);
                struct udev_device* dev =
                    udev_device_new_from_syspath(udev, path);

                const char* dev_path = udev_device_get_devnode(dev);
                if (dev_path) {
                    std::string device = dev_path;
                    std::string description;
                    uint16_t vid = 0, pid = 0;

                    // 获取USB设备信息
                    struct udev_device* usb_dev = nullptr;
                    usb_dev = udev_device_get_parent_with_subsystem_devtype(
                        dev, "usb", "usb_device");

                    if (usb_dev) {
                        const char* vendor =
                            udev_device_get_sysattr_value(usb_dev, "idVendor");
                        const char* product =
                            udev_device_get_sysattr_value(usb_dev, "idProduct");
                        const char* manufacturer =
                            udev_device_get_sysattr_value(usb_dev,
                                                          "manufacturer");
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

                    auto [is_ch340, model] =
                        is_ch340_device(vid, pid, description);

                    PortInfo port_info{.device = device,
                                       .description = description,
                                       .is_ch340 = is_ch340,
                                       .ch340_model = model};

                    result.push_back(port_info);
                }

                udev_device_unref(dev);
            }

            udev_enumerate_unref(enumerate);
            udev_unref(udev);
#endif
        } catch (const std::exception& e) {
            std::cerr << "Error listing serial ports: " << e.what()
                      << std::endl;
        }

        return result;
    }

    /**
     * 获取特定串口的详细信息
     *
     * @param port_name 要检索详细信息的串口名称
     * @return std::optional<PortDetails>
     * 包含指定端口的详细信息的可选对象，如果找不到端口则为空
     */
    std::optional<PortDetails> get_port_details(const std::string& port_name) {
        std::vector<PortInfo> ports = list_available_ports();

        // 在可用端口列表中查找匹配的端口
        auto it = std::find_if(ports.begin(), ports.end(),
                               [&port_name](const PortInfo& info) {
                                   return info.device == port_name;
                               });

        if (it != ports.end()) {
            PortDetails details;

            // 填充基本信息
            details.device_name = it->device;
            details.description = it->description;
            details.is_ch340 = it->is_ch340;
            details.ch340_model = it->ch340_model;

            // 填充附加信息（在实际实现中，这些信息将从系统中获取）
#ifdef _WIN32
            // Windows平台实现
            try {
                HDEVINFO device_info_set = SetupDiGetClassDevs(
                    &GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                    DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

                if (device_info_set == INVALID_HANDLE_VALUE) {
                    throw std::runtime_error(
                        "Failed to get device information set");
                }

                SP_DEVINFO_DATA device_info_data{};
                device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

                bool found = false;
                for (DWORD i = 0; SetupDiEnumDeviceInfo(device_info_set, i,
                                                        &device_info_data) &&
                                  !found;
                     i++) {
                    char buffer[256];
                    DWORD property_type;
                    DWORD required_size;

                    // 获取端口名称并检查是否匹配
                    HKEY key;
                    if (SetupDiOpenDevRegKey(device_info_set, &device_info_data,
                                             DICS_FLAG_GLOBAL, 0, DIREG_DEV,
                                             KEY_READ)) {
                        DWORD type;
                        DWORD size = sizeof(buffer);

                        if (RegQueryValueEx(key, "PortName", nullptr, &type,
                                            reinterpret_cast<LPBYTE>(buffer),
                                            &size) == ERROR_SUCCESS) {
                            std::string current_port = buffer;
                            if (current_port == port_name) {
                                found = true;

                                // 获取硬件ID
                                if (SetupDiGetDeviceRegistryProperty(
                                        device_info_set, &device_info_data,
                                        SPDRP_HARDWAREID, &property_type,
                                        reinterpret_cast<PBYTE>(buffer),
                                        sizeof(buffer), &required_size)) {
                                    details.hardware_id = buffer;

                                    // 从硬件ID提取VID和PID
                                    std::regex vid_regex("VID_(\\w{4})");
                                    std::regex pid_regex("PID_(\\w{4})");
                                    std::smatch match;

                                    if (std::regex_search(details.hardware_id,
                                                          match, vid_regex) &&
                                        match.size() > 1) {
                                        details.vid = match[1].str();
                                    }

                                    if (std::regex_search(details.hardware_id,
                                                          match, pid_regex) &&
                                        match.size() > 1) {
                                        details.pid = match[1].str();
                                    }
                                }

                                // 获取制造商
                                if (SetupDiGetDeviceRegistryProperty(
                                        device_info_set, &device_info_data,
                                        SPDRP_MFG, &property_type,
                                        reinterpret_cast<PBYTE>(buffer),
                                        sizeof(buffer), &required_size)) {
                                    details.manufacturer = buffer;
                                }

                                // 获取位置信息
                                if (SetupDiGetDeviceRegistryProperty(
                                        device_info_set, &device_info_data,
                                        SPDRP_LOCATION_INFORMATION,
                                        &property_type,
                                        reinterpret_cast<PBYTE>(buffer),
                                        sizeof(buffer), &required_size)) {
                                    details.location = buffer;
                                }
                            }
                        }

                        RegCloseKey(key);
                    }
                }

                SetupDiDestroyDeviceInfoList(device_info_set);

                if (!found) {
                    return std::nullopt;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error getting port details: " << e.what()
                          << std::endl;
            }
#else
            // Linux平台实现
            try {
                struct udev* udev = udev_new();
                if (!udev) {
                    throw std::runtime_error("Failed to create udev context");
                }

                struct udev_enumerate* enumerate = udev_enumerate_new(udev);
                udev_enumerate_add_match_subsystem(enumerate, "tty");
                udev_enumerate_scan_devices(enumerate);

                struct udev_list_entry* devices =
                    udev_enumerate_get_list_entry(enumerate);
                struct udev_list_entry* dev_list_entry;

                bool found = false;
                udev_list_entry_foreach(dev_list_entry, devices) {
                    const char* path = udev_list_entry_get_name(dev_list_entry);
                    struct udev_device* dev =
                        udev_device_new_from_syspath(udev, path);

                    const char* dev_path = udev_device_get_devnode(dev);
                    if (dev_path && std::string(dev_path) == port_name) {
                        found = true;

                        // 获取USB设备信息
                        struct udev_device* usb_dev = nullptr;
                        usb_dev = udev_device_get_parent_with_subsystem_devtype(
                            dev, "usb", "usb_device");

                        if (usb_dev) {
                            const char* vendor = udev_device_get_sysattr_value(
                                usb_dev, "idVendor");
                            const char* product = udev_device_get_sysattr_value(
                                usb_dev, "idProduct");
                            const char* manufacturer =
                                udev_device_get_sysattr_value(usb_dev,
                                                              "manufacturer");
                            const char* product_name =
                                udev_device_get_sysattr_value(usb_dev,
                                                              "product");
                            const char* serial = udev_device_get_sysattr_value(
                                usb_dev, "serial");

                            if (vendor)
                                details.vid = vendor;
                            if (product)
                                details.pid = product;
                            if (manufacturer)
                                details.manufacturer = manufacturer;
                            if (product_name)
                                details.product = product_name;
                            if (serial)
                                details.serial_number = serial;

                            details.hardware_id =
                                udev_device_get_syspath(usb_dev);
                        }

                        break;
                    }

                    udev_device_unref(dev);
                }

                udev_enumerate_unref(enumerate);
                udev_unref(udev);

                if (!found) {
                    return std::nullopt;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error getting port details: " << e.what()
                          << std::endl;
            }
#endif

            // 为CH340设备提供建议配置
            if (details.is_ch340) {
                details.recommended_baud_rates = "9600, 115200";
                details.notes = "CH340设备可能需要专用驱动程序";
            }

            return details;
        }

        return std::nullopt;
    }

private:
    std::map<uint16_t, std::map<uint16_t, std::string>> ch340_identifiers;
};

int main() {
    try {
        SerialPortScanner scanner;

        // 列出所有可用的串口，突出显示CH340设备
        auto ports = scanner.list_available_ports();

        std::cout << "可用串口列表:" << std::endl;
        std::cout
            << "============================================================"
            << std::endl;
        std::cout << std::format("{:<5}{:<15}{:<10}{:<15}{}", "索引", "端口",
                                 "是CH340", "型号", "描述")
                  << std::endl;
        std::cout
            << "------------------------------------------------------------"
            << std::endl;

        std::vector<std::pair<size_t, SerialPortScanner::PortInfo>> ch340_ports;

        // 遍历每个端口并打印其信息
        for (size_t i = 0; i < ports.size(); ++i) {
            const auto& port_info = ports[i];

            // 使用对勾标记CH340设备
            std::string ch340_mark = port_info.is_ch340 ? "✓" : "";

            // 获取CH340型号
            std::string model = port_info.is_ch340 ? port_info.ch340_model : "";

            // 如果端口是CH340设备，将其添加到CH340端口列表
            if (port_info.is_ch340) {
                ch340_ports.emplace_back(i + 1, port_info);
            }

            std::cout << std::format("{:<5}{:<15}{:<10}{:<15}{}", i + 1,
                                     port_info.device, ch340_mark, model,
                                     port_info.description)
                      << std::endl;
        }

        // 优先显示CH340设备信息
        if (!ch340_ports.empty()) {
            std::cout << "\n检测到的CH340设备:" << std::endl;
            for (const auto& [idx, port_info] : ch340_ports) {
                std::cout << std::format("  {}. {} - {}", idx, port_info.device,
                                         port_info.description)
                          << std::endl;
            }
        }

        // 如果未找到端口，打印消息
        if (ports.empty()) {
            std::cout << "未找到可用串口" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}