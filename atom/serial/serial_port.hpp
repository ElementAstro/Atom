#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace serial {

/**
 * @brief Base class for all serial port related exceptions.
 *
 * This class serves as the parent for the exception hierarchy used in the
 * serial port implementation.
 */
class SerialException : public std::runtime_error {
public:
    /**
     * @brief Constructs a new serial exception.
     *
     * @param message The error message describing the exception.
     */
    explicit SerialException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when attempting an operation on a closed port.
 *
 * This exception is thrown when an operation requiring an open port is
 * attempted while the port is closed.
 */
class SerialPortNotOpenException : public SerialException {
public:
    /**
     * @brief Constructs a new serial port not open exception.
     */
    explicit SerialPortNotOpenException()
        : SerialException("Port is not open") {}
};

/**
 * @brief Exception thrown when a serial operation times out.
 *
 * This exception indicates that a read or write operation did not complete
 * within the configured timeout period.
 */
class SerialTimeoutException : public SerialException {
public:
    /**
     * @brief Constructs a new serial timeout exception.
     */
    explicit SerialTimeoutException()
        : SerialException("Serial operation timed out") {}

    /**
     * @brief Constructs a new serial timeout exception with a detailed message.
     *
     * @param message Additional details about the timeout.
     */
    explicit SerialTimeoutException(const std::string& message)
        : SerialException("Serial operation timed out: " + message) {}
};

/**
 * @brief Exception thrown when a general I/O error occurs during a serial
 * operation.
 *
 * This exception indicates a failure in the underlying I/O operations, such as
 * system errors, hardware failures, or corrupted data.
 */
class SerialIOException : public SerialException {
public:
    /**
     * @brief Constructs a new serial I/O exception.
     *
     * @param message The specific I/O error message.
     */
    explicit SerialIOException(const std::string& message)
        : SerialException(message) {}
};

/**
 * @brief Exception thrown when an invalid configuration is provided.
 */
class SerialConfigException : public SerialException {
public:
    /**
     * @brief Constructs a new configuration exception.
     *
     * @param message The error message.
     */
    explicit SerialConfigException(const std::string& message)
        : SerialException("Configuration error: " + message) {}
};

/**
 * @brief Structure for serial port parameter configuration.
 *
 * Designed using the builder pattern, supporting chained configuration methods.
 */
class SerialConfig {
public:
    /**
     * @brief Parity configuration options.
     */
    enum class Parity { None, Odd, Even, Mark, Space };

    /**
     * @brief Stop bits configuration options.
     */
    enum class StopBits { One, OnePointFive, Two };

    /**
     * @brief Flow control method configuration.
     */
    enum class FlowControl { None, Software, Hardware };

    /**
     * @brief Default constructor.
     *
     * Initializes the configuration with the most common default parameters.
     */
    SerialConfig() = default;

    // Fluent interface methods for the builder pattern

    /**
     * @brief Sets the baud rate.
     *
     * @param rate The communication speed in bits per second (bps).
     * @return SerialConfig& Reference to self for chaining.
     * @throws SerialConfigException If the baud rate is invalid.
     */
    SerialConfig& withBaudRate(int rate) {
        if (rate <= 0) {
            throw SerialConfigException("Baud rate must be greater than 0");
        }
        baudRate = rate;
        return *this;
    }

    /**
     * @brief Sets the data bits.
     *
     * @param bits The number of data bits per byte (5-8).
     * @return SerialConfig& Reference to self for chaining.
     * @throws SerialConfigException If the data bits value is invalid.
     */
    SerialConfig& withDataBits(int bits) {
        if (bits < 5 || bits > 8) {
            throw SerialConfigException("Data bits must be between 5 and 8");
        }
        dataBits = bits;
        return *this;
    }

    /**
     * @brief Sets the parity.
     *
     * @param p The parity configuration.
     * @return SerialConfig& Reference to self for chaining.
     */
    SerialConfig& withParity(Parity p) {
        parity = p;
        return *this;
    }

    /**
     * @brief Sets the stop bits.
     *
     * @param sb The stop bits configuration.
     * @return SerialConfig& Reference to self for chaining.
     */
    SerialConfig& withStopBits(StopBits sb) {
        stopBits = sb;
        return *this;
    }

    /**
     * @brief Sets the flow control.
     *
     * @param flow The flow control method.
     * @return SerialConfig& Reference to self for chaining.
     */
    SerialConfig& withFlowControl(FlowControl flow) {
        flowControl = flow;
        return *this;
    }

    /**
     * @brief Sets the read timeout.
     *
     * @param timeout The timeout value in milliseconds.
     * @return SerialConfig& Reference to self for chaining.
     */
    SerialConfig& withReadTimeout(std::chrono::milliseconds timeout) {
        readTimeout = timeout;
        return *this;
    }

