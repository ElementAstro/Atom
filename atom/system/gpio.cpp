#include "gpio.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <atomic>
#include <csignal>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_PATH "/sys/class/gpio"

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

// Global callback manager for all GPIO pins
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

}  // namespace atom::system
