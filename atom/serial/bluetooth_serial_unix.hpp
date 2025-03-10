#include <iostream>
#include <unordered_set>
#if defined(__linux__)

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include "bluetooth_serial.hpp"

namespace serial {

/**
 * @class BluetoothSerialImpl
 * @brief Implementation class for Bluetooth Serial communication
 *
 * This class provides the concrete implementation for Bluetooth Serial
 * communication on Linux systems, handling device discovery, connection
 * management, and data transfer operations.
 */
class BluetoothSerialImpl {
public:
    /**
     * @brief Default constructor
     *
     * Initializes member variables to their default values.
     */
    BluetoothSerialImpl()
        : socket_(-1),
          config_{},
          connectedDevice_{},
          stopAsyncRead_(false),
          stopScan_(false),
          stats_{} {}

    /**
     * @brief Destructor
     *
     * Stops any ongoing asynchronous operations and closes open connections.
     */
    ~BluetoothSerialImpl() {
        stopAsyncWorker();
        disconnect();
    }

    /**
     * @brief Checks if Bluetooth is enabled on the system
     *
     * @return true if Bluetooth is enabled, false otherwise
     */
    bool isBluetoothEnabled() const {
        int dev_id = hci_get_route(nullptr);
        return (dev_id >= 0);
    }

    /**
     * @brief Enables or disables the Bluetooth adapter
     *
     * @param enable true to enable Bluetooth, false to disable
     * @throws BluetoothException if the operation fails
     */
    void enableBluetooth(bool enable) {
        int dev_id = hci_get_route(nullptr);
        if (dev_id < 0) {
            throw BluetoothException("Bluetooth adapter not found");
        }

        int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
        if (ctl < 0) {
            throw BluetoothException("Cannot open Bluetooth control socket: " +
                                     std::string(strerror(errno)));
        }

        struct hci_dev_req dr;
        dr.dev_id = dev_id;
        dr.dev_opt = enable ? SCAN_DISABLED : SCAN_DISABLED;

        // Enable/disable adapter
        if (ioctl(ctl, enable ? HCIDEVUP : HCIDEVDOWN,
                  static_cast<unsigned long>(dev_id)) < 0) {
            close(ctl);
            if (errno == EALREADY) {
                // Already in requested state
                return;
            }
            throw BluetoothException(
                std::string(enable ? "Enable" : "Disable") +
                " Bluetooth adapter failed: " + strerror(errno));
        }

        close(ctl);
    }

    /**
     * @brief Scans for available Bluetooth devices
     *
     * @param timeout Duration for which to scan for devices
     * @return std::vector<BluetoothDeviceInfo> List of discovered devices
     * @throws BluetoothException if scanning fails
     */
    std::vector<BluetoothDeviceInfo> scanDevices(std::chrono::seconds timeout) {
        std::vector<BluetoothDeviceInfo> devices;

        int dev_id = hci_get_route(nullptr);
        if (dev_id < 0) {
            throw BluetoothException("Bluetooth adapter not found");
        }

        int sock = hci_open_dev(dev_id);
        if (sock < 0) {
            throw BluetoothException("Cannot open Bluetooth device: " +
                                     std::string(strerror(errno)));
        }

        // Set scan parameters
        uint8_t scan_type = 0x01;             // Inquiry with RSSI
        uint8_t lap[3] = {0x33, 0x8b, 0x9e};  // GIAC
        uint8_t length = timeout.count();     // Scan duration (seconds)
        uint8_t num_rsp = 255;                // Maximum response count

        // Start scanning
        int result = hci_inquiry(dev_id, length, num_rsp, lap, nullptr,
                                 IREQ_CACHE_FLUSH);
        if (result < 0) {
            close(sock);
            throw BluetoothException("Bluetooth scan failed: " +
                                     std::string(strerror(errno)));
        }

        // Get scan results
        inquiry_info* info = new inquiry_info[256];
        result =
            hci_inquiry(dev_id, length, num_rsp, lap, &info, IREQ_CACHE_FLUSH);

        if (result > 0) {
            for (int i = 0; i < result; i++) {
                BluetoothDeviceInfo device;

                // Get device address
                char addr[18] = {0};
                ba2str(&info[i].bdaddr, addr);
                device.address = addr;

                // Get device name
                char name[248] = {0};
                if (hci_read_remote_name(sock, &info[i].bdaddr, sizeof(name),
                                         name, 0) >= 0) {
                    device.name = name;
                } else {
                    device.name = "[Unknown]";
                }

                // Get RSSI
                int8_t rssi;
                if (hci_read_rssi(
                        sock,
                        htobs(info[i].bdaddr.b[0] | (info[i].bdaddr.b[1] << 8) |
                              (info[i].bdaddr.b[2] << 16) |
                              (info[i].bdaddr.b[3] << 24)),
                        &rssi, 1000) >= 0) {
                    device.rssi = rssi;
                }

                // Check if paired
                device.paired = false;  // Needs separate query
                device.connected = false;

                devices.push_back(device);
            }
        }

        delete[] info;
        close(sock);

        // Get paired device information
        auto pairedDevices = getPairedDevices();
        for (auto& device : devices) {
            for (const auto& paired : pairedDevices) {
                if (device.address == paired.address) {
                    device.paired = true;
                    break;
                }
            }
        }

        return devices;
    }

