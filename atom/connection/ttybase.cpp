#include "ttybase.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <io.h>
// clang-format on
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

class TTYBase::Impl {
public:
    explicit Impl(std::string_view driverName)
        : m_PortFD(-1),
          m_Debug(false),
          m_DriverName(driverName),
          m_IsRunning(false) {}

    ~Impl() noexcept {
        try {
            stopAsyncOperations();
            if (m_PortFD != -1) {
                disconnect();
            }
        } catch (...) {
        }
    }

    TTYResponse checkTimeout(uint8_t timeout) {
        if (m_PortFD == -1) {
            return TTYResponse::Errno;
        }

#ifdef _WIN32
        COMMTIMEOUTS timeouts = {};
        timeouts.ReadIntervalTimeout = timeout * 1000;
        timeouts.ReadTotalTimeoutConstant = timeout * 1000;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = timeout * 1000;
        timeouts.WriteTotalTimeoutMultiplier = 0;

        HANDLE hPort = reinterpret_cast<HANDLE>(m_PortFD);
        if (!SetCommTimeouts(hPort, &timeouts))
            return TTYResponse::Errno;

        return TTYResponse::OK;
#else
        struct timeval tv;
        fd_set readout;
        int retval;

        FD_ZERO(&readout);
        FD_SET(m_PortFD, &readout);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        retval = select(m_PortFD + 1, &readout, nullptr, nullptr, &tv);

        if (retval > 0) {
            return TTYResponse::OK;
        }
        if (retval == -1) {
            if (errno == EINTR) {
                // 被信号中断，不是致命错误
                if (m_Debug) {
                    LOG_F(INFO, "select() interrupted by signal");
                }
                return TTYResponse::Timeout;
            }
            if (m_Debug) {
                LOG_F(ERROR, "select() error: {}", strerror(errno));
            }
            return TTYResponse::SelectError;
        }
        return TTYResponse::Timeout;
#endif
    }

    TTYResponse read(std::span<uint8_t> buffer, uint8_t timeout,
                     uint32_t& nbytesRead) {
        if (buffer.empty()) {
            return TTYResponse::ParamError;
        }

        try {
            if (m_PortFD == -1) {
                throw std::system_error(errno, std::system_category(),
                                        "Invalid port descriptor");
            }

            nbytesRead = 0;
            const uint32_t nbytes = static_cast<uint32_t>(buffer.size());

#ifdef _WIN32
            DWORD bytesRead = 0;
            HANDLE hPort = reinterpret_cast<HANDLE>(m_PortFD);

            COMMTIMEOUTS timeouts = {};
            timeouts.ReadIntervalTimeout = timeout * 1000;
            timeouts.ReadTotalTimeoutConstant = timeout * 1000;
            timeouts.ReadTotalTimeoutMultiplier = 0;
            timeouts.WriteTotalTimeoutConstant = timeout * 1000;
            timeouts.WriteTotalTimeoutMultiplier = 0;

            if (!SetCommTimeouts(hPort, &timeouts)) {
                return TTYResponse::Errno;
            }

            if (!ReadFile(hPort, buffer.data(), nbytes, &bytesRead, nullptr)) {
                auto error = GetLastError();
                if (m_Debug) {
                    LOG_F(ERROR, "ReadFile error: {}", error);
                }
                return TTYResponse::ReadError;
            }

            nbytesRead = bytesRead;
            return TTYResponse::OK;
#else
            uint32_t numBytesToRead = nbytes;
            int bytesRead = 0;
            TTYResponse timeoutResponse = TTYResponse::OK;

            while (numBytesToRead > 0) {
                if ((timeoutResponse = checkTimeout(timeout)) !=
                    TTYResponse::OK) {
                    if (m_Debug && timeoutResponse == TTYResponse::Timeout) {
                        LOG_F(INFO,
                              "Read operation timed out after reading {} bytes",
                              nbytesRead);
                    }
                    return timeoutResponse;
                }

                bytesRead = ::read(m_PortFD, buffer.data() + nbytesRead,
                                   numBytesToRead);

                if (bytesRead < 0) {
                    if (errno == EINTR) {
                        // System call interrupted, retry
                        continue;
                    }
                    if (m_Debug) {
                        LOG_F(ERROR, "Read error: {}", strerror(errno));
                    }
                    return TTYResponse::ReadError;
                }

                if (bytesRead == 0) {
                    // End of file reached
                    break;
                }

                nbytesRead += bytesRead;
                numBytesToRead -= bytesRead;
            }

            return TTYResponse::OK;
#endif
        } catch (const std::system_error& e) {
            if (m_Debug) {
                LOG_F(ERROR, "System error during read: {}", e.what());
            }
            return TTYResponse::Errno;
        } catch (const std::exception& e) {
            if (m_Debug) {
                LOG_F(ERROR, "Exception during read: {}", e.what());
            }
            return TTYResponse::ReadError;
        }
    }

