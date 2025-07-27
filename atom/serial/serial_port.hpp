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
#include <variant>
#include <vector>

namespace serial {

/**
 * @brief Error information for non-exception error handling
 */
struct SerialError {
    enum class Code {
        None = 0,
        PortNotOpen,
        Timeout,
        IOError,
        ConfigError,
        InvalidParameter,
        PermissionDenied,
        DeviceNotFound,
        ResourceBusy,
        SystemError
    };

    Code code{Code::None};
    std::string message;
    int systemErrorCode{0}; // Platform-specific error code

    SerialError() = default;
    SerialError(Code c, std::string msg, int sysCode = 0)
        : code(c), message(std::move(msg)), systemErrorCode(sysCode) {}

    [[nodiscard]] bool hasError() const noexcept { return code != Code::None; }
    explicit operator bool() const noexcept { return hasError(); }

    /**
     * @brief Get a human-readable description of the error
     */
    [[nodiscard]] std::string getDescription() const {
        std::string desc = message;
        if (systemErrorCode != 0) {
            desc += " (System error: " + std::to_string(systemErrorCode) + ")";
        }
        return desc;
    }
};

/**
 * @brief Result type for operations that can fail without exceptions
 * @tparam T The success value type
 */
template<typename T>
using Result = std::variant<T, SerialError>;

/**
 * @brief Base class for all serial port related exceptions.
 */
class SerialException : public std::runtime_error {
public:
    /**
     * @brief Constructs a new serial exception.
     * @param message The error message describing the exception.
     */
    explicit SerialException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when attempting an operation on a closed port.
 */
class SerialPortNotOpenException : public SerialException {
public:
    explicit SerialPortNotOpenException()
        : SerialException("Port is not open") {}
};

/**
 * @brief Exception thrown when a serial operation times out.
 */
class SerialTimeoutException : public SerialException {
public:
    explicit SerialTimeoutException()
        : SerialException("Serial operation timed out") {}

    explicit SerialTimeoutException(const std::string& message)
        : SerialException("Serial operation timed out: " + message) {}
};

/**
 * @brief Exception thrown when a general I/O error occurs during a serial
 * operation.
 */
class SerialIOException : public SerialException {
public:
    explicit SerialIOException(const std::string& message)
        : SerialException(message) {}
};

/**
 * @brief Exception thrown when an invalid configuration is provided.
 */
class SerialConfigException : public SerialException {
public:
    explicit SerialConfigException(const std::string& message)
        : SerialException("Configuration error: " + message) {}
};

/**
 * @brief Helper to check if a Result contains a value
 */
template<typename T>
[[nodiscard]] bool isOk(const Result<T>& result) noexcept {
    return std::holds_alternative<T>(result);
}

/**
 * @brief Helper to extract value from Result (throws if error)
 */
template<typename T>
[[nodiscard]] T getValue(const Result<T>& result) {
    if (auto* value = std::get_if<T>(&result)) {
        return *value;
    }
    const auto& error = std::get<SerialError>(result);
    throw SerialException(error.message);
}

/**
 * @brief Helper to extract error from Result
 */
template<typename T>
[[nodiscard]] SerialError getError(const Result<T>& result) noexcept {
    if (auto* error = std::get_if<SerialError>(&result)) {
        return *error;
    }
    return {};
}

/**
 * @brief RAII helper for automatic resource cleanup
 */
template<typename Resource, typename Deleter>
class ResourceGuard {
public:
    ResourceGuard(Resource resource, Deleter deleter)
        : resource_(resource), deleter_(std::move(deleter)), active_(true) {}

    ~ResourceGuard() {
        if (active_) {
            deleter_(resource_);
        }
    }

    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;

    ResourceGuard(ResourceGuard&& other) noexcept
        : resource_(other.resource_), deleter_(std::move(other.deleter_)), active_(other.active_) {
        other.active_ = false;
    }