    /**
     * @brief Asynchronously scans for Bluetooth devices
     *
     * @param onDeviceFound Callback invoked when a device is discovered
     * @param onScanComplete Callback invoked when the scan completes
     * @param timeout Maximum duration to scan for devices
     */
    void scanDevicesAsync(
        std::function<void(const BluetoothDeviceInfo&)> onDeviceFound,
        std::function<void()> onScanComplete, std::chrono::seconds timeout) {
        // Ensure previous scan has stopped
        stopScan_ = true;
        if (scanThread_.joinable()) {
            scanThread_.join();
        }

        stopScan_ = false;
        scanThread_ = std::thread([this, onDeviceFound, onScanComplete,
                                   timeout]() {
            try {
                int dev_id = hci_get_route(nullptr);
                if (dev_id < 0) {
                    throw BluetoothException("Bluetooth adapter not found");
                }

                int sock = hci_open_dev(dev_id);
                if (sock < 0) {
                    throw BluetoothException("Cannot open Bluetooth device: " +
                                             std::string(strerror(errno)));
                }

                // Set scan parameters
                uint8_t scan_type = 0x01;             // Inquiry with RSSI
                uint8_t lap[3] = {0x33, 0x8b, 0x9e};  // GIAC
                uint8_t length = 8;     // Scan for 8 seconds each time
                uint8_t num_rsp = 255;  // Maximum response count

                // Store reported devices to avoid duplication
                std::unordered_set<std::string> reportedDevices;

                auto startTime = std::chrono::steady_clock::now();

                while (!stopScan_ && (std::chrono::steady_clock::now() -
                                      startTime) < timeout) {
                    // Start scanning
                    inquiry_info* info = new inquiry_info[256];
                    int result = hci_inquiry(dev_id, length, num_rsp, lap,
                                             &info, IREQ_CACHE_FLUSH);

                    if (result > 0) {
                        for (int i = 0; i < result; i++) {
                            if (stopScan_)
                                break;

                            // Get device address
                            char addr[18] = {0};
                            ba2str(&info[i].bdaddr, addr);
                            std::string address = addr;

                            // Check if this device has already been reported
                            if (reportedDevices.find(address) !=
                                reportedDevices.end()) {
                                continue;
                            }

                            BluetoothDeviceInfo device;
                            device.address = address;

                            // Get device name
                            char name[248] = {0};
                            if (hci_read_remote_name(sock, &info[i].bdaddr,
                                                     sizeof(name), name,
                                                     0) >= 0) {
                                device.name = name;
                            } else {
                                device.name = "[Unknown]";
                            }

                            // Get RSSI
                            int8_t rssi;
                            if (hci_read_rssi(
                                    sock,
                                    htobs(info[i].bdaddr.b[0] |
                                          (info[i].bdaddr.b[1] << 8) |
                                          (info[i].bdaddr.b[2] << 16) |
                                          (info[i].bdaddr.b[3] << 24)),
                                    &rssi, 1000) >= 0) {
                                device.rssi = rssi;
                            }

                            // Add to reported devices
                            reportedDevices.insert(address);

                            // Notify callback
                            onDeviceFound(device);
                        }
                    }

                    delete[] info;

                    // Avoid continuous scanning to save resources
                    if (!stopScan_) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(500));
                    }
                }

                close(sock);

                if (!stopScan_) {
                    onScanComplete();
                }
            } catch (const std::exception& e) {
                // Handle exception
                std::cerr << "Bluetooth scan exception: " << e.what()
                          << std::endl;
                if (!stopScan_) {
                    onScanComplete();  // Notify scan complete (with error)
                }
            }
        });
    }

    /**
     * @brief Stops an ongoing asynchronous device scan
     */
    void stopScan() {
        stopScan_ = true;
        if (scanThread_.joinable()) {
            scanThread_.join();
        }
    }

    /**
     * @brief Connects to a Bluetooth device
     *
     * @param address MAC address of the device to connect to
     * @param config Configuration for the connection
     * @throws BluetoothException if connection fails
     */
    void connect(const std::string& address, const BluetoothConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (isConnected()) {
            disconnect();
        }

        config_ = config;

        // Create socket
        socket_ = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
        if (socket_ < 0) {
            throw BluetoothException("Cannot create Bluetooth socket: " +
                                     std::string(strerror(errno)));
        }

        // Set local address
        struct sockaddr_rc local_addr = {0};
        local_addr.rc_family = AF_BLUETOOTH;
        local_addr.rc_bdaddr = {{0, 0, 0, 0, 0, 0}};  // Use any local address
        local_addr.rc_channel = (uint8_t)1;           // Use first channel

        if (bind(socket_, (struct sockaddr*)&local_addr, sizeof(local_addr)) <
            0) {
            close(socket_);
            socket_ = -1;
            throw BluetoothException(
                "Binding local Bluetooth address failed: " +
                std::string(strerror(errno)));
        }

        // Set timeout
        struct timeval tv;
        tv.tv_sec = config.connectTimeout.count() / 1000;
        tv.tv_usec = (config.connectTimeout.count() % 1000) * 1000;

        if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            close(socket_);
            socket_ = -1;
            throw BluetoothException("Setting receive timeout failed: " +
                                     std::string(strerror(errno)));
        }

        if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            close(socket_);
            socket_ = -1;
            throw BluetoothException("Setting send timeout failed: " +
                                     std::string(strerror(errno)));
        }

        // Parse address
        struct sockaddr_rc remote_addr = {0};
        remote_addr.rc_family = AF_BLUETOOTH;
        remote_addr.rc_channel = (uint8_t)1;  // Most SPP devices use channel 1
        str2ba(address.c_str(), &remote_addr.rc_bdaddr);

        // Connect to remote device
        if (::connect(socket_, (struct sockaddr*)&remote_addr,
                      sizeof(remote_addr)) < 0) {
            close(socket_);
            socket_ = -1;
            throw BluetoothException("Connection to Bluetooth device failed: " +
                                     std::string(strerror(errno)));
        }

        // Set non-blocking mode
        int flags = fcntl(socket_, F_GETFL, 0);
        if (flags < 0) {
            close(socket_);
            socket_ = -1;
            throw BluetoothException("Failed to get socket flags: " +
                                     std::string(strerror(errno)));
        }

        if (fcntl(socket_, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(socket_);
            socket_ = -1;
            throw BluetoothException("Setting non-blocking mode failed: " +
                                     std::string(strerror(errno)));
        }

        // Save connected device information
        connectedDevice_ = BluetoothDeviceInfo{};
        connectedDevice_->address = address;
        connectedDevice_->connected = true;

        // Try to get device name
        int hci_sock = hci_open_dev(hci_get_route(nullptr));
        if (hci_sock >= 0) {
            bdaddr_t bdaddr;
            str2ba(address.c_str(), &bdaddr);

            char name[248] = {0};
            if (hci_read_remote_name(hci_sock, &bdaddr, sizeof(name), name,
                                     0) >= 0) {
                connectedDevice_->name = name;
            } else {
                connectedDevice_->name = "[Unknown Device]";
            }

            close(hci_sock);
        }

        // Reset statistics
        stats_.bytesSent = 0;
        stats_.bytesReceived = 0;
        stats_.connectionTime = std::chrono::steady_clock::now();

        // Notify connection success if listener is set
        if (connectionListener_) {
            connectionListener_(true);
        }
    }

    /**
     * @brief Disconnects from the currently connected device
     */
    void disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (socket_ >= 0) {
            close(socket_);
            socket_ = -1;

            // Notify disconnection if listener is set
            if (connectionListener_) {
                connectionListener_(false);
            }

            connectedDevice_.reset();
        }
    }

    /**
     * @brief Checks if currently connected to a device
     *
     * @return true if connected, false otherwise
     */
    bool isConnected() const { return socket_ >= 0; }

    /**
     * @brief Gets information about the currently connected device
     *
     * @return std::optional<BluetoothDeviceInfo> Device information if
     * connected
     */
    std::optional<BluetoothDeviceInfo> getConnectedDevice() const {
        return connectedDevice_;
    }

    /**
     * @brief Pairs with a Bluetooth device
     *
     * @param address MAC address of the device to pair with
     * @param pin PIN code for pairing (if required)
     * @return true if pairing succeeded, false otherwise
     */
    bool pair(const std::string& address, const std::string& pin) {
        // On Linux, pairing is typically done via bluetoothctl or D-Bus API
        // Here we use exec to call bluetoothctl command

        // Build pairing command
        std::string cmd = "echo -e 'agent on\\npairtrust " + address +
                          "\\nquit' | bluetoothctl";

        int result = system(cmd.c_str());
        return (result == 0);
    }

    /**
     * @brief Unpairs from a Bluetooth device
     *
     * @param address MAC address of the device to unpair from
     * @return true if unpairing succeeded, false otherwise
     */
    bool unpair(const std::string& address) {
        // Use bluetoothctl command to unpair
        std::string cmd =
            "echo -e 'remove " + address + "\\nquit' | bluetoothctl";

        int result = system(cmd.c_str());
        return (result == 0);
    }

    /**
     * @brief Gets a list of all paired Bluetooth devices
     *
     * @return std::vector<BluetoothDeviceInfo> List of paired devices
     */
    std::vector<BluetoothDeviceInfo> getPairedDevices() {
        std::vector<BluetoothDeviceInfo> pairedDevices;

        // Use bluetoothctl to get paired devices
        FILE* pipe = popen("bluetoothctl paired-devices", "r");
        if (!pipe) {
            return pairedDevices;
        }

        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // Parse output, format: "Device XX:XX:XX:XX:XX:XX DeviceName"
            std::string line(buffer);

            size_t addrStartPos = line.find("Device ") + 7;
            if (addrStartPos != std::string::npos) {
                size_t addrEndPos = line.find(" ", addrStartPos);
                if (addrEndPos != std::string::npos) {
                    std::string address =
                        line.substr(addrStartPos, addrEndPos - addrStartPos);
                    std::string name = line.substr(addrEndPos + 1);

                    // Remove trailing newline
                    if (!name.empty() && name.back() == '\n') {
                        name.pop_back();
                    }

                    BluetoothDeviceInfo info;
                    info.address = address;
                    info.name = name;
                    info.paired = true;

                    pairedDevices.push_back(info);
                }
            }
        }

        pclose(pipe);
        return pairedDevices;
    }

    /**
     * @brief Reads data from the connected device
     *
     * @param maxBytes Maximum number of bytes to read
     * @return std::vector<uint8_t> Data read from the device
     * @throws SerialPortNotOpenException if not connected
     * @throws SerialIOException if reading fails
     */
    std::vector<uint8_t> read(size_t maxBytes) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        if (maxBytes == 0) {
            return {};
        }

        // Use poll to check if data is available for reading
        struct pollfd pfd;
        pfd.fd = socket_;
        pfd.events = POLLIN;

        int pollResult =
            poll(&pfd, 1, config_.serialConfig.readTimeout.count());

        if (pollResult < 0) {
            throw SerialIOException("Read error: " +
                                    std::string(strerror(errno)));
        } else if (pollResult == 0) {
            // Timeout, no data available to read
            return {};
        }

        std::vector<uint8_t> buffer(maxBytes);
        ssize_t bytesRead = ::read(socket_, buffer.data(), maxBytes);

        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available to read in non-blocking mode
                return {};
            }
            throw SerialIOException("Read error: " +
                                    std::string(strerror(errno)));
        } else if (bytesRead == 0) {
            // Connection closed
            disconnect();
            throw SerialPortNotOpenException();
        }

        buffer.resize(bytesRead);
        stats_.bytesReceived += bytesRead;
        return buffer;
    }

    /**
     * @brief Reads exactly the specified number of bytes
     *
     * @param bytes Number of bytes to read
     * @param timeout Maximum time to wait for the data
     * @return std::vector<uint8_t> Exactly the requested number of bytes
     * @throws SerialPortNotOpenException if not connected
     * @throws SerialTimeoutException if timeout occurs before reading all data
     */
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

            // Save original timeout setting
            auto originalTimeout = config_.serialConfig.readTimeout;

            try {
                // Temporarily set timeout
                config_.serialConfig.readTimeout = remainingTimeout;

                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }

                // Restore original timeout setting
                config_.serialConfig.readTimeout = originalTimeout;
            } catch (...) {
                // Ensure original timeout is restored
                config_.serialConfig.readTimeout = originalTimeout;
                throw;
            }

            // Short sleep to avoid high CPU usage if no data available
            if (result.size() < bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        return result;
    }

    /**
     * @brief Sets up asynchronous reading from the device
     *
     * @param maxBytes Maximum number of bytes to read in each operation
     * @param callback Function to call with the read data
     * @throws SerialPortNotOpenException if not connected
     */
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
                    // Timeout is expected, continue
                } catch (const std::exception& e) {
                    if (!stopAsyncRead_) {
                        // Notify disconnection on error
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

    /**
     * @brief Reads all available data from the device
     *
     * @return std::vector<uint8_t> All data currently available
     * @throws SerialPortNotOpenException if not connected
     * @throws SerialIOException if reading fails
     */
    std::vector<uint8_t> readAvailable() {
        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        int bytesAvailable = 0;
        if (ioctl(socket_, FIONREAD, &bytesAvailable) < 0) {
            throw SerialIOException("Cannot get available bytes: " +
                                    std::string(strerror(errno)));
        }

        if (bytesAvailable == 0) {
            return {};
        }

        return read(bytesAvailable);
    }

    /**
     * @brief Writes data to the connected device
     *
     * @param data The data to write
     * @return size_t Number of bytes successfully written
     * @throws SerialPortNotOpenException if not connected
     * @throws SerialIOException if writing fails
     * @throws SerialTimeoutException if write times out
     */
    size_t write(std::span<const uint8_t> data) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        if (data.empty()) {
            return 0;
        }

        // Use poll to check if socket is writable
        struct pollfd pfd;
        pfd.fd = socket_;
        pfd.events = POLLOUT;

        int pollResult =
            poll(&pfd, 1, config_.serialConfig.writeTimeout.count());

        if (pollResult < 0) {
            throw SerialIOException("Write error: " +
                                    std::string(strerror(errno)));
        } else if (pollResult == 0) {
            throw SerialTimeoutException();
        }

        ssize_t bytesSent = ::write(socket_, data.data(), data.size());

        if (bytesSent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Buffer full in non-blocking mode
                return 0;
            }
            throw SerialIOException("Write error: " +
                                    std::string(strerror(errno)));
        }

        stats_.bytesSent += bytesSent;
        return static_cast<size_t>(bytesSent);
    }

    /**
     * @brief Flushes the communication buffers
     */
    void flush() {
        // For Bluetooth sockets, there's no direct flush operation
        // Instead, we can read all available data to achieve similar effect
        try {
            readAvailable();
        } catch (...) {
            // Ignore errors
        }
    }

    /**
     * @brief Gets the number of bytes available to read
     *
     * @return size_t Number of bytes available
     * @throws SerialPortNotOpenException if not connected
     * @throws SerialIOException if the operation fails
     */
    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isConnected()) {
            throw SerialPortNotOpenException();
        }

        int bytesAvailable = 0;
        if (ioctl(socket_, FIONREAD, &bytesAvailable) < 0) {
            throw SerialIOException("Cannot get available bytes: " +
                                    std::string(strerror(errno)));
        }

        return static_cast<size_t>(bytesAvailable);
    }

    /**
     * @brief Sets a callback function to be notified of connection status
     * changes
     *
     * @param listener Function to call when connection status changes
     */
    void setConnectionListener(std::function<void(bool connected)> listener) {
        connectionListener_ = std::move(listener);
    }

    /**
     * @brief Gets communication statistics
     *
     * @return BluetoothSerial::Statistics Current statistics
     */
    BluetoothSerial::Statistics getStatistics() const { return stats_; }

private:
    int socket_;
    BluetoothConfig config_;
    std::optional<BluetoothDeviceInfo> connectedDevice_;
    std::function<void(bool)> connectionListener_;
    mutable std::mutex mutex_;
    std::thread asyncReadThread_;
    std::atomic<bool> stopAsyncRead_;
    std::thread scanThread_;
    std::atomic<bool> stopScan_;
    BluetoothSerial::Statistics stats_;

    /**
     * @brief Stops the asynchronous read worker thread
     */
    void stopAsyncWorker() {
        if (asyncReadThread_.joinable()) {
            stopAsyncRead_ = true;
            asyncReadThread_.join();
        }
    }
};

}  // namespace serial

#endif  // defined(__linux__)