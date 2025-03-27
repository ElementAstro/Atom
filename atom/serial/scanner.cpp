#include "scanner.hpp"

#ifdef _WIN32
#include <devguid.h>
#include <setupapi.h>
#include <windows.h>
#include <algorithm>
#include <regex>
#pragma comment(lib, "setupapi.lib")
#else
#include <fcntl.h>
#include <libudev.h>
#include <unistd.h>
#include <algorithm>
#endif

#include "atom/error/exception.hpp"

SerialPortScanner::SerialPortScanner() {
    ch340_identifiers[0x1a86] = {
        {0x7523, "CH340"},   // Product ID for CH340
        {0x5523, "CH341"},   // Product ID for CH341
        {0x7522, "CH340K"},  // Product ID for CH340K
        {0x5512, "CH341A"},  // Product ID for CH341A
        {0x55D5, "CH343"},   // Product ID for CH343
        {0x55D4, "CH9102"}   // Product ID for CH9102 (Compatible chip)
    };
}

std::pair<bool, std::string> SerialPortScanner::is_ch340_device(
    uint16_t vid, uint16_t pid, const std::string& description) const {
    // Exact match if description contains "USB-SERIAL CH340"
    if (description.find("USB-SERIAL CH340") != std::string::npos) {
        return {true, "USB-SERIAL CH340(Exact Match)"};
    }

    // Check if VID exists in our CH340 identifiers
    if (ch340_identifiers.contains(vid)) {
        // Check if PID exists in the CH340 identifiers for the given VID
        const auto& pid_map = ch340_identifiers.at(vid);
        if (pid_map.contains(pid)) {
            return {true, pid_map.at(pid)};
        }
    }

    // Consider it a CH340 device if description contains "ch340" (case
    // insensitive)
    std::string lower_desc = description;
    std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower_desc.find("ch340") != std::string::npos) {
        return {true, "CH340 Series(From Description)"};
    }

    // Not a CH340 device if none of the above checks passed
    return {false, ""};
}

std::vector<SerialPortScanner::PortInfo>
SerialPortScanner::list_available_ports([[maybe_unused]] bool highlight_ch340) {
    std::vector<PortInfo> result;

    try {
#ifdef _WIN32
        // Windows平台实现
        HDEVINFO device_info_set =
            SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (device_info_set == INVALID_HANDLE_VALUE) {
            THROW_RUNTIME_ERROR("Failed to get device information set");
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

            // Get friendly name (description)
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
                auto [is_ch340, model] = is_ch340_device(vid, pid, description);

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
            THROW_RUNTIME_ERROR("Failed to create udev context");
        }

        struct udev_enumerate* enumerate = udev_enumerate_new(udev);
        udev_enumerate_add_match_subsystem(enumerate, "tty");
        udev_enumerate_scan_devices(enumerate);

        struct udev_list_entry* devices =
            udev_enumerate_get_list_entry(enumerate);
        struct udev_list_entry* dev_list_entry;

        udev_list_entry_foreach(dev_list_entry, devices) {
            const char* path = udev_list_entry_get_name(dev_list_entry);
            struct udev_device* dev = udev_device_new_from_syspath(udev, path);

            const char* dev_path = udev_device_get_devnode(dev);
            if (dev_path) {
                std::string device = dev_path;
                std::string description;
                uint16_t vid = 0, pid = 0;

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
    } catch (const atom::error::Exception& e) {
        THROW_RUNTIME_ERROR("Error listing serial ports: ", e.what());
    }

    return result;
}

std::optional<SerialPortScanner::PortDetails>
SerialPortScanner::get_port_details(const std::string& port_name) {
    std::vector<PortInfo> ports = list_available_ports();

    auto it = std::ranges::find_if(ports.begin(), ports.end(),
                                   [&port_name](const PortInfo& info) {
                                       return info.device == port_name;
                                   });

    if (it != ports.end()) {
        PortDetails details;

        // Fill basic information
        details.device_name = it->device;
        details.description = it->description;
        details.is_ch340 = it->is_ch340;
        details.ch340_model = it->ch340_model;

        // Fill additional information (in actual implementation, this
        // information will be retrieved from the system)
#ifdef _WIN32
        // Windows platform implementation
        try {
            HDEVINFO device_info_set = SetupDiGetClassDevs(
                &GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

            if (device_info_set == INVALID_HANDLE_VALUE) {
                THROW_RUNTIME_ERROR("Failed to get device information set");
            }

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

                            // Get hardware ID
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

                            // Get manufacturer
                            if (SetupDiGetDeviceRegistryProperty(
                                    device_info_set, &device_info_data,
                                    SPDRP_MFG, &property_type,
                                    reinterpret_cast<PBYTE>(buffer),
                                    sizeof(buffer), &required_size)) {
                                details.manufacturer = buffer;
                            }

                            // Get location information
                            if (SetupDiGetDeviceRegistryProperty(
                                    device_info_set, &device_info_data,
                                    SPDRP_LOCATION_INFORMATION, &property_type,
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
        } catch (const atom::error::Exception& e) {
            THROW_RUNTIME_ERROR("Error getting port details: ", e.what());
        }
#else
        try {
            struct udev* udev = udev_new();
            if (!udev) {
                THROW_RUNTIME_ERROR("Failed to create udev context");
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
                            udev_device_get_sysattr_value(usb_dev,
                                                          "manufacturer");
                        const char* product_name =
                            udev_device_get_sysattr_value(usb_dev, "product");
                        const char* serial =
                            udev_device_get_sysattr_value(usb_dev, "serial");

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

                        details.hardware_id = udev_device_get_syspath(usb_dev);
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
        } catch (const atom::error::Exception& e) {
            THROW_RUNTIME_ERROR("Error getting port details: ", e.what());
        }
#endif

        // Provide suggested configuration for CH340 devices
        if (details.is_ch340) {
            details.recommended_baud_rates = "9600, 115200";
            details.notes = "CH340 devices may require dedicated drivers";
        }

        return details;
    }

    return std::nullopt;
}
