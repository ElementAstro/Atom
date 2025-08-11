#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "serial_port.hpp"

namespace serial {

/**
 * @brief Structure containing information about a Bluetooth device.
 */
struct BluetoothDeviceInfo {
    /** @brief MAC address or UUID of the Bluetooth device. */
    std::string address;
    /** @brief Name of the Bluetooth device. */
    std::string name;
    /** @brief Received Signal Strength Indication (RSSI) in dBm. */
    int rssi{0};
    /** @brief Indicates whether the device is paired. */
    bool paired{false};
    /** @brief Indicates whether the device is currently connected. */
    bool connected{false};

    /** @brief Optional list of services offered by the device. */
    std::vector<std::string> services;
};

/**
 * @brief Exception class for Bluetooth-related errors.
 *
 * This class inherits from SerialException and is used to throw exceptions
 * specific to Bluetooth operations.
 */
class BluetoothException : public SerialException {
public:
    /**
     * @brief Constructor for the BluetoothException class.
     *
     * @param message A descriptive error message.
     */
    explicit BluetoothException(const std::string& message)
        : SerialException(message) {}
};

/**
 * @brief Structure containing configuration options for Bluetooth serial
 * communication.
 */
struct BluetoothConfig {
    /** @brief Duration for scanning Bluetooth devices. Default is 5 seconds. */
    std::chrono::seconds scanDuration{5};
    /** @brief Flag indicating whether to automatically reconnect to the device
     * if the connection is lost. */
    bool autoReconnect{false};
    /** @brief Interval between reconnection attempts. Default is 5 seconds. */
    std::chrono::seconds reconnectInterval{5};
    /** @brief PIN code used for pairing with the Bluetooth device. Default is
     * "1234". */
    std::string pin{"1234"};
    /** @brief Timeout for establishing a connection with the Bluetooth device.
     * Default is 5000 milliseconds. */
    std::chrono::milliseconds connectTimeout{5000};
    /** @brief Basic serial port configuration for the Bluetooth connection. */
    SerialConfig serialConfig{};
};

/**
 * @brief Forward declaration of the platform-specific implementation class.
 *
 * This class is used to hide the platform-specific details of the
 * BluetoothSerial class.
 */
class BluetoothSerialImpl;

/**
 * @brief Class for managing Bluetooth serial communication.
 *
 * This class provides an interface for scanning, connecting, and communicating
 * with Bluetooth devices using serial communication. It uses the PIMPL pattern
 * to hide platform-specific implementation details.
 */
class BluetoothSerial {
public:
    /**
     * @brief Default constructor for the BluetoothSerial class.
     *
     * Initializes the BluetoothSerial object.
     */
    BluetoothSerial();

    /**
     * @brief Destructor for the BluetoothSerial class.
     *
     * Releases any resources used by the BluetoothSerial object.
     */
    ~BluetoothSerial();

    // Disallow copy construction
    BluetoothSerial(const BluetoothSerial&) = delete;

    // Disallow copy assignment
    BluetoothSerial& operator=(const BluetoothSerial&) = delete;

    // Allow move construction
    BluetoothSerial(BluetoothSerial&& other) noexcept;

    // Allow move assignment
    BluetoothSerial& operator=(BluetoothSerial&& other) noexcept;

    /**
     * @brief Checks if the Bluetooth adapter is enabled.
     *
     * @return True if the Bluetooth adapter is enabled, false otherwise.
     */
    [[nodiscard]] bool isBluetoothEnabled() const;

    /**
     * @brief Enables or disables the Bluetooth adapter.
     *
     * @param enable True to enable the Bluetooth adapter, false to disable it.
     *               Requires appropriate permissions.
     */
    void enableBluetooth(bool enable = true);

    /**
     * @brief Scans for available Bluetooth devices.
     *
     * This method performs a synchronous scan for Bluetooth devices and returns
     * a vector of BluetoothDeviceInfo objects containing information about the
     * discovered devices.
     *
     * @param timeout The duration of the scan. Default is 5 seconds.
     * @return A vector of BluetoothDeviceInfo objects representing the
     * discovered devices.
     */
    std::vector<BluetoothDeviceInfo> scanDevices(
        std::chrono::seconds timeout = std::chrono::seconds(5));

    /**
     * @brief Scans for available Bluetooth devices asynchronously.
     *
     * This method performs an asynchronous scan for Bluetooth devices and calls
     * the provided callbacks when a device is found and when the scan is
     * complete.
     *
     * @param onDeviceFound A callback function that is called when a Bluetooth
     *                      device is found. The function should accept a
     *                      BluetoothDeviceInfo object as input.
     * @param onScanComplete A callback function that is called when the scan is
     *                       complete.
     * @param timeout The duration of the scan. Default is 5 seconds.
     */
    void scanDevicesAsync(
        std::function<void(const BluetoothDeviceInfo&)> onDeviceFound,
        std::function<void()> onScanComplete,
        std::chrono::seconds timeout = std::chrono::seconds(5));

