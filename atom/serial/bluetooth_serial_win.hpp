#ifdef _WIN32

#include <BluetoothAPIs.h>
#include <Windows.h>
#include <devguid.h>
#include <initguid.h>
#include <setupapi.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include "BluetoothSerial.h"
#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "ws2_32.lib")

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
        // 初始化Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw BluetoothException("WSAStartup失败");
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
            "在Windows上程序无法直接开启/"
            "关闭蓝牙适配器，需用户通过系统设置操作");
    }

    std::vector<BluetoothDeviceInfo> scanDevices(std::chrono::seconds timeout) {
        std::vector<BluetoothDeviceInfo> devices;

        BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {
            sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),
            1,  // 返回已认证和未认证的设备
            1,  // 返回已连接和未连接的设备
            1,  // 返回已记住和未记住的设备
            1,  // 返回未知和已知的设备
            1,  // 发起新的查询
            static_cast<UCHAR>(timeout.count())  // 扫描超时(秒)
        };

        BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};

        HBLUETOOTH_DEVICE_FIND hFind =
            BluetoothFindFirstDevice(&searchParams, &deviceInfo);
        if (hFind) {
            do {
                BluetoothDeviceInfo info;
                // 获取设备地址
                wchar_t addressStr[40] = {0};
                RPC_WSTR rpcAddressStr = addressStr;
                RpcStringFromUuid(&deviceInfo.Address.rgBytes, &rpcAddressStr);
                char narrowAddress[40] = {0};
                wcstombs(narrowAddress, addressStr, 40);
                info.address = narrowAddress;

                // 获取设备名称
                char narrowName[248] = {0};
                wcstombs(narrowName, deviceInfo.szName, 248);
                info.name = narrowName;

                // 设置其他属性
                info.paired = (deviceInfo.fAuthenticated != 0);
                info.connected = (deviceInfo.fConnected != 0);
                info.rssi = 0;  // Windows API 不直接提供RSSI值

                devices.push_back(info);
            } while (BluetoothFindNextDevice(hFind, &deviceInfo));

            BluetoothFindDeviceClose(hFind);
        }

        return devices;
    }

    void scanDevicesAsync(
        std::function<void(const BluetoothDeviceInfo&)> onDeviceFound,
        std::function<void()> onScanComplete, std::chrono::seconds timeout) {
        // 确保之前的扫描已停止
        stopScan_ = true;
        if (scanThread_.joinable()) {
            scanThread_.join();
        }

        stopScan_ = false;
        scanThread_ = std::thread([this, onDeviceFound, onScanComplete,
                                   timeout]() {
            // 缓存已发现的设备，避免重复通知
            std::unordered_set<std::string> discoveredAddresses;

            BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {
                sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),
                1,  // 返回已认证和未认证的设备
                1,  // 返回已连接和未连接的设备
                1,  // 返回已记住和未记住的设备
                1,  // 返回未知和已知的设备
                1,  // 发起新的查询
                static_cast<UCHAR>(timeout.count())  // 扫描超时(秒)
            };

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
                        // 获取设备地址
                        wchar_t addressStr[40] = {0};
                        RPC_WSTR rpcAddressStr = addressStr;
                        RpcStringFromUuid(&deviceInfo.Address.rgBytes,
                                          &rpcAddressStr);
                        char narrowAddress[40] = {0};
                        wcstombs(narrowAddress, addressStr, 40);
                        info.address = narrowAddress;

                        // 如果是新设备，通知回调
                        if (discoveredAddresses.find(info.address) ==
                            discoveredAddresses.end()) {
                            // 获取设备名称
                            char narrowName[248] = {0};
                            wcstombs(narrowName, deviceInfo.szName, 248);
                            info.name = narrowName;

                            // 设置其他属性
                            info.paired = (deviceInfo.fAuthenticated != 0);
                            info.connected = (deviceInfo.fConnected != 0);
                            info.rssi = 0;  // Windows API 不直接提供RSSI值

                            discoveredAddresses.insert(info.address);
                            onDeviceFound(info);
                        }
                    } while (!stopScan_ &&
                             BluetoothFindNextDevice(hFind, &deviceInfo));

                    BluetoothFindDeviceClose(hFind);
                }

                // 避免CPU过度使用
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

        // 解析地址字符串
        SOCKADDR_BTH btAddr = {0};
        btAddr.addressFamily = AF_BTH;
        btAddr.port = 1;  // RFCOMM Channel 1 (SPP通常使用此通道)

        // 将字符串地址转换为蓝牙地址
        if (!stringToBluetoothAddress(address, &btAddr.btAddr)) {
            throw BluetoothException("无效的蓝牙地址: " + address);
        }

        // 创建蓝牙套接字
        socket_ = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        if (socket_ == INVALID_SOCKET) {
            throw BluetoothException("无法创建蓝牙套接字: " +
                                     std::to_string(WSAGetLastError()));
        }

        // 设置连接超时
        DWORD timeout = static_cast<DWORD>(config.connectTimeout.count());
        if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout,
                       sizeof(timeout)) != 0) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw BluetoothException("设置接收超时失败: " +
                                     std::to_string(WSAGetLastError()));
        }

        if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout,
                       sizeof(timeout)) != 0) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw BluetoothException("设置发送超时失败: " +
                                     std::to_string(WSAGetLastError()));
        }

        // 连接到蓝牙设备
        if (::connect(socket_, (SOCKADDR*)&btAddr, sizeof(btAddr)) ==
            SOCKET_ERROR) {
            int error = WSAGetLastError();
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw BluetoothException("连接到蓝牙设备失败: " +
                                     std::to_string(error));
        }

        // 设置为非阻塞模式
        u_long mode = 1;
        if (ioctlsocket(socket_, FIONBIO, &mode) != 0) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            throw BluetoothException("设置非阻塞模式失败: " +
                                     std::to_string(WSAGetLastError()));
        }

        // 保存连接的设备信息
        connectedDevice_ = BluetoothDeviceInfo{};
        connectedDevice_->address = address;

        // 获取更详细的设备信息
        auto devices = scanDevices(std::chrono::seconds(1));
        for (const auto& device : devices) {
            if (device.address == address) {
                connectedDevice_ = device;
                connectedDevice_->connected = true;
                break;
            }
        }

        // 重置统计信息
        stats_.bytesSent = 0;
        stats_.bytesReceived = 0;
        stats_.connectionTime = std::chrono::steady_clock::now();

        // 如果设置了连接监听器，通知连接成功
        if (connectionListener_) {
            connectionListener_(true);
        }
    }

    void disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;

            // 如果设置了连接监听器，通知断开连接
            if (connectionListener_) {
                connectionListener_(false);
            }

            connectedDevice_.reset();
        }
    }

    bool isConnected() const { return socket_ != INVALID_SOCKET; }

    std::optional<BluetoothDeviceInfo> getConnectedDevice() const {
        return connectedDevice_;
    }

    bool pair(const std::string& address, const std::string& pin) {
        // 将字符串地址转换为蓝牙地址
        BLUETOOTH_ADDRESS btAddr = {0};
        if (!stringToBluetoothAddress(address, &btAddr.ullLong)) {
            throw BluetoothException("无效的蓝牙地址: " + address);
        }

        // 转换PIN码为宽字符
        wchar_t widePin[16] = {0};
        mbstowcs(widePin, pin.c_str(), pin.length());

        BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};
        deviceInfo.Address = btAddr;
        deviceInfo.ulClassofDevice = 0;

        // 尝试认证设备
        DWORD authResult = BluetoothAuthenticateDevice(
            nullptr, nullptr, &deviceInfo, widePin, pin.length());

        return (authResult == ERROR_SUCCESS);
    }

    bool unpair(const std::string& address) {
        // 将字符串地址转换为蓝牙地址
        BLUETOOTH_ADDRESS btAddr = {0};
        if (!stringToBluetoothAddress(address, &btAddr.ullLong)) {
            throw BluetoothException("无效的蓝牙地址: " + address);
        }

        BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};
        deviceInfo.Address = btAddr;

        // 尝试移除设备
        DWORD removeResult = BluetoothRemoveDevice(&btAddr);

        return (removeResult == ERROR_SUCCESS);
    }

    std::vector<BluetoothDeviceInfo> getPairedDevices() {
        std::vector<BluetoothDeviceInfo> pairedDevices;

        BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {
            sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),
            1,  // 返回已认证和未认证的设备
            0,  // 只返回已记住的设备
            1,  // 返回已连接和未连接的设备
            1,  // 返回未知和已知的设备
            1,  // 发起新的查询
            15  // 15秒超时
        };

        BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(BLUETOOTH_DEVICE_INFO)};

        HBLUETOOTH_DEVICE_FIND hFind =
            BluetoothFindFirstDevice(&searchParams, &deviceInfo);
        if (hFind) {
            do {
                if (deviceInfo.fAuthenticated) {
                    BluetoothDeviceInfo info;

                    // 获取设备地址
                    wchar_t addressStr[40] = {0};
                    RPC_WSTR rpcAddressStr = addressStr;
                    RpcStringFromUuid(&deviceInfo.Address.rgBytes,
                                      &rpcAddressStr);
                    char narrowAddress[40] = {0};
                    wcstombs(narrowAddress, addressStr, 40);
                    info.address = narrowAddress;

                    // 获取设备名称
                    char narrowName[248] = {0};
                    wcstombs(narrowName, deviceInfo.szName, 248);
                    info.name = narrowName;

                    // 设置其他属性
                    info.paired = true;
                    info.connected = (deviceInfo.fConnected != 0);
                    info.rssi = 0;  // Windows API 不直接提供RSSI值

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

        // Windows socket在非阻塞模式下，使用select检查是否有数据可读
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_, &readSet);

        struct timeval timeout;
        timeout.tv_sec = config_.serialConfig.readTimeout.count() / 1000;
        timeout.tv_usec =
            (config_.serialConfig.readTimeout.count() % 1000) * 1000;

        int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);

        if (selectResult == SOCKET_ERROR) {
            throw SerialIOException("读取错误: " +
                                    std::to_string(WSAGetLastError()));
        } else if (selectResult == 0) {
            // 超时，没有数据可读
            return {};
        }

        int bytesRead = recv(socket_, reinterpret_cast<char*>(buffer.data()),
                             static_cast<int>(maxBytes), 0);

        if (bytesRead == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                // 非阻塞模式下没有数据可读
                return {};
            }
            throw SerialIOException("读取错误: " + std::to_string(error));
        } else if (bytesRead == 0) {
            // 连接已关闭
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

            // 临时保存原始超时设置
            auto originalTimeout = config_.serialConfig.readTimeout;

            try {
                // 临时设置超时
                config_.serialConfig.readTimeout = remainingTimeout;

                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }

                // 恢复原始超时设置
                config_.serialConfig.readTimeout = originalTimeout;
            } catch (...) {
                // 确保恢复原始超时设置
                config_.serialConfig.readTimeout = originalTimeout;
                throw;
            }

            // 如果没有读取到任何数据，短暂休眠以避免CPU负载过高
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
                    // 超时是正常的，继续
                } catch (const std::exception& e) {
                    if (!stopAsyncRead_) {
                        // 在出现错误时通知断开连接
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
            throw SerialIOException("无法获取可用字节数: " +
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

        // Windows socket在非阻塞模式下，使用select检查是否可写
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(socket_, &writeSet);

        struct timeval timeout;
        timeout.tv_sec = config_.serialConfig.writeTimeout.count() / 1000;
        timeout.tv_usec =
            (config_.serialConfig.writeTimeout.count() % 1000) * 1000;

        int selectResult = select(0, nullptr, &writeSet, nullptr, &timeout);

        if (selectResult == SOCKET_ERROR) {
            throw SerialIOException("写入错误: " +
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
                // 非阻塞模式下缓冲区已满
                return 0;
            }
            throw SerialIOException("写入错误: " + std::to_string(error));
        }

        stats_.bytesSent += bytesSent;
        return static_cast<size_t>(bytesSent);
    }

    void flush() {
        // 对于蓝牙Socket，没有直接的flush操作
        // 可以考虑读取所有可用数据来达到类似的效果
        try {
            readAvailable();
        } catch (...) {
            // 忽略错误
        }
    }

    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        u_long bytesAvailable = 0;
        if (ioctlsocket(socket_, FIONREAD, &bytesAvailable) != 0) {
            throw SerialIOException("无法获取可用字节数: " +
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

    // 辅助函数：将字符串地址转换为蓝牙地址
    bool stringToBluetoothAddress(const std::string& addressStr,
                                  ULONGLONG* btAddr) {
        if (btAddr == nullptr)
            return false;

        // 接受以下格式：
        // aa:bb:cc:dd:ee:ff
        // aabbccddeeff
        // {aabbccddeeff}

        std::string cleanAddress = addressStr;

        // 移除任何非字母数字字符
        cleanAddress.erase(
            std::remove_if(cleanAddress.begin(), cleanAddress.end(),
                           [](char c) { return !std::isxdigit(c); }),
            cleanAddress.end());

        // 检查长度是否为12
        if (cleanAddress.length() != 12) {
            return false;
        }

        // 从十六进制字符串解析
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