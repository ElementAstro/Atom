#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace serial {

// 串口异常类层次结构
class SerialException : public std::runtime_error {
public:
    explicit SerialException(const std::string& message)
        : std::runtime_error(message) {}
};

class SerialPortNotOpenException : public SerialException {
public:
    explicit SerialPortNotOpenException() : SerialException("串口未打开") {}
};

class SerialTimeoutException : public SerialException {
public:
    explicit SerialTimeoutException() : SerialException("串口操作超时") {}
};

class SerialIOException : public SerialException {
public:
    explicit SerialIOException(const std::string& message)
        : SerialException(message) {}
};

// 串口配置结构体
struct SerialConfig {
    enum class Parity { None, Odd, Even, Mark, Space };
    enum class StopBits { One, OnePointFive, Two };
    enum class FlowControl { None, Software, Hardware };

    int baudRate = 9600;
    int dataBits = 8;
    Parity parity = Parity::None;
    StopBits stopBits = StopBits::One;
    FlowControl flowControl = FlowControl::None;

    std::chrono::milliseconds readTimeout{1000};
    std::chrono::milliseconds writeTimeout{1000};
};

// 平台特定实现的前向声明
class SerialPortImpl;

// 主串口类
class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    // 不允许拷贝
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // 允许移动
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    // 打开串口
    void open(const std::string& portName, const SerialConfig& config = {});

    // 关闭串口
    void close();

    // 检查串口是否打开
    [[nodiscard]] bool isOpen() const;

    // 读取最多maxBytes字节数据
    std::vector<uint8_t> read(size_t maxBytes);

    // 精确读取指定字节数，带超时
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout);

    // 异步读取（使用回调）
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback);

    // 读取所有可用数据
    std::vector<uint8_t> readAvailable();

    // 写入数据
    size_t write(std::span<const uint8_t> data);
    size_t write(const std::string& data);

    // 刷新缓冲区
    void flush();

    // 等待缓冲区数据写完
    void drain();

    // 获取可用字节数
    [[nodiscard]] size_t available() const;

    // 配置操作
    void setConfig(const SerialConfig& config);
    [[nodiscard]] SerialConfig getConfig() const;

    // 控制信号
    void setDTR(bool value);
    void setRTS(bool value);
    [[nodiscard]] bool getCTS() const;
    [[nodiscard]] bool getDSR() const;
    [[nodiscard]] bool getRI() const;
    [[nodiscard]] bool getCD() const;

    // 获取串口名称
    [[nodiscard]] std::string getPortName() const;

    // 获取系统可用串口列表
    static std::vector<std::string> getAvailablePorts();

private:
    std::unique_ptr<SerialPortImpl> impl_;  // PIMPL模式隔离平台实现
};

}  // namespace serial