    TTYResponse readSection(std::span<uint8_t> buffer, uint8_t stopByte,
                            uint8_t timeout, uint32_t& nbytesRead) {
        if (buffer.empty()) {
            return TTYResponse::ParamError;
        }

        try {
            if (m_PortFD == -1) {
                throw std::system_error(errno, std::system_category(),
                                        "Invalid port descriptor");
            }

            nbytesRead = 0;
            std::fill(buffer.begin(), buffer.end(), 0);
            const size_t nsize = buffer.size();

            while (nbytesRead < nsize) {
                if (auto timeoutResponse = checkTimeout(timeout);
                    timeoutResponse != TTYResponse::OK) {
                    return timeoutResponse;
                }

                uint8_t readChar;
                int bytesRead = ::read(m_PortFD, &readChar, 1);

                if (bytesRead < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return TTYResponse::ReadError;
                }

                if (bytesRead == 0) {
                    break;
                }

                buffer[nbytesRead++] = readChar;

                if (readChar == stopByte) {
                    return TTYResponse::OK;
                }
            }

            return TTYResponse::Overflow;
        } catch (const std::system_error& e) {
            if (m_Debug) {
                LOG_F(ERROR, "System error during readSection: {}", e.what());
            }
            return TTYResponse::Errno;
        } catch (const std::exception& e) {
            if (m_Debug) {
                LOG_F(ERROR, "Exception during readSection: {}", e.what());
            }
            return TTYResponse::ReadError;
        }
    }

    TTYResponse write(std::span<const uint8_t> buffer,
                      uint32_t& nbytesWritten) {
        if (buffer.empty()) {
            nbytesWritten = 0;
            return TTYResponse::OK;
        }

        try {
            if (m_PortFD == -1) {
                throw std::system_error(errno, std::system_category(),
                                        "Invalid port descriptor");
            }

#ifdef _WIN32
            DWORD bytesWritten;
            HANDLE hPort = reinterpret_cast<HANDLE>(m_PortFD);

            if (!WriteFile(hPort, buffer.data(),
                           static_cast<DWORD>(buffer.size()), &bytesWritten,
                           nullptr)) {
                auto error = GetLastError();
                if (m_Debug) {
                    LOG_F(ERROR, "WriteFile error: {}", error);
                }
                return TTYResponse::WriteError;
            }

            nbytesWritten = bytesWritten;
            return TTYResponse::OK;
#else
            int bytesW = 0;
            nbytesWritten = 0;
            uint32_t remaining = static_cast<uint32_t>(buffer.size());

            while (remaining > 0) {
                bytesW =
                    ::write(m_PortFD, buffer.data() + nbytesWritten, remaining);

                if (bytesW < 0) {
                    if (errno == EINTR) {
                        // 中断的系统调用，重试
                        continue;
                    }
                    if (m_Debug) {
                        LOG_F(ERROR, "Write error: {}", strerror(errno));
                    }
                    return TTYResponse::WriteError;
                }

                nbytesWritten += bytesW;
                remaining -= bytesW;
            }

            return TTYResponse::OK;
#endif
        } catch (const std::system_error& e) {
            if (m_Debug) {
                LOG_F(ERROR, "System error during write: {}", e.what());
            }
            return TTYResponse::Errno;
        } catch (const std::exception& e) {
            if (m_Debug) {
                LOG_F(ERROR, "Exception during write: {}", e.what());
            }
            return TTYResponse::WriteError;
        }
    }

