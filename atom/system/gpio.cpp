#include "gpio.hpp"

#include <spdlog/spdlog.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "atom/error/exception.hpp"

#ifdef _WIN32
#include <cfgmgr32.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#endif
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <filesystem>

#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_PATH "/sys/class/gpio"
#endif

namespace atom::system {

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
/**
 * @brief Windows GPIO callback manager for simulating GPIO functionality
 *
 * This class provides GPIO simulation on Windows platforms by using USB
 * devices or serial ports that can be configured as GPIO controllers.
 */
class GPIOCallbackManager {
public:
    static GPIOCallbackManager& getInstance() {
        static GPIOCallbackManager instance;
        return instance;
    }

    /**
     * @brief Register a callback for GPIO pin state changes
     * @param pin GPIO pin identifier
     * @param callback Function to call when pin state changes
     */
    void registerCallback(const std::string& pin,
                          std::function<void(bool)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Initialize hardware and start monitoring if this is the first
        // callback
        if (callbacks_.empty() && !monitorThreadRunning_) {
            if (!deviceInitialized_) {
                if (!initializeDevice()) {
                    spdlog::error("Failed to initialize GPIO device");
                    return;
                }
            }
            startMonitorThread();
        }

        callbacks_[pin] = std::move(callback);
        pinStates_[pin] = readPinState(pin);
    }

    /**
     * @brief Unregister callback for specified pin
     * @param pin GPIO pin identifier
     */
    void unregisterCallback(const std::string& pin) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(pin);
        pinStates_.erase(pin);

        // Stop monitoring thread if no more callbacks exist
        if (callbacks_.empty() && monitorThreadRunning_) {
            stopMonitorThread();
        }
    }