    /**
     * @brief Sets the write timeout.
     *
     * @param timeout The timeout value in milliseconds.
     * @return SerialConfig& Reference to self for chaining.
     */
    SerialConfig& withWriteTimeout(std::chrono::milliseconds timeout) {
        writeTimeout = timeout;
        return *this;
    }

    /**
     * @brief Factory method to create a common configuration for a given baud
     * rate.
     *
     * @param baudRate The baud rate value.
     * @return SerialConfig A pre-configured object.
     */
    static SerialConfig standardConfig(int baudRate) {
        return SerialConfig()
            .withBaudRate(baudRate)
            .withDataBits(8)
            .withStopBits(StopBits::One)
            .withParity(Parity::None)
            .withFlowControl(FlowControl::None);
    }

    /**
     * @brief Gets the baud rate.
     * @return The current baud rate setting.
     */
    [[nodiscard]] int getBaudRate() const { return baudRate; }

    /**
     * @brief Gets the data bits.
     * @return The current data bits setting.
     */
    [[nodiscard]] int getDataBits() const { return dataBits; }

    /**
     * @brief Gets the parity setting.
     * @return The current parity configuration.
     */
    [[nodiscard]] Parity getParity() const { return parity; }

    /**
     * @brief Gets the stop bits setting.
     * @return The current stop bits configuration.
     */
    [[nodiscard]] StopBits getStopBits() const { return stopBits; }

    /**
     * @brief Gets the flow control setting.
     * @return The current flow control configuration.
     */
    [[nodiscard]] FlowControl getFlowControl() const { return flowControl; }

    /**
     * @brief Gets the read timeout.
     * @return The current read timeout setting.
     */
    [[nodiscard]] std::chrono::milliseconds getReadTimeout() const {
        return readTimeout;
    }

    /**
     * @brief Gets the write timeout.
     * @return The current write timeout setting.
     */
    [[nodiscard]] std::chrono::milliseconds getWriteTimeout() const {
        return writeTimeout;
    }

    // Data members are private, accessed via accessor methods
private:
    int baudRate = 9600;           ///< Communication speed (bits per second)
    int dataBits = 8;              ///< Data bits per byte (5-8)
    Parity parity = Parity::None;  ///< Parity configuration
    StopBits stopBits = StopBits::One;            ///< Number of stop bits
    FlowControl flowControl = FlowControl::None;  ///< Flow control method

    std::chrono::milliseconds readTimeout{1000};   ///< Read operation timeout
    std::chrono::milliseconds writeTimeout{1000};  ///< Write operation timeout
};

// Forward declaration of the platform-specific implementation
class SerialPortImpl;

/**
 * @brief Concept to check if a type can be serialized into bytes.
 */
template <typename T>
concept Serializable = std::is_trivially_copyable_v<T>;

/**
 * @brief Main interface class for serial communication.
 *
 * Provides a platform-independent interface for serial communication,
 * supporting reading and writing data, configuring port parameters, and
 * managing port state. Follows RAII principles and modern C++ design practices.
 *
 * Usage Example:
 * ```cpp
 * try {
 *     // Create and configure the serial port
 *     serial::SerialPort port;
 *
 *     // Use the fluent configuration API
 *     auto config = serial::SerialConfig()
 *         .withBaudRate(115200)
 *         .withDataBits(8)
 *         .withParity(serial::SerialConfig::Parity::None)
 *         .withStopBits(serial::SerialConfig::StopBits::One)
 *         .withFlowControl(serial::SerialConfig::FlowControl::None)
 *         .withReadTimeout(std::chrono::milliseconds(500));
 *
 *     // Open the port
 *     port.open("/dev/ttyUSB0", config);
 *
 *     // Write data
 *     std::string message = "Hello, Serial!";
 *     port.write(message);
 *
 *     // Read response
 *     auto response = port.readUntil('\n', std::chrono::seconds(1));
 *
 *     // Process response data
 *     std::cout << "Received: " << response << std::endl;
 *
 *     // Port is automatically closed when it goes out of scope
 * } catch (const serial::SerialException& e) {
 *     std::cerr << "Serial error: " << e.what() << std::endl;
 * }
 * ```
 */
class SerialPort {
public:
    /**
     * @brief Default constructor.
     *
     * Creates a new serial port instance without opening any port.
     */
    SerialPort();

    /**
     * @brief Destructor.
     *
     * Closes any open port and releases all resources.
     */
    ~SerialPort();

    // Disable copy operations
    /**
     * @brief Copy constructor (deleted).
     *
     * Serial ports cannot be copied.
     */
    SerialPort(const SerialPort&) = delete;

    /**
     * @brief Copy assignment operator (deleted).
     *
     * Serial ports cannot be copied.
     *
     * @return SerialPort& Reference to the assigned object.
     */
    SerialPort& operator=(const SerialPort&) = delete;

