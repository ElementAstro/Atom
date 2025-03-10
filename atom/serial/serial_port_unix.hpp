#include <iostream>
#if defined(__unix__) || defined(__APPLE__)

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <regex>
#include <thread>
#include "serial_port.hpp"

namespace serial {

/**
 * @brief Implementation class for the SerialPort
 *
 * This class handles the low-level Unix/Apple serial port operations.
 * It provides the actual implementation for the SerialPort interface.
 */
class SerialPortImpl {
public:
    /**
     * @brief Default constructor
     *
     * Initializes a new instance with default values and closed port.
     */
    SerialPortImpl()
        : fd_(-1),
          config_{},
          portName_(""),
          asyncReadThread_(),
          stopAsyncRead_(false) {}

    /**
     * @brief Destructor
     *
     * Stops any ongoing asynchronous read operations and closes the port.
     */
    ~SerialPortImpl() {
        stopAsyncWorker();
        close();
    }

    /**
     * @brief Opens a serial port
     *
     * @param portName The name of the serial port to open
     * @param config The configuration settings for the serial port
     * @throws SerialException If the port cannot be opened or is invalid
     */
    void open(const std::string& portName, const SerialConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (isOpen())
            close();

        // Try to open the serial port
        fd_ = ::open(portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

        if (fd_ < 0) {
            throw SerialException("Cannot open serial port: " + portName +
                                  " (Error: " + strerror(errno) + ")");
        }

        // Ensure this is a serial port
        int isatty_res = isatty(fd_);
        if (isatty_res != 1) {
            ::close(fd_);
            fd_ = -1;
            throw SerialException(portName +
                                  " is not a valid serial port device");
        }

        portName_ = portName;
        config_ = config;
        applyConfig();
    }

    /**
     * @brief Closes the serial port
     *
     * Releases resources and closes the port if it is open.
     */
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
            portName_ = "";
        }
    }

    /**
     * @brief Checks if the serial port is open
     *
     * @return true if the port is open, false otherwise
     */
    bool isOpen() const { return fd_ >= 0; }

    /**
     * @brief Reads data from the serial port
     *
     * @param maxBytes Maximum number of bytes to read
     * @return Vector containing the read bytes
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error during the read operation
     */
    std::vector<uint8_t> read(size_t maxBytes) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        if (maxBytes == 0) {
            return {};
        }

