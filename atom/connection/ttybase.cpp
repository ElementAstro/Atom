#include "ttybase.hpp"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <system_error>

// Windows和POSIX特定的头文件
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "atom/log/loguru.hpp"

/**
 * @class TTYBase::Impl
 * @brief TTYBase类的私有实现
 *
 * 这个类包含所有实际的实现细节，对库用户隐藏
 */
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
            // 尽力清理，不从析构函数抛出异常
        }
    }

    TTYResponse checkTimeout(uint8_t timeout) {
        if (m_PortFD == -1) {
            return TTYResponse::Errno;
        }

#ifdef _WIN32
        // Windows特定实现
        COMMTIMEOUTS timeouts = {0};
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
                                        "无效的端口描述符");
            }

            nbytesRead = 0;
            const uint32_t nbytes = static_cast<uint32_t>(buffer.size());

#ifdef _WIN32
            // Windows特定读取实现，具有适当的错误处理
            DWORD bytesRead = 0;
            HANDLE hPort = reinterpret_cast<HANDLE>(m_PortFD);

            // 设置超时
            COMMTIMEOUTS timeouts = {0};
            timeouts.ReadIntervalTimeout = timeout * 1000;
            timeouts.ReadTotalTimeoutConstant = timeout * 1000;

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
                        // 中断的系统调用，重试
                        continue;
                    }
                    if (m_Debug) {
                        LOG_F(ERROR, "Read error: {}", strerror(errno));
                    }
                    return TTYResponse::ReadError;
                }

                if (bytesRead == 0) {
                    // 达到文件结尾
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
                                        "无效的端口描述符");
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
                        // 中断的系统调用，重试
                        continue;
                    }
                    return TTYResponse::ReadError;
                }

                if (bytesRead == 0) {
                    // 达到文件结尾
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
                                        "无效的端口描述符");
            }

#ifdef _WIN32
            // Windows特定写入实现，具有适当的错误处理
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
            // 验证参数
            if (device.empty()) {
                throw std::invalid_argument("设备名称不能为空");
            }

            // 验证字长（5-8位）
            if (wordSize < 5 || wordSize > 8) {
                throw std::invalid_argument("字长必须在5到8位之间");
            }

            // 验证奇偶校验（0=无, 1=偶, 2=奇）
            if (parity > 2) {
                throw std::invalid_argument("无效的奇偶校验值");
            }

            // 验证停止位（1或2）
            if (stopBits != 1 && stopBits != 2) {
                throw std::invalid_argument("停止位必须为1或2");
            }

#ifdef _WIN32
            // Windows特定实现，具有适当的错误处理
            std::string devicePath(device);

            // 如果设备没有以"\\.\"前缀开始，对于高于COM9的COM端口
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

            DCB dcbSerialParams = {0};
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

            // 设置奇偶校验
            switch (parity) {
                case 0:  // 无
                    dcbSerialParams.Parity = NOPARITY;
                    break;
                case 1:  // 偶
                    dcbSerialParams.Parity = EVENPARITY;
                    break;
                case 2:  // 奇
                    dcbSerialParams.Parity = ODDPARITY;
                    break;
            }

            // 禁用流控制
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
            COMMTIMEOUTS timeouts = {0};
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

            // 将bitRate映射到POSIX速度常量
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

            // 设置波特率
            if ((cfsetispeed(&ttySetting, bps) < 0) ||
                (cfsetospeed(&ttySetting, bps) < 0)) {
                if (m_Debug) {
                    LOG_F(ERROR, "connect: failed setting bit rate: {}",
                          strerror(errno));
                }
                close(tFd);
                return TTYResponse::PortFailure;
            }

            // 清除所有相关标志
            ttySetting.c_cflag &=
                ~(CSIZE | CSTOPB | PARENB | PARODD | HUPCL | CRTSCTS);
            ttySetting.c_cflag |= (CLOCAL | CREAD);

            // 设置字长
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

            // 设置奇偶校验
            if (parity == 1) {
                ttySetting.c_cflag |= PARENB;  // 偶校验
            } else if (parity == 2) {
                ttySetting.c_cflag |= PARENB | PARODD;  // 奇校验
            }

            // 设置停止位
            if (stopBits == 2) {
                ttySetting.c_cflag |= CSTOPB;
            }

            /* 忽略奇偶校验错误的字节，并使终端原始和简单 */
            ttySetting.c_iflag &= ~(PARMRK | ISTRIP | IGNCR | ICRNL | INLCR |
                                    IXOFF | IXON | IXANY);
            ttySetting.c_iflag |= INPCK | IGNPAR | IGNBRK;

            /* 原始输出 */
            ttySetting.c_oflag &= ~(OPOST | ONLCR);

            /* 本地模式 - 不回显，不生成信号，不处理字符 */
            ttySetting.c_lflag &=
                ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN | NOFLSH | TOSTOP);
            ttySetting.c_lflag |= NOFLSH;

            /* 设置读取超时 */
            ttySetting.c_cc[VMIN] = 1;   // 等待至少1个字符
            ttySetting.c_cc[VTIME] = 0;  // 第一个字符没有超时

            // 刷新所有待处理数据
            tcflush(tFd, TCIOFLUSH);

            // 设置原始输入模式（非规范）
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
                    return "无错误";
                case TTYResponse::ReadError:
                    return "读取错误: " + std::string(strerror(errno));
                case TTYResponse::WriteError:
                    return "写入错误: " + std::string(strerror(errno));
                case TTYResponse::SelectError:
                    return "选择错误: " + std::string(strerror(errno));
                case TTYResponse::Timeout:
                    return "超时错误";
                case TTYResponse::PortFailure:
                    if (errno == EACCES) {
                        return "端口失败：访问被拒绝。尝试将您的用户添加到拨号"
                               "组并重启"
                               "（sudo adduser $USER dialout）";
                    } else {
                        return "端口失败：" + std::string(strerror(errno)) +
                               "。检查设备是否连接到此端口。";
                    }
                case TTYResponse::ParamError:
                    return "参数错误";
                case TTYResponse::Errno:
                    return "错误: " + std::string(strerror(errno));
                case TTYResponse::Overflow:
                    return "读取溢出错误";
                default:
                    return "未知错误";
            }
        } catch (...) {
            return "检索错误消息时出错";
        }
    }

    int getPortFD() const noexcept { return m_PortFD; }

    bool isConnected() const noexcept { return m_PortFD != -1; }

    void startAsyncOperations() {
        if (m_IsRunning.load() || m_PortFD == -1) {
            return;
        }

        m_IsRunning.store(true);

        // 在这里您可以初始化任何异步操作线程
        // 这是潜在实现的存根
    }

    void stopAsyncOperations() {
        if (!m_IsRunning.load()) {
            return;
        }

        m_IsRunning.store(false);

        // 等待任何异步操作完成
        // 这是潜在实现的存根
    }

    // 成员变量
    int m_PortFD{-1};          ///< TTY端口的文件描述符
    bool m_Debug{false};       ///< 指示是否启用调试的标志
    std::string m_DriverName;  ///< 该TTY的驱动程序名称
    std::atomic<bool> m_IsRunning;  ///< 指示异步操作是否正在运行的标志

    // 用于线程安全的互斥锁
    std::mutex m_Mutex;
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