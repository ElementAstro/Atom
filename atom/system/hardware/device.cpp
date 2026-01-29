#include "device.hpp"

#include <array>
#include <format>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <bluetoothapis.h>
#include <setupapi.h>
// clang-format on
#else
#include <dirent.h>
#include <fcntl.h>
#include <libusb-1.0/libusb.h>
#include <linux/serial.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#if __has_include(<bluetooth/bluetooth.h>)
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

#ifdef _WIN32

namespace {
constexpr size_t BUFFER_SIZE = 512;
constexpr size_t ADDRESS_SIZE = 18;
constexpr int BLUETOOTH_SEARCH_TIMEOUT = 15;
constexpr std::array<int, 6> BYTE_ORDER = {5, 4, 3, 2, 1, 0};
}  // namespace

auto enumerateUsbDevices() -> std::vector<DeviceInfo> {
    spdlog::info("Enumerating USB devices");
    std::vector<DeviceInfo> devices;
    devices.reserve(32);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        nullptr, TEXT("USB"), nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to get USB device info set");
        return devices;
    }

    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    std::array<TCHAR, BUFFER_SIZE> buffer;

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData);
         ++i) {
        DWORD dataType;
        DWORD size;

        if (SetupDiGetDeviceRegistryProperty(
                deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC, &dataType,
                reinterpret_cast<PBYTE>(buffer.data()),
                static_cast<DWORD>(buffer.size() * sizeof(TCHAR)), &size)) {
#ifdef UNICODE
            std::wstring wideDesc(buffer.data());
            std::string description(wideDesc.begin(), wideDesc.end());
#else
            std::string description(buffer.data());
#endif
            devices.emplace_back(std::move(description), "");
            spdlog::debug("Found USB device: {}", devices.back().description);
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    spdlog::info("USB enumeration completed, found {} devices", devices.size());
    return devices;
}

auto enumerateSerialPorts() -> std::vector<DeviceInfo> {
    spdlog::info("Enumerating serial ports");
    std::vector<DeviceInfo> devices;
    devices.reserve(16);

    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     TEXT("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0, KEY_READ,
                     &hKey) != ERROR_SUCCESS) {
        spdlog::error("Failed to open serial comm registry key");
        return devices;
    }

    DWORD index = 0;
    std::array<TCHAR, 256> valueName;
    std::array<TCHAR, 256> portName;
    DWORD valueNameSize, portNameSize;

    while (true) {
        valueNameSize = static_cast<DWORD>(valueName.size());
        portNameSize = static_cast<DWORD>(portName.size() * sizeof(TCHAR));

        LONG result = RegEnumValue(
            hKey, index, valueName.data(), &valueNameSize, nullptr, nullptr,
            reinterpret_cast<LPBYTE>(portName.data()), &portNameSize);

        if (result == ERROR_NO_MORE_ITEMS)
            break;
        if (result != ERROR_SUCCESS) {
            ++index;
            continue;
        }

#ifdef UNICODE
        std::wstring widePort(portName.data());
        std::string port(widePort.begin(), widePort.end());
#else
        std::string port(portName.data());
#endif
        devices.emplace_back(port, port);
        spdlog::debug("Found serial port: {}", port);
        ++index;
    }

    RegCloseKey(hKey);
    spdlog::info("Serial port enumeration completed, found {} devices",
                 devices.size());
    return devices;
}

auto enumerateBluetoothDevices() -> std::vector<DeviceInfo> {
    spdlog::info("Enumerating Bluetooth devices");
    std::vector<DeviceInfo> devices;
    devices.reserve(16);

    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams{};
    searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    searchParams.fReturnAuthenticated = TRUE;
    searchParams.fReturnRemembered = TRUE;
    searchParams.fReturnConnected = TRUE;
    searchParams.fReturnUnknown = TRUE;
    searchParams.fIssueInquiry = TRUE;
    searchParams.cTimeoutMultiplier = BLUETOOTH_SEARCH_TIMEOUT;

    BLUETOOTH_DEVICE_INFO deviceInfo{};
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

    HBLUETOOTH_DEVICE_FIND btFind =
        BluetoothFindFirstDevice(&searchParams, &deviceInfo);
    if (btFind == nullptr) {
        spdlog::warn("No Bluetooth devices found or Bluetooth not available");
        return devices;
    }

    do {
        std::wstring wideName(deviceInfo.szName);
        std::string name(wideName.begin(), wideName.end());

        std::string address =
            std::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                        deviceInfo.Address.rgBytes[BYTE_ORDER[0]],
                        deviceInfo.Address.rgBytes[BYTE_ORDER[1]],
                        deviceInfo.Address.rgBytes[BYTE_ORDER[2]],
                        deviceInfo.Address.rgBytes[BYTE_ORDER[3]],
                        deviceInfo.Address.rgBytes[BYTE_ORDER[4]],
                        deviceInfo.Address.rgBytes[BYTE_ORDER[5]]);

        devices.emplace_back(std::move(name), std::move(address));
        spdlog::debug("Found Bluetooth device: {} - {}",
                      devices.back().description, devices.back().address);
    } while (BluetoothFindNextDevice(btFind, &deviceInfo));

    BluetoothFindDeviceClose(btFind);
    spdlog::info("Bluetooth enumeration completed, found {} devices",
                 devices.size());
    return devices;
}

