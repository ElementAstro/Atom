#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace serial {

/**
 * @brief Base exception class for all serial port related exceptions
 *
 * This class serves as the parent class for the exception hierarchy used
 * by the serial port implementation.
 */
class SerialException : public std::runtime_error {
public:
    /**
     * @brief Constructs a new Serial Exception
     *
     * @param message The error message describing the exception
     */
    explicit SerialException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when attempting to operate on a closed port
 *
 * This exception is thrown when operations that require an open port
 * are attempted while the port is closed.
 */
class SerialPortNotOpenException : public SerialException {
public:
    /**
     * @brief Constructs a new Serial Port Not Open Exception
     */
    explicit SerialPortNotOpenException() : SerialException("Port not open") {}
};

/**
 * @brief Exception thrown when a serial operation times out
 *
 * This exception indicates that a read or write operation did not
 * complete within the configured timeout period.
 */
class SerialTimeoutException : public SerialException {
public:
    /**
     * @brief Constructs a new Serial Timeout Exception
     */
    explicit SerialTimeoutException()
        : SerialException("Serial operation timed out") {}
};

/**
 * @brief Exception thrown for general I/O errors during serial operations
 *
 * This exception indicates failures in the underlying I/O operations,
 * such as system errors, hardware failures, or corrupted data.
 */
class SerialIOException : public SerialException {
public:
    /**
     * @brief Constructs a new Serial IO Exception
     *
     * @param message The specific I/O error message
     */
    explicit SerialIOException(const std::string& message)
        : SerialException(message) {}
};

/**
 * @brief Configuration structure for serial port parameters
 *
 * This structure encapsulates all configurable parameters for a serial port,
 * including communication settings and timeout values.
 */
struct SerialConfig {
    /**
     * @brief Parity bit configuration options
     */
    enum class Parity { None, Odd, Even, Mark, Space };

    /**
     * @brief Stop bits configuration options
     */
    enum class StopBits { One, OnePointFive, Two };

    /**
     * @brief Flow control method configuration
     */
    enum class FlowControl { None, Software, Hardware };

    int baudRate = 9600;           ///< Communication speed in bits per second
    int dataBits = 8;              ///< Number of data bits in each byte (5-8)
    Parity parity = Parity::None;  ///< Parity checking configuration
    StopBits stopBits = StopBits::One;            ///< Number of stop bits
    FlowControl flowControl = FlowControl::None;  ///< Flow control method

    std::chrono::milliseconds readTimeout{
        1000};  ///< Timeout for read operations
    std::chrono::milliseconds writeTimeout{
        1000};  ///< Timeout for write operations
};

// Forward declaration for platform-specific implementation
class SerialPortImpl;

/**
 * @brief Main serial port interface class
 *
 * This class provides a platform-independent interface for serial port
 * communication. It supports reading and writing data, configuring port
 * parameters, and managing the port state.
 */
class SerialPort {
public:
    /**
     * @brief Default constructor
     *
     * Creates a new serial port instance without opening any port.
     */
    SerialPort();

    /**
     * @brief Destructor
     *
     * Closes any open port and releases all resources.
     */
    ~SerialPort();

    // Disable copy operations
    /**
     * @brief Copy constructor (deleted)
     *
     * Serial ports cannot be copied.
     */
    SerialPort(const SerialPort&) = delete;

    /**
     * @brief Copy assignment operator (deleted)
     *
     * Serial ports cannot be copied.
     *
     * @return SerialPort& Reference to the assigned object
     */
    SerialPort& operator=(const SerialPort&) = delete;

    // Enable move operations
    /**
     * @brief Move constructor
     *
     * Transfers ownership of the serial port from another instance.
     *
     * @param other The serial port instance to move from
     */
    SerialPort(SerialPort&& other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * Transfers ownership of the serial port from another instance.
     *
     * @param other The serial port instance to move from
     * @return SerialPort& Reference to the assigned object
     */
    SerialPort& operator=(SerialPort&& other) noexcept;

    /**
     * @brief Opens the specified serial port
     *
     * Attempts to open the serial port with the given name and configuration.
     * If the port is already open, it will be closed first.
     *
     * @param portName The name of the port to open (e.g., "COM1" on Windows,
     * "/dev/ttyUSB0" on Linux)
     * @param config Configuration parameters for the port
     * @throws SerialIOException If the port cannot be opened or configured
     */
    void open(const std::string& portName, const SerialConfig& config = {});

    /**
     * @brief Closes the currently open port
     *
     * If no port is open, this method does nothing.
     */
    void close();

    /**
     * @brief Checks if the serial port is currently open
     *
     * @return true If the port is open
     * @return false If the port is closed
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * @brief Reads up to maxBytes bytes from the serial port
     *
     * This method will read available data up to the specified maximum,
     * but may return fewer bytes if not enough data is available before
     * the read timeout expires.
     *
     * @param maxBytes Maximum number of bytes to read
     * @return std::vector<uint8_t> The data read from the port
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If the read operation times out
     * @throws SerialIOException If an I/O error occurs
     */
    std::vector<uint8_t> read(size_t maxBytes);

    /**
     * @brief Reads exactly the specified number of bytes
     *
     * This method blocks until either all requested bytes are read or
     * the specified timeout expires.
     *
     * @param bytes Number of bytes to read
     * @param timeout Maximum time to wait for the requested data
     * @return std::vector<uint8_t> The data read from the port
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If the read operation times out
     * @throws SerialIOException If an I/O error occurs
     */
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout);