    TTYResponse connect(std::string_view device, uint32_t bitRate,
                        uint8_t wordSize, uint8_t parity, uint8_t stopBits) {
        try {
            if (device.empty()) {
                THROW_INVALID_ARGUMENT("Device name cannot be empty");
            }

            if (wordSize < 5 || wordSize > 8) {
                THROW_INVALID_ARGUMENT(
                    "Word size must be between 5 and 8 bits");
            }

            if (parity > 2) {
                THROW_INVALID_ARGUMENT("Invalid parity value");
            }

            if (stopBits != 1 && stopBits != 2) {
                THROW_INVALID_ARGUMENT("Stop bits must be 1 or 2");
            }

#ifdef _WIN32
            std::string devicePath(device);

            if (devicePath.find("COM") != std::string::npos &&
                devicePath.find("\\\\.\\") != 0 &&
                std::stoi(devicePath.substr(3)) > 9) {
                devicePath = "\\\\.\\" + devicePath;
            }

            HANDLE hSerial = CreateFileA(
                devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hSerial == INVALID_HANDLE_VALUE) {
                auto error = GetLastError();
                if (m_Debug) {
                    LOG_F(ERROR, "Failed to open port {}: Error code {}",
                          devicePath, error);
                }
                return TTYResponse::PortFailure;
            }

            DCB dcbSerialParams = {};
            dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

            if (!GetCommState(hSerial, &dcbSerialParams)) {
                CloseHandle(hSerial);
                if (m_Debug) {
                    LOG_F(ERROR, "Failed to get comm state for {}", devicePath);
                }
                return TTYResponse::PortFailure;
            }

            dcbSerialParams.BaudRate = bitRate;
            dcbSerialParams.ByteSize = wordSize;
            dcbSerialParams.StopBits =
                (stopBits == 1) ? ONESTOPBIT : TWOSTOPBITS;

            switch (parity) {
                case 0:  // None
                    dcbSerialParams.Parity = NOPARITY;
                    break;
                case 1:  // Even
                    dcbSerialParams.Parity = EVENPARITY;
                    break;
                case 2:  // Odd
                    dcbSerialParams.Parity = ODDPARITY;
                    break;
            }

            dcbSerialParams.fOutxCtsFlow = FALSE;
            dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
            dcbSerialParams.fOutX = FALSE;
            dcbSerialParams.fInX = FALSE;

            if (!SetCommState(hSerial, &dcbSerialParams)) {
                auto error = GetLastError();
                CloseHandle(hSerial);
                if (m_Debug) {
                    LOG_F(ERROR, "Failed to set comm state for {}: Error {}",
                          devicePath, error);
                }
                return TTYResponse::PortFailure;
            }

            // 设置超时
            COMMTIMEOUTS timeouts = {};
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.ReadTotalTimeoutMultiplier = 0;
            timeouts.ReadTotalTimeoutConstant = 0;
            timeouts.WriteTotalTimeoutMultiplier = 0;
            timeouts.WriteTotalTimeoutConstant = 0;

            if (!SetCommTimeouts(hSerial, &timeouts)) {
                CloseHandle(hSerial);
                if (m_Debug) {
                    LOG_F(ERROR, "Failed to set comm timeouts for {}",
                          devicePath);
                }
                return TTYResponse::PortFailure;
            }

            m_PortFD = reinterpret_cast<intptr_t>(hSerial);
            return TTYResponse::OK;
#else
            int tFd = open(std::string(device).c_str(),
                           O_RDWR | O_NOCTTY | O_NONBLOCK);
            if (tFd == -1) {
                if (m_Debug) {
                    LOG_F(ERROR, "Error opening {}: {}", device.data(),
                          strerror(errno));
                }
                return TTYResponse::PortFailure;
            }

            // 清除O_NONBLOCK标志以进行阻塞I/O
            int flags = fcntl(tFd, F_GETFL, 0);
            if (flags == -1 || fcntl(tFd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
                if (m_Debug) {
                    LOG_F(ERROR, "Error clearing O_NONBLOCK flag: {}",
                          strerror(errno));
                }
                close(tFd);
                return TTYResponse::PortFailure;
            }

            termios ttySetting{};
            if (tcgetattr(tFd, &ttySetting) == -1) {
                if (m_Debug) {
                    LOG_F(ERROR, "Error getting {} tty attributes: {}",
                          device.data(), strerror(errno));
                }
                close(tFd);
                return TTYResponse::PortFailure;
            }

            speed_t bps;
            switch (bitRate) {
                case 0:
                    bps = B0;
                    break;
                case 50:
                    bps = B50;
                    break;
                case 75:
                    bps = B75;
                    break;
                case 110:
                    bps = B110;
                    break;
                case 134:
                    bps = B134;
                    break;
                case 150:
                    bps = B150;
                    break;
                case 200:
                    bps = B200;
                    break;
                case 300:
                    bps = B300;
                    break;
                case 600:
                    bps = B600;
                    break;
                case 1200:
                    bps = B1200;
                    break;
                case 1800:
                    bps = B1800;
                    break;
                case 2400:
                    bps = B2400;
                    break;
                case 4800:
                    bps = B4800;
                    break;
                case 9600:
                    bps = B9600;
                    break;
                case 19200:
                    bps = B19200;
                    break;
                case 38400:
                    bps = B38400;
                    break;
                case 57600:
                    bps = B57600;
                    break;
                case 115200:
                    bps = B115200;
                    break;
                case 230400:
                    bps = B230400;
                    break;
                default:
                    if (m_Debug) {
                        LOG_F(ERROR, "connect: {} is not a valid bit rate.",
                              bitRate);
                    }
                    close(tFd);
                    return TTYResponse::ParamError;
            }

            if ((cfsetispeed(&ttySetting, bps) < 0) ||
                (cfsetospeed(&ttySetting, bps) < 0)) {
                if (m_Debug) {
                    LOG_F(ERROR, "connect: failed setting bit rate: {}",
                          strerror(errno));
                }
                close(tFd);
                return TTYResponse::PortFailure;
            }

            ttySetting.c_cflag &=
                ~(CSIZE | CSTOPB | PARENB | PARODD | HUPCL | CRTSCTS);
            ttySetting.c_cflag |= (CLOCAL | CREAD);

            switch (wordSize) {
                case 5:
                    ttySetting.c_cflag |= CS5;
                    break;
                case 6:
                    ttySetting.c_cflag |= CS6;
                    break;
                case 7:
                    ttySetting.c_cflag |= CS7;
                    break;
                case 8:
                    ttySetting.c_cflag |= CS8;
                    break;
                default:
                    if (m_Debug) {
                        LOG_F(ERROR,
                              "connect: {} is not a valid data bit count.",
                              wordSize);
                    }
                    close(tFd);
                    return TTYResponse::ParamError;
            }

            if (parity == 1) {
                ttySetting.c_cflag |= PARENB;
            } else if (parity == 2) {
                ttySetting.c_cflag |= PARENB | PARODD;
            }

            if (stopBits == 2) {
                ttySetting.c_cflag |= CSTOPB;
            }

            ttySetting.c_iflag &= ~(PARMRK | ISTRIP | IGNCR | ICRNL | INLCR |
                                    IXOFF | IXON | IXANY);
            ttySetting.c_iflag |= INPCK | IGNPAR | IGNBRK;

            /* 原始输出 */
            ttySetting.c_oflag &= ~(OPOST | ONLCR);

            ttySetting.c_lflag &=
                ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN | NOFLSH | TOSTOP);
            ttySetting.c_lflag |= NOFLSH;

            ttySetting.c_cc[VMIN] = 1;
            ttySetting.c_cc[VTIME] = 0;

            tcflush(tFd, TCIOFLUSH);

            cfmakeraw(&ttySetting);

            // 应用设置
            if (tcsetattr(tFd, TCSANOW, &ttySetting) != 0) {
                if (m_Debug) {
                    LOG_F(ERROR, "Failed to set terminal attributes: {}",
                          strerror(errno));
                }
                close(tFd);
                return TTYResponse::PortFailure;
            }

            m_PortFD = tFd;

            // 如果尚未运行，则启动异步读取线程
            startAsyncOperations();

            return TTYResponse::OK;
#endif
        } catch (const std::invalid_argument& e) {
            if (m_Debug) {
                LOG_F(ERROR, "Invalid argument during connect: {}", e.what());
            }
            return TTYResponse::ParamError;
        } catch (const std::system_error& e) {
            if (m_Debug) {
                LOG_F(ERROR, "System error during connect: {}", e.what());
            }
            return TTYResponse::Errno;
        } catch (const std::exception& e) {
            if (m_Debug) {
                LOG_F(ERROR, "Exception during connect: {}", e.what());
            }
            return TTYResponse::PortFailure;
        }
    }

