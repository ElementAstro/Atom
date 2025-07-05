#include "serial_port.hpp"

#if defined(_WIN32)
#include "serial_port_win.hpp"
#elif defined(__unix__) || defined(__APPLE__)
#include "serial_port_unix.hpp"
#else
#error "Unsupported platform"
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

void SerialPort::open(std::string_view portName, const SerialConfig& config) {
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
    const auto startTime = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                  startTime);

        if (elapsed >= timeout) {
            throw SerialTimeoutException("Waiting for terminator timed out");
        }

        const auto remainingTime = timeout - elapsed;
        auto buffer = impl_->readExactly(1, remainingTime);

        if (buffer.empty()) {
            continue;
        }

        const char c = static_cast<char>(buffer[0]);
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

    const auto startTime = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                  startTime);

        if (elapsed >= timeout) {
            throw SerialTimeoutException(
                "Waiting for termination sequence timed out");
        }

        const auto remainingTime = timeout - elapsed;
        auto chunk = impl_->readExactly(1, remainingTime);

        if (chunk.empty()) {
            continue;
        }

        const uint8_t byte = chunk[0];
        result.push_back(byte);

        buffer.push_back(byte);
        if (buffer.size() > sequence.size()) {
            buffer.erase(buffer.begin());
        }

        if (buffer.size() == sequence.size() &&
            std::equal(buffer.begin(), buffer.end(), sequence.begin())) {
            if (!includeSequence) {
                result.erase(result.end() - static_cast<long>(sequence.size()),
                             result.end());
            }
            break;
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
    auto future = promise->get_future();

    impl_->asyncRead(maxBytes, [promise](std::vector<uint8_t> data) {
        promise->set_value(std::move(data));
    });

    return future;
}

std::vector<uint8_t> SerialPort::readAvailable() {
    return impl_->readAvailable();
}

size_t SerialPort::write(std::string_view data) {
    return impl_->write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

size_t SerialPort::write(std::span<const uint8_t> data) {
    return impl_->write(data);
}

std::future<size_t> SerialPort::asyncWrite(std::span<const uint8_t> data) {
    return std::async(std::launch::async,
                      [this, data]() { return write(data); });
}

std::future<size_t> SerialPort::asyncWrite(std::string_view data) {
    return std::async(std::launch::async, [this, data = std::string(data)]() {
        return write(data);
    });
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

std::optional<std::string> SerialPort::tryOpen(std::string_view portName,
                                               const SerialConfig& config) {
    try {
        open(portName, config);
        return std::nullopt;
    } catch (const SerialException& e) {
        return e.what();
    }
}

}  // namespace serial