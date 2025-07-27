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
    result.reserve(256); // Pre-allocate reasonable buffer size

    const auto startTime = std::chrono::steady_clock::now();
    constexpr size_t CHUNK_SIZE = 64; // Read in larger chunks
    std::vector<uint8_t> buffer;
    buffer.reserve(CHUNK_SIZE);

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                  startTime);

        if (elapsed >= timeout) {
            throw SerialTimeoutException("Waiting for terminator timed out");
        }

        const auto remainingTime = timeout - elapsed;

        // Try to read available data first, fall back to smaller read if nothing available
        auto chunk = impl_->readAvailable();
        if (chunk.empty()) {
            chunk = impl_->readExactly(1, remainingTime);
            if (chunk.empty()) {
                continue;
            }
        }

        // Process the chunk looking for terminator
        for (size_t i = 0; i < chunk.size(); ++i) {
            const char c = static_cast<char>(chunk[i]);
            if (c == terminator) {
                if (includeTerminator) {
                    result.push_back(c);
                }
                return result;
            }
            result.push_back(c);
        }
    }
}

std::vector<uint8_t> SerialPort::readUntilSequence(
    std::span<const uint8_t> sequence, std::chrono::milliseconds timeout,
    bool includeSequence) {
    if (sequence.empty()) {
        return {};
    }

    std::vector<uint8_t> result;
    result.reserve(512); // Pre-allocate reasonable buffer size

    const auto startTime = std::chrono::steady_clock::now();

    // Use Boyer-Moore-like approach for efficient sequence matching
    const size_t seqLen = sequence.size();
    size_t matchPos = 0;

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

        // Try to read available data first, fall back to smaller read if nothing available
        auto chunk = impl_->readAvailable();
        if (chunk.empty()) {
            chunk = impl_->readExactly(1, remainingTime);
            if (chunk.empty()) {
                continue;
            }
        }

        // Process the chunk looking for sequence
        for (size_t i = 0; i < chunk.size(); ++i) {
            const uint8_t byte = chunk[i];
            result.push_back(byte);

            if (byte == sequence[matchPos]) {
                ++matchPos;
                if (matchPos == seqLen) {
                    // Found complete sequence
                    if (!includeSequence) {
                        result.erase(result.end() - static_cast<long>(seqLen),
                                     result.end());
                    }
                    return result;
                }
            } else {
                // Reset match position, but check if current byte starts a new match
                matchPos = (byte == sequence[0]) ? 1 : 0;
            }
        }
    }
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
    // Create a promise/future pair for the result
    auto promise = std::make_shared<std::promise<size_t>>();
    auto future = promise->get_future();

    // Copy data to ensure it remains valid during async operation
    std::vector<uint8_t> dataCopy(data.begin(), data.end());

    // Use thread pool or async mechanism for better resource management
    try {
        // Use std::async with async policy for better thread management
        // Store the future to ensure the task runs
        static thread_local std::vector<std::future<void>> asyncTasks;

        auto task = std::async(std::launch::async, [this, promise, dataCopy = std::move(dataCopy)]() {
            try {
                auto result = write(std::span<const uint8_t>(dataCopy));
                promise->set_value(result);
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        // Clean up completed tasks periodically
        asyncTasks.erase(
            std::remove_if(asyncTasks.begin(), asyncTasks.end(),
                [](const std::future<void>& f) {
                    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                }),
            asyncTasks.end());

        asyncTasks.push_back(std::move(task));
    } catch (...) {
        promise->set_exception(std::current_exception());
    }

    return future;
}

std::future<size_t> SerialPort::asyncWrite(std::string_view data) {
    // Convert to span and delegate to the optimized version
    return asyncWrite(std::span<const uint8_t>(
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

std::optional<std::string> SerialPort::tryOpen(std::string_view portName,
                                               const SerialConfig& config) {
    try {
        open(portName, config);
        return std::nullopt;
    } catch (const SerialException& e) {
        return e.what();
    }
}

Result<std::vector<uint8_t>> SerialPort::tryRead(size_t maxBytes) noexcept {
    try {
        return read(maxBytes);
    } catch (const SerialPortNotOpenException&) {
        return SerialError{SerialError::Code::PortNotOpen, "Port is not open"};
    } catch (const SerialTimeoutException&) {
        return SerialError{SerialError::Code::Timeout, "Read operation timed out"};
    } catch (const SerialIOException& e) {
        return SerialError{SerialError::Code::IOError, e.what()};
    } catch (const std::exception& e) {
        return SerialError{SerialError::Code::IOError, e.what()};
    }
}

Result<size_t> SerialPort::tryWrite(std::span<const uint8_t> data) noexcept {
    try {
        return write(data);
    } catch (const SerialPortNotOpenException&) {
        return SerialError{SerialError::Code::PortNotOpen, "Port is not open"};
    } catch (const SerialTimeoutException&) {
        return SerialError{SerialError::Code::Timeout, "Write operation timed out"};
    } catch (const SerialIOException& e) {
        return SerialError{SerialError::Code::IOError, e.what()};
    } catch (const std::exception& e) {
        return SerialError{SerialError::Code::IOError, e.what()};
    }
}

Result<size_t> SerialPort::tryWrite(std::string_view data) noexcept {
    return tryWrite(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

}  // namespace serial