    TTYResponse disconnect() noexcept {
        try {
            stopAsyncOperations();

            if (m_PortFD == -1) {
                return TTYResponse::OK;  // 已经断开连接
            }

#ifdef _WIN32
            // Windows特定断开连接，带有错误处理
            HANDLE hPort = reinterpret_cast<HANDLE>(m_PortFD);
            if (!CloseHandle(hPort)) {
                auto error = GetLastError();
                if (m_Debug) {
                    LOG_F(ERROR, "Error closing handle: {}", error);
                }
                return TTYResponse::Errno;
            }

            m_PortFD = -1;
            return TTYResponse::OK;
#else
            // 刷新任何待处理的数据
            tcflush(m_PortFD, TCIOFLUSH);

            if (close(m_PortFD) != 0) {
                if (m_Debug) {
                    LOG_F(ERROR, "Error closing port: {}", strerror(errno));
                }
                return TTYResponse::Errno;
            }

            m_PortFD = -1;
            return TTYResponse::OK;
#endif
        } catch (const std::exception& e) {
            if (m_Debug) {
                LOG_F(ERROR, "Exception during disconnect: {}", e.what());
            }
            return TTYResponse::Errno;
        }
    }

    void setDebug(bool enabled) noexcept {
        m_Debug = enabled;
        if (m_Debug)
            LOG_F(INFO, "Debugging enabled for {}", m_DriverName);
        else
            LOG_F(INFO, "Debugging disabled for {}", m_DriverName);
    }