        // Set select timeout
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_, &readfds);

        struct timeval timeout;
        timeout.tv_sec = config_.readTimeout.count() / 1000;
        timeout.tv_usec = (config_.readTimeout.count() % 1000) * 1000;

        int selectResult =
            select(fd_ + 1, &readfds, nullptr, nullptr, &timeout);

        if (selectResult < 0) {
            throw SerialIOException("Read error: " +
                                    std::string(strerror(errno)));
        } else if (selectResult == 0) {
            // Timeout
            return {};
        }

        std::vector<uint8_t> buffer(maxBytes);
        ssize_t bytesRead = ::read(fd_, buffer.data(), maxBytes);

        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available to read in non-blocking mode
                return {};
            }
            throw SerialIOException("Read error: " +
                                    std::string(strerror(errno)));
        }

        buffer.resize(bytesRead);
        return buffer;
    }

    /**
     * @brief Reads exactly the specified number of bytes
     *
     * @param bytes Number of bytes to read
     * @param timeout Maximum time to wait for the data
     * @return Vector containing the read bytes
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If the operation times out
     * @throws SerialIOException If there's an error during the read operation
     */
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout) {
        if (!isOpen()) {
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

            // Calculate remaining timeout time
            auto remainingTimeout = timeout - elapsed;

            // Create temporary object to store original timeout settings
            SerialConfig originalConfig = config_;

            try {
                // Temporarily set timeout
                config_.readTimeout = remainingTimeout;

                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }

                // Restore original timeout settings
                config_ = originalConfig;
            } catch (...) {
                // Ensure original timeout settings are restored
                config_ = originalConfig;
                throw;
            }

            // Short sleep to avoid high CPU usage if no data was read
            if (result.size() < bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        return result;
    }

    /**
     * @brief Starts asynchronous read operation
     *
     * @param maxBytes Maximum number of bytes to read in each operation
     * @param callback Function to call when data is received
     * @throws SerialPortNotOpenException If the port is not open
     */
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback) {
        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        stopAsyncWorker();

        stopAsyncRead_ = false;
        asyncReadThread_ = std::thread([this, maxBytes, callback]() {
            while (!stopAsyncRead_) {
                try {
                    auto data = read(maxBytes);
                    if (!data.empty() && !stopAsyncRead_) {
                        callback(std::move(data));
                    }
                } catch (const SerialTimeoutException&) {
                    // Timeouts are normal, continue
                } catch (const std::exception& e) {
                    if (!stopAsyncRead_) {
                        // Pass error information or handle it here
                        std::cerr
                            << "Serial port async read error: " << e.what()
                            << std::endl;
                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    /**
     * @brief Reads all available bytes from the serial port
     *
     * @return Vector containing the read bytes
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error during the read operation
     */
    std::vector<uint8_t> readAvailable() {
        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        int availableBytes = available();
        if (availableBytes == 0) {
            return {};
        }

        return read(availableBytes);
    }

    /**
     * @brief Writes data to the serial port
     *
     * @param data Data to write
     * @return Number of bytes successfully written
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If the operation times out
     * @throws SerialIOException If there's an error during the write operation
     */
    size_t write(std::span<const uint8_t> data) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        if (data.empty()) {
            return 0;
        }

        // Set select timeout
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(fd_, &writefds);

        struct timeval timeout;
        timeout.tv_sec = config_.writeTimeout.count() / 1000;
        timeout.tv_usec = (config_.writeTimeout.count() % 1000) * 1000;

        int selectResult =
            select(fd_ + 1, nullptr, &writefds, nullptr, &timeout);

        if (selectResult < 0) {
            throw SerialIOException("Write error: " +
                                    std::string(strerror(errno)));
        } else if (selectResult == 0) {
            throw SerialTimeoutException();
        }

        ssize_t bytesWritten = ::write(fd_, data.data(), data.size());

        if (bytesWritten < 0) {
            throw SerialIOException("Write error: " +
                                    std::string(strerror(errno)));
        }

        return static_cast<size_t>(bytesWritten);
    }

    /**
     * @brief Flushes both input and output buffers
     *
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error during the flush operation
     */
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        if (tcflush(fd_, TCIOFLUSH) != 0) {
            throw SerialIOException("Unable to flush serial port buffers: " +
                                    std::string(strerror(errno)));
        }
    }

    /**
     * @brief Waits until all output written to the serial port has been
     * transmitted
     *
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error during the drain operation
     */
    void drain() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        if (tcdrain(fd_) != 0) {
            throw SerialIOException("Unable to complete buffer writing: " +
                                    std::string(strerror(errno)));
        }
    }

    /**
     * @brief Gets the number of bytes available to read
     *
     * @return Number of bytes available
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error getting the available byte
     * count
     */
    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        int bytes = 0;

        if (ioctl(fd_, FIONREAD, &bytes) < 0) {
            throw SerialIOException("Unable to get available byte count: " +
                                    std::string(strerror(errno)));
        }

        return static_cast<size_t>(bytes);
    }

    /**
     * @brief Updates the serial port configuration
     *
     * @param config The new configuration settings
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error applying the configuration
     */
    void setConfig(const SerialConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        config_ = config;
        applyConfig();
    }

    /**
     * @brief Gets the current configuration of the serial port
     *
     * @return Current serial port configuration
     */
    SerialConfig getConfig() const { return config_; }

    /**
     * @brief Sets the DTR (Data Terminal Ready) signal
     *
     * @param value New state for the DTR signal (true = high, false = low)
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error setting the signal
     */
    void setDTR(bool value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            throw SerialIOException("Unable to get serial port status: " +
                                    std::string(strerror(errno)));
        }

        if (value) {
            status |= TIOCM_DTR;
        } else {
            status &= ~TIOCM_DTR;
        }

        if (ioctl(fd_, TIOCMSET, &status) < 0) {
            throw SerialIOException("Unable to set DTR signal: " +
                                    std::string(strerror(errno)));
        }
    }

    /**
     * @brief Sets the RTS (Request To Send) signal
     *
     * @param value New state for the RTS signal (true = high, false = low)
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error setting the signal
     */
    void setRTS(bool value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            throw SerialIOException("Unable to get serial port status: " +
                                    std::string(strerror(errno)));
        }

        if (value) {
            status |= TIOCM_RTS;
        } else {
            status &= ~TIOCM_RTS;
        }

        if (ioctl(fd_, TIOCMSET, &status) < 0) {
            throw SerialIOException("Unable to set RTS signal: " +
                                    std::string(strerror(errno)));
        }
    }

    /**
     * @brief Gets the state of the CTS (Clear To Send) signal
     *
     * @return Current state of the CTS signal
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error getting the signal state
     */
    bool getCTS() const { return getModemStatus(TIOCM_CTS); }

    /**
     * @brief Gets the state of the DSR (Data Set Ready) signal
     *
     * @return Current state of the DSR signal
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error getting the signal state
     */
    bool getDSR() const { return getModemStatus(TIOCM_DSR); }

    /**
     * @brief Gets the state of the RI (Ring Indicator) signal
     *
     * @return Current state of the RI signal
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error getting the signal state
     */
    bool getRI() const { return getModemStatus(TIOCM_RI); }

    /**
     * @brief Gets the state of the CD (Carrier Detect) signal
     *
     * @return Current state of the CD signal
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error getting the signal state
     */
    bool getCD() const { return getModemStatus(TIOCM_CD); }

    /**
     * @brief Gets the name of the currently open port
     *
     * @return Name of the serial port
     */
    std::string getPortName() const { return portName_; }

    /**
     * @brief Gets a list of available serial ports in the system
     *
     * @return Vector of available serial port names
     */
    static std::vector<std::string> getAvailablePorts() {
        std::vector<std::string> result;

#if defined(__linux__)
        // Linux: Check /dev/ttyS*, /dev/ttyUSB*, /dev/ttyACM*, /dev/ttyAMA*
        const std::vector<std::string> patterns = {
            "/dev/ttyS[0-9]*", "/dev/ttyUSB[0-9]*", "/dev/ttyACM[0-9]*",
            "/dev/ttyAMA[0-9]*"};

        for (const auto& pattern : patterns) {
            std::regex regPattern(pattern);

            for (const auto& entry :
                 std::filesystem::directory_iterator("/dev")) {
                std::string path = entry.path().string();
                if (std::regex_match(path, regPattern)) {
                    result.push_back(path);
                }
            }
        }
#elif defined(__APPLE__)
        // macOS: Check /dev/tty.* and /dev/cu.*
        const std::vector<std::string> patterns = {"/dev/tty\\..*",
                                                   "/dev/cu\\..*"};

        for (const auto& pattern : patterns) {
            std::regex regPattern(pattern);

            for (const auto& entry :
                 std::filesystem::directory_iterator("/dev")) {
                std::string path = entry.path().string();
                if (std::regex_match(path, regPattern)) {
                    result.push_back(path);
                }
            }
        }
#else
        // Other Unix systems
        DIR* dir = opendir("/dev");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name.substr(0, 3) == "tty") {
                    result.push_back("/dev/" + name);
                }
            }
            closedir(dir);
        }
