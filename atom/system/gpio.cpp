#include "gpio.hpp"

#include <atomic>
#include <csignal>
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
            write(edgeFd, "both", 4);
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
        read(fd, buffer, sizeof(buffer));

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
          pullMode_(PullMode::NONE) {
        exportGPIO();
        setGPIODirection("out");
    }

    Impl(std::string pin, Direction direction, bool initialValue)
        : pin_(std::move(pin)),
          direction_(direction),
          edge_(Edge::NONE),
          pullMode_(PullMode::NONE) {
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

    static void notifyOnChange(const std::string& pin,
                               const std::function<void(bool)>& callback) {
        GPIOCallbackManager::getInstance().registerCallback(pin, callback);
    }

private:
    std::string pin_;
    Direction direction_;
    Edge edge_;
    PullMode pullMode_;

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
