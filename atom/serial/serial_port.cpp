#include "serial_port.hpp"

#if defined(_WIN32)
#include "serial_port_win.hpp"
#elif defined(__unix__) || defined(__APPLE__)
#include "serial_port_unix.hpp"
#else
#error "不支持的平台"
#endif

namespace serial {

SerialPort::SerialPort() : impl_(std::make_unique<SerialPortImpl>()) {}

SerialPort::~SerialPort() = default;

SerialPort::SerialPort(SerialPort&& other) noexcept
    : impl_(std::move(other.impl_)) {
    other.impl_ = std::make_unique<SerialPortImpl>();
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        other.impl_ = std::make_unique<SerialPortImpl>();
    }
    return *this;
}

void SerialPort::open(const std::string& portName, const SerialConfig& config) {
    impl_->open(portName, config);
}

void SerialPort::close() { impl_->close(); }

bool SerialPort::isOpen() const { return impl_->isOpen(); }

std::vector<uint8_t> SerialPort::read(size_t maxBytes) {
    return impl_->read(maxBytes);
}

std::vector<uint8_t> SerialPort::readExactly(
    size_t bytes, std::chrono::milliseconds timeout) {
    return impl_->readExactly(bytes, timeout);
}

std::string SerialPort::readUntil(char terminator,
                                  std::chrono::milliseconds timeout,
                                  bool includeTerminator) {
    std::string result;
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        // 检查是否超时
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime);
        if (elapsed >= timeout) {
            throw SerialTimeoutException("等待终止符超时");
        }

        // 计算剩余超时时间
        auto remainingTime = timeout - elapsed;

        // 读取一个字节
        auto buffer = impl_->readExactly(1, remainingTime);
        if (buffer.empty()) {
            continue;
        }

        char c = static_cast<char>(buffer[0]);
        if (c == terminator) {
            if (includeTerminator) {
                result.push_back(c);
            }
            break;
        }

        result.push_back(c);
    }

    return result;
}

std::vector<uint8_t> SerialPort::readUntilSequence(
    std::span<const uint8_t> sequence, std::chrono::milliseconds timeout,
    bool includeSequence) {
    if (sequence.empty()) {
        return {};
    }

    std::vector<uint8_t> result;
    std::vector<uint8_t> buffer;
    buffer.reserve(sequence.size());

    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        // 检查是否超时
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime);
        if (elapsed >= timeout) {
            throw SerialTimeoutException("等待终止序列超时");
        }

        // 计算剩余超时时间
        auto remainingTime = timeout - elapsed;

        // 读取一个字节
        auto chunk = impl_->readExactly(1, remainingTime);
        if (chunk.empty()) {
            continue;
        }

        uint8_t byte = chunk[0];
        result.push_back(byte);

        // 更新缓冲区
        buffer.push_back(byte);
        if (buffer.size() > sequence.size()) {
            buffer.erase(buffer.begin());
        }

        // 检查是否匹配
        if (buffer.size() == sequence.size()) {
            bool match = true;
            for (size_t i = 0; i < sequence.size(); ++i) {
                if (buffer[i] != sequence[i]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                if (!includeSequence) {
                    // 从结果中删除序列
                    result.erase(result.end() - sequence.size(), result.end());
                }
                break;
            }
        }
    }

    return result;
}

void SerialPort::asyncRead(size_t maxBytes,
                           std::function<void(std::vector<uint8_t>)> callback) {
    impl_->asyncRead(maxBytes, std::move(callback));
}

std::future<std::vector<uint8_t>> SerialPort::asyncReadFuture(size_t maxBytes) {
    auto promise = std::make_shared<std::promise<std::vector<uint8_t>>>();
    std::future<std::vector<uint8_t>> future = promise->get_future();

    impl_->asyncRead(maxBytes, [promise](std::vector<uint8_t> data) {
        promise->set_value(std::move(data));
    });

    return future;
}

std::vector<uint8_t> SerialPort::readAvailable() {
    return impl_->readAvailable();
}

size_t SerialPort::write(std::span<const uint8_t> data) {
    return impl_->write(data);
}

size_t SerialPort::write(const std::string& data) {
    return impl_->write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

void SerialPort::flush() { impl_->flush(); }

void SerialPort::drain() { impl_->drain(); }

size_t SerialPort::available() const { return impl_->available(); }

void SerialPort::setConfig(const SerialConfig& config) {
    impl_->setConfig(config);
}

SerialConfig SerialPort::getConfig() const { return impl_->getConfig(); }

void SerialPort::setDTR(bool value) { impl_->setDTR(value); }

void SerialPort::setRTS(bool value) { impl_->setRTS(value); }

bool SerialPort::getCTS() const { return impl_->getCTS(); }

bool SerialPort::getDSR() const { return impl_->getDSR(); }

bool SerialPort::getRI() const { return impl_->getRI(); }

bool SerialPort::getCD() const { return impl_->getCD(); }

std::string SerialPort::getPortName() const { return impl_->getPortName(); }

std::vector<std::string> SerialPort::getAvailablePorts() {
    return SerialPortImpl::getAvailablePorts();
}

}  // namespace serial