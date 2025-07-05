#include <unordered_set>
#ifdef _WIN32

#include "bluetooth_serial.hpp"

// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <bluetoothAPIs.h>
#include <devguid.h>
#include <initguid.h>
#include <setupapi.h>
#include <ws2bth.h>
#include <rpc.h>
// clang-format on

#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#ifdef _MSC_VER
#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "rpcrt4.lib")
#endif

namespace serial {

class BluetoothSerialImpl {
public:
    BluetoothSerialImpl()
        : socket_(INVALID_SOCKET),
          config_{},
          connectedDevice_{},
          stopAsyncRead_(false),
          stopScan_(false),
          stats_{} {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw BluetoothException("WSAStartup failed");
        }
    }

    ~BluetoothSerialImpl() {
        stopAsyncWorker();
        disconnect();
        WSACleanup();
    }

    bool isBluetoothEnabled() const {
        BLUETOOTH_RADIO_INFO radioInfo = {sizeof(BLUETOOTH_RADIO_INFO)};
        HANDLE radio;
        BLUETOOTH_FIND_RADIO_PARAMS findParams = {
            sizeof(BLUETOOTH_FIND_RADIO_PARAMS)};

        HBLUETOOTH_RADIO_FIND hFind =
            BluetoothFindFirstRadio(&findParams, &radio);
        if (hFind) {
            BluetoothFindRadioClose(hFind);
            CloseHandle(radio);
            return true;
        }
        return false;
    }

    void enableBluetooth(bool enable) {
        throw BluetoothException(
            "Cannot directly enable/disable Bluetooth adapter on Windows, "
            "user must operate through system settings");
    }

    std::vector<BluetoothDeviceInfo> scanDevices(std::chrono::seconds timeout) {
        std::vector<BluetoothDeviceInfo> devices;

        BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {
            sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS), 1, 1, 1, 1, 1,
            static_cast<UCHAR>(timeout.count())};

        BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};

        HBLUETOOTH_DEVICE_FIND hFind =
            BluetoothFindFirstDevice(&searchParams, &deviceInfo);
        if (hFind) {
            do {
                BluetoothDeviceInfo info;
                char addressStr[18] = {0};
                sprintf_s(addressStr, sizeof(addressStr),
                          "%02X:%02X:%02X:%02X:%02X:%02X",
                          deviceInfo.Address.rgBytes[5],
                          deviceInfo.Address.rgBytes[4],
                          deviceInfo.Address.rgBytes[3],
                          deviceInfo.Address.rgBytes[2],
                          deviceInfo.Address.rgBytes[1],
                          deviceInfo.Address.rgBytes[0]);
                info.address = addressStr;

                char narrowName[248] = {0};
                wcstombs(narrowName, deviceInfo.szName, 248);
                info.name = narrowName;

                info.paired = (deviceInfo.fAuthenticated != 0);
                info.connected = (deviceInfo.fConnected != 0);
                info.rssi = 0;

                devices.push_back(info);
            } while (BluetoothFindNextDevice(hFind, &deviceInfo));

            BluetoothFindDeviceClose(hFind);
        }

        return devices;
    }

    void scanDevicesAsync(
        std::function<void(const BluetoothDeviceInfo&)> onDeviceFound,
        std::function<void()> onScanComplete, std::chrono::seconds timeout) {
        stopScan_ = true;
        if (scanThread_.joinable()) {
            scanThread_.join();
        }

        stopScan_ = false;
        scanThread_ = std::thread([this, onDeviceFound, onScanComplete,
                                   timeout]() {
            std::unordered_set<std::string> discoveredAddresses;

            BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {
                sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS), 1, 1, 1, 1, 1,
                static_cast<UCHAR>(timeout.count())};

            BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};

            auto startTime = std::chrono::steady_clock::now();

            while (!stopScan_ &&
                   (std::chrono::steady_clock::now() - startTime) < timeout) {
                HBLUETOOTH_DEVICE_FIND hFind =
                    BluetoothFindFirstDevice(&searchParams, &deviceInfo);
                if (hFind) {
                    do {
                        if (stopScan_)
                            break;

                        BluetoothDeviceInfo info;
                        char addressStr[18] = {0};
                        sprintf_s(addressStr, sizeof(addressStr),
                                  "%02X:%02X:%02X:%02X:%02X:%02X",
                                  deviceInfo.Address.rgBytes[5],
                                  deviceInfo.Address.rgBytes[4],
                                  deviceInfo.Address.rgBytes[3],
                                  deviceInfo.Address.rgBytes[2],
                                  deviceInfo.Address.rgBytes[1],
                                  deviceInfo.Address.rgBytes[0]);
                        info.address = addressStr;

                        if (discoveredAddresses.find(info.address) ==
                            discoveredAddresses.end()) {
                            char narrowName[248] = {0};
                            wcstombs(narrowName, deviceInfo.szName, 248);
                            info.name = narrowName;

                            info.paired = (deviceInfo.fAuthenticated != 0);
                            info.connected = (deviceInfo.fConnected != 0);
                            info.rssi = 0;

                            discoveredAddresses.insert(info.address);
                            onDeviceFound(info);
                        }
                    } while (!stopScan_ &&
                             BluetoothFindNextDevice(hFind, &deviceInfo));

                    BluetoothFindDeviceClose(hFind);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            if (!stopScan_) {
                onScanComplete();
            }
        });
    }

    void stopScan() {
        stopScan_ = true;
        if (scanThread_.joinable()) {
            scanThread_.join();
        }
    }

    void connect(const std::string& address, const BluetoothConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (isConnected()) {
            disconnect();
        }

        config_ = config;

        SOCKADDR_BTH btAddr = {0};
        btAddr.addressFamily = AF_BTH;
        btAddr.port = 1;

        if (!stringToBluetoothAddress(address, &btAddr.btAddr)) {
            throw BluetoothException("Invalid Bluetooth address: " + address);
        }

        socket_ = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        if (socket_ == INVALID_SOCKET) {
            throw BluetoothException("Failed to create Bluetooth socket: " +
                                     std::to_string(WSAGetLastError()));
        }

        DWORD timeout = static_cast<DWORD>(config.connectTimeout.count());
        if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout,
                       sizeof(timeout)) != 0) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw BluetoothException("Failed to set receive timeout: " +
                                     std::to_string(WSAGetLastError()));
        }

        if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout,
                       sizeof(timeout)) != 0) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw BluetoothException("Failed to set send timeout: " +
                                     std::to_string(WSAGetLastError()));
        }

        if (::connect(socket_, (SOCKADDR*)&btAddr, sizeof(btAddr)) ==
            SOCKET_ERROR) {
            int error = WSAGetLastError();
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw BluetoothException("Failed to connect to Bluetooth device: " +
                                     std::to_string(error));
        }

        u_long mode = 1;
        if (ioctlsocket(socket_, FIONBIO, &mode) != 0) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw BluetoothException("Failed to set non-blocking mode: " +
                                     std::to_string(WSAGetLastError()));
        }

        connectedDevice_ = BluetoothDeviceInfo{};
        connectedDevice_->address = address;

        auto devices = scanDevices(std::chrono::seconds(1));
        for (const auto& device : devices) {
            if (device.address == address) {
                connectedDevice_ = device;
                connectedDevice_->connected = true;
                break;
            }
        }

        stats_.bytesSent = 0;
        stats_.bytesReceived = 0;
        stats_.connectionTime = std::chrono::steady_clock::now();

        if (connectionListener_) {
            connectionListener_(true);
        }

        spdlog::info("Bluetooth device connected: {}", address);
    }

    void disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;

            if (connectionListener_) {
                connectionListener_(false);
            }

            if (connectedDevice_) {
                spdlog::info("Bluetooth device disconnected: {}",
                             connectedDevice_->address);
            }
            connectedDevice_.reset();
        }
    }

    bool isConnected() const { return socket_ != INVALID_SOCKET; }

    std::optional<BluetoothDeviceInfo> getConnectedDevice() const {
        return connectedDevice_;
    }

    bool pair(const std::string& address, const std::string& pin) {
        BLUETOOTH_ADDRESS btAddr = {0};
        if (!stringToBluetoothAddress(address, &btAddr.ullLong)) {
            throw BluetoothException("Invalid Bluetooth address: " + address);
        }

        wchar_t widePin[16] = {0};
        mbstowcs(widePin, pin.c_str(), pin.length());

        BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};
        deviceInfo.Address = btAddr;
        deviceInfo.ulClassofDevice = 0;

        DWORD authResult = BluetoothAuthenticateDevice(
            nullptr, nullptr, &deviceInfo, widePin, pin.length());

        return (authResult == ERROR_SUCCESS);
    }

    bool unpair(const std::string& address) {
        BLUETOOTH_ADDRESS btAddr = {0};
        if (!stringToBluetoothAddress(address, &btAddr.ullLong)) {
            throw BluetoothException("Invalid Bluetooth address: " + address);
        }

        BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};
        deviceInfo.Address = btAddr;

        DWORD removeResult = BluetoothRemoveDevice(&btAddr);

        return (removeResult == ERROR_SUCCESS);
    }

    std::vector<BluetoothDeviceInfo> getPairedDevices() {
        std::vector<BluetoothDeviceInfo> pairedDevices;

        BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {
            sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS), 1, 0, 1, 1, 1, 15};

        BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};

        HBLUETOOTH_DEVICE_FIND hFind =
            BluetoothFindFirstDevice(&searchParams, &deviceInfo);
        if (hFind) {
            do {
                if (deviceInfo.fAuthenticated) {
                    BluetoothDeviceInfo info;

                    char addressStr[18] = {0};
                    sprintf_s(addressStr, sizeof(addressStr),
                              "%02X:%02X:%02X:%02X:%02X:%02X",
                              deviceInfo.Address.rgBytes[5],
                              deviceInfo.Address.rgBytes[4],
                              deviceInfo.Address.rgBytes[3],
                              deviceInfo.Address.rgBytes[2],
                              deviceInfo.Address.rgBytes[1],
                              deviceInfo.Address.rgBytes[0]);
                    info.address = addressStr;

                    char narrowName[248] = {0};
                    wcstombs(narrowName, deviceInfo.szName, 248);
                    info.name = narrowName;

                    info.paired = true;
                    info.connected = (deviceInfo.fConnected != 0);
                    info.rssi = 0;

                    pairedDevices.push_back(info);
                }
            } while (BluetoothFindNextDevice(hFind, &deviceInfo));

            BluetoothFindDeviceClose(hFind);
        }

        return pairedDevices;
    }

    std::vector<uint8_t> read(size_t maxBytes) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        if (maxBytes == 0) {
            return {};
        }

        std::vector<uint8_t> buffer(maxBytes);

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_, &readSet);

        struct timeval timeout;
        auto readTimeoutMs = config_.serialConfig.getReadTimeout();
        timeout.tv_sec = readTimeoutMs.count() / 1000;
        timeout.tv_usec = (readTimeoutMs.count() % 1000) * 1000;

        int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);

        if (selectResult == SOCKET_ERROR) {
            throw SerialIOException("Read error: " +
                                    std::to_string(WSAGetLastError()));
        } else if (selectResult == 0) {
            return {};
        }

        int bytesRead = recv(socket_, reinterpret_cast<char*>(buffer.data()),
                             static_cast<int>(maxBytes), 0);

        if (bytesRead == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                return {};
            }
            throw SerialIOException("Read error: " + std::to_string(error));
        } else if (bytesRead == 0) {
            disconnect();
            throw SerialPortNotOpenException();
        }

        buffer.resize(bytesRead);
        stats_.bytesReceived += bytesRead;
        return buffer;
    }

    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout) {
        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        if (bytes == 0) {
            return {};
        }

        std::vector<uint8_t> result;
        result.reserve(bytes);

        auto startTime = std::chrono::steady_clock::now();

        while (result.size() < bytes) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - startTime);

            if (elapsed >= timeout) {
                throw SerialTimeoutException();
            }

            auto remainingTimeout = timeout - elapsed;

            auto originalTimeout = config_.serialConfig.getReadTimeout();

            try {
                config_.serialConfig.withReadTimeout(remainingTimeout);

                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }

                config_.serialConfig.withReadTimeout(originalTimeout);
            } catch (...) {
                config_.serialConfig.withReadTimeout(originalTimeout);
                throw;
            }

            if (result.size() < bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        return result;
    }

    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback) {
        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        stopAsyncWorker();

        stopAsyncRead_ = false;
        asyncReadThread_ = std::thread([this, maxBytes, callback]() {
            while (!stopAsyncRead_ && isConnected()) {
                try {
                    auto data = read(maxBytes);
                    if (!data.empty() && !stopAsyncRead_) {
                        callback(std::move(data));
                    }
                } catch (const SerialTimeoutException&) {
                } catch (const std::exception& e) {
                    if (!stopAsyncRead_) {
                        if (connectionListener_) {
                            connectionListener_(false);
                        }
                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    std::vector<uint8_t> readAvailable() {
        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        u_long bytesAvailable = 0;
        if (ioctlsocket(socket_, FIONREAD, &bytesAvailable) != 0) {
            throw SerialIOException("Cannot get available bytes: " +
                                    std::to_string(WSAGetLastError()));
        }

        if (bytesAvailable == 0) {
            return {};
        }

        return read(bytesAvailable);
    }

    size_t write(std::span<const uint8_t> data) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        if (data.empty()) {
            return 0;
        }

        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(socket_, &writeSet);

        struct timeval timeout;
        auto writeTimeoutMs = config_.serialConfig.getWriteTimeout();
        timeout.tv_sec = writeTimeoutMs.count() / 1000;
        timeout.tv_usec = (writeTimeoutMs.count() % 1000) * 1000;

        int selectResult = select(0, nullptr, &writeSet, nullptr, &timeout);

        if (selectResult == SOCKET_ERROR) {
            throw SerialIOException("Write error: " +
                                    std::to_string(WSAGetLastError()));
        } else if (selectResult == 0) {
            throw SerialTimeoutException();
        }

        int bytesSent =
            send(socket_, reinterpret_cast<const char*>(data.data()),
                 static_cast<int>(data.size()), 0);

        if (bytesSent == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                return 0;
            }
            throw SerialIOException("Write error: " + std::to_string(error));
        }

        stats_.bytesSent += bytesSent;
        return static_cast<size_t>(bytesSent);
    }

    void flush() {
        try {
            readAvailable();
        } catch (...) {
        }
    }

    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        u_long bytesAvailable = 0;
        if (ioctlsocket(socket_, FIONREAD, &bytesAvailable) != 0) {
            throw SerialIOException("Cannot get available bytes: " +
                                    std::to_string(WSAGetLastError()));
        }

        return static_cast<size_t>(bytesAvailable);
    }

    void setConnectionListener(std::function<void(bool connected)> listener) {
        connectionListener_ = std::move(listener);
    }

    BluetoothSerial::Statistics getStatistics() const { return stats_; }

private:
    SOCKET socket_;
    BluetoothConfig config_;
    std::optional<BluetoothDeviceInfo> connectedDevice_;
    std::function<void(bool)> connectionListener_;
    mutable std::mutex mutex_;
    std::thread asyncReadThread_;
    std::atomic<bool> stopAsyncRead_;
    std::thread scanThread_;
    std::atomic<bool> stopScan_;
    BluetoothSerial::Statistics stats_;

    bool stringToBluetoothAddress(const std::string& addressStr,
                                  ULONGLONG* btAddr) {
        if (btAddr == nullptr)
            return false;

        std::string cleanAddress = addressStr;

        cleanAddress.erase(
            std::remove_if(cleanAddress.begin(), cleanAddress.end(),
                           [](char c) { return !std::isxdigit(c); }),
            cleanAddress.end());

        if (cleanAddress.length() != 12) {
            return false;
        }

        *btAddr = std::stoull(cleanAddress, nullptr, 16);
        return true;
    }

    void stopAsyncWorker() {
        if (asyncReadThread_.joinable()) {
            stopAsyncRead_ = true;
            asyncReadThread_.join();
        }
    }
};

}  // namespace serial

#endif  // _WIN32
