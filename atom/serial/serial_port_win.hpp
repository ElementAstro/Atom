#pragma once

#ifdef _WIN32

#include <Strsafe.h>
#include <Windows.h>
#include <devguid.h>
#include <initguid.h>
#include <setupapi.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include "serial_port.hpp"

#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#endif

namespace serial {

/**
 * @brief Windows平台的串口实现类
 *
 * 该类提供了Windows平台上串口通信的特定实现，使用Win32 API。
 */
class SerialPortImpl {
public:
    /**
     * @brief 默认构造函数
     *
     * 使用无效句柄和默认设置初始化一个新实例。
     */
    SerialPortImpl()
        : handle_(INVALID_HANDLE_VALUE),
          config_{},
          portName_(),
          asyncReadThread_(),
          stopAsyncRead_(false),
          asyncReadActive_(false) {}

    /**
     * @brief 析构函数
     *
     * 停止所有异步读取操作并关闭已打开的端口。
     */
    ~SerialPortImpl() {
        stopAsyncWorker();
        close();
    }

    /**
     * @brief 打开具有指定配置的串口
     *
     * @param portName 串口名称（如"COM1"）
     * @param config 串口配置
     * @throws SerialException 无法打开或配置端口时抛出
     */
    void open(std::string_view portName, const SerialConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (isOpen())
            close();

        // Windows 需要 \\.\\ 前缀
        std::string fullPortName = std::string(portName);
        if (fullPortName.substr(0, 4) != "\\\\.\\") {
            fullPortName = "\\\\.\\" + fullPortName;
        }

        handle_ =
            CreateFileA(fullPortName.c_str(), GENERIC_READ | GENERIC_WRITE,
                        0,                      // 不共享
                        nullptr,                // 默认安全属性
                        OPEN_EXISTING,          // 串口必须存在
                        FILE_ATTRIBUTE_NORMAL,  // 正常文件属性
                        nullptr                 // 无模板
            );

        if (handle_ == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            std::string errorMsg = getLastErrorAsString(error);
            throw SerialException("无法打开串口: " + std::string(portName) +
                                  " (错误: " + errorMsg + ")");
        }

        portName_ = portName;
        config_ = config;

        try {
            applyConfig();
        } catch (...) {
            // 如果配置失败，确保端口已关闭
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            throw;
        }
    }

