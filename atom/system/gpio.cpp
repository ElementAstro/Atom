#include "gpio.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#ifdef _WIN32
// Windows specific headers
// clang-format off
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <hidsdi.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#endif
#else
// Linux specific headers
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_PATH "/sys/class/gpio"
#endif

namespace atom::system {

// Utility functions for string conversions
GPIO::Direction stringToDirection(const std::string& direction) {
    if (direction == "in")
        return GPIO::Direction::INPUT;
    if (direction == "out")
        return GPIO::Direction::OUTPUT;
    THROW_RUNTIME_ERROR("Invalid GPIO direction: " + direction);
}

std::string directionToString(GPIO::Direction direction) {
    switch (direction) {
        case GPIO::Direction::INPUT:
            return "in";
        case GPIO::Direction::OUTPUT:
            return "out";
        default:
            THROW_RUNTIME_ERROR("Invalid GPIO direction enum value");
    }
}

GPIO::Edge stringToEdge(const std::string& edge) {
    if (edge == "none")
        return GPIO::Edge::NONE;
    if (edge == "rising")
        return GPIO::Edge::RISING;
    if (edge == "falling")
        return GPIO::Edge::FALLING;
    if (edge == "both")
        return GPIO::Edge::BOTH;
    THROW_RUNTIME_ERROR("Invalid GPIO edge: " + edge);
}

std::string edgeToString(GPIO::Edge edge) {
    switch (edge) {
        case GPIO::Edge::NONE:
            return "none";
        case GPIO::Edge::RISING:
            return "rising";
        case GPIO::Edge::FALLING:
            return "falling";
        case GPIO::Edge::BOTH:
            return "both";
        default:
            THROW_RUNTIME_ERROR("Invalid GPIO edge enum value");
    }
}

#ifdef _WIN32
// Windows 版本的 GPIO 回调管理器
class GPIOCallbackManager {
public:
    static GPIOCallbackManager& getInstance() {
        static GPIOCallbackManager instance;
        return instance;
    }

    void registerCallback(const std::string& pin,
                          std::function<void(bool)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 如果这是第一个回调，初始化硬件并启动监视线程
        if (callbacks_.empty() && !monitorThreadRunning_) {
            if (!deviceInitialized_) {
                if (!initializeDevice()) {
                    LOG_F(ERROR, "无法初始化 GPIO 设备");
                    return;
                }
            }
            startMonitorThread();
        }

        callbacks_[pin] = std::move(callback);

        // 添加此引脚到引脚状态跟踪中
        pinStates_[pin] = readPinState(pin);
    }

    void unregisterCallback(const std::string& pin) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(pin);
        pinStates_.erase(pin);

        // 如果没有更多回调，停止线程
        if (callbacks_.empty() && monitorThreadRunning_) {
            stopMonitorThread();
        }
    }

    // 模拟引脚状态变化，供测试使用
    void simulatePinStateChange(const std::string& pin, bool state) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = callbacks_.find(pin);
        if (it != callbacks_.end() && pinStates_[pin] != state) {
            pinStates_[pin] = state;
            try {
                it->second(state);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "GPIO 回调中发生异常: %s", e.what());
            } catch (...) {
                LOG_F(ERROR, "GPIO 回调中发生未知异常");
            }
        }
    }

    ~GPIOCallbackManager() {
        stopMonitorThread();
        closeDevice();
    }