#endif

        return result;
    }

private:
    int fd_;                           ///< File descriptor for the serial port
    SerialConfig config_;              ///< Current configuration
    std::string portName_;             ///< Name of the open port
    mutable std::mutex mutex_;         ///< Mutex for thread safety
    std::thread asyncReadThread_;      ///< Thread for asynchronous reading
    std::atomic<bool> stopAsyncRead_;  ///< Flag to stop async read thread

    /**
     * @brief Applies the current configuration to the serial port
     *
     * @throws SerialIOException If there's an error applying the configuration
     */
    void applyConfig() {
        if (!isOpen()) {
            return;
        }

        struct termios tty;

        if (tcgetattr(fd_, &tty) != 0) {
            throw SerialIOException(
                "Unable to get serial port configuration: " +
                std::string(strerror(errno)));
        }

        // Set baud rate
        speed_t baudRate = getBaudRateConstant(config_.baudRate);
        cfsetispeed(&tty, baudRate);
        cfsetospeed(&tty, baudRate);

        // Set basic mode: non-canonical, no echo, no special handling
        tty.c_iflag &=
            ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        tty.c_oflag &= ~OPOST;
        tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        tty.c_cflag |= (CLOCAL | CREAD);

        // Data bits
        tty.c_cflag &= ~CSIZE;
        switch (config_.dataBits) {
            case 5:
                tty.c_cflag |= CS5;
                break;
            case 6:
                tty.c_cflag |= CS6;
                break;
            case 7:
                tty.c_cflag |= CS7;
                break;
            case 8:
            default:
                tty.c_cflag |= CS8;
                break;
        }

        // Parity
        switch (config_.parity) {
            case SerialConfig::Parity::None:
                tty.c_cflag &= ~PARENB;
                tty.c_iflag &= ~INPCK;
                break;
            case SerialConfig::Parity::Odd:
                tty.c_cflag |= PARENB;
                tty.c_cflag |= PARODD;
                tty.c_iflag |= INPCK;
                break;
            case SerialConfig::Parity::Even:
                tty.c_cflag |= PARENB;
                tty.c_cflag &= ~PARODD;
                tty.c_iflag |= INPCK;
                break;
            case SerialConfig::Parity::Mark:
                // Mark parity is not supported on most POSIX systems
                throw SerialException(
                    "Mark parity is not supported on POSIX systems");
                break;
            case SerialConfig::Parity::Space:
                // Space parity is not supported on most POSIX systems
                throw SerialException(
                    "Space parity is not supported on POSIX systems");
                break;
        }

        // Stop bits
        switch (config_.stopBits) {
            case SerialConfig::StopBits::One:
                tty.c_cflag &= ~CSTOPB;
                break;
            case SerialConfig::StopBits::Two:
                tty.c_cflag |= CSTOPB;
                break;
            case SerialConfig::StopBits::OnePointFive:
                // POSIX has no definition for 1.5 stop bits, usually use 2
                // instead
                tty.c_cflag |= CSTOPB;
                break;
        }

        // Flow control
        switch (config_.flowControl) {
            case SerialConfig::FlowControl::None:
                tty.c_cflag &= ~CRTSCTS;
                tty.c_iflag &= ~(IXON | IXOFF | IXANY);
                break;
            case SerialConfig::FlowControl::Software:
                tty.c_cflag &= ~CRTSCTS;
                tty.c_iflag |= (IXON | IXOFF);
                break;
            case SerialConfig::FlowControl::Hardware:
                tty.c_cflag |= CRTSCTS;
                tty.c_iflag &= ~(IXON | IXOFF | IXANY);
                break;
        }

        // Set for non-blocking read
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;

        // Apply configuration
        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            throw SerialIOException(
                "Unable to set serial port configuration: " +
                std::string(strerror(errno)));
        }
    }

    /**
     * @brief Converts an integer baud rate to the corresponding termios
     * constant
     *
     * @param baudRate Integer baud rate
     * @return Corresponding termios baud rate constant
     * @throws SerialException If the baud rate is not supported
     */
    speed_t getBaudRateConstant(int baudRate) const {
        switch (baudRate) {
            case 50:
                return B50;
            case 75:
                return B75;
            case 110:
                return B110;
            case 134:
                return B134;
            case 150:
                return B150;
            case 200:
                return B200;
            case 300:
                return B300;
            case 600:
                return B600;
            case 1200:
                return B1200;
            case 1800:
                return B1800;
            case 2400:
                return B2400;
            case 4800:
                return B4800;
            case 9600:
                return B9600;
            case 19200:
                return B19200;
            case 38400:
                return B38400;
            case 57600:
                return B57600;
            case 115200:
                return B115200;
            case 230400:
                return B230400;
#ifdef B460800
            case 460800:
                return B460800;
#endif
#ifdef B500000
            case 500000:
                return B500000;
#endif
#ifdef B576000
            case 576000:
                return B576000;
#endif
#ifdef B921600
            case 921600:
                return B921600;
#endif
#ifdef B1000000
            case 1000000:
                return B1000000;
#endif
#ifdef B1152000
            case 1152000:
                return B1152000;
#endif
#ifdef B1500000
            case 1500000:
                return B1500000;
#endif
#ifdef B2000000
            case 2000000:
                return B2000000;
#endif
#ifdef B2500000
            case 2500000:
                return B2500000;
#endif
#ifdef B3000000
            case 3000000:
                return B3000000;
#endif
#ifdef B3500000
            case 3500000:
                return B3500000;
#endif
#ifdef B4000000
            case 4000000:
                return B4000000;
#endif
            default:
                throw SerialException("Unsupported baud rate: " +
                                      std::to_string(baudRate));
        }
    }

    /**
     * @brief Gets the state of a modem control line
     *
     * @param flag The modem status flag to check
     * @return true if the flag is set, false otherwise
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error getting the modem status
     */
    bool getModemStatus(int flag) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            throw SerialIOException("Unable to get modem status: " +
                                    std::string(strerror(errno)));
        }

        return (status & flag) != 0;
    }

    /**
     * @brief Stops the asynchronous read worker thread
     *
     * Signals the worker thread to stop and waits for it to complete.
     */
    void stopAsyncWorker() {
        if (asyncReadThread_.joinable()) {
            stopAsyncRead_ = true;
            asyncReadThread_.join();
        }
    }
};

}  // namespace serial

#endif  // defined(__unix__) || defined(__APPLE__)