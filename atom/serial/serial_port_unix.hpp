#pragma once

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
#include <shared_mutex>
#include <thread>
#include <condition_variable>
#include "serial_port.hpp"

namespace serial {

/**
 * @brief Unix/Apple 平台的串口实现类
 *
 * 提供串口通信的低级平台特定实现。
 */
class SerialPortImpl {
public:
    /**
     * @brief 默认构造函数
     */
    SerialPortImpl()
        : fd_(-1),
          config_{},
          portName_(),
          asyncReadThread_(),
          stopAsyncRead_(false),
          asyncReadActive_(false) {}

    /**
     * @brief 析构函数
     */
    ~SerialPortImpl() {
        stopAsyncWorker();
        close();
    }

    /**
     * @brief 打开串口
     *
     * @param portName 要打开的串口名称
     * @param config 串口配置
     * @throws SerialException 如果无法打开或配置端口
     */
    void open(std::string_view portName, const SerialConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (isOpen()) {
            close();
        }

        // 尝试打开串口
        fd_ = ::open(std::string(portName).c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

        if (fd_ < 0) {
            throw SerialException("无法打开串口: " + std::string(portName) +
                                 " (错误: " + strerror(errno) + ")");
        }

        // 确保这是一个串口设备
        int isatty_res = isatty(fd_);
        if (isatty_res != 1) {
            ::close(fd_);
            fd_ = -1;
            throw SerialException(std::string(portName) + " 不是有效的串口设备");
        }

        portName_ = portName;
        config_ = config;
        try {
            applyConfig();
        } catch (...) {
            // 如果配置失败，确保端口已关闭
            ::close(fd_);
            fd_ = -1;
            throw;
        }
    }

    /**
     * @brief 关闭串口
     */
    void close() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
            portName_ = "";
        }
    }

    /**
     * @brief 检查串口是否已打开
     *
     * @return true 如果端口已打开
     */
    [[nodiscard]] bool isOpen() const { 
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return fd_ >= 0; 
    }

    /**
     * @brief 从串口读取数据
     *
     * @param maxBytes 要读取的最大字节数
     * @return 从端口读取的数据
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果读取过程中发生错误
     */
    std::vector<uint8_t> read(size_t maxBytes) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (maxBytes == 0) {
            return {};
        }