private:
    GPIOCallbackManager()
        : monitorThreadRunning_(false),
          deviceInitialized_(false),
          deviceHandle_(INVALID_HANDLE_VALUE),
          useSerialMode_(false) {}

    // 初始化设备 - 尝试 USB 设备，如果失败则尝试串口
    bool initializeDevice() {
        // 首先尝试 USB GPIO 适配器
        if (initializeUsbDevice()) {
            useSerialMode_ = false;
            deviceInitialized_ = true;
            LOG_F(INFO, "已成功初始化 USB GPIO 设备");
            return true;
        }

        // 如果 USB 设备失败，尝试串口
        if (initializeSerialDevice()) {
            useSerialMode_ = true;
            deviceInitialized_ = true;
            LOG_F(INFO, "已成功初始化串口 GPIO 设备");
            return true;
        }

        LOG_F(ERROR, "无法找到可用的 GPIO 设备");
        return false;
    }

    // 初始化 USB GPIO 适配器
    bool initializeUsbDevice() {
        // 查找具有特定 VID/PID 的 USB 设备
        GUID guid;
        HDEVINFO deviceInfo;
        SP_DEVICE_INTERFACE_DATA interfaceData;

        // 使用 HID 设备类 GUID
        HidD_GetHidGuid(&guid);
        deviceInfo = SetupDiGetClassDevs(&guid, NULL, NULL,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (deviceInfo == INVALID_HANDLE_VALUE) {
            LOG_F(ERROR, "无法获取设备信息集");
            return false;
        }

        interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        // 枚举设备
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfo, NULL, &guid,
                                                      i, &interfaceData);
             i++) {
            DWORD requiredSize = 0;

            // 获取详细信息所需的大小
            SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData, NULL, 0,
                                            &requiredSize, NULL);

            // 分配内存
            PSP_DEVICE_INTERFACE_DETAIL_DATA detailData =
                (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);

            if (!detailData)
                continue;

            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            // 获取详细信息
            if (SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData,
                                                detailData, requiredSize, NULL,
                                                NULL)) {
                // 尝试打开设备
                deviceHandle_ = CreateFile(detailData->DevicePath,
                                           GENERIC_READ | GENERIC_WRITE,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           NULL, OPEN_EXISTING, 0, NULL);

                if (deviceHandle_ != INVALID_HANDLE_VALUE) {
                    // TODO: 这里可以添加设备识别代码，检查是否为目标 GPIO 设备
                    // 例如检查 VID/PID

                    free(detailData);
                    SetupDiDestroyDeviceInfoList(deviceInfo);
                    return true;
                }
            }

            free(detailData);
        }

        SetupDiDestroyDeviceInfoList(deviceInfo);
        return false;
    }

    // 初始化串口设备
    bool initializeSerialDevice() {
        // 尝试常见的串口名称
        std::vector<std::string> comPorts = {"COM1", "COM2", "COM3", "COM4",
                                             "COM5"};

        for (const auto& port : comPorts) {
            // 尝试打开串口
            std::string portName = "\\\\.\\" + port;
            deviceHandle_ =
                CreateFileA(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                            NULL, OPEN_EXISTING, 0, NULL);

            if (deviceHandle_ != INVALID_HANDLE_VALUE) {
                // 配置串口参数
                DCB dcbSerialParams = {};
                dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

                if (!GetCommState(deviceHandle_, &dcbSerialParams)) {
                    CloseHandle(deviceHandle_);
                    deviceHandle_ = INVALID_HANDLE_VALUE;
                    continue;
                }

                // 设置波特率，通常是 9600 或其他值
                dcbSerialParams.BaudRate = CBR_9600;
                dcbSerialParams.ByteSize = 8;
                dcbSerialParams.StopBits = ONESTOPBIT;
                dcbSerialParams.Parity = NOPARITY;

                if (!SetCommState(deviceHandle_, &dcbSerialParams)) {
                    CloseHandle(deviceHandle_);
                    deviceHandle_ = INVALID_HANDLE_VALUE;
                    continue;
                }

                // 设置超时
                COMMTIMEOUTS timeouts = {};
                timeouts.ReadIntervalTimeout = 50;
                timeouts.ReadTotalTimeoutConstant = 50;
                timeouts.ReadTotalTimeoutMultiplier = 10;
                timeouts.WriteTotalTimeoutConstant = 50;
                timeouts.WriteTotalTimeoutMultiplier = 10;

                if (!SetCommTimeouts(deviceHandle_, &timeouts)) {
                    CloseHandle(deviceHandle_);
                    deviceHandle_ = INVALID_HANDLE_VALUE;
                    continue;
                }

                // 验证设备是否为 GPIO 设备
                if (verifyGpioDevice()) {
                    LOG_F(INFO, "成功初始化串口 GPIO 设备: %s", port.c_str());
                    return true;
                }

                CloseHandle(deviceHandle_);
                deviceHandle_ = INVALID_HANDLE_VALUE;
            }
        }

        return false;
    }

    // 验证串口设备是否为 GPIO 控制器
    bool verifyGpioDevice() {
        if (deviceHandle_ == INVALID_HANDLE_VALUE)
            return false;

        // 发送识别命令
        const char* cmd = "IDENTIFY\r\n";
        DWORD bytesWritten;

        if (!WriteFile(deviceHandle_, cmd, strlen(cmd), &bytesWritten, NULL)) {
            return false;
        }

        // 读取响应
        char buffer[64] = {0};
        DWORD bytesRead;

        if (!ReadFile(deviceHandle_, buffer, sizeof(buffer) - 1, &bytesRead,
                      NULL)) {
            return false;
        }

        // 检查响应是否包含 GPIO 标识符
        std::string response(buffer);
        return (response.find("GPIO") != std::string::npos);
    }

    // 关闭设备
    void closeDevice() {
        if (deviceHandle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(deviceHandle_);
            deviceHandle_ = INVALID_HANDLE_VALUE;
        }
        deviceInitialized_ = false;
    }

    // 读取引脚状态，根据连接模式采用不同实现
    bool readPinState(const std::string& pin) {
        if (!deviceInitialized_ || deviceHandle_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        return useSerialMode_ ? readPinStateSerial(pin) : readPinStateUsb(pin);
    }

    // 通过 USB 设备读取引脚状态
    bool readPinStateUsb(const std::string& pin) {
        // 构建包含 GPIO 引脚编号的命令缓冲区
        unsigned char buffer[8] = {0};
        int pinNumber = std::stoi(pin);

        // 命令格式: [命令码 0x01][引脚号]
        buffer[0] = 0x01;  // 读取命令
        buffer[1] = static_cast<unsigned char>(pinNumber);

        DWORD bytesWritten = 0;
        if (!WriteFile(deviceHandle_, buffer, 2, &bytesWritten, NULL)) {
            LOG_F(ERROR, "写入 USB GPIO 命令失败: %d", GetLastError());
            return false;
        }

        // 读取响应
        memset(buffer, 0, sizeof(buffer));
        DWORD bytesRead = 0;

        if (!ReadFile(deviceHandle_, buffer, sizeof(buffer), &bytesRead,
                      NULL)) {
            LOG_F(ERROR, "读取 USB GPIO 状态失败: %d", GetLastError());
            return false;
        }

        // 假设第一个字节是状态值
        return (buffer[0] != 0);
    }

    // 通过串口读取引脚状态
    bool readPinStateSerial(const std::string& pin) {
        // 构建串口命令
        std::string cmd = "READ " + pin + "\r\n";
        DWORD bytesWritten = 0;

        if (!WriteFile(deviceHandle_, cmd.c_str(), cmd.length(), &bytesWritten,
                       NULL)) {
            LOG_F(ERROR, "写入串口 GPIO 命令失败: %d", GetLastError());
            return false;
        }

        // 读取响应
        char buffer[64] = {0};
        DWORD bytesRead = 0;

        if (!ReadFile(deviceHandle_, buffer, sizeof(buffer) - 1, &bytesRead,
                      NULL)) {
            LOG_F(ERROR, "读取串口 GPIO 状态失败: %d", GetLastError());
            return false;
        }

        // 分析响应字符串
        std::string response(buffer, bytesRead);
        return (response.find("HIGH") != std::string::npos ||
                response.find("1") != std::string::npos);
    }

    void startMonitorThread() {
        if (monitorThreadRunning_)
            return;

        monitorThreadRunning_ = true;
        monitorThread_ = std::thread([this]() {
            while (monitorThreadRunning_) {
                // 定期轮询引脚状态
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (auto& [pin, callback] : callbacks_) {
                        bool currentState = readPinState(pin);
                        if (pinStates_[pin] != currentState) {
                            pinStates_[pin] = currentState;
                            try {
                                callback(currentState);
                            } catch (const std::exception& e) {
                                LOG_F(ERROR, "GPIO 回调中发生异常: %s",
                                      e.what());
                            } catch (...) {
                                LOG_F(ERROR, "GPIO 回调中发生未知异常");
                            }
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void stopMonitorThread() {
        if (!monitorThreadRunning_)
            return;

        monitorThreadRunning_ = false;

        if (monitorThread_.joinable()) {
            monitorThread_.join();
        }
    }

    std::mutex mutex_;
    std::unordered_map<std::string, std::function<void(bool)>> callbacks_;
    std::unordered_map<std::string, bool> pinStates_;  // 跟踪引脚状态
    std::thread monitorThread_;
    std::atomic<bool> monitorThreadRunning_;

    // 硬件相关成员
    bool deviceInitialized_;
    HANDLE deviceHandle_;
    bool useSerialMode_;  // 标识使用串口模式还是USB模式
};

#else
// Linux 版本的 GPIO 回调管理器
class GPIOCallbackManager {
public:
    static GPIOCallbackManager& getInstance() {
        static GPIOCallbackManager instance;
        return instance;
    }

    void registerCallback(const std::string& pin,
                          std::function<void(bool)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);

        // If this is our first callback, start the monitor thread
        if (callbacks_.empty() && !monitorThreadRunning_) {
            startMonitorThread();
        }

        callbacks_[pin] = std::move(callback);

        // Add this pin to the monitoring if not already present
        setupPinMonitoring(pin);
    }

    void unregisterCallback(const std::string& pin) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(pin);

        // Remove from epoll monitoring
        auto it = fdMap_.find(pin);
        if (it != fdMap_.end()) {
            epoll_ctl(epollFd_, EPOLL_CTL_DEL, it->second, nullptr);
            close(it->second);
            fdMap_.erase(it);
        }

        // If no more callbacks, stop the thread
        if (callbacks_.empty() && monitorThreadRunning_) {
            stopMonitorThread();
        }
    }

    ~GPIOCallbackManager() {
        stopMonitorThread();

        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [pin, fd] : fdMap_) {
            close(fd);
        }

        if (epollFd_ >= 0) {
            close(epollFd_);
            epollFd_ = -1;
        }
    }

private:
    GPIOCallbackManager() : monitorThreadRunning_(false), epollFd_(-1) {
        epollFd_ = epoll_create1(0);
        if (epollFd_ < 0) {
            LOG_F(ERROR, "Failed to create epoll instance: %s",
                  strerror(errno));
        }
    }

    void setupPinMonitoring(const std::string& pin) {
        // Check if already monitoring this pin
        if (fdMap_.find(pin) != fdMap_.end()) {
            return;
        }

        std::string path = std::string(GPIO_PATH) + "/gpio" + pin + "/value";
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            LOG_F(ERROR, "Failed to open gpio value for reading: %s",
                  strerror(errno));
            return;
        }

        // Configure the pin for edge-triggered interrupts
        std::string edgePath = std::string(GPIO_PATH) + "/gpio" + pin + "/edge";
        int edgeFd = open(edgePath.c_str(), O_WRONLY);
        if (edgeFd >= 0) {
            ssize_t res = write(edgeFd, "both", 4);
            if (res != 4) {
                LOG_F(WARNING, "Failed to set edge to 'both' for GPIO %s",
                      pin.c_str());
            }
            close(edgeFd);
        }

        // Add to epoll
        struct epoll_event ev;
        ev.events = EPOLLPRI | EPOLLET;
        ev.data.fd = fd;

        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            LOG_F(ERROR, "Failed to add fd to epoll: %s", strerror(errno));
            close(fd);
            return;
        }

        // Initial read to clear any pending interrupts
        char buffer[2];
        lseek(fd, 0, SEEK_SET);
        ssize_t res = read(fd, buffer, sizeof(buffer));
        if (res < 0) {
            LOG_F(WARNING, "Failed to read initial GPIO value: %s",
                  strerror(errno));
        }

        fdMap_[pin] = fd;
    }

    void startMonitorThread() {
        if (monitorThreadRunning_)
            return;

        monitorThreadRunning_ = true;
        monitorThread_ = std::thread([this]() {
            constexpr int MAX_EVENTS = 10;
            struct epoll_event events[MAX_EVENTS];

            while (monitorThreadRunning_) {
                int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, 500);
                if (nfds < 0) {
                    if (errno != EINTR) {
                        LOG_F(ERROR, "epoll_wait failed: %s", strerror(errno));
                    }
                    continue;
                }

                for (int i = 0; i < nfds; i++) {
                    if (events[i].events & EPOLLPRI) {
                        int fd = events[i].data.fd;

                        // Read the value
                        char buffer[2] = {0};
                        lseek(fd, 0, SEEK_SET);
                        if (read(fd, buffer, sizeof(buffer) - 1) > 0) {
                            bool value = (buffer[0] == '1');

                            // Find which pin this is
                            std::string pin;
                            {
                                std::lock_guard<std::mutex> lock(mutex_);
                                for (const auto& [p, fileFd] : fdMap_) {
                                    if (fileFd == fd) {
                                        pin = p;
                                        break;
                                    }
                                }
                            }

                            if (!pin.empty()) {
                                // Call the callback
                                std::function<void(bool)> callback;
                                {
                                    std::lock_guard<std::mutex> lock(mutex_);
                                    auto it = callbacks_.find(pin);
                                    if (it != callbacks_.end()) {
                                        callback = it->second;
                                    }
                                }

                                if (callback) {
                                    try {
                                        callback(value);
                                    } catch (const std::exception& e) {
                                        LOG_F(ERROR,
                                              "Exception in GPIO callback: %s",
                                              e.what());
                                    } catch (...) {
                                        LOG_F(ERROR,
                                              "Unknown exception in GPIO "
                                              "callback");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        });
    }

    void stopMonitorThread() {
        if (!monitorThreadRunning_)
            return;

        monitorThreadRunning_ = false;

        if (monitorThread_.joinable()) {
            monitorThread_.join();
        }
    }

    std::mutex mutex_;
    std::unordered_map<std::string, std::function<void(bool)>> callbacks_;
    std::unordered_map<std::string, int> fdMap_;
    std::thread monitorThread_;
    std::atomic<bool> monitorThreadRunning_;
    int epollFd_;
};
#endif

class GPIO::Impl {
public:
    explicit Impl(std::string pin)
        : pin_(std::move(pin)),
          direction_(Direction::OUTPUT),
          edge_(Edge::NONE),
          pullMode_(PullMode::NONE),
          pwmActive_(false),
          pwmFrequency_(0),
          pwmDutyCycle_(0),
          pwmMode_(PwmMode::HARDWARE),
          interruptCounterActive_(false),
          interruptCount_(0),
          debounceActive_(false) {
        exportGPIO();
        setGPIODirection("out");
    }

    Impl(std::string pin, Direction direction, bool initialValue)
        : pin_(std::move(pin)),
          direction_(direction),
          edge_(Edge::NONE),
          pullMode_(PullMode::NONE),
          pwmActive_(false),
          pwmFrequency_(0),
          pwmDutyCycle_(0),
          pwmMode_(PwmMode::HARDWARE),
          interruptCounterActive_(false),
          interruptCount_(0),
          debounceActive_(false) {
        exportGPIO();
        setGPIODirection(directionToString(direction));

        if (direction == Direction::OUTPUT) {
            setGPIOValue(initialValue ? "1" : "0");
        }
    }

    ~Impl() {
        try {
            // Remove any callbacks
            stopCallbacks();

            // Unexport GPIO to release the pin
            unexportGPIO();
        } catch (...) {
            // Suppress all exceptions
        }
    }

    void setValue(bool value) {
        if (direction_ != Direction::OUTPUT) {
            THROW_RUNTIME_ERROR("Cannot set value on input GPIO pin");
        }
        setGPIOValue(value ? "1" : "0");
    }

    bool getValue() const { return readGPIOValue(); }

    void setDirection(Direction direction) {
        setGPIODirection(directionToString(direction));
        direction_ = direction;
    }

    Direction getDirection() const { return direction_; }

    void setEdge(Edge edge) {
        if (direction_ != Direction::INPUT) {
            THROW_RUNTIME_ERROR("Edge detection only works on input GPIO pins");
        }

        setGPIOEdge(edgeToString(edge));
        edge_ = edge;
    }

    Edge getEdge() const { return edge_; }

    void setPullMode(PullMode mode) {
        // Some platforms like the Raspberry Pi might require setting pull mode
        // through a device tree overlay or other means. We'll just store the
        // mode for now.
        pullMode_ = mode;
    }

    PullMode getPullMode() const { return pullMode_; }

    std::string getPin() const { return pin_; }

    bool toggle() {
        if (direction_ != Direction::OUTPUT) {
            THROW_RUNTIME_ERROR("Cannot toggle value on input GPIO pin");
        }

        bool currentValue = getValue();
        setValue(!currentValue);
        return !currentValue;
    }

    void pulse(bool value, std::chrono::milliseconds duration) {
        if (direction_ != Direction::OUTPUT) {
            THROW_RUNTIME_ERROR("Cannot pulse on input GPIO pin");
        }

        bool originalValue = getValue();
        setValue(value);
        std::this_thread::sleep_for(duration);
        setValue(originalValue);
    }

    // 新增PWM功能实现
    bool setPwm(double frequency, double dutyCycle, PwmMode mode) {
        // 检查参数
        if (frequency <= 0 || dutyCycle < 0 || dutyCycle > 1.0) {
            LOG_F(ERROR,
                  "Invalid PWM parameters: frequency=%.2fHz, dutyCycle=%.2f",
                  frequency, dutyCycle);
            return false;
        }

        // 确保是输出模式
        if (direction_ != Direction::OUTPUT) {
            LOG_F(ERROR, "Cannot setup PWM on input GPIO pin %s", pin_.c_str());
            return false;
        }

        // 如果已有PWM运行，先停止
        if (pwmActive_) {
            stopPwm();
        }

        pwmFrequency_ = frequency;
        pwmDutyCycle_ = dutyCycle;
        pwmMode_ = (mode == GPIO::PwmMode::HARDWARE) ? PwmMode::HARDWARE
                                                     : PwmMode::SOFTWARE;

#ifdef _WIN32
        // Windows模拟PWM (总是使用软件PWM)
        return startSoftwarePwm();
#else
        // Linux上尝试硬件PWM，如果不可用则回退到软件PWM
        if (mode == PwmMode::HARDWARE && tryHardwarePwm()) {
            LOG_F(INFO, "Hardware PWM started on pin %s: %.2fHz, %.2f%%",
                  pin_.c_str(), frequency, dutyCycle * 100);
            return true;
        } else if (mode == PwmMode::SOFTWARE || mode == PwmMode::HARDWARE) {
            // 硬件PWM不可用或用户请求软件PWM
            return startSoftwarePwm();
        }
#endif
        return false;
    }

    bool updatePwmDutyCycle(double dutyCycle) {
        if (!pwmActive_) {
            LOG_F(ERROR, "Cannot update duty cycle, PWM not active on pin %s",
                  pin_.c_str());
            return false;
        }

        if (dutyCycle < 0 || dutyCycle > 1.0) {
            LOG_F(ERROR, "Invalid duty cycle: %.2f", dutyCycle);
            return false;
        }

        pwmDutyCycle_ = dutyCycle;

#ifdef _WIN32
        // Windows模拟实现中不需要特殊处理，软件PWM线程会自动使用新的dutyCycle
        return true;
#else
        if (pwmMode_ == PwmMode::HARDWARE) {
            // 更新硬件PWM占空比
            std::string pwmPath = std::string(GPIO_PATH) + "/pwm" + pin_;
            std::string dutyCyclePath = pwmPath + "/duty_cycle";

            try {
                int period = static_cast<int>(
                    1.0e9 / pwmFrequency_);  // 周期以纳秒为单位
                int onTime =
                    static_cast<int>(period * dutyCycle);  // 高电平时间

                std::ofstream fs(dutyCyclePath);
                if (!fs) {
                    LOG_F(ERROR, "Failed to open %s for writing",
                          dutyCyclePath.c_str());
                    return false;
                }
                fs << onTime;
                return true;
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to update hardware PWM duty cycle: %s",
                      e.what());
                return false;
            }
        }
        // 软件PWM会自动使用新的dutyCycle
#endif
        return true;
    }

    void stopPwm() {
        if (!pwmActive_) {
            return;
        }

        LOG_F(INFO, "Stopping PWM on pin %s", pin_.c_str());

#ifndef _WIN32
        if (pwmMode_ == PwmMode::HARDWARE) {
            // 关闭硬件PWM
            try {
                std::string pwmPath = std::string(GPIO_PATH) + "/pwm" + pin_;
                std::string enablePath = pwmPath + "/enable";

                std::ofstream fs(enablePath);
                if (fs) {
                    fs << "0";
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Error stopping hardware PWM: %s", e.what());
            }
        }
#endif

        // 停止软件PWM线程
        if (pwmThread_.joinable()) {
            pwmThreadRunning_ = false;
            pwmThread_.join();
        }

        pwmActive_ = false;
    }

#ifndef _WIN32
    bool tryHardwarePwm() {
        // 检查硬件PWM是否可用并尝试配置
        std::string pwmPath = std::string(GPIO_PATH) + "/pwm" + pin_;

        // 检查是否有PWM硬件支持
        if (!std::filesystem::exists(pwmPath)) {
            LOG_F(INFO, "Hardware PWM not available for pin %s", pin_.c_str());
            return false;
        }

        try {
            // 配置硬件PWM
            std::string periodPath = pwmPath + "/period";
            std::string dutyCyclePath = pwmPath + "/duty_cycle";
            std::string enablePath = pwmPath + "/enable";

            // 先禁用PWM
            std::ofstream enableFs(enablePath);
            if (!enableFs) {
                LOG_F(ERROR, "Failed to open %s for writing",
                      enablePath.c_str());
                return false;
            }
            enableFs << "0";
            enableFs.close();

            // 设置频率(周期)，单位为纳秒
            int period = static_cast<int>(1.0e9 / pwmFrequency_);
            std::ofstream periodFs(periodPath);
            if (!periodFs) {
                LOG_F(ERROR, "Failed to open %s for writing",
                      periodPath.c_str());
                return false;
            }
            periodFs << period;
            periodFs.close();

            // 设置占空比
            int onTime = static_cast<int>(period * pwmDutyCycle_);
            std::ofstream dutyFs(dutyCyclePath);
            if (!dutyFs) {
                LOG_F(ERROR, "Failed to open %s for writing",
                      dutyCyclePath.c_str());
                return false;
            }
            dutyFs << onTime;
            dutyFs.close();

            // 启用PWM
            enableFs.open(enablePath);
            if (!enableFs) {
                LOG_F(ERROR, "Failed to open %s for writing",
                      enablePath.c_str());
                return false;
            }
            enableFs << "1";

            pwmActive_ = true;
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to setup hardware PWM: %s", e.what());
            return false;
        }
    }
#endif

    bool startSoftwarePwm() {
        // 启动软件PWM
        if (pwmActive_ && pwmThread_.joinable()) {
            LOG_F(ERROR, "PWM already active on pin %s", pin_.c_str());
            return false;
        }

        pwmThreadRunning_ = true;
        pwmThread_ = std::thread([this]() {
            LOG_F(INFO, "Software PWM started on pin %s: %.2fHz, %.2f%%",
                  pin_.c_str(), pwmFrequency_, pwmDutyCycle_ * 100);

            // 计算周期时间（微秒）
            const auto periodUs =
                static_cast<int64_t>(1000000.0 / pwmFrequency_);

            while (pwmThreadRunning_) {
                auto startTime = std::chrono::steady_clock::now();

                // 计算高电平持续时间（微秒）
                auto highTimeUs =
                    static_cast<int64_t>(periodUs * pwmDutyCycle_);

                // 如果占空比为0或1，不需要切换状态
                if (pwmDutyCycle_ <= 0.0) {
                    setValue(false);
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(periodUs));
                    continue;
                } else if (pwmDutyCycle_ >= 1.0) {
                    setValue(true);
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(periodUs));
                    continue;
                }

                // 设置高电平
                setValue(true);
                std::this_thread::sleep_for(
                    std::chrono::microseconds(highTimeUs));

                // 设置低电平
                setValue(false);

                // 计算剩余低电平时间
                auto elapsedTime =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
                auto lowTimeUs = periodUs - elapsedTime;

                if (lowTimeUs > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(lowTimeUs));
                }
            }
        });

        pwmActive_ = true;
        pwmMode_ = PwmMode::SOFTWARE;
        return true;
    }

    bool onValueChange(std::function<void(bool)> callback) {
        if (direction_ != Direction::INPUT) {
            THROW_RUNTIME_ERROR(
                "Value change callback only works on input GPIO pins");
        }

        // Set edge to "both" if not already set
        if (edge_ == Edge::NONE) {
            setEdge(Edge::BOTH);
        }

        GPIOCallbackManager::getInstance().registerCallback(
            pin_, std::move(callback));
        return true;
    }

    bool onEdgeChange(Edge edge, std::function<void(bool)> callback) {
        if (direction_ != Direction::INPUT) {
            THROW_RUNTIME_ERROR(
                "Edge change callback only works on input GPIO pins");
        }

        // Set the requested edge mode
        setEdge(edge);

        GPIOCallbackManager::getInstance().registerCallback(
            pin_, std::move(callback));
        return true;
    }

    void stopCallbacks() {
        GPIOCallbackManager::getInstance().unregisterCallback(pin_);
    }

    // 按钮防抖功能实现
    bool setupButtonDebounce(std::function<void()> callback,
                             unsigned int debounceTimeMs) {
        if (direction_ != Direction::INPUT) {
            LOG_F(ERROR, "Button debounce only works on input GPIO pins");
            return false;
        }

        if (debounceActive_) {
            LOG_F(ERROR, "Button debounce already active on pin %s",
                  pin_.c_str());
            return false;
        }

        // 设置边缘检测(通常按钮需要检测下降沿或上升沿)
        setEdge(Edge::BOTH);

        // 创建防抖回调
        debounceActive_ = true;
        debouncePeriodMs_ = debounceTimeMs;
        lastDebounceTime_ = std::chrono::steady_clock::now();

        // 注册防抖回调
        return onValueChange([this, callback](bool state) {
            // 只在按钮被按下(低电平)时触发
            if (state)
                return;

            auto now = std::chrono::steady_clock::now();
            auto elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - lastDebounceTime_)
                    .count();

            if (elapsedMs > debouncePeriodMs_) {
                lastDebounceTime_ = now;
                callback();  // 触发用户回调
            }
        });
    }

    // 中断计数器功能实现
    bool setupInterruptCounter(Edge edge) {
        if (direction_ != Direction::INPUT) {
            LOG_F(ERROR, "Interrupt counter only works on input GPIO pins");
            return false;
        }

        // 设置边缘检测
        setEdge(edge);

        // 重置计数器
        interruptCount_ = 0;
        interruptCounterActive_ = true;

        // 注册回调以增加计数器
        return onValueChange([this](bool /*state*/) {
            if (interruptCounterActive_) {
                interruptCount_++;
            }
        });
    }

    uint64_t getInterruptCount(bool resetAfterReading) {
        uint64_t count = interruptCount_;
        if (resetAfterReading) {
            interruptCount_ = 0;
        }
        return count;
    }

    void resetInterruptCount() { interruptCount_ = 0; }

    static void notifyOnChange(const std::string& pin,
                               const std::function<void(bool)>& callback) {
        GPIOCallbackManager::getInstance().registerCallback(pin, callback);
    }

private:
    std::string pin_;
    Direction direction_;
    Edge edge_;
    PullMode pullMode_;
    bool pwmActive_;
    int pwmFrequency_;
    int pwmDutyCycle_;
    enum class PwmMode { HARDWARE, SOFTWARE } pwmMode_;
    bool interruptCounterActive_;
    int interruptCount_;
    bool debounceActive_;
    std::thread pwmThread_;
    std::atomic<bool> pwmThreadRunning_;
    unsigned int debouncePeriodMs_;
    std::chrono::steady_clock::time_point lastDebounceTime_;

#ifdef _WIN32
    // Windows 模拟的 GPIO 状态
    bool currentValue_ = false;

    void exportGPIO() {
        // Windows 上，这是一个模拟实现
        LOG_F(INFO, "GPIO pin %s exported (Windows simulation)", pin_.c_str());
    }

    void unexportGPIO() {
        // Windows 上，这是一个模拟实现
        LOG_F(INFO, "GPIO pin %s unexported (Windows simulation)",
              pin_.c_str());
    }

    void setGPIODirection(const std::string& direction) {
        // Windows 上，这是一个模拟实现
        LOG_F(INFO, "GPIO pin %s direction set to %s (Windows simulation)",
              pin_.c_str(), direction.c_str());
    }

    void setGPIOValue(const std::string& value) {
        // Windows 上，这是一个模拟实现
        currentValue_ = (value == "1");
        LOG_F(INFO, "GPIO pin %s value set to %s (Windows simulation)",
              pin_.c_str(), value.c_str());
    }

    void setGPIOEdge(const std::string& edge) {
        // Windows 上，这是一个模拟实现
        LOG_F(INFO, "GPIO pin %s edge set to %s (Windows simulation)",
              pin_.c_str(), edge.c_str());
    }

    bool readGPIOValue() const {
        // Windows 上，这是一个模拟实现
        return currentValue_;
    }
#else
    // Linux 实现
    void exportGPIO() {
        // Check if GPIO is already exported
        std::string path = std::string(GPIO_PATH) + "/gpio" + pin_;
        if (access(path.c_str(), F_OK) == 0) {
            // Already exported, no need to export again
            return;
        }
        executeGPIOCommand(GPIO_EXPORT, pin_);

        // Wait for the GPIO to be properly exported
        int retries = 10;
        while (access(path.c_str(), F_OK) != 0 && retries > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            retries--;
        }

        if (access(path.c_str(), F_OK) != 0) {
            THROW_RUNTIME_ERROR("Failed to export GPIO pin: " + pin_);
        }
    }

    void unexportGPIO() { executeGPIOCommand(GPIO_UNEXPORT, pin_); }

    void setGPIODirection(const std::string& direction) {
        std::string path =
            std::string(GPIO_PATH) + "/gpio" + pin_ + "/direction";
        executeGPIOCommand(path, direction);
    }

    void setGPIOValue(const std::string& value) {
        std::string path = std::string(GPIO_PATH) + "/gpio" + pin_ + "/value";
        executeGPIOCommand(path, value);
    }

    void setGPIOEdge(const std::string& edge) {
        std::string path = std::string(GPIO_PATH) + "/gpio" + pin_ + "/edge";
        executeGPIOCommand(path, edge);
    }

    bool readGPIOValue() const {
        std::string path = std::string(GPIO_PATH) + "/gpio" + pin_ + "/value";
        char value[3] = {0};
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            THROW_RUNTIME_ERROR("Failed to open gpio value for reading");
        }
        ssize_t bytes = read(fd, value, sizeof(value) - 1);
        close(fd);
        if (bytes < 0) {
            THROW_RUNTIME_ERROR("Failed to read gpio value");
        }
        return value[0] == '1';
    }

    static void executeGPIOCommand(const std::string& path,
                                   const std::string& command) {
        int fd = open(path.c_str(), O_WRONLY);
        if (fd < 0) {
            THROW_RUNTIME_ERROR("Failed to open gpio path: " + path);
        }
        ssize_t bytes = write(fd, command.c_str(), command.length());
        close(fd);
        if (bytes != static_cast<ssize_t>(command.length())) {
            THROW_RUNTIME_ERROR("Failed to write to gpio path: " + path);
        }
    }
#endif
};

GPIO::GPIO(const std::string& pin) : impl_(std::make_unique<Impl>(pin)) {}

GPIO::GPIO(const std::string& pin, Direction direction, bool initialValue)
    : impl_(std::make_unique<Impl>(pin, direction, initialValue)) {}

GPIO::~GPIO() = default;

GPIO::GPIO(GPIO&& other) noexcept = default;
GPIO& GPIO::operator=(GPIO&& other) noexcept = default;

void GPIO::setValue(bool value) { impl_->setValue(value); }

bool GPIO::getValue() const { return impl_->getValue(); }

void GPIO::setDirection(Direction direction) { impl_->setDirection(direction); }

GPIO::Direction GPIO::getDirection() const { return impl_->getDirection(); }

void GPIO::setEdge(Edge edge) { impl_->setEdge(edge); }

GPIO::Edge GPIO::getEdge() const { return impl_->getEdge(); }

void GPIO::setPullMode(PullMode mode) { impl_->setPullMode(mode); }

GPIO::PullMode GPIO::getPullMode() const { return impl_->getPullMode(); }

std::string GPIO::getPin() const { return impl_->getPin(); }

bool GPIO::toggle() { return impl_->toggle(); }

void GPIO::pulse(bool value, std::chrono::milliseconds duration) {
    impl_->pulse(value, duration);
}

// 新增的PWM相关方法实现
bool GPIO::setPwm(double frequency, double dutyCycle, PwmMode mode) {
    return impl_->setPwm(frequency, dutyCycle, mode);
}

bool GPIO::updatePwmDutyCycle(double dutyCycle) {
    return impl_->updatePwmDutyCycle(dutyCycle);
}

void GPIO::stopPwm() { impl_->stopPwm(); }

// 新增的按钮防抖功能
bool GPIO::setupButtonDebounce(std::function<void()> callback,
                               unsigned int debounceTimeMs) {
    return impl_->setupButtonDebounce(std::move(callback), debounceTimeMs);
}

// 新增的中断计数器功能
bool GPIO::setupInterruptCounter(Edge edge) {
    return impl_->setupInterruptCounter(edge);
}

uint64_t GPIO::getInterruptCount(bool resetAfterReading) {
    return impl_->getInterruptCount(resetAfterReading);
}

void GPIO::resetInterruptCount() { impl_->resetInterruptCount(); }

bool GPIO::onValueChange(std::function<void(bool)> callback) {
    return impl_->onValueChange(std::move(callback));
}

bool GPIO::onEdgeChange(Edge edge, std::function<void(bool)> callback) {
    return impl_->onEdgeChange(edge, std::move(callback));
}

void GPIO::stopCallbacks() { impl_->stopCallbacks(); }

void GPIO::notifyOnChange(const std::string& pin,
                          std::function<void(bool)> callback) {
    Impl::notifyOnChange(pin, std::move(callback));
}

// ShiftRegister类实现
GPIO::ShiftRegister::ShiftRegister(const std::string& dataPin,
                                   const std::string& clockPin,
                                   const std::string& latchPin, uint8_t numBits)
    : dataPin_(std::make_unique<GPIO>(dataPin, GPIO::Direction::OUTPUT, false)),
      clockPin_(
          std::make_unique<GPIO>(clockPin, GPIO::Direction::OUTPUT, false)),
      latchPin_(
          std::make_unique<GPIO>(latchPin, GPIO::Direction::OUTPUT, false)),
      numBits_(numBits),
      state_(0) {
    // 确保所有引脚都初始化为低电平
    dataPin_->setValue(false);
    clockPin_->setValue(false);
    latchPin_->setValue(false);
}

GPIO::ShiftRegister::~ShiftRegister() = default;

void GPIO::ShiftRegister::shiftOut(uint32_t data, bool msbFirst) {
    // 保存新状态
    state_ = data;

    // 拉低锁存引脚，准备发送数据
    latchPin_->setValue(false);

    // 计算需要移位的位数
    uint8_t bitsToShift = numBits_ <= 32 ? numBits_ : 32;

    // 移出数据位
    for (uint8_t i = 0; i < bitsToShift; i++) {
        // 确定当前要发送的位
        uint8_t bitPos = msbFirst ? (bitsToShift - 1 - i) : i;
        bool bitValue = ((data >> bitPos) & 0x01) != 0;

        // 设置数据引脚
        dataPin_->setValue(bitValue);

        // 时钟上升沿，锁存数据
        clockPin_->setValue(true);
        // 短暂延时确保数据稳定
        std::this_thread::sleep_for(std::chrono::microseconds(1));

        // 时钟下降沿
        clockPin_->setValue(false);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    // 拉高锁存引脚，将数据输出到端口
    latchPin_->setValue(true);
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    latchPin_->setValue(false);
}

void GPIO::ShiftRegister::setBit(uint8_t position, bool value) {
    if (position >= numBits_) {
        LOG_F(ERROR, "Bit position %u out of range for %u-bit shift register",
              position, numBits_);
        return;
    }

    uint32_t newState = state_;

    if (value) {
        // 设置指定位
        newState |= (1UL << position);
    } else {
        // 清除指定位
        newState &= ~(1UL << position);
    }

    // 如果状态发生了变化，就更新移位寄存器
    if (newState != state_) {
        shiftOut(newState, true);
    }
}

uint32_t GPIO::ShiftRegister::getState() const { return state_; }

void GPIO::ShiftRegister::clear() { shiftOut(0, true); }

// Implementation of GPIOGroup
GPIO::GPIOGroup::GPIOGroup(const std::vector<std::string>& pins) {
    for (const auto& pin : pins) {
        gpios_.push_back(std::make_unique<GPIO>(pin));
    }
}

GPIO::GPIOGroup::~GPIOGroup() = default;

void GPIO::GPIOGroup::setValues(const std::vector<bool>& values) {
    if (values.size() != gpios_.size()) {
        THROW_RUNTIME_ERROR("Values count doesn't match GPIO count");
    }

    for (size_t i = 0; i < gpios_.size(); ++i) {
        gpios_[i]->setValue(values[i]);
    }
}

std::vector<bool> GPIO::GPIOGroup::getValues() const {
    std::vector<bool> values;
    values.reserve(gpios_.size());

    for (const auto& gpio : gpios_) {
        values.push_back(gpio->getValue());
    }

    return values;
}

void GPIO::GPIOGroup::setDirection(Direction direction) {
    for (auto& gpio : gpios_) {
        gpio->setDirection(direction);
    }
}

#ifdef _WIN32
// Windows 平台特有的辅助函数，用于模拟 GPIO 状态变化
// 这仅用于测试目的
namespace windows {

// 模拟 GPIO 状态变化
void simulateGPIOStateChange(const std::string& pin, bool state) {
    GPIOCallbackManager::getInstance().simulatePinStateChange(pin, state);
}

}  // namespace windows
#endif

}  // namespace atom::system