    std::string getErrorMessage(TTYResponse code) const noexcept {
        try {
            switch (code) {
                case TTYResponse::OK:
                    return "No error";
                case TTYResponse::ReadError:
                    return "Read error: " + std::string(strerror(errno));
                case TTYResponse::WriteError:
                    return "Write error: " + std::string(strerror(errno));
                case TTYResponse::SelectError:
                    return "Select error: " + std::string(strerror(errno));
                case TTYResponse::Timeout:
                    return "Timeout error";
                case TTYResponse::PortFailure:
                    if (errno == EACCES) {
                        return "Port failure: Access denied. Try adding your "
                               "user "
                               "to the dialout group and restart "
                               "(sudo adduser $USER dialout)";
                    } else {
                        return "Port failure: " + std::string(strerror(errno)) +
                               ". Check if device is connected to this port.";
                    }
                case TTYResponse::ParamError:
                    return "Parameter error";
                case TTYResponse::Errno:
                    return "Error: " + std::string(strerror(errno));
                case TTYResponse::Overflow:
                    return "Read overflow error";
                default:
                    return "Unknown error";
            }
        } catch (...) {
            return "Error retrieving error message";
        }
    }

    int getPortFD() const noexcept { return m_PortFD; }

    bool isConnected() const noexcept { return m_PortFD != -1; }