        // 设置select超时
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_, &readfds);

        struct timeval timeout;
        auto timeoutMs = config_.getReadTimeout().count();
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        int selectResult = select(fd_ + 1, &readfds, nullptr, nullptr, &timeout);

        if (selectResult < 0) {
            throw SerialIOException("读取错误: " + std::string(strerror(errno)));
        } else if (selectResult == 0) {
            // 超时
            return {};
        }

        std::vector<uint8_t> buffer(maxBytes);
        ssize_t bytesRead = ::read(fd_, buffer.data(), maxBytes);

        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下没有可用数据
                return {};
            }
            throw SerialIOException("读取错误: " + std::string(strerror(errno)));
        }

        buffer.resize(bytesRead);
        return buffer;
    }

    /**
     * @brief 精确读取指定字节数
     *
     * @param bytes 要读取的字节数
     * @param timeout 等待数据的最长时间
     * @return 读取的数据
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialTimeoutException 如果操作超时
     * @throws SerialIOException 如果读取过程中发生错误
     */
    std::vector<uint8_t> readExactly(size_t bytes,
                                    std::chrono::milliseconds timeout) {
        checkPortOpen();

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
                throw SerialTimeoutException(
                    "读取" + std::to_string(bytes) + "字节超时，仅读取了" + 
                    std::to_string(result.size()) + "字节");
            }

            // 计算剩余超时时间
            auto remainingTimeout = timeout - elapsed;

            // 临时保存原始超时设置
            const auto originalTimeout = config_.getReadTimeout();

            try {
                // 临时设置超时
                SerialConfig tempConfig = config_;
                tempConfig.withReadTimeout(remainingTimeout);
                
                std::unique_lock<std::shared_mutex> lock(mutex_);
                applyConfigInternal(tempConfig);

                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }

                // 恢复原始超时设置
                applyConfigInternal(config_);
                lock.unlock();
            } catch (const std::exception&) {
                // 确保恢复原始超时设置
                std::unique_lock<std::shared_mutex> lock(mutex_);
                SerialConfig originalConfig = config_;
                originalConfig.withReadTimeout(originalTimeout);
                applyConfigInternal(originalConfig);
                throw;
            }

            // 如果没有读取所有字节，短暂休眠以避免CPU使用率过高
            if (result.size() < bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        return result;
    }

    /**
     * @brief 启动异步读取操作
     *
     * @param maxBytes 每次读取的最大字节数
     * @param callback 当数据可用时调用的回调函数
     * @throws SerialPortNotOpenException 如果端口未打开
     */
    void asyncRead(size_t maxBytes,
                  std::function<void(std::vector<uint8_t>)> callback) {
        checkPortOpen();
        stopAsyncWorker();

        stopAsyncRead_ = false;
        asyncReadActive_ = true;
        asyncReadThread_ = std::thread([this, maxBytes, callback = std::move(callback)]() {
            try {
                while (!stopAsyncRead_) {
                    try {
                        auto data = read(maxBytes);
                        if (!data.empty() && !stopAsyncRead_) {
                            callback(std::move(data));
                        }
                    } catch (const SerialTimeoutException&) {
                        // 超时是正常的，继续
                    } catch (const std::exception& e) {
                        if (!stopAsyncRead_) {
                            std::cerr << "串口异步读取错误: " << e.what() << std::endl;
                            break;
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } catch (...) {
                // 捕获所有异常，确保asyncReadActive_被设置为false
            }
            
            {
                std::unique_lock<std::mutex> lock(asyncMutex_);
                asyncReadActive_ = false;
                asyncCv_.notify_all();
            }
        });
    }

    /**
     * @brief 读取所有可用字节
     *
     * @return 从端口读取的数据
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果读取过程中发生错误
     */
    std::vector<uint8_t> readAvailable() {
        checkPortOpen();

        int availableBytes = available();
        if (availableBytes == 0) {
            return {};
        }

        return read(availableBytes);
    }

    /**
     * @brief 向串口写入数据
     *
     * @param data 要写入的数据
     * @return 实际写入的字节数
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialTimeoutException 如果操作超时
     * @throws SerialIOException 如果写入过程中发生错误
     */
    size_t write(std::span<const uint8_t> data) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (data.empty()) {
            return 0;
        }

        // 设置select超时
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(fd_, &writefds);

        struct timeval timeout;
        auto timeoutMs = config_.getWriteTimeout().count();
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        int selectResult = select(fd_ + 1, nullptr, &writefds, nullptr, &timeout);

        if (selectResult < 0) {
            throw SerialIOException("写入错误: " + std::string(strerror(errno)));
        } else if (selectResult == 0) {
            throw SerialTimeoutException();
        }

        ssize_t bytesWritten = ::write(fd_, data.data(), data.size());

        if (bytesWritten < 0) {
            throw SerialIOException("写入错误: " + std::string(strerror(errno)));
        }

        return static_cast<size_t>(bytesWritten);
    }

    /**
     * @brief 清除输入和输出缓冲区
     *
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    void flush() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (tcflush(fd_, TCIOFLUSH) != 0) {
            throw SerialIOException("无法刷新串口缓冲区: " +
                                   std::string(strerror(errno)));
        }
    }

    /**
     * @brief 等待所有输出数据被传输
     *
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    void drain() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (tcdrain(fd_) != 0) {
            throw SerialIOException("无法完成缓冲区写入: " +
                                   std::string(strerror(errno)));
        }
    }

    /**
     * @brief 获取可用字节数
     *
     * @return 输入缓冲区中的字节数
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    size_t available() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        int bytes = 0;

        if (ioctl(fd_, FIONREAD, &bytes) < 0) {
            throw SerialIOException("无法获取可用字节数: " +
                                   std::string(strerror(errno)));
        }

        return static_cast<size_t>(bytes);
    }

    /**
     * @brief 更新串口配置
     *
     * @param config 新配置
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果配置失败
     */
    void setConfig(const SerialConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        config_ = config;
        applyConfigInternal(config);
    }

    /**
     * @brief 获取当前串口配置
     *
     * @return 当前配置
     */
    [[nodiscard]] SerialConfig getConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_;
    }

    /**
     * @brief 设置DTR信号
     *
     * @param value 信号状态
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    void setDTR(bool value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            throw SerialIOException("无法获取串口状态: " +
                                   std::string(strerror(errno)));
        }

        if (value) {
            status |= TIOCM_DTR;
        } else {
            status &= ~TIOCM_DTR;
        }

        if (ioctl(fd_, TIOCMSET, &status) < 0) {
            throw SerialIOException("无法设置DTR信号: " +
                                   std::string(strerror(errno)));
        }
    }

    /**
     * @brief 设置RTS信号
     *
     * @param value 信号状态
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    void setRTS(bool value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            throw SerialIOException("无法获取串口状态: " +
                                   std::string(strerror(errno)));
        }

        if (value) {
            status |= TIOCM_RTS;
        } else {
            status &= ~TIOCM_RTS;
        }

        if (ioctl(fd_, TIOCMSET, &status) < 0) {
            throw SerialIOException("无法设置RTS信号: " +
                                   std::string(strerror(errno)));
        }
    }

    /**
     * @brief 获取CTS信号状态
     *
     * @return 信号状态
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    [[nodiscard]] bool getCTS() const { return getModemStatus(TIOCM_CTS); }

    /**
     * @brief 获取DSR信号状态
     *
     * @return 信号状态
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    [[nodiscard]] bool getDSR() const { return getModemStatus(TIOCM_DSR); }

    /**
     * @brief 获取RI信号状态
     *
     * @return 信号状态
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    [[nodiscard]] bool getRI() const { return getModemStatus(TIOCM_RI); }

    /**
     * @brief 获取CD信号状态
     *
     * @return 信号状态
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    [[nodiscard]] bool getCD() const { return getModemStatus(TIOCM_CD); }

    /**
     * @brief 获取当前端口名称
     *
     * @return 端口名称
     */
    [[nodiscard]] std::string getPortName() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return portName_;
    }

    /**
     * @brief 获取系统上可用的串口列表
     *
     * @return 可用串口列表
     */
    static std::vector<std::string> getAvailablePorts() {
        std::vector<std::string> result;

#if defined(__linux__)
        // Linux: 检查 /dev/ttyS*, /dev/ttyUSB*, /dev/ttyACM*, /dev/ttyAMA*
        const std::vector<std::string> patterns = {
            "/dev/ttyS[0-9]*", "/dev/ttyUSB[0-9]*", "/dev/ttyACM[0-9]*",
            "/dev/ttyAMA[0-9]*"};

        for (const auto& pattern : patterns) {
            std::regex regPattern(pattern);

            // 使用C++17的文件系统API
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator("/dev", ec)) {
                if (ec) {
                    continue; // 如果有错误，跳过此条目
                }
                
                std::string path = entry.path().string();
                if (std::regex_match(path, regPattern)) {
                    result.push_back(path);
                }
            }
        }
