#include "bluetooth_serial.hpp"

#if defined(_WIN32)
#include "bluetooth_serial_win.hpp"
#elif defined(__linux__)
#include "bluetooth_serial_unix.hpp"
#elif defined(__APPLE__)
// macOS实现在BluetoothSerialMac.cpp中
#else
#error "不支持的平台"
#endif

namespace serial {

BluetoothSerial::BluetoothSerial()
    : impl_(std::make_unique<BluetoothSerialImpl>()) {}

BluetoothSerial::~BluetoothSerial() = default;

BluetoothSerial::BluetoothSerial(BluetoothSerial&& other) noexcept
    : impl_(std::move(other.impl_)) {
    other.impl_ = std::make_unique<BluetoothSerialImpl>();
}

BluetoothSerial& BluetoothSerial::operator=(BluetoothSerial&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        other.impl_ = std::make_unique<BluetoothSerialImpl>();
    }
    return *this;
}

bool BluetoothSerial::isBluetoothEnabled() const {
    return impl_->isBluetoothEnabled();
}

void BluetoothSerial::enableBluetooth(bool enable) {
    impl_->enableBluetooth(enable);
}

std::vector<BluetoothDeviceInfo> BluetoothSerial::scanDevices(
    std::chrono::seconds timeout) {
    return impl_->scanDevices(timeout);
}

void BluetoothSerial::scanDevicesAsync(
    std::function<void(const BluetoothDeviceInfo&)> onDeviceFound,
    std::function<void()> onScanComplete, std::chrono::seconds timeout) {
    impl_->scanDevicesAsync(std::move(onDeviceFound), std::move(onScanComplete),
                            timeout);
}

void BluetoothSerial::stopScan() { impl_->stopScan(); }

void BluetoothSerial::connect(const std::string& address,
                              const BluetoothConfig& config) {
    impl_->connect(address, config);
}

void BluetoothSerial::disconnect() { impl_->disconnect(); }

bool BluetoothSerial::isConnected() const { return impl_->isConnected(); }

std::optional<BluetoothDeviceInfo> BluetoothSerial::getConnectedDevice() const {
    return impl_->getConnectedDevice();
}

bool BluetoothSerial::pair(const std::string& address, const std::string& pin) {
    return impl_->pair(address, pin);
}

bool BluetoothSerial::unpair(const std::string& address) {
    return impl_->unpair(address);
}

std::vector<BluetoothDeviceInfo> BluetoothSerial::getPairedDevices() {
    return impl_->getPairedDevices();
}

std::vector<uint8_t> BluetoothSerial::read(size_t maxBytes) {
    return impl_->read(maxBytes);
}

std::vector<uint8_t> BluetoothSerial::readExactly(
    size_t bytes, std::chrono::milliseconds timeout) {
    return impl_->readExactly(bytes, timeout);
}

void BluetoothSerial::asyncRead(
    size_t maxBytes, std::function<void(std::vector<uint8_t>)> callback) {
    impl_->asyncRead(maxBytes, std::move(callback));
}

std::vector<uint8_t> BluetoothSerial::readAvailable() {
    return impl_->readAvailable();
}

size_t BluetoothSerial::write(std::span<const uint8_t> data) {
    return impl_->write(data);
}

size_t BluetoothSerial::write(const std::string& data) {
    return impl_->write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

void BluetoothSerial::flush() { impl_->flush(); }

size_t BluetoothSerial::available() const { return impl_->available(); }

void BluetoothSerial::setConnectionListener(
    std::function<void(bool connected)> listener) {
    impl_->setConnectionListener(std::move(listener));
}

BluetoothSerial::Statistics BluetoothSerial::getStatistics() const {
    return impl_->getStatistics();
}

}  // namespace serial
