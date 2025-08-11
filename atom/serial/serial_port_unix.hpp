#pragma once

#if defined(__unix__) || defined(__APPLE__)

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <thread>
#include "serial_port.hpp"

namespace serial {

/**
 * @brief Unix/Apple platform serial port implementation class.
 */
class SerialPortImpl {
public:
    SerialPortImpl()
        : fd_(-1),
          config_{},
          portName_(),
          asyncReadThread_(),
          stopAsyncRead_(false),
          asyncReadActive_(false) {}

    ~SerialPortImpl() {
        stopAsyncWorker();
        close();
    }

    void open(std::string_view portName, const SerialConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (isOpen()) {
            close();
        }

        fd_ = ::open(std::string(portName).c_str(),
                     O_RDWR | O_NOCTTY | O_NONBLOCK);

        if (fd_ < 0) {
            const std::string error =
                "Cannot open serial port: " + std::string(portName) +
                " (error: " + strerror(errno) + ")";
            spdlog::error(error);
            throw SerialException(error);
        }

        if (isatty(fd_) != 1) {
            ::close(fd_);
            fd_ = -1;
            const std::string error =
                std::string(portName) + " is not a valid serial device";
            spdlog::error(error);
            throw SerialException(error);
        }

        portName_ = portName;
        config_ = config;

        try {
            applyConfig();
            spdlog::debug("Successfully opened serial port: {}", portName);
        } catch (...) {
            ::close(fd_);
            fd_ = -1;
            throw;
        }
    }