    /**
     * @brief Simulate pin state change for testing purposes
     * @param pin GPIO pin identifier
     * @param state New pin state
     */
    void simulatePinStateChange(const std::string& pin, bool state) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = callbacks_.find(pin);
        if (it != callbacks_.end() && pinStates_[pin] != state) {
            pinStates_[pin] = state;
            try {
                it->second(state);
            } catch (const std::exception& e) {
                spdlog::error("Exception in GPIO callback: {}", e.what());
            } catch (...) {
                spdlog::error("Unknown exception in GPIO callback");
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

    /**
     * @brief Initialize GPIO device - try USB first, fallback to serial
     * @return true if device initialization successful
     */
    bool initializeDevice() {
        // Try USB GPIO adapter first
        if (initializeUsbDevice()) {
            useSerialMode_ = false;
            deviceInitialized_ = true;
            spdlog::info("Successfully initialized USB GPIO device");
            return true;
        }

        // Fallback to serial port if USB fails
        if (initializeSerialDevice()) {
            useSerialMode_ = true;
            deviceInitialized_ = true;
            spdlog::info("Successfully initialized serial GPIO device");
            return true;
        }

        spdlog::error("No available GPIO device found");
        return false;
    }

    /**
     * @brief Initialize USB GPIO adapter
     * @return true if USB device found and initialized
     */
    bool initializeUsbDevice() {
        GUID guid;
        HDEVINFO deviceInfo;
        SP_DEVICE_INTERFACE_DATA interfaceData;

        // Use HID device class GUID
        HidD_GetHidGuid(&guid);
        deviceInfo = SetupDiGetClassDevs(&guid, NULL, NULL,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (deviceInfo == INVALID_HANDLE_VALUE) {
            spdlog::error("Failed to get device information set");
            return false;
        }

        interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        // Enumerate devices
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfo, NULL, &guid,
                                                      i, &interfaceData);
             i++) {
            DWORD requiredSize = 0;

            // Get required size for detail data
            SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData, NULL, 0,
                                            &requiredSize, NULL);

            // Allocate memory
            PSP_DEVICE_INTERFACE_DETAIL_DATA detailData =
                (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);

            if (!detailData)
                continue;

            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            // Get detailed information
            if (SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData,
                                                detailData, requiredSize, NULL,
                                                NULL)) {
                // Try to open device
                deviceHandle_ = CreateFile(detailData->DevicePath,
                                           GENERIC_READ | GENERIC_WRITE,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           NULL, OPEN_EXISTING, 0, NULL);

                if (deviceHandle_ != INVALID_HANDLE_VALUE) {
                    // TODO: Add device identification code to verify target
                    // GPIO device For example, check VID/PID

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

    /**
     * @brief Initialize serial port device
     * @return true if serial GPIO device found and initialized
     */
    bool initializeSerialDevice() {
        // Try common serial port names
        std::vector<std::string> comPorts = {"COM1", "COM2", "COM3", "COM4",
                                             "COM5"};

        for (const auto& port : comPorts) {
            // Try to open serial port
            std::string portName = "\\\\.\\" + port;
            deviceHandle_ =
                CreateFileA(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                            NULL, OPEN_EXISTING, 0, NULL);

            if (deviceHandle_ != INVALID_HANDLE_VALUE) {
                // Configure serial parameters
                DCB dcbSerialParams = {};
                dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

                if (!GetCommState(deviceHandle_, &dcbSerialParams)) {
                    CloseHandle(deviceHandle_);
                    deviceHandle_ = INVALID_HANDLE_VALUE;
                    continue;
                }

                // Set baud rate, typically 9600 or other values
                dcbSerialParams.BaudRate = CBR_9600;
                dcbSerialParams.ByteSize = 8;
                dcbSerialParams.StopBits = ONESTOPBIT;
                dcbSerialParams.Parity = NOPARITY;

                if (!SetCommState(deviceHandle_, &dcbSerialParams)) {
                    CloseHandle(deviceHandle_);
                    deviceHandle_ = INVALID_HANDLE_VALUE;
                    continue;
                }

                // Set timeouts
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

                // Verify device is a GPIO controller
                if (verifyGpioDevice()) {
                    spdlog::info(
                        "Successfully initialized serial GPIO device: {}",
                        port);
                    return true;
                }

                CloseHandle(deviceHandle_);
                deviceHandle_ = INVALID_HANDLE_VALUE;
            }
        }

        return false;
    }

    /**
     * @brief Verify if serial device is a GPIO controller
     * @return true if device responds as GPIO controller
     */
    bool verifyGpioDevice() {
        if (deviceHandle_ == INVALID_HANDLE_VALUE)
            return false;

        // Send identification command
        const char* cmd = "IDENTIFY\r\n";
        DWORD bytesWritten;

        if (!WriteFile(deviceHandle_, cmd, strlen(cmd), &bytesWritten, NULL)) {
            return false;
        }

        // Read response
        char buffer[64] = {0};
        DWORD bytesRead;

        if (!ReadFile(deviceHandle_, buffer, sizeof(buffer) - 1, &bytesRead,
                      NULL)) {
            return false;
        }

        // Check if response contains GPIO identifier
        std::string response(buffer);
        return (response.find("GPIO") != std::string::npos);
    }

    /**
     * @brief Close GPIO device handle
     */
    void closeDevice() {
        if (deviceHandle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(deviceHandle_);
            deviceHandle_ = INVALID_HANDLE_VALUE;
        }
        deviceInitialized_ = false;
    }

    /**
     * @brief Read pin state using appropriate method based on connection mode
     * @param pin GPIO pin identifier
     * @return Current pin state
     */
    bool readPinState(const std::string& pin) {
        if (!deviceInitialized_ || deviceHandle_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        return useSerialMode_ ? readPinStateSerial(pin) : readPinStateUsb(pin);
    }

    /**
     * @brief Read pin state via USB device
     * @param pin GPIO pin identifier
     * @return Pin state
     */
    bool readPinStateUsb(const std::string& pin) {
        // Build command buffer containing GPIO pin number
        unsigned char buffer[8] = {0};
        int pinNumber = std::stoi(pin);

        // Command format: [command code 0x01][pin number]
        buffer[0] = 0x01;  // Read command
        buffer[1] = static_cast<unsigned char>(pinNumber);

        DWORD bytesWritten = 0;
        if (!WriteFile(deviceHandle_, buffer, 2, &bytesWritten, NULL)) {
            spdlog::error("Failed to write USB GPIO command: {}",
                          GetLastError());
            return false;
        }

        // Read response
        memset(buffer, 0, sizeof(buffer));
        DWORD bytesRead = 0;

        if (!ReadFile(deviceHandle_, buffer, sizeof(buffer), &bytesRead,
                      NULL)) {
            spdlog::error("Failed to read USB GPIO state: {}", GetLastError());
            return false;
        }

        // Assume first byte is status value
        return (buffer[0] != 0);
    }

    /**
     * @brief Read pin state via serial port
     * @param pin GPIO pin identifier
     * @return Pin state
     */
    bool readPinStateSerial(const std::string& pin) {
        // Build serial command
        std::string cmd = "READ " + pin + "\r\n";
        DWORD bytesWritten = 0;

        if (!WriteFile(deviceHandle_, cmd.c_str(), cmd.length(), &bytesWritten,
                       NULL)) {
            spdlog::error("Failed to write serial GPIO command: {}",
                          GetLastError());
            return false;
        }

        // Read response
        char buffer[64] = {0};
        DWORD bytesRead = 0;

        if (!ReadFile(deviceHandle_, buffer, sizeof(buffer) - 1, &bytesRead,
                      NULL)) {
            spdlog::error("Failed to read serial GPIO state: {}",
                          GetLastError());
            return false;
        }

        // Parse response string
        std::string response(buffer, bytesRead);
        return (response.find("HIGH") != std::string::npos ||
                response.find("1") != std::string::npos);
    }

    /**
     * @brief Start GPIO monitoring thread
     */
    void startMonitorThread() {
        if (monitorThreadRunning_)
            return;

        monitorThreadRunning_ = true;
        monitorThread_ = std::thread([this]() {
            while (monitorThreadRunning_) {
                // Periodically poll pin states
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (auto& [pin, callback] : callbacks_) {
                        bool currentState = readPinState(pin);
                        if (pinStates_[pin] != currentState) {
                            pinStates_[pin] = currentState;
                            try {
                                callback(currentState);
                            } catch (const std::exception& e) {
                                spdlog::error("Exception in GPIO callback: {}",
                                              e.what());
                            } catch (...) {
                                spdlog::error(
                                    "Unknown exception in GPIO callback");
                            }
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    /**
     * @brief Stop GPIO monitoring thread
     */
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
    std::unordered_map<std::string, bool> pinStates_;
    std::thread monitorThread_;
    std::atomic<bool> monitorThreadRunning_;

    // Hardware-related members
    bool deviceInitialized_;
    HANDLE deviceHandle_;
    bool useSerialMode_;  // Flag for serial mode vs USB mode
};

#else
/**
 * @brief Linux GPIO callback manager using sysfs interface
 *
 * This class provides real GPIO functionality on Linux platforms using
 * the standard sysfs GPIO interface with epoll for efficient monitoring.
 */
class GPIOCallbackManager {
public:
    static GPIOCallbackManager& getInstance() {
        static GPIOCallbackManager instance;
        return instance;
    }

    /**
     * @brief Register a callback for GPIO pin state changes
     * @param pin GPIO pin identifier
     * @param callback Function to call when pin state changes
     */
    void registerCallback(const std::string& pin,
                          std::function<void(bool)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Start monitor thread if this is our first callback
        if (callbacks_.empty() && !monitorThreadRunning_) {
            startMonitorThread();
        }

        callbacks_[pin] = std::move(callback);

        // Add this pin to monitoring if not already present
        setupPinMonitoring(pin);
    }

    /**
     * @brief Unregister callback for specified pin
     * @param pin GPIO pin identifier
     */
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

        // Stop thread if no more callbacks
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
            spdlog::error("Failed to create epoll instance: {}",
                          strerror(errno));
        }
    }

    /**
     * @brief Setup monitoring for specified GPIO pin
     * @param pin GPIO pin identifier
     */
    void setupPinMonitoring(const std::string& pin) {
        // Check if already monitoring this pin
        if (fdMap_.find(pin) != fdMap_.end()) {
            return;
        }

        std::string path = std::string(GPIO_PATH) + "/gpio" + pin + "/value";
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            spdlog::error("Failed to open gpio value for reading: {}",
                          strerror(errno));
            return;
        }

        // Configure pin for edge-triggered interrupts
        std::string edgePath = std::string(GPIO_PATH) + "/gpio" + pin + "/edge";
        int edgeFd = open(edgePath.c_str(), O_WRONLY);
        if (edgeFd >= 0) {
            ssize_t res = write(edgeFd, "both", 4);
            if (res != 4) {
                spdlog::warn("Failed to set edge to 'both' for GPIO {}", pin);
            }
            close(edgeFd);
        }

        // Add to epoll
        struct epoll_event ev;
        ev.events = EPOLLPRI | EPOLLET;
        ev.data.fd = fd;

        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            spdlog::error("Failed to add fd to epoll: {}", strerror(errno));
            close(fd);
            return;
        }

        // Initial read to clear any pending interrupts
        char buffer[2];
        lseek(fd, 0, SEEK_SET);
        ssize_t res = read(fd, buffer, sizeof(buffer));
        if (res < 0) {
            spdlog::warn("Failed to read initial GPIO value: {}",
                         strerror(errno));
        }

        fdMap_[pin] = fd;
    }

    /**
     * @brief Start GPIO monitoring thread using epoll
     */
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
                        spdlog::error("epoll_wait failed: {}", strerror(errno));
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
                                        spdlog::error(
                                            "Exception in GPIO callback: {}",
                                            e.what());
                                    } catch (...) {
                                        spdlog::error(
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

    /**
     * @brief Stop GPIO monitoring thread
     */
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

/**
 * @brief GPIO implementation class providing platform-specific functionality
 */
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
          debounceActive_(false),
          pwmThreadRunning_(false) {
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
          debounceActive_(false),
          pwmThreadRunning_(false) {
        exportGPIO();
        setGPIODirection(directionToString(direction));

        if (direction == Direction::OUTPUT) {
            setGPIOValue(initialValue ? "1" : "0");
        }
    }

    ~Impl() {
        try {
            stopCallbacks();
            unexportGPIO();
        } catch (...) {
            // Suppress all exceptions in destructor
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
        // Store pull mode - platform-specific implementation may be required
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

    /**
     * @brief Setup PWM signal generation on GPIO pin
     * @param frequency PWM frequency in Hz
     * @param dutyCycle Duty cycle (0.0 to 1.0)
     * @param mode PWM mode (hardware or software)
     * @return true if PWM setup successful
     */
    bool setPwm(double frequency, double dutyCycle, PwmMode mode) {
        if (frequency <= 0 || dutyCycle < 0 || dutyCycle > 1.0) {
            spdlog::error(
                "Invalid PWM parameters: frequency={:.2f}Hz, dutyCycle={:.2f}",
                frequency, dutyCycle);
            return false;
        }

        if (direction_ != Direction::OUTPUT) {
            spdlog::error("Cannot setup PWM on input GPIO pin {}", pin_);
            return false;
        }

        // Stop existing PWM if active
        if (pwmActive_) {
            stopPwm();
        }

        pwmFrequency_ = frequency;
        pwmDutyCycle_ = dutyCycle;
        pwmMode_ = mode;

#ifdef _WIN32
        // Windows simulation always uses software PWM
        return startSoftwarePwm();
#else
        // Try hardware PWM on Linux, fallback to software
        if (mode == PwmMode::HARDWARE && tryHardwarePwm()) {
            spdlog::info("Hardware PWM started on pin {}: {:.2f}Hz, {:.2f}%%",
                         pin_, frequency, dutyCycle * 100);
            return true;
        } else {
            return startSoftwarePwm();
        }
#endif
    }

    /**
     * @brief Update PWM duty cycle while keeping frequency unchanged
     * @param dutyCycle New duty cycle (0.0 to 1.0)
     * @return true if update successful
     */
    bool updatePwmDutyCycle(double dutyCycle) {
        if (!pwmActive_) {
            spdlog::error("Cannot update duty cycle, PWM not active on pin {}",
                          pin_);
            return false;
        }

        if (dutyCycle < 0 || dutyCycle > 1.0) {
            spdlog::error("Invalid duty cycle: {:.2f}", dutyCycle);
            return false;
        }

        pwmDutyCycle_ = dutyCycle;

#ifdef _WIN32
        // Software PWM thread automatically uses new duty cycle
        return true;
#else
        if (pwmMode_ == PwmMode::HARDWARE) {
            // Update hardware PWM duty cycle
            std::string pwmPath = std::string(GPIO_PATH) + "/pwm" + pin_;
            std::string dutyCyclePath = pwmPath + "/duty_cycle";

            try {
                int period = static_cast<int>(1.0e9 / pwmFrequency_);
                int onTime = static_cast<int>(period * dutyCycle);

                std::ofstream fs(dutyCyclePath);
                if (!fs) {
                    spdlog::error("Failed to open {} for writing",
                                  dutyCyclePath);
                    return false;
                }
                fs << onTime;
                return true;
            } catch (const std::exception& e) {
                spdlog::error("Failed to update hardware PWM duty cycle: {}",
                              e.what());
                return false;
            }
        }
#endif
        return true;
    }

    /**
     * @brief Stop PWM signal generation
     */
    void stopPwm() {
        if (!pwmActive_) {
            return;
        }

        spdlog::info("Stopping PWM on pin {}", pin_);

#ifndef _WIN32
        if (pwmMode_ == PwmMode::HARDWARE) {
            // Disable hardware PWM
            try {
                std::string pwmPath = std::string(GPIO_PATH) + "/pwm" + pin_;
                std::string enablePath = pwmPath + "/enable";

                std::ofstream fs(enablePath);
                if (fs) {
                    fs << "0";
                }
            } catch (const std::exception& e) {
                spdlog::error("Error stopping hardware PWM: {}", e.what());
            }
        }
#endif

        // Stop software PWM thread
        if (pwmThread_.joinable()) {
            pwmThreadRunning_ = false;
            pwmThread_.join();
        }

        pwmActive_ = false;
    }

#ifndef _WIN32
    /**
     * @brief Attempt to configure hardware PWM
     * @return true if hardware PWM available and configured
     */
    bool tryHardwarePwm() {
        std::string pwmPath = std::string(GPIO_PATH) + "/pwm" + pin_;

        // Check if hardware PWM is available
        if (!std::filesystem::exists(pwmPath)) {
            spdlog::info("Hardware PWM not available for pin {}", pin_);
            return false;
        }

        try {
            // Configure hardware PWM
            std::string periodPath = pwmPath + "/period";
            std::string dutyCyclePath = pwmPath + "/duty_cycle";
            std::string enablePath = pwmPath + "/enable";

            // Disable PWM first
            std::ofstream enableFs(enablePath);
            if (!enableFs) {
                spdlog::error("Failed to open {} for writing", enablePath);
                return false;
            }
            enableFs << "0";
            enableFs.close();

            // Set period in nanoseconds
            int period = static_cast<int>(1.0e9 / pwmFrequency_);
            std::ofstream periodFs(periodPath);
            if (!periodFs) {
                spdlog::error("Failed to open {} for writing", periodPath);
                return false;
            }
            periodFs << period;
            periodFs.close();

            // Set duty cycle
            int onTime = static_cast<int>(period * pwmDutyCycle_);
            std::ofstream dutyFs(dutyCyclePath);
            if (!dutyFs) {
                spdlog::error("Failed to open {} for writing", dutyCyclePath);
                return false;
            }
            dutyFs << onTime;
            dutyFs.close();

            // Enable PWM
            enableFs.open(enablePath);
            if (!enableFs) {
                spdlog::error("Failed to open {} for writing", enablePath);
                return false;
            }
            enableFs << "1";

            pwmActive_ = true;
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to setup hardware PWM: {}", e.what());
            return false;
        }
    }
#endif

    /**
     * @brief Start software PWM implementation
     * @return true if software PWM started successfully
     */
    bool startSoftwarePwm() {
        if (pwmActive_ && pwmThread_.joinable()) {
            spdlog::error("PWM already active on pin {}", pin_);
            return false;
        }

        pwmThreadRunning_ = true;
        pwmThread_ = std::thread([this]() {
            spdlog::info("Software PWM started on pin {}: {:.2f}Hz, {:.2f}%%",
                         pin_, pwmFrequency_, pwmDutyCycle_ * 100);

            // Calculate period time in microseconds
            const auto periodUs =
                static_cast<int64_t>(1000000.0 / pwmFrequency_);

            while (pwmThreadRunning_) {
                auto startTime = std::chrono::steady_clock::now();

                // Calculate high time in microseconds
                auto highTimeUs =
                    static_cast<int64_t>(periodUs * pwmDutyCycle_);

                // Handle edge cases for duty cycle
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

                // Set high level
                setValue(true);
                std::this_thread::sleep_for(
                    std::chrono::microseconds(highTimeUs));

                // Set low level
                setValue(false);

                // Calculate remaining low time
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

    /**
     * @brief Register callback for GPIO value changes
     * @param callback Function to call when value changes
     * @return true if callback registered successfully
     */
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

    /**
     * @brief Register callback for specific edge changes
     * @param edge Edge type to monitor
     * @param callback Function to call when edge detected
     * @return true if callback registered successfully
     */
    bool onEdgeChange(Edge edge, std::function<void(bool)> callback) {
        if (direction_ != Direction::INPUT) {
            THROW_RUNTIME_ERROR(
                "Edge change callback only works on input GPIO pins");
        }

        setEdge(edge);
        GPIOCallbackManager::getInstance().registerCallback(
            pin_, std::move(callback));
        return true;
    }

    /**
     * @brief Stop all callbacks for this GPIO pin
     */
    void stopCallbacks() {
        GPIOCallbackManager::getInstance().unregisterCallback(pin_);
    }

    /**
     * @brief Setup button debounce functionality
     * @param callback Function to call when button pressed (debounced)
     * @param debounceTimeMs Debounce time in milliseconds
     * @return true if debounce setup successful
     */
    bool setupButtonDebounce(std::function<void()> callback,
                             unsigned int debounceTimeMs) {
        if (direction_ != Direction::INPUT) {
            spdlog::error("Button debounce only works on input GPIO pins");
            return false;
        }

        if (debounceActive_) {
            spdlog::error("Button debounce already active on pin {}", pin_);
            return false;
        }

        setEdge(Edge::BOTH);

        debounceActive_ = true;
        debouncePeriodMs_ = debounceTimeMs;
        lastDebounceTime_ = std::chrono::steady_clock::now();

        // Register debounced callback
        return onValueChange([this, callback](bool state) {
            // Trigger only on button press (low level)
            if (state)
                return;

            auto now = std::chrono::steady_clock::now();
            auto elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - lastDebounceTime_)
                    .count();

            if (elapsedMs > debouncePeriodMs_) {
                lastDebounceTime_ = now;
                callback();
            }
        });
    }

    /**
     * @brief Setup interrupt counter for edge detection
     * @param edge Edge type to count
     * @return true if counter setup successful
     */
    bool setupInterruptCounter(Edge edge) {
        if (direction_ != Direction::INPUT) {
            spdlog::error("Interrupt counter only works on input GPIO pins");
            return false;
        }

        setEdge(edge);

        interruptCount_ = 0;
        interruptCounterActive_ = true;

        // Register callback to increment counter
        return onValueChange([this](bool /*state*/) {
            if (interruptCounterActive_) {
                interruptCount_++;
            }
        });
    }

    /**
     * @brief Get interrupt count with optional reset
     * @param resetAfterReading Reset counter after reading if true
     * @return Current interrupt count
     */
    uint64_t getInterruptCount(bool resetAfterReading) {
        uint64_t count = interruptCount_;
        if (resetAfterReading) {
            interruptCount_ = 0;
        }
        return count;
    }

    /**
     * @brief Reset interrupt counter to zero
     */
    void resetInterruptCount() { interruptCount_ = 0; }

    /**
     * @brief Static method to register callback for any pin
     * @param pin GPIO pin identifier
     * @param callback Function to call when pin state changes
     */
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
    double pwmFrequency_;
    double pwmDutyCycle_;
    PwmMode pwmMode_;
    bool interruptCounterActive_;
    std::atomic<uint64_t> interruptCount_;
    bool debounceActive_;
    std::thread pwmThread_;
    std::atomic<bool> pwmThreadRunning_;
    unsigned int debouncePeriodMs_;
    std::chrono::steady_clock::time_point lastDebounceTime_;

#ifdef _WIN32
    // Windows simulation state
    bool currentValue_ = false;

    void exportGPIO() {
        spdlog::info("GPIO pin {} exported (Windows simulation)", pin_);
    }

    void unexportGPIO() {
        spdlog::info("GPIO pin {} unexported (Windows simulation)", pin_);
    }

    void setGPIODirection(const std::string& direction) {
        spdlog::info("GPIO pin {} direction set to {} (Windows simulation)",
                     pin_, direction);
    }

    void setGPIOValue(const std::string& value) {
        currentValue_ = (value == "1");
        spdlog::info("GPIO pin {} value set to {} (Windows simulation)", pin_,
                     value);
    }

    void setGPIOEdge(const std::string& edge) {
        spdlog::info("GPIO pin {} edge set to {} (Windows simulation)", pin_,
                     edge);
    }

    bool readGPIOValue() const { return currentValue_; }
#else
    // Linux implementation using sysfs
    void exportGPIO() {
        // Check if GPIO is already exported
        std::string path = std::string(GPIO_PATH) + "/gpio" + pin_;
        if (access(path.c_str(), F_OK) == 0) {
            return;  // Already exported
        }

        executeGPIOCommand(GPIO_EXPORT, pin_);

        // Wait for GPIO to be properly exported
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

// GPIO public interface implementation
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

bool GPIO::setPwm(double frequency, double dutyCycle, PwmMode mode) {
    return impl_->setPwm(frequency, dutyCycle, mode);
}

bool GPIO::updatePwmDutyCycle(double dutyCycle) {
    return impl_->updatePwmDutyCycle(dutyCycle);
}

void GPIO::stopPwm() { impl_->stopPwm(); }

bool GPIO::setupButtonDebounce(std::function<void()> callback,
                               unsigned int debounceTimeMs) {
    return impl_->setupButtonDebounce(std::move(callback), debounceTimeMs);
}

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

/**
 * @brief Shift register implementation for controlling multiple outputs
 */
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
    // Initialize all pins to low
    dataPin_->setValue(false);
    clockPin_->setValue(false);
    latchPin_->setValue(false);
}

GPIO::ShiftRegister::~ShiftRegister() = default;

/**
 * @brief Shift data out to the shift register
 * @param data Data to shift out
 * @param msbFirst Shift MSB first if true, LSB first if false
 */
void GPIO::ShiftRegister::shiftOut(uint32_t data, bool msbFirst) {
    state_ = data;

    // Pull latch low to prepare for data transmission
    latchPin_->setValue(false);

    // Calculate number of bits to shift
    uint8_t bitsToShift = numBits_ <= 32 ? numBits_ : 32;

    // Shift out data bits
    for (uint8_t i = 0; i < bitsToShift; i++) {
        // Determine current bit to send
        uint8_t bitPos = msbFirst ? (bitsToShift - 1 - i) : i;
        bool bitValue = ((data >> bitPos) & 0x01) != 0;

        // Set data pin
        dataPin_->setValue(bitValue);

        // Clock rising edge to latch data
        clockPin_->setValue(true);
        std::this_thread::sleep_for(std::chrono::microseconds(1));

        // Clock falling edge
        clockPin_->setValue(false);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    // Pull latch high to output data to ports
    latchPin_->setValue(true);
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    latchPin_->setValue(false);
}

/**
 * @brief Set individual bit in shift register
 * @param position Bit position (0-based)
 * @param value Bit value to set
 */
void GPIO::ShiftRegister::setBit(uint8_t position, bool value) {
    if (position >= numBits_) {
        spdlog::error("Bit position {} out of range for {}-bit shift register",
                      position, numBits_);
        return;
    }

    uint32_t newState = state_;

    if (value) {
        newState |= (1UL << position);
    } else {
        newState &= ~(1UL << position);
    }

    // Update shift register if state changed
    if (newState != state_) {
        shiftOut(newState, true);
    }
}

uint32_t GPIO::ShiftRegister::getState() const { return state_; }

void GPIO::ShiftRegister::clear() { shiftOut(0, true); }

/**
 * @brief GPIO group implementation for controlling multiple pins together
 */
GPIO::GPIOGroup::GPIOGroup(const std::vector<std::string>& pins) {
    for (const auto& pin : pins) {
        gpios_.push_back(std::make_unique<GPIO>(pin));
    }
}

GPIO::GPIOGroup::~GPIOGroup() = default;

/**
 * @brief Set values for all GPIOs in the group
 * @param values Vector of boolean values for each GPIO
 */
void GPIO::GPIOGroup::setValues(const std::vector<bool>& values) {
    if (values.size() != gpios_.size()) {
        THROW_RUNTIME_ERROR("Values count doesn't match GPIO count");
    }

    for (size_t i = 0; i < gpios_.size(); ++i) {
        gpios_[i]->setValue(values[i]);
    }
}

/**
 * @brief Get values from all GPIOs in the group
 * @return Vector of current GPIO values
 */
std::vector<bool> GPIO::GPIOGroup::getValues() const {
    std::vector<bool> values;
    values.reserve(gpios_.size());

    for (const auto& gpio : gpios_) {
        values.push_back(gpio->getValue());
    }

    return values;
}

/**
 * @brief Set direction for all GPIOs in the group
 * @param direction Direction to set for all GPIOs
 */
void GPIO::GPIOGroup::setDirection(Direction direction) {
    for (auto& gpio : gpios_) {
        gpio->setDirection(direction);
    }
}

#ifdef _WIN32
/**
 * @brief Windows-specific helper functions for GPIO simulation
 */
namespace windows {

/**
 * @brief Simulate GPIO state change for testing purposes
 * @param pin GPIO pin identifier
 * @param state New pin state
 */
void simulateGPIOStateChange(const std::string& pin, bool state) {
    GPIOCallbackManager::getInstance().simulatePinStateChange(pin, state);
}

}  // namespace windows
#endif

}  // namespace atom::system
