#include "serial_port.hpp"

#if defined(_WIN32)
#include "SerialPortWin.hpp"
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

void SerialPort::asyncRead(size_t maxBytes,
                           std::function<void(std::vector<uint8_t>)> callback) {
    impl_->asyncRead(maxBytes, std::move(callback));
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