    /**
     * @brief 关闭已打开的串口
     */
    void close() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            portName_ = "";
        }
    }

    /**
     * @brief 检查串口当前是否打开
     *
     * @return true 如果端口已打开，否则false
     */
    [[nodiscard]] bool isOpen() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return handle_ != INVALID_HANDLE_VALUE;
    }

    /**
     * @brief 从串口读取数据
     *
     * @param maxBytes 最大读取字节数
     * @return 从端口读取的字节向量
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialTimeoutException 如果读取超时
     * @throws SerialIOException 如果读取错误
     */
    std::vector<uint8_t> read(size_t maxBytes) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (maxBytes == 0) {
            return {};
        }

        std::vector<uint8_t> buffer(maxBytes);
        DWORD bytesRead = 0;

        if (!ReadFile(handle_, buffer.data(), static_cast<DWORD>(maxBytes),
                      &bytesRead, nullptr)) {
            DWORD error = GetLastError();
            if (error == ERROR_TIMEOUT) {
                throw SerialTimeoutException();
            } else {
                throw SerialIOException("读取错误: " +
                                        getLastErrorAsString(error));
            }
        }

        buffer.resize(bytesRead);
        return buffer;
    }

    /**
     * @brief 精确读取指定数量的字节，带超时
     *
     * @param bytes 要读取的字节数
     * @param timeout 等待请求字节的最长时间
     * @return 包含请求字节数的向量
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialTimeoutException 如果在读取所有字节之前超时
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

            auto remainingTimeout = timeout - elapsed;

            // 保存原始超时设置
            COMMTIMEOUTS originalTimeouts;
            GetCommTimeouts(handle_, &originalTimeouts);

            // 设置临时超时
            COMMTIMEOUTS tempTimeouts{};
            tempTimeouts.ReadIntervalTimeout = MAXDWORD;
            tempTimeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
            tempTimeouts.ReadTotalTimeoutConstant =
                static_cast<DWORD>(remainingTimeout.count());
            SetCommTimeouts(handle_, &tempTimeouts);

            try {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }

                // 恢复原始超时设置
                SetCommTimeouts(handle_, &originalTimeouts);
            } catch (...) {
                // 恢复原始超时设置并重新抛出异常
                SetCommTimeouts(handle_, &originalTimeouts);
                throw;
            }

            // 如果没有读取所有请求的数据，短暂休眠以避免CPU使用率过高
            if (result.size() < bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        return result;
    }

    /**
     * @brief 设置串口异步读取
     *
     * @param maxBytes 每次操作读取的最大字节数
     * @param callback 用读取数据调用的函数
     * @throws SerialPortNotOpenException 如果端口未打开
     */
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback) {
        checkPortOpen();
        stopAsyncWorker();

        stopAsyncRead_ = false;
        asyncReadActive_ = true;
        asyncReadThread_ = std::thread([this, maxBytes,
                                        callback = std::move(callback)]() {
            try {
                while (!stopAsyncRead_) {
                    try {
                        auto data = read(maxBytes);
                        if (!data.empty() && !stopAsyncRead_) {
                            callback(std::move(data));
                        }
                    } catch (const SerialTimeoutException&) {
                        // 超时正常，继续
                    } catch (const std::exception& e) {
                        if (!stopAsyncRead_) {
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
     * @brief 读取串口的所有可用数据
     *
     * @return 包含所有可用字节的向量
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果获取端口状态错误
     */
    std::vector<uint8_t> readAvailable() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        DWORD errors;
        COMSTAT comStat;

        if (!ClearCommError(handle_, &errors, &comStat)) {
            throw SerialIOException("无法获取串口状态: " +
                                    getLastErrorAsString(GetLastError()));
        }

        if (comStat.cbInQue == 0) {
            return {};
        }

        return read(comStat.cbInQue);
    }

    /**
     * @brief 向串口写入数据
     *
     * @param data 要写入端口的数据
     * @return 实际写入的字节数
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialTimeoutException 如果写入超时
     * @throws SerialIOException 如果写入错误
     */
    size_t write(std::span<const uint8_t> data) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (data.empty()) {
            return 0;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()),
                       &bytesWritten, nullptr)) {
            DWORD error = GetLastError();
            if (error == ERROR_TIMEOUT) {
                throw SerialTimeoutException();
            } else {
                throw SerialIOException("写入错误: " +
                                        getLastErrorAsString(error));
            }
        }

        return bytesWritten;
    }

    /**
     * @brief 清空输入和输出缓冲区
     *
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果清空操作失败
     */
    void flush() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (!PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
            throw SerialIOException("无法清空串口缓冲区: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief 等待所有传输的数据发送完成
     *
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果排空操作失败
     */
    void drain() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        // 在Windows中，FlushFileBuffers相当于排空操作
        if (!FlushFileBuffers(handle_)) {
            throw SerialIOException("无法完成缓冲区写入: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief 获取可读取的字节数
     *
     * @return 输入缓冲区中的字节数
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果获取端口状态错误
     */
    [[nodiscard]] size_t available() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        DWORD errors;
        COMSTAT comStat;

        if (!ClearCommError(handle_, &errors, &comStat)) {
            throw SerialIOException("无法获取串口状态: " +
                                    getLastErrorAsString(GetLastError()));
        }

        return comStat.cbInQue;
    }

    /**
     * @brief 设置串口配置
     *
     * @param config 要应用的新配置
     * @throws SerialPortNotOpenException 如果端口未打开
     */
    void setConfig(const SerialConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        config_ = config;
        applyConfigInternal();
    }

    /**
     * @brief 获取当前串口配置
     *
     * @return 当前串口配置
     */
    [[nodiscard]] SerialConfig getConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_;
    }

    /**
     * @brief 设置DTR（数据终端就绪）信号
     *
     * @param value True表示断言DTR，False表示清除
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    void setDTR(bool value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        DWORD func = value ? SETDTR : CLRDTR;
        if (!EscapeCommFunction(handle_, func)) {
            throw SerialIOException("无法设置DTR信号: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief 设置RTS（请求发送）信号
     *
     * @param value True表示断言RTS，False表示清除
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    void setRTS(bool value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        DWORD func = value ? SETRTS : CLRRTS;
        if (!EscapeCommFunction(handle_, func)) {
            throw SerialIOException("无法设置RTS信号: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief 获取CTS（清除发送）信号状态
     *
     * @return True表示CTS被断言，否则False
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    [[nodiscard]] bool getCTS() const { return getModemStatus(MS_CTS_ON); }

    /**
     * @brief 获取DSR（数据设备就绪）信号状态
     *
     * @return True表示DSR被断言，否则False
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    [[nodiscard]] bool getDSR() const { return getModemStatus(MS_DSR_ON); }

    /**
     * @brief 获取RI（振铃指示器）信号状态
     *
     * @return True表示RI被断言，否则False
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    [[nodiscard]] bool getRI() const { return getModemStatus(MS_RING_ON); }

    /**
     * @brief 获取CD（载波检测）信号状态
     *
     * @return True表示CD被断言，否则False
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 如果操作失败
     */
    [[nodiscard]] bool getCD() const {
        return getModemStatus(MS_RLSD_ON);  // RLSD = 接收线路信号检测 = CD
    }

    /**
     * @brief 获取端口名称
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
     * @return 可用端口名称向量
     */
    static std::vector<std::string> getAvailablePorts() {
        std::vector<std::string> ports;

        // 使用SetupDi API获取串口列表
        HDEVINFO deviceInfoSet =
            SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            return ports;
        }

        SP_DEVINFO_DATA deviceInfoData{};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0;
             SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
            char portName[256] = {0};
            HKEY deviceKey =
                SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfoData,
                                     DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

            if (deviceKey != INVALID_HANDLE_VALUE) {
                DWORD portNameLength = sizeof(portName);
                DWORD regDataType = 0;

                if (RegQueryValueExA(deviceKey, "PortName", nullptr,
                                     &regDataType,
                                     reinterpret_cast<LPBYTE>(portName),
                                     &portNameLength) == ERROR_SUCCESS) {
                    ports.push_back(portName);
                }

                RegCloseKey(deviceKey);
            }
        }

        if (deviceInfoSet) {
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
        }

        return ports;
    }

private:
    HANDLE handle_;                      ///< Windows串口句柄
    SerialConfig config_;                ///< 当前端口配置
    std::string portName_;               ///< 当前打开端口的名称
    mutable std::shared_mutex mutex_;    ///< 线程安全的互斥锁
    std::thread asyncReadThread_;        ///< 异步读取线程
    std::atomic<bool> stopAsyncRead_;    ///< 停止异步读取的标志
    std::atomic<bool> asyncReadActive_;  ///< 异步读取活动标志
    std::mutex asyncMutex_;              ///< 异步操作互斥锁
    std::condition_variable asyncCv_;    ///< 用于异步操作的条件变量

    /**
     * @brief 将当前配置应用于串口
     *
     * @throws SerialIOException 无法配置端口时抛出
     */
    void applyConfig() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        applyConfigInternal();
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
     * @brief 应用配置的内部方法（无锁）
     *
     * @throws SerialIOException 无法配置端口时抛出
     */
    void applyConfigInternal() {
        if (handle_ == INVALID_HANDLE_VALUE) {
            return;
        }

        DCB dcb{};
        dcb.DCBlength = sizeof(DCB);

        if (!GetCommState(handle_, &dcb)) {
            throw SerialIOException("无法获取串口配置: " +
                                    getLastErrorAsString(GetLastError()));
        }

        // 波特率
        dcb.BaudRate = config_.getBaudRate();

        // 数据位
        dcb.ByteSize = config_.getDataBits();

        // 奇偶校验
        switch (config_.getParity()) {
            case SerialConfig::Parity::None:
                dcb.Parity = NOPARITY;
                break;
            case SerialConfig::Parity::Odd:
                dcb.Parity = ODDPARITY;
                break;
            case SerialConfig::Parity::Even:
                dcb.Parity = EVENPARITY;
                break;
            case SerialConfig::Parity::Mark:
                dcb.Parity = MARKPARITY;
                break;
            case SerialConfig::Parity::Space:
                dcb.Parity = SPACEPARITY;
                break;
        }

        // 停止位
        switch (config_.getStopBits()) {
            case SerialConfig::StopBits::One:
                dcb.StopBits = ONESTOPBIT;
                break;
            case SerialConfig::StopBits::OnePointFive:
                dcb.StopBits = ONE5STOPBITS;
                break;
            case SerialConfig::StopBits::Two:
                dcb.StopBits = TWOSTOPBITS;
                break;
        }

        // 流控制
        switch (config_.getFlowControl()) {
            case SerialConfig::FlowControl::None:
                dcb.fOutxCtsFlow = FALSE;
                dcb.fOutxDsrFlow = FALSE;
                dcb.fDtrControl = DTR_CONTROL_ENABLE;
                dcb.fRtsControl = RTS_CONTROL_ENABLE;
                dcb.fOutX = FALSE;
                dcb.fInX = FALSE;
                break;
            case SerialConfig::FlowControl::Software:
                dcb.fOutxCtsFlow = FALSE;
                dcb.fOutxDsrFlow = FALSE;
                dcb.fDtrControl = DTR_CONTROL_ENABLE;
                dcb.fRtsControl = RTS_CONTROL_ENABLE;
                dcb.fOutX = TRUE;
                dcb.fInX = TRUE;
                dcb.XonChar = 17;   // XON = Ctrl+Q
                dcb.XoffChar = 19;  // XOFF = Ctrl+S
                dcb.XonLim = 100;
                dcb.XoffLim = 100;
                break;
            case SerialConfig::FlowControl::Hardware:
                dcb.fOutxCtsFlow = TRUE;
                dcb.fOutxDsrFlow = FALSE;
                dcb.fDtrControl = DTR_CONTROL_ENABLE;
                dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
                dcb.fOutX = FALSE;
                dcb.fInX = FALSE;
                break;
        }

        // 其他设置
        dcb.fBinary = TRUE;  // 二进制模式
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fAbortOnError = FALSE;

        if (!SetCommState(handle_, &dcb)) {
            throw SerialIOException("无法设置串口配置: " +
                                    getLastErrorAsString(GetLastError()));
        }

        // 设置超时
        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = config_.getReadTimeout().count();
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = config_.getWriteTimeout().count();

        if (!SetCommTimeouts(handle_, &timeouts)) {
            throw SerialIOException("无法设置串口超时: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief 获取调制解调器状态标志
     *
     * @param flag 要检查的调制解调器状态标志
     * @return True表示标志已设置，否则False
     * @throws SerialPortNotOpenException 如果端口未打开
     * @throws SerialIOException 无法获取调制解调器状态时抛出
     */
    [[nodiscard]] bool getModemStatus(DWORD flag) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        DWORD status = 0;
        if (!GetCommModemStatus(handle_, &status)) {
            throw SerialIOException("无法获取调制解调器状态: " +
                                    getLastErrorAsString(GetLastError()));
        }

        return (status & flag) != 0;
    }

    /**
     * @brief 停止异步读取工作线程
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

    /**
     * @brief 将Windows错误代码转换为可读字符串
     *
     * @param errorCode 要转换的Windows错误代码
     * @return 错误描述字符串
     */
    static std::string getLastErrorAsString(DWORD errorCode) {
        if (errorCode == 0) {
            return "无错误";
        }

        LPSTR messageBuffer = nullptr;

        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&messageBuffer, 0, nullptr);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);

        return message;
    }
};

}  // namespace serial

#endif  // _WIN32