    void startAsyncOperations() {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (m_IsRunning.load() || m_PortFD == -1) {
            return;
        }

        m_IsRunning.store(true);
        m_ShouldExit.store(false);

        // Start worker thread
        m_WorkerThread = std::thread([this]() {
            std::vector<uint8_t> buffer(m_ReadBufferSize);

            while (!m_ShouldExit.load()) {
                if (m_PortFD == -1) {
                    // Port closed, sleep and try again
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                // Check if data is available
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(m_PortFD, &readSet);

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 100000;  // 100ms timeout

                int result =
                    select(m_PortFD + 1, &readSet, nullptr, nullptr, &tv);

                if (result > 0) {
                    // Data available
                    uint32_t bytesRead = 0;
                    TTYResponse response = read(buffer, 0, bytesRead);

                    if (response == TTYResponse::OK && bytesRead > 0) {
                        // Process data
                        if (m_DataCallback) {
                            // Call callback directly
                            m_DataCallback(buffer, bytesRead);
                        } else {
                            // Queue data for later processing
                            std::vector<uint8_t> data(
                                buffer.begin(), buffer.begin() + bytesRead);
                            {
                                std::lock_guard<std::mutex> asyncLock(
                                    m_AsyncMutex);
                                m_DataQueue.push(std::move(data));
                            }
                            m_AsyncCV.notify_one();
                        }
                    }
                } else if (result < 0 && errno != EINTR) {
                    // Error occurred
                    if (m_Debug) {
                        LOG_F(ERROR, "Async read select error: {}",
                              strerror(errno));
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });

        if (m_Debug) {
            LOG_F(INFO, "Started async operations for {}", m_DriverName);
        }
    }

    void stopAsyncOperations() {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (!m_IsRunning.load()) {
            return;
        }

        m_ShouldExit.store(true);
        m_IsRunning.store(false);

        if (m_WorkerThread.joinable()) {
            m_WorkerThread.join();
        }

        // Clear data queue
        {
            std::lock_guard<std::mutex> asyncLock(m_AsyncMutex);
            std::queue<std::vector<uint8_t>> empty;
            std::swap(m_DataQueue, empty);
        }

        if (m_Debug) {
            LOG_F(INFO, "Stopped async operations for {}", m_DriverName);
        }
    }

    void setDataCallback(
        std::function<void(const std::vector<uint8_t>&, size_t)> callback) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_DataCallback = std::move(callback);
    }

    bool getQueuedData(std::vector<uint8_t>& data,
                       std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_AsyncMutex);

        if (m_DataQueue.empty()) {
            // Wait for data with timeout
            auto result = m_AsyncCV.wait_for(lock, timeout, [this]() {
                return !m_DataQueue.empty() || !m_IsRunning.load();
            });

            if (!result || !m_IsRunning.load()) {
                return false;
            }
        }

        if (!m_DataQueue.empty()) {
            data = std::move(m_DataQueue.front());
            m_DataQueue.pop();
            return true;
        }

        return false;
    }

    void setReadBufferSize(size_t size) {
        if (size > 0) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_ReadBufferSize = size;
        }
    }

    // 成员变量
    int m_PortFD{-1};          ///< File descriptor for TTY port
    bool m_Debug{false};       ///< Flag indicating if debugging is enabled
    std::string m_DriverName;  ///< Driver name for this TTY
    std::atomic<bool>
        m_IsRunning;  ///< Flag indicating if async operations are running

    // Mutex for thread safety
    std::mutex m_Mutex;

    // New members for async operations
    std::thread m_WorkerThread;
    std::atomic<bool> m_ShouldExit{false};
    std::function<void(const std::vector<uint8_t>&, size_t)> m_DataCallback;
    std::condition_variable m_AsyncCV;
    std::mutex m_AsyncMutex;
    std::queue<std::vector<uint8_t>> m_DataQueue;
    size_t m_ReadBufferSize{1024};
};

// TTYBase类的实现，通过委托到Impl类

TTYBase::TTYBase(std::string_view driverName)
    : m_pImpl(std::make_unique<Impl>(driverName)) {}

TTYBase::~TTYBase() = default;

TTYBase::TTYBase(TTYBase&& other) noexcept = default;
TTYBase& TTYBase::operator=(TTYBase&& other) noexcept = default;

TTYBase::TTYResponse TTYBase::read(std::span<uint8_t> buffer, uint8_t timeout,
                                   uint32_t& nbytesRead) {
    return m_pImpl->read(buffer, timeout, nbytesRead);
}

TTYBase::TTYResponse TTYBase::readSection(std::span<uint8_t> buffer,
                                          uint8_t stopByte, uint8_t timeout,
                                          uint32_t& nbytesRead) {
    return m_pImpl->readSection(buffer, stopByte, timeout, nbytesRead);
}

TTYBase::TTYResponse TTYBase::write(std::span<const uint8_t> buffer,
                                    uint32_t& nbytesWritten) {
    return m_pImpl->write(buffer, nbytesWritten);
}

TTYBase::TTYResponse TTYBase::writeString(std::string_view string,
                                          uint32_t& nbytesWritten) {
    return m_pImpl->write(
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(string.data()), string.size()),
        nbytesWritten);
}

std::future<std::pair<TTYBase::TTYResponse, uint32_t>> TTYBase::readAsync(
    std::span<uint8_t> buffer, uint8_t timeout) {
    return std::async(std::launch::async, [this, buffer, timeout]() {
        uint32_t bytesRead = 0;
        TTYResponse response = this->read(buffer, timeout, bytesRead);
        return std::make_pair(response, bytesRead);
    });
}

std::future<std::pair<TTYBase::TTYResponse, uint32_t>> TTYBase::writeAsync(
    std::span<const uint8_t> buffer) {
    return std::async(std::launch::async, [this, buffer]() {
        uint32_t bytesWritten = 0;
        TTYResponse response = this->write(buffer, bytesWritten);
        return std::make_pair(response, bytesWritten);
    });
}

TTYBase::TTYResponse TTYBase::connect(std::string_view device, uint32_t bitRate,
                                      uint8_t wordSize, uint8_t parity,
                                      uint8_t stopBits) {
    return m_pImpl->connect(device, bitRate, wordSize, parity, stopBits);
}

TTYBase::TTYResponse TTYBase::disconnect() noexcept {
    return m_pImpl->disconnect();
}

void TTYBase::setDebug(bool enabled) noexcept { m_pImpl->setDebug(enabled); }

std::string TTYBase::getErrorMessage(TTYResponse code) const noexcept {
    return m_pImpl->getErrorMessage(code);
}

int TTYBase::getPortFD() const noexcept { return m_pImpl->getPortFD(); }

bool TTYBase::isConnected() const noexcept { return m_pImpl->isConnected(); }