    ResourceGuard& operator=(ResourceGuard&& other) noexcept {
        if (this != &other) {
            if (active_) {
                deleter_(resource_);
            }
            resource_ = other.resource_;
            deleter_ = std::move(other.deleter_);
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

    void release() noexcept { active_ = false; }
    Resource get() const noexcept { return resource_; }

private:
    Resource resource_;
    Deleter deleter_;
    bool active_;
};

/**
 * @brief Helper function to create resource guards
 */
template<typename Resource, typename Deleter>
auto makeResourceGuard(Resource resource, Deleter deleter) {
    return ResourceGuard<Resource, Deleter>(resource, std::move(deleter));
}

/**
 * @brief Structure for serial port parameter configuration.
 */
class SerialConfig {
public:
    enum class Parity { None, Odd, Even, Mark, Space };
    enum class StopBits { One, OnePointFive, Two };
    enum class FlowControl { None, Software, Hardware };

    SerialConfig() = default;

    SerialConfig& withBaudRate(int rate) {
        if (rate <= 0) {
            throw SerialConfigException("Baud rate must be greater than 0");
        }
        baudRate = rate;
        return *this;
    }

    SerialConfig& withDataBits(int bits) {
        if (bits < 5 || bits > 8) {
            throw SerialConfigException("Data bits must be between 5 and 8");
        }
        dataBits = bits;
        return *this;
    }

    SerialConfig& withParity(Parity p) {
        parity = p;
        return *this;
    }

    SerialConfig& withStopBits(StopBits sb) {
        stopBits = sb;
        return *this;
    }

    SerialConfig& withFlowControl(FlowControl flow) {
        flowControl = flow;
        return *this;
    }

    SerialConfig& withReadTimeout(std::chrono::milliseconds timeout) {
        readTimeout = timeout;
        return *this;
    }

    SerialConfig& withWriteTimeout(std::chrono::milliseconds timeout) {
        writeTimeout = timeout;
        return *this;
    }

    static SerialConfig standardConfig(int baudRate) {
        return SerialConfig()
            .withBaudRate(baudRate)
            .withDataBits(8)
            .withStopBits(StopBits::One)
            .withParity(Parity::None)
            .withFlowControl(FlowControl::None);
    }

    [[nodiscard]] int getBaudRate() const noexcept { return baudRate; }
    [[nodiscard]] int getDataBits() const noexcept { return dataBits; }
    [[nodiscard]] Parity getParity() const noexcept { return parity; }
    [[nodiscard]] StopBits getStopBits() const noexcept { return stopBits; }
    [[nodiscard]] FlowControl getFlowControl() const noexcept {
        return flowControl;
    }
    [[nodiscard]] std::chrono::milliseconds getReadTimeout() const noexcept {
        return readTimeout;
    }
    [[nodiscard]] std::chrono::milliseconds getWriteTimeout() const noexcept {
        return writeTimeout;
    }

    // Add public setters for timeouts
    void setReadTimeout(std::chrono::milliseconds timeout) noexcept {
        readTimeout = timeout;
    }
    void setWriteTimeout(std::chrono::milliseconds timeout) noexcept {
        writeTimeout = timeout;
    }

    // Add validation method for better error handling
    [[nodiscard]] bool isValid() const noexcept {
        return baudRate > 0 && dataBits >= 5 && dataBits <= 8 &&
               readTimeout.count() >= 0 && writeTimeout.count() >= 0;
    }

private:
    int baudRate = 9600;
    int dataBits = 8;
    Parity parity = Parity::None;
    StopBits stopBits = StopBits::One;
    FlowControl flowControl = FlowControl::None;
    std::chrono::milliseconds readTimeout{1000};
    std::chrono::milliseconds writeTimeout{1000};
};

class SerialPortImpl;

/**
 * @brief Concept to check if a type can be serialized into bytes.
 */
template <typename T>
concept Serializable = std::is_trivially_copyable_v<T>;

/**
 * @brief Main interface class for serial communication.
 */
class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    /**
     * @brief Opens the specified serial port.
     * @param portName The name of the port to open.
     * @param config The configuration parameters for the port.
     */
    void open(std::string_view portName, const SerialConfig& config = {});

    /**
     * @brief Closes the currently open port.
     */
    void close();

    /**
     * @brief Checks if the serial port is currently open.
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * @brief Reads up to maxBytes bytes from the serial port.
     * @param maxBytes The maximum number of bytes to read.
     */
    std::vector<uint8_t> read(size_t maxBytes);

    /**
     * @brief Reads exactly the specified number of bytes.
     * @param bytes The number of bytes to read.
     * @param timeout The maximum time to wait for the requested data.
     */
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout);

    /**
     * @brief Reads until the specified terminator character is encountered.
     * @param terminator The terminating character.
     * @param timeout The maximum time to wait.
     * @param includeTerminator Whether to include the terminator in the result.
     */
    std::string readUntil(char terminator, std::chrono::milliseconds timeout,
                          bool includeTerminator = false);

    /**
     * @brief Reads until the specified byte sequence is matched.
     * @param sequence The terminating sequence to match.
     * @param timeout The maximum time to wait.
     * @param includeSequence Whether to include the terminating sequence in the
     * result.
     */
    std::vector<uint8_t> readUntilSequence(std::span<const uint8_t> sequence,
                                           std::chrono::milliseconds timeout,
                                           bool includeSequence = false);