#else  // Linux implementation

auto enumerateUsbDevices() -> std::vector<DeviceInfo> {
    spdlog::info("Enumerating USB devices");
    std::vector<DeviceInfo> devices;
    devices.reserve(32);

    libusb_context *ctx = nullptr;
    int ret = libusb_init(&ctx);
    if (ret < 0) {
        spdlog::error("Failed to initialize libusb: {}",
                      libusb_error_name(ret));
        return devices;
    }

    libusb_device **devList = nullptr;
    ssize_t count = libusb_get_device_list(ctx, &devList);
    if (count < 0) {
        spdlog::error("Failed to get device list: {}",
                      libusb_error_name(static_cast<int>(count)));
        libusb_exit(ctx);
        return devices;
    }

    for (ssize_t i = 0; i < count; ++i) {
        libusb_device *dev = devList[i];
        libusb_device_descriptor desc{};

        ret = libusb_get_device_descriptor(dev, &desc);
        if (ret < 0) {
            spdlog::warn("Failed to get device descriptor: {}",
                         libusb_error_name(ret));
            continue;
        }

        std::string address =
            std::format("Bus {:03d} Device {:03d}", libusb_get_bus_number(dev),
                        libusb_get_device_address(dev));

        libusb_device_handle *handle = nullptr;
        if (libusb_open(dev, &handle) == 0 && desc.iManufacturer != 0) {
            std::array<unsigned char, 256> buffer;
            int len = libusb_get_string_descriptor_ascii(
                handle, desc.iManufacturer, buffer.data(),
                static_cast<int>(buffer.size()));
            if (len > 0) {
                address += std::format(
                    " ({})", std::string(buffer.begin(), buffer.begin() + len));
            }
            libusb_close(handle);
        }

        devices.emplace_back(address, address);
        spdlog::debug("Found USB device: {}", address);
    }

    libusb_free_device_list(devList, 1);
    libusb_exit(ctx);

    spdlog::info("USB enumeration completed, found {} devices", devices.size());
    return devices;
}

auto enumerateSerialPorts() -> std::vector<DeviceInfo> {
    spdlog::info("Enumerating serial ports");
    std::vector<DeviceInfo> devices;
    devices.reserve(16);

    DIR *dp = opendir("/dev");
    if (!dp) {
        spdlog::error("Failed to open /dev directory");
        return devices;
    }

    while (auto entry = readdir(dp)) {
        std::string filename(entry->d_name);
        if (filename.starts_with("ttyS") || filename.starts_with("ttyUSB") ||
            filename.starts_with("ttyACM")) {
            std::string fullPath = "/dev/" + filename;
            devices.emplace_back(filename, fullPath);
            spdlog::debug("Found serial port: {}", filename);
        }
    }

    closedir(dp);
    spdlog::info("Serial port enumeration completed, found {} devices",
                 devices.size());
    return devices;
}

auto enumerateBluetoothDevices() -> std::vector<DeviceInfo> {
    spdlog::info("Enumerating Bluetooth devices");
    std::vector<DeviceInfo> devices;
    devices.reserve(16);

#if __has_include(<bluetooth/bluetooth.h>)
    int devId = hci_get_route(nullptr);
    if (devId < 0) {
        spdlog::error("No Bluetooth adapter available");
        return devices;
    }

    int sock = hci_open_dev(devId);
    if (sock < 0) {
        spdlog::error("Failed to open socket to Bluetooth adapter");
        return devices;
    }

    constexpr int maxRsp = 255;
    constexpr int len = 8;
    constexpr int flags = IREQ_CACHE_FLUSH;

    auto ii = std::make_unique<inquiry_info[]>(maxRsp);
    inquiry_info *localIi = ii.get();

    int numRsp = hci_inquiry(devId, len, maxRsp, nullptr, &localIi, flags);
    if (numRsp < 0) {
        spdlog::error("HCI inquiry failed");
        close(sock);
        return devices;
    }

    for (int i = 0; i < numRsp; ++i) {
        std::array<char, 19> addr{};
        std::array<char, 248> name{};

        ba2str(&(ii[i].bdaddr), addr.data());

        if (hci_read_remote_name(sock, &(ii[i].bdaddr),
                                 static_cast<int>(name.size()), name.data(),
                                 0) < 0) {
            std::strcpy(name.data(), "[unknown]");
        }

        devices.emplace_back(std::string(name.data()),
                             std::string(addr.data()));
        spdlog::debug("Found Bluetooth device: {} - {}", name.data(),
                      addr.data());
    }

    close(sock);
#else
    spdlog::warn("Bluetooth support not available (missing bluetooth headers)");
#endif

    spdlog::info("Bluetooth enumeration completed, found {} devices",
                 devices.size());
    return devices;
}

#endif

}  // namespace atom::system