    /**
     * @brief Stops an ongoing asynchronous scan for Bluetooth devices.
     */
    void stopScan();

    /**
     * @brief Connects to a Bluetooth device.
     *
     * @param address The MAC address or UUID of the Bluetooth device to connect
     * to.
     * @param config  The Bluetooth configuration to use for the connection.
     */
    void connect(const std::string& address,
                 const BluetoothConfig& config = {});

    /**
     * @brief Disconnects from the currently connected Bluetooth device.
     */
    void disconnect();

    /**
     * @brief Checks if a Bluetooth device is currently connected.
     *
     * @return True if a Bluetooth device is connected, false otherwise.
     */
    [[nodiscard]] bool isConnected() const;

    /**
     * @brief Gets information about the currently connected Bluetooth device.
     *
     * @return An optional BluetoothDeviceInfo object containing information
     *         about the connected device, or std::nullopt if no device is
     *         connected.
     */
    [[nodiscard]] std::optional<BluetoothDeviceInfo> getConnectedDevice() const;

    /**
     * @brief Pairs with a Bluetooth device.
     *
     * @param address The MAC address or UUID of the Bluetooth device to pair
     * with.
     * @param pin     The PIN code to use for pairing. Default is "1234".
     * @return True if pairing was successful, false otherwise.
     */
    bool pair(const std::string& address, const std::string& pin = "1234");

    /**
     * @brief Unpairs with a Bluetooth device.
     *
     * @param address The MAC address or UUID of the Bluetooth device to unpair
     * from.
     * @return True if unpairing was successful, false otherwise.
     */
    bool unpair(const std::string& address);

    /**
     * @brief Gets a list of paired Bluetooth devices.
     *
     * @return A vector of BluetoothDeviceInfo objects representing the paired
     * devices.
     */
    std::vector<BluetoothDeviceInfo> getPairedDevices();

    /**
     * @brief Reads data from the Bluetooth serial port.
     *
     * @param maxBytes The maximum number of bytes to read.
     * @return A vector of bytes read from the serial port.
     */
    std::vector<uint8_t> read(size_t maxBytes);

    /**
     * @brief Reads exactly the specified number of bytes from the Bluetooth
     * serial port.
     *
     * @param bytes   The number of bytes to read.
     * @param timeout The maximum time to wait for the data.
     * @return A vector of bytes read from the serial port.
     */
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout);

    /**
     * @brief Reads data from the Bluetooth serial port asynchronously.
     *
     * @param maxBytes The maximum number of bytes to read in each operation.
     * @param callback A callback function that is called when data is received.
     */
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback);

    /**
     * @brief Reads all available bytes from the Bluetooth serial port.
     *
     * @return A vector of bytes read from the serial port.
     */
    std::vector<uint8_t> readAvailable();

    /**
     * @brief Writes data to the Bluetooth serial port.
     *
     * @param data A span of constant bytes to write to the serial port.
     * @return The number of bytes written to the serial port.
     */
    size_t write(std::span<const uint8_t> data);

    /**
     * @brief Writes a string to the Bluetooth serial port.
     *
     * @param data The string to write to the serial port.
     * @return The number of bytes written to the serial port.
     */
    size_t write(const std::string& data);

    /**
     * @brief Flushes the Bluetooth serial port.
     */
    void flush();

    /**
     * @brief Gets the number of bytes available to read from the Bluetooth
     * serial port.
     *
     * @return The number of bytes available to read.
     */
    [[nodiscard]] size_t available() const;

    /**
     * @brief Sets a connection listener to be notified of connection events.
     *
     * @param listener A callback function that is called when the connection
     *                 status changes. The function should accept a boolean
     *                 value indicating whether the device is connected.
     */
    void setConnectionListener(std::function<void(bool connected)> listener);

    /**
     * @brief Structure containing statistics about the Bluetooth serial
     * communication.
     */
    struct Statistics {
        /** @brief The number of bytes sent. */
        size_t bytesSent{0};
        /** @brief The number of bytes received. */
        size_t bytesReceived{0};
        /** @brief The time the connection was established. */
        std::chrono::steady_clock::time_point connectionTime;
        /** @brief The current RSSI value. */
        int currentRssi{0};
    };

    /**
     * @brief Gets statistics about the Bluetooth serial communication.
     *
     * @return A Statistics object containing information about the
     * communication.
     */
    [[nodiscard]] Statistics getStatistics() const;

private:
    /** @brief Platform-specific implementation of the BluetoothSerial class. */
    std::unique_ptr<BluetoothSerialImpl>
        impl_;  // PIMPL pattern for platform implementation
};

}  // namespace serial