    // Enable move operations
    /**
     * @brief Move constructor.
     *
     * Transfers ownership of the serial port from another instance.
     *
     * @param other The serial port instance to move from.
     */
    SerialPort(SerialPort&& other) noexcept;

    /**
     * @brief Move assignment operator.
     *
     * Transfers ownership of the serial port from another instance.
     *
     * @param other The serial port instance to move from.
     * @return SerialPort& Reference to the assigned object.
     */
    SerialPort& operator=(SerialPort&& other) noexcept;

    /**
     * @brief Opens the specified serial port.
     *
     * Attempts to open the serial port with the given name and configuration.
     * If the port is already open, it will be closed first.
     *
     * @param portName The name of the port to open (e.g., "COM1" on Windows,
     * "/dev/ttyUSB0" on Linux).
     * @param config The configuration parameters for the port.
     * @throws SerialIOException If the port cannot be opened or configured.
     */
    void open(std::string_view portName, const SerialConfig& config = {});

    /**
     * @brief Closes the currently open port.
     *
     * If no port is open, this method does nothing.
     */
    void close();

    /**
     * @brief Checks if the serial port is currently open.
     *
     * @return true If the port is open.
     * @return false If the port is closed.
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * @brief Reads up to maxBytes bytes from the serial port.
     *
     * This method reads available data up to the specified maximum,
     * but may return fewer bytes if not enough data is available before the
     * read timeout expires.
     *
     * @param maxBytes The maximum number of bytes to read.
     * @return std::vector<uint8_t> The data read from the port.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialTimeoutException If the read operation times out.
     * @throws SerialIOException If an I/O error occurs.
     */
    std::vector<uint8_t> read(size_t maxBytes);

    /**
     * @brief Reads exactly the specified number of bytes.
     *
     * This method blocks until all requested bytes are read or the specified
     * timeout expires.
     *
     * @param bytes The number of bytes to read.
     * @param timeout The maximum time to wait for the requested data.
     * @return std::vector<uint8_t> The data read from the port.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialTimeoutException If the read operation times out before
     * reading all bytes.
     * @throws SerialIOException If an I/O error occurs.
     */
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout);

    /**
     * @brief Reads until the specified terminator character is encountered or a
     * timeout occurs.
     *
     * @param terminator The terminating character.
     * @param timeout The maximum time to wait.
     * @param includeTerminator Whether to include the terminator in the result.
     * @return std::string The string read, excluding the terminator (unless
     * includeTerminator is true).
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialTimeoutException If the read operation times out.
     * @throws SerialIOException If an I/O error occurs.
     */
    std::string readUntil(char terminator, std::chrono::milliseconds timeout,
                          bool includeTerminator = false);

    /**
     * @brief Reads until the specified byte sequence is matched or a timeout
     * occurs.
     *
     * @param sequence The terminating sequence to match.
     * @param timeout The maximum time to wait.
     * @param includeSequence Whether to include the terminating sequence in the
     * result.
     * @return std::vector<uint8_t> The data read, excluding the sequence
     * (unless includeSequence is true).
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialTimeoutException If the read operation times out.
     * @throws SerialIOException If an I/O error occurs.
     */
    std::vector<uint8_t> readUntilSequence(std::span<const uint8_t> sequence,
                                           std::chrono::milliseconds timeout,
                                           bool includeSequence = false);