    /**
     * @brief Performs an asynchronous read operation.
     * @param maxBytes The maximum number of bytes to read in each operation.
     * @param callback The function to call with the data read.
     */
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback);

    /**
     * @brief Asynchronously reads using futures.
     * @param maxBytes The maximum number of bytes to read.
     */
    std::future<std::vector<uint8_t>> asyncReadFuture(size_t maxBytes);

    /**
     * @brief Reads all currently available data on the serial port.
     */
    std::vector<uint8_t> readAvailable();

    /**
     * @brief Writes data to the serial port.
     * @param data A span of bytes to write.
     */
    size_t write(std::span<const uint8_t> data);

    /**
     * @brief Writes a string to the serial port.
     * @param data The string to write.
     */
    size_t write(std::string_view data);

    /**
     * @brief Writes any serializable object.
     * @tparam T The type to write (must be trivially copyable).
     * @param value The value to write.
     */
    template <Serializable T>
    size_t writeObject(const T& value) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
        return write(std::span<const uint8_t>(data, sizeof(T)));
    }

    /**
     * @brief Asynchronously writes data.
     * @param data The data to write.
     */
    std::future<size_t> asyncWrite(std::span<const uint8_t> data);

    /**
     * @brief Asynchronously writes a string.
     * @param data The string to write.
     */
    std::future<size_t> asyncWrite(std::string_view data);

    /**
     * @brief Flushes the input and output buffers.
     */
    void flush();

    /**
     * @brief Waits for all output data to be transmitted.
     */
    void drain();

    /**
     * @brief Gets the number of bytes available to read.
     */
    [[nodiscard]] size_t available() const;

    /**
     * @brief Updates the port configuration.
     * @param config The new configuration parameters.
     */
    void setConfig(const SerialConfig& config);

    /**
     * @brief Gets the current port configuration.
     */
    [[nodiscard]] SerialConfig getConfig() const;

    /**
     * @brief Sets the DTR (Data Terminal Ready) signal.
     * @param value True to assert the signal, False to clear it.
     */
    void setDTR(bool value);

    /**
     * @brief Sets the RTS (Request To Send) signal.
     * @param value True to assert the signal, False to clear it.
     */
    void setRTS(bool value);

    /**
     * @brief Gets the state of the CTS (Clear To Send) signal.
     */
    [[nodiscard]] bool getCTS() const;

    /**
     * @brief Gets the state of the DSR (Data Set Ready) signal.
     */
    [[nodiscard]] bool getDSR() const;

    /**
     * @brief Gets the state of the RI (Ring Indicator) signal.
     */
    [[nodiscard]] bool getRI() const;

    /**
     * @brief Gets the state of the CD (Carrier Detect) signal.
     */
    [[nodiscard]] bool getCD() const;

    /**
     * @brief Gets the name of the currently open port.
     */
    [[nodiscard]] std::string getPortName() const;

    /**
     * @brief Lists all available serial ports on the system.
     */
    static std::vector<std::string> getAvailablePorts();

    /**
     * @brief Attempts to open the specified port.
     * @param portName The name of the port to open.
     * @param config The port configuration.
     * @return An error message if opening failed, or std::nullopt on success.
     */
    std::optional<std::string> tryOpen(std::string_view portName,
                                       const SerialConfig& config = {});

    /**
     * @brief Non-throwing version of read operation.
     * @param maxBytes The maximum number of bytes to read.
     * @return Result containing either the data or error information.
     */
    [[nodiscard]] Result<std::vector<uint8_t>> tryRead(size_t maxBytes) noexcept;

    /**
     * @brief Non-throwing version of write operation.
     * @param data The data to write.
     * @return Result containing either bytes written or error information.
     */
    [[nodiscard]] Result<size_t> tryWrite(std::span<const uint8_t> data) noexcept;

    /**
     * @brief Non-throwing version of write operation for strings.
     * @param data The string to write.
     * @return Result containing either bytes written or error information.
     */
    [[nodiscard]] Result<size_t> tryWrite(std::string_view data) noexcept;

    /**
     * @brief Get performance statistics for the serial port
     * @return Struct containing performance metrics
     */
    struct PerformanceStats {
        size_t totalBytesRead{0};
        size_t totalBytesWritten{0};
        size_t totalReadOperations{0};
        size_t totalWriteOperations{0};
        std::chrono::milliseconds totalReadTime{0};
        std::chrono::milliseconds totalWriteTime{0};
        size_t bufferPoolHits{0};
        size_t bufferPoolMisses{0};
    };

    [[nodiscard]] PerformanceStats getPerformanceStats() const;

private:
    std::unique_ptr<SerialPortImpl> impl_;
};

}  // namespace serial