#elif defined(__APPLE__)
        // macOS: 检查 /dev/tty.* 和 /dev/cu.*
        const std::vector<std::string> patterns = {"/dev/tty\\..*",
                                                 "/dev/cu\\..*"};

        for (const auto& pattern : patterns) {
            std::regex regPattern(pattern);

            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator("/dev", ec)) {
                if (ec) {
                    continue;
                }
                
                std::string path = entry.path().string();
                if (std::regex_match(path, regPattern)) {
                    result.push_back(path);
                }
            }
        }
#else
        // 其他Unix系统
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
    int fd_;                           ///< 串口文件描述符
    SerialConfig config_;              ///< 当前配置
    std::string portName_;             ///< 已打开端口的名称
    mutable std::shared_mutex mutex_;  ///< 线程安全的读写互斥锁
    std::thread asyncReadThread_;      ///< 异步读取线程
    std::atomic<bool> stopAsyncRead_;  ///< 停止异步读取的标志
    std::atomic<bool> asyncReadActive_; ///< 异步读取活动标志
    std::mutex asyncMutex_;            ///< 异步操作互斥锁
    std::condition_variable asyncCv_;  ///< 用于异步操作的条件变量

    /**
     * @brief 应用当前配置到串口
     *
     * @throws SerialIOException 如果配置应用失败
     */
    void applyConfig() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        applyConfigInternal(config_);
    }

    /**
     * @brief 检查端口是否打开
     * 
     * @throws SerialPortNotOpenException 如果端口未打开
     */
    void checkPortOpen() const {
        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }
    }

    /**
     * @brief 内部应用配置方法（无锁）
     *
     * @param config 要应用的配置
     * @throws SerialIOException 如果配置应用失败
     */
    void applyConfigInternal(const SerialConfig& config) {
        if (fd_ < 0) {
            return;
        }

        struct termios tty;

        if (tcgetattr(fd_, &tty) != 0) {
            throw SerialIOException("无法获取串口配置: " +
                                  std::string(strerror(errno)));
        }

        // 设置波特率
        speed_t baudRate = getBaudRateConstant(config.getBaudRate());
        cfsetispeed(&tty, baudRate);
        cfsetospeed(&tty, baudRate);

        // 设置基本模式：非规范、无回显、无特殊处理
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        tty.c_oflag &= ~OPOST;
        tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        tty.c_cflag |= (CLOCAL | CREAD);

        // 数据位
        tty.c_cflag &= ~CSIZE;
        switch (config.getDataBits()) {
            case 5: tty.c_cflag |= CS5; break;
            case 6: tty.c_cflag |= CS6; break;
            case 7: tty.c_cflag |= CS7; break;
            case 8:
            default: tty.c_cflag |= CS8; break;
        }

        // 奇偶校验
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
                // Mark奇偶校验在大多数POSIX系统上不支持
                throw SerialException("Mark奇偶校验在POSIX系统上不支持");
                break;
            case SerialConfig::Parity::Space:
                // Space奇偶校验在大多数POSIX系统上不支持
                throw SerialException("Space奇偶校验在POSIX系统上不支持");
                break;
        }

        // 停止位
        switch (config.getStopBits()) {
            case SerialConfig::StopBits::One:
                tty.c_cflag &= ~CSTOPB;
                break;
            case SerialConfig::StopBits::Two:
                tty.c_cflag |= CSTOPB;
                break;
            case SerialConfig::StopBits::OnePointFive:
                // POSIX没有1.5停止位的定义，通常使用2位代替
                tty.c_cflag |= CSTOPB;
                break;
        }

        // 流控制
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

        // 设置为非阻塞读取
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;

        // 应用配置
        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            throw SerialIOException("无法设置串口配置: " +
                                  std::string(strerror(errno)));
        }
    }

    /**
     * @brief 将整数波特率转换为termios常量
     *
     * @param baudRate 整数波特率
     * @return 对应的termios波特率常量
     * @throws SerialException 如果波特率不支持
     */
    [[nodiscard]] speed_t getBaudRateConstant(int baudRate) const {
        switch (baudRate) {
            case 50: return B50;
            case 75: return B75;
            case 110: return B110;
            case 134: return B134;
            case 150: return B150;
            case 200: return B200;
            case 300: return B300;
            case 600: return B600;
            case 1200: return B1200;
            case 1800: return B1800;
            case 2400: return B2400;
            case 4800: return B4800;
            case 9600: return B9600;
            case 19200: return B19200;
            case 38400: return B38400;
            case 57600: return B57600;
            case 115200: return B115200;
            case 230400: return B230400;
#ifdef B460800
            case 460800: return B460800;
#endif
#ifdef B500000
            case 500000: return B500000;
#endif
#ifdef B576000
            case 576000: return B576000;
#endif
#ifdef B921600
            case 921600: return B921600;
#endif
#ifdef B1000000
            case 1000000: return B1000000;
#endif
#ifdef B1152000
            case 1152000: return B1152000;
#endif
#ifdef B1500000
            case 1500000: return B1500000;
#endif
#ifdef B2000000
            case 2000000: return B2000000;
#endif
#ifdef B2500000
            case 2500000: return B2500000;
#endif
#ifdef B3000000
            case 3000000: return B3000000;
#endif
#ifdef B3500000
            case 3500000: return B3500000;
#endif
#ifdef B4000000
            case 4000000: return B4000000;
#endif
            default:
                throw SerialException("不支持的波特率: " + std::to_string(baudRate));
        }
    }

    /**
     * @brief 获取调制解调器控制线状态
     *
     * @param flag 要检查的调制解调器状态标志
     * @return 如果标志已设置则为true，否则为false
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果获取状态失败
     */
    [[nodiscard]] bool getModemStatus(int flag) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        int status;
        if (ioctl(fd_, TIOCMGET, &status) < 0) {
            throw SerialIOException("无法获取调制解调器状态: " +
                                  std::string(strerror(errno)));
        }

        return (status & flag) != 0;
    }

    /**
     * @brief 停止异步读取工作线程
     *
     * 通知工作线程停止并等待其完成。
     */
    void stopAsyncWorker() {
        if (asyncReadThread_.joinable()) {
            stopAsyncRead_ = true;
            
            {
                std::unique_lock<std::mutex> lock(asyncMutex_);
                // 等待异步读取线程真正结束
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