    /**
     * @brief Performs an asynchronous read operation.
     *
     * Starts a read operation that will invoke the provided callback function
     * when data is available or an error occurs.
     *
     * @param maxBytes The maximum number of bytes to read in each operation.
     * @param callback The function to call with the data read.
     * @throws SerialPortNotOpenException If the port is not open.
     */
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback);

    /**
     * @brief Asynchronously reads using futures (C++20).
     *
     * @param maxBytes The maximum number of bytes to read.
     * @return std::future<std::vector<uint8_t>> A future representing the
     * asynchronous read operation.
     * @throws SerialPortNotOpenException If the port is not open.
     */
    std::future<std::vector<uint8_t>> asyncReadFuture(size_t maxBytes);

    /**
     * @brief Reads all currently available data on the serial port.
     *
     * This method reads all data currently available in the input buffer
     * without waiting for more data to arrive.
     *
     * @return std::vector<uint8_t> The data read from the port.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If an I/O error occurs.
     */
    std::vector<uint8_t> readAvailable();

    /**
     * @brief Writes data to the serial port.
     *
     * Attempts to write all provided bytes to the serial port.
     *
     * @param data A span of bytes to write.
     * @return size_t The number of bytes actually written.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialTimeoutException If the write operation times out.
     * @throws SerialIOException If an I/O error occurs.
     */
    size_t write(std::span<const uint8_t> data);

    /**
     * @brief Writes a string to the serial port.
     *
     * Convenience method for writing string data to the port.
     *
     * @param data The string to write.
     * @return size_t The number of bytes actually written.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialTimeoutException If the write operation times out.
     * @throws SerialIOException If an I/O error occurs.
     */
    size_t write(std::string_view data);

    /**
     * @brief Writes any serializable object.
     *
     * Template method for writing C++ fundamental types or trivially copyable
     * types.
     *
     * @tparam T The type to write (must be trivially copyable).
     * @param value The value to write.
     * @return size_t The number of bytes written.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialTimeoutException If the write operation times out.
     * @throws SerialIOException If an I/O error occurs.
     */
    template <Serializable T>
    size_t writeObject(const T& value) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
        return write(std::span<const uint8_t>(data, sizeof(T)));
    }

    /**
     * @brief Asynchronously writes data.
     *
     * @param data The data to write.
     * @return std::future<size_t> A future containing the number of bytes
     * written.
     * @throws SerialPortNotOpenException If the port is not open.
     */
    std::future<size_t> asyncWrite(std::span<const uint8_t> data);

    /**
     * @brief Asynchronously writes a string.
     *
     * @param data The string to write.
     * @return std::future<size_t> A future containing the number of bytes
     * written.
     * @throws SerialPortNotOpenException If the port is not open.
     */
    std::future<size_t> asyncWrite(std::string_view data);

    /**
     * @brief Flushes the input and output buffers.
     *
     * Discards all data in the input and output buffers.
     *
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If an I/O error occurs.
     */
    void flush();

    /**
     * @brief Waits for all output data to be transmitted.
     *
     * Blocks until all data in the output buffer has been sent.
     *
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If an I/O error occurs.
     */
    void drain();

    /**
     * @brief Gets the number of bytes available to read.
     *
     * @return size_t The number of bytes available in the input buffer.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If an I/O error occurs.
     */
    [[nodiscard]] size_t available() const;

    /**
     * @brief Updates the port configuration.
     *
     * Changes the configuration of an already open port.
     *
     * @param config The new configuration parameters.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If the configuration cannot be applied.
     */
    void setConfig(const SerialConfig& config);

    /**
     * @brief Gets the current port configuration.
     *
     * @return SerialConfig The current configuration of the port.
     * @throws SerialPortNotOpenException If the port is not open.
     */
    [[nodiscard]] SerialConfig getConfig() const;

    /**
     * @brief Sets the DTR (Data Terminal Ready) signal.
     *
     * @param value True to assert the signal, False to clear it.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If the signal cannot be set.
     */
    void setDTR(bool value);

    /**
     * @brief Sets the RTS (Request To Send) signal.
     *
     * @param value True to assert the signal, False to clear it.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If the signal cannot be set.
     */
    void setRTS(bool value);

    /**
     * @brief Gets the state of the CTS (Clear To Send) signal.
     *
     * @return true If the signal is asserted.
     * @return false If the signal is not asserted.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If the signal state cannot be read.
     */
    [[nodiscard]] bool getCTS() const;

    /**
     * @brief Gets the state of the DSR (Data Set Ready) signal.
     *
     * @return true If the signal is asserted.
     * @return false If the signal is not asserted.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If the signal state cannot be read.
     */
    [[nodiscard]] bool getDSR() const;

    /**
     * @brief Gets the state of the RI (Ring Indicator) signal.
     *
     * @return true If the signal is asserted.
     * @return false If the signal is not asserted.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If the signal state cannot be read.
     */
    [[nodiscard]] bool getRI() const;

    /**
     * @brief Gets the state of the CD (Carrier Detect) signal.
     *
     * @return true If the signal is asserted.
     * @return false If the signal is not asserted.
     * @throws SerialPortNotOpenException If the port is not open.
     * @throws SerialIOException If the signal state cannot be read.
     */
    [[nodiscard]] bool getCD() const;

    /**
     * @brief Gets the name of the currently open port.
     *
     * @return std::string The name of the open port, or an empty string if no
     * port is open.
     */
    [[nodiscard]] std::string getPortName() const;

    /**
     * @brief Lists all available serial ports on the system.
     *
     * @return std::vector<std::string> A list of names of all available serial
     * ports.
     */
    static std::vector<std::string> getAvailablePorts();

    /**
     * @brief Attempts to open the specified port.
     *
     * Unlike the open method, this method does not throw an exception on
     * failure.
     *
     * @param portName The name of the port to open.
     * @param config The port configuration.
     * @return std::optional<std::string> An error message if opening failed, or
     * std::nullopt on success.
     */
    std::optional<std::string> tryOpen(std::string_view portName,
                                       const SerialConfig& config = {});

private:
    std::unique_ptr<SerialPortImpl>
        impl_;  ///< PIMPL pattern for platform-specific implementation
};

}  // namespace serial