    /**
     * @brief Performs an asynchronous read operation
     *
     * Initiates a read operation that will call the provided callback function
     * when data is available or an error occurs.
     *
     * @param maxBytes Maximum number of bytes to read
     * @param callback Function to call with the read data
     * @throws SerialPortNotOpenException If the port is not open
     */
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback);

    /**
     * @brief Reads all currently available data from the serial port
     *
     * This method reads whatever data is currently available in the input
     * buffer without waiting for more data to arrive.
     *
     * @return std::vector<uint8_t> The data read from the port
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If an I/O error occurs
     */
    std::vector<uint8_t> readAvailable();

    /**
     * @brief Writes data to the serial port
     *
     * Attempts to write all provided bytes to the serial port.
     *
     * @param data Span of bytes to write
     * @return size_t The number of bytes actually written
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If the write operation times out
     * @throws SerialIOException If an I/O error occurs
     */
    size_t write(std::span<const uint8_t> data);

    /**
     * @brief Writes a string to the serial port
     *
     * Convenience method for writing string data to the port.
     *
     * @param data String to write
     * @return size_t The number of bytes actually written
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If the write operation times out
     * @throws SerialIOException If an I/O error occurs
     */
    size_t write(const std::string& data);

    /**
     * @brief Flushes both input and output buffers
     *
     * Discards any data in the input and output buffers.
     *
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If an I/O error occurs
     */
    void flush();

    /**
     * @brief Waits for all output data to be transmitted
     *
     * Blocks until all data in the output buffer has been sent.
     *
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If an I/O error occurs
     */
    void drain();

    /**
     * @brief Gets the number of bytes available to read
     *
     * @return size_t Number of bytes available in the input buffer
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If an I/O error occurs
     */
    [[nodiscard]] size_t available() const;

    /**
     * @brief Updates the port configuration
     *
     * Changes the configuration of an already open port.
     *
     * @param config New configuration parameters
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the configuration cannot be applied
     */
    void setConfig(const SerialConfig& config);

    /**
     * @brief Gets the current port configuration
     *
     * @return SerialConfig Current configuration of the port
     * @throws SerialPortNotOpenException If the port is not open
     */
    [[nodiscard]] SerialConfig getConfig() const;

    /**
     * @brief Sets the DTR (Data Terminal Ready) signal
     *
     * @param value True to assert the signal, false to clear it
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the signal cannot be set
     */
    void setDTR(bool value);

    /**
     * @brief Sets the RTS (Request To Send) signal
     *
     * @param value True to assert the signal, false to clear it
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the signal cannot be set
     */
    void setRTS(bool value);

    /**
     * @brief Gets the state of the CTS (Clear To Send) signal
     *
     * @return true If the signal is asserted
     * @return false If the signal is not asserted
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the signal state cannot be read
     */
    [[nodiscard]] bool getCTS() const;

    /**
     * @brief Gets the state of the DSR (Data Set Ready) signal
     *
     * @return true If the signal is asserted
     * @return false If the signal is not asserted
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the signal state cannot be read
     */
    [[nodiscard]] bool getDSR() const;

    /**
     * @brief Gets the state of the RI (Ring Indicator) signal
     *
     * @return true If the signal is asserted
     * @return false If the signal is not asserted
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the signal state cannot be read
     */
    [[nodiscard]] bool getRI() const;

    /**
     * @brief Gets the state of the CD (Carrier Detect) signal
     *
     * @return true If the signal is asserted
     * @return false If the signal is not asserted
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the signal state cannot be read
     */
    [[nodiscard]] bool getCD() const;

    /**
     * @brief Gets the name of the current port
     *
     * @return std::string The name of the open port, or empty string if no port
     * is open
     */
    [[nodiscard]] std::string getPortName() const;

    /**
     * @brief Lists all available serial ports on the system
     *
     * @return std::vector<std::string> Names of all available serial ports
     */
    static std::vector<std::string> getAvailablePorts();

private:
    std::unique_ptr<SerialPortImpl>
        impl_;  ///< PIMPL idiom for platform-specific implementation
};

}  // namespace serial