    void close() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
            spdlog::debug("Closed serial port: {}", portName_);
            portName_.clear();
        }
    }

    [[nodiscard]] bool isOpen() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return fd_ >= 0;
    }

    std::vector<uint8_t> read(size_t maxBytes) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (maxBytes == 0) {
            return {};
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_, &readfds);

        struct timeval timeout;
        const auto timeoutMs = config_.getReadTimeout().count();
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        const int selectResult =
            select(fd_ + 1, &readfds, nullptr, nullptr, &timeout);

        if (selectResult < 0) {
            const std::string error =
                "Read error: " + std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        } else if (selectResult == 0) {
            return {};
        }

        std::vector<uint8_t> buffer(maxBytes);
        const ssize_t bytesRead = ::read(fd_, buffer.data(), maxBytes);

        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return {};
            }
            const std::string error =
                "Read error: " + std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }

        buffer.resize(bytesRead);
        return buffer;
    }

    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout) {
        checkPortOpen();

        if (bytes == 0) {
            return {};
        }

        std::vector<uint8_t> result;
        result.reserve(bytes);

        const auto startTime = std::chrono::steady_clock::now();

        while (result.size() < bytes) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - startTime);

            if (elapsed >= timeout) {
                const std::string error = "Reading " + std::to_string(bytes) +
                                          " bytes timed out, only read " +
                                          std::to_string(result.size()) +
                                          " bytes";
                spdlog::warn(error);
                throw SerialTimeoutException(error);
            }

            const auto remainingTimeout = timeout - elapsed;
            const auto originalTimeout = config_.getReadTimeout();

            try {
                SerialConfig tempConfig = config_;
                tempConfig.withReadTimeout(remainingTimeout);

                std::unique_lock<std::shared_mutex> lock(mutex_);
                applyConfigInternal(tempConfig);

                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }

                applyConfigInternal(config_);
            } catch (const std::exception&) {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                SerialConfig originalConfig = config_;
                originalConfig.withReadTimeout(originalTimeout);
                applyConfigInternal(originalConfig);
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
        checkPortOpen();
        stopAsyncWorker();

        stopAsyncRead_ = false;
        asyncReadActive_ = true;
        asyncReadThread_ = std::thread([this, maxBytes,
                                        callback = std::move(callback)]() {
            spdlog::debug("Starting async read thread");
            try {
                while (!stopAsyncRead_) {
                    try {
                        auto data = read(maxBytes);
                        if (!data.empty() && !stopAsyncRead_) {
                            callback(std::move(data));
                        }
                    } catch (const SerialTimeoutException&) {
                        // Timeout is normal, continue
                    } catch (const std::exception& e) {
                        if (!stopAsyncRead_) {
                            spdlog::error("Serial async read error: {}",
                                          e.what());
                            break;
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } catch (...) {
                spdlog::error("Unexpected error in async read thread");
            }

            {
                std::unique_lock<std::mutex> lock(asyncMutex_);
                asyncReadActive_ = false;
                asyncCv_.notify_all();
            }
            spdlog::debug("Async read thread stopped");
        });
    }

    std::vector<uint8_t> readAvailable() {
        checkPortOpen();

        const int availableBytes = available();
        if (availableBytes == 0) {
            return {};
        }

        return read(availableBytes);
    }

    size_t write(std::span<const uint8_t> data) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (data.empty()) {
            return 0;
        }

        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(fd_, &writefds);

        struct timeval timeout;
        const auto timeoutMs = config_.getWriteTimeout().count();
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        const int selectResult =
            select(fd_ + 1, nullptr, &writefds, nullptr, &timeout);

        if (selectResult < 0) {
            const std::string error =
                "Write error: " + std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        } else if (selectResult == 0) {
            spdlog::warn("Write operation timed out");
            throw SerialTimeoutException();
        }

        const ssize_t bytesWritten = ::write(fd_, data.data(), data.size());

        if (bytesWritten < 0) {
            const std::string error =
                "Write error: " + std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }

        return static_cast<size_t>(bytesWritten);
    }

    void flush() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (tcflush(fd_, TCIOFLUSH) != 0) {
            const std::string error = "Cannot flush serial port buffers: " +
                                      std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }
    }

    void drain() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (tcdrain(fd_) != 0) {
            const std::string error =
                "Cannot complete buffer write: " + std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }
    }

    size_t available() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        int bytes = 0;

        if (ioctl(fd_, FIONREAD, &bytes) < 0) {
            const std::string error = "Cannot get available bytes count: " +
                                      std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }

        return static_cast<size_t>(bytes);
    }

    void setConfig(const SerialConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        config_ = config;
        applyConfigInternal(config);
    }

    [[nodiscard]] SerialConfig getConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_;
    }

    void setDTR(bool value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            const std::string error = "Cannot get serial port status: " +
                                      std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }

        if (value) {
            status |= TIOCM_DTR;
        } else {
            status &= ~TIOCM_DTR;
        }

        if (ioctl(fd_, TIOCMSET, &status) < 0) {
            const std::string error =
                "Cannot set DTR signal: " + std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }
    }

    void setRTS(bool value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            const std::string error = "Cannot get serial port status: " +
                                      std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }

        if (value) {
            status |= TIOCM_RTS;
        } else {
            status &= ~TIOCM_RTS;
        }

        if (ioctl(fd_, TIOCMSET, &status) < 0) {
            const std::string error =
                "Cannot set RTS signal: " + std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }
    }

    [[nodiscard]] bool getCTS() const { return getModemStatus(TIOCM_CTS); }
    [[nodiscard]] bool getDSR() const { return getModemStatus(TIOCM_DSR); }
    [[nodiscard]] bool getRI() const { return getModemStatus(TIOCM_RI); }
    [[nodiscard]] bool getCD() const { return getModemStatus(TIOCM_CD); }

    [[nodiscard]] std::string getPortName() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return portName_;
    }

    static std::vector<std::string> getAvailablePorts() {
        std::vector<std::string> result;

#if defined(__linux__)
        const std::vector<std::string> patterns = {
            "/dev/ttyS[0-9]*", "/dev/ttyUSB[0-9]*", "/dev/ttyACM[0-9]*",
            "/dev/ttyAMA[0-9]*"};

        for (const auto& pattern : patterns) {
            std::regex regPattern(pattern);

            std::error_code ec;
            for (const auto& entry :
                 std::filesystem::directory_iterator("/dev", ec)) {
                if (ec)
                    continue;

                const std::string path = entry.path().string();
                if (std::regex_match(path, regPattern)) {
                    result.push_back(path);
                }
            }
        }
#elif defined(__APPLE__)
        const std::vector<std::string> patterns = {"/dev/tty\\..*",
                                                   "/dev/cu\\..*"};

        for (const auto& pattern : patterns) {
            std::regex regPattern(pattern);

            std::error_code ec;
            for (const auto& entry :
                 std::filesystem::directory_iterator("/dev", ec)) {
                if (ec)
                    continue;

                const std::string path = entry.path().string();
                if (std::regex_match(path, regPattern)) {
                    result.push_back(path);
                }
            }
        }
#else
        DIR* dir = opendir("/dev");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                const std::string name = entry->d_name;
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
    int fd_;
    SerialConfig config_;
    std::string portName_;
    mutable std::shared_mutex mutex_;
    std::thread asyncReadThread_;
    std::atomic<bool> stopAsyncRead_;
    std::atomic<bool> asyncReadActive_;
    std::mutex asyncMutex_;
    std::condition_variable asyncCv_;

    void applyConfig() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        applyConfigInternal(config_);
    }

    void checkPortOpen() const {
        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }
    }

    void applyConfigInternal(const SerialConfig& config) {
        if (fd_ < 0)
            return;

        struct termios tty;

        if (tcgetattr(fd_, &tty) != 0) {
            const std::string error = "Cannot get serial port configuration: " +
                                      std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }

        const speed_t baudRate = getBaudRateConstant(config.getBaudRate());
        cfsetispeed(&tty, baudRate);
        cfsetospeed(&tty, baudRate);

        tty.c_iflag &=
            ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        tty.c_oflag &= ~OPOST;
        tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        tty.c_cflag |= (CLOCAL | CREAD);

        tty.c_cflag &= ~CSIZE;
        switch (config.getDataBits()) {
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

        switch (config.getParity()) {
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
                spdlog::error("Mark parity not supported on POSIX systems");
                throw SerialException(
                    "Mark parity not supported on POSIX systems");
            case SerialConfig::Parity::Space:
                spdlog::error("Space parity not supported on POSIX systems");
                throw SerialException(
                    "Space parity not supported on POSIX systems");
        }

        switch (config.getStopBits()) {
            case SerialConfig::StopBits::One:
                tty.c_cflag &= ~CSTOPB;
                break;
            case SerialConfig::StopBits::Two:
            case SerialConfig::StopBits::OnePointFive:
                tty.c_cflag |= CSTOPB;
                break;
        }

        switch (config.getFlowControl()) {
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

        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;

        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            const std::string error = "Cannot set serial port configuration: " +
                                      std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }
    }

    [[nodiscard]] speed_t getBaudRateConstant(int baudRate) const {
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
                const std::string error =
                    "Unsupported baud rate: " + std::to_string(baudRate);
                spdlog::error(error);
                throw SerialException(error);
        }
    }

    [[nodiscard]] bool getModemStatus(int flag) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            const std::string error =
                "Cannot get modem status: " + std::string(strerror(errno));
            spdlog::error(error);
            throw SerialIOException(error);
        }

        return (status & flag) != 0;
    }

    void stopAsyncWorker() {
        if (asyncReadThread_.joinable()) {
            stopAsyncRead_ = true;

            {
                std::unique_lock<std::mutex> lock(asyncMutex_);
                if (asyncReadActive_) {
                    asyncCv_.wait(lock, [this]() { return !asyncReadActive_; });
                }
            }

            asyncReadThread_.join();
        }
    }
};

}  // namespace serial

#endif  // defined(__unix__) || defined(__APPLE__)
