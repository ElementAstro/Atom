#include "usb.hpp"

#include <iomanip>
#include <sstream>

namespace atom::serial {

UsbTransfer::UsbTransfer()
    : transfer_(libusb_alloc_transfer(0)),
      completed_(false),
      data_buffer_(nullptr) {
    if (!transfer_) {
        throw UsbException(LIBUSB_ERROR_NO_MEM, "Failed to allocate transfer");
    }
}

UsbTransfer::~UsbTransfer() {
    if (transfer_) {
        if (!completed_.load()) {
            libusb_cancel_transfer(transfer_);
        }
        libusb_free_transfer(transfer_);
    }
}

void UsbTransfer::prepareControl(libusb_device_handle* handle,
                                 uint8_t request_type, uint8_t request,
                                 uint16_t value, uint16_t index,
                                 std::span<uint8_t> data,
                                 unsigned int timeout) {
    data_buffer_ = data.data();
    buffer_length_ = static_cast<int>(data.size());

    libusb_fill_control_setup(setup_buffer_, request_type, request, value,
                              index, static_cast<uint16_t>(data.size()));

    if (!data.empty()) {
        std::memcpy(setup_buffer_ + LIBUSB_CONTROL_SETUP_SIZE, data.data(),
                    data.size());
    }

    libusb_fill_control_transfer(transfer_, handle, setup_buffer_,
                                 &UsbTransfer::transferCallback, this, timeout);
}

void UsbTransfer::prepareBulkWrite(libusb_device_handle* handle,
                                   unsigned char endpoint,
                                   std::span<const uint8_t> data,
                                   unsigned int timeout) {
    data_copy_.reserve(data.size());
    data_copy_.assign(data.begin(), data.end());
    data_buffer_ = data_copy_.data();
    buffer_length_ = static_cast<int>(data_copy_.size());

    libusb_fill_bulk_transfer(
        transfer_, handle, endpoint, const_cast<unsigned char*>(data_buffer_),
        buffer_length_, &UsbTransfer::transferCallback, this, timeout);
}

void UsbTransfer::prepareBulkRead(libusb_device_handle* handle,
                                  unsigned char endpoint,
                                  std::span<uint8_t> data,
                                  unsigned int timeout) {
    data_buffer_ = data.data();
    buffer_length_ = static_cast<int>(data.size());

    libusb_fill_bulk_transfer(transfer_, handle, endpoint, data.data(),
                              buffer_length_, &UsbTransfer::transferCallback,
                              this, timeout);
}

UsbTransfer::SubmitAwaiter UsbTransfer::submit() {
    completed_.store(false);
    return SubmitAwaiter{*this};
}

libusb_transfer_status UsbTransfer::getStatus() const { return status_; }

int UsbTransfer::getActualLength() const { return actual_length_.load(); }

void UsbTransfer::transferCallback(libusb_transfer* transfer) {
    auto* self = static_cast<UsbTransfer*>(transfer->user_data);
    self->status_ = static_cast<libusb_transfer_status>(transfer->status);
    self->actual_length_.store(transfer->actual_length);
    self->completed_.store(true);

    if (self->completion_handle_) {
        self->completion_handle_.resume();
    }
}

UsbContext::UsbContext() : hotplug_running_(false) {
    int result = libusb_init(&context_);
    if (result != LIBUSB_SUCCESS) {
        throw UsbException(result, "Failed to initialize libusb context");
    }
    spdlog::debug("USB context initialized successfully");
}

UsbContext::~UsbContext() {
    stopHotplugDetection();
    libusb_exit(context_);
    spdlog::debug("USB context destroyed");
}

std::vector<std::shared_ptr<UsbDevice>> UsbContext::getDevices() {
    libusb_device** device_list;
    ssize_t count = libusb_get_device_list(context_, &device_list);

    if (count < 0) {
        throw UsbException(static_cast<int>(count),
                           "Failed to get device list");
    }

    std::vector<std::shared_ptr<UsbDevice>> devices;
    devices.reserve(static_cast<size_t>(count));

    for (ssize_t i = 0; i < count; ++i) {
        try {
            devices.emplace_back(
                std::make_shared<UsbDevice>(*this, device_list[i]));
        } catch (const UsbException& ex) {
            spdlog::warn("Failed to create device wrapper: {}", ex.what());
        }
    }

    libusb_free_device_list(device_list, 1);
    spdlog::debug("Found {} USB devices", devices.size());
    return devices;
}

void UsbContext::stopHotplugDetection() {
    if (!hotplug_running_.load()) {
        return;
    }

    hotplug_running_.store(false);

    if (hotplug_thread_.joinable()) {
        hotplug_thread_.join();
    }

    if (hotplug_handle_ != -1) {
        libusb_hotplug_deregister_callback(context_, hotplug_handle_);
        hotplug_handle_ = -1;
    }

    spdlog::debug("Hotplug detection stopped");
}

libusb_context* UsbContext::getNativeContext() const { return context_; }

UsbDevice::UsbDevice(UsbContext& context, libusb_device* device)
    : context_(context), device_(device), handle_(nullptr) {
    if (device_) {
        libusb_ref_device(device_);
    }
}

UsbDevice::~UsbDevice() {
    close();
    if (device_) {
        libusb_unref_device(device_);
    }
}

void UsbDevice::open() {
    if (!device_) {
        throw UsbException(LIBUSB_ERROR_NO_DEVICE, "Invalid device");
    }

    if (handle_) {
        return;
    }

    int result = libusb_open(device_, &handle_);
    if (result != LIBUSB_SUCCESS) {
        throw UsbException(result, "Failed to open device");
    }
    spdlog::debug("USB device opened successfully");
}

void UsbDevice::close() {
    if (handle_) {
        for (int interface_num : claimed_interfaces_) {
            releaseInterface(interface_num);
        }
        claimed_interfaces_.clear();

        libusb_close(handle_);
        handle_ = nullptr;
        spdlog::debug("USB device closed");
    }
}

void UsbDevice::claimInterface(int interface_number) {
    ensureOpen();

    int result = libusb_claim_interface(handle_, interface_number);
    if (result != LIBUSB_SUCCESS) {
        throw UsbException(result, "Failed to claim interface " +
                                       std::to_string(interface_number));
    }

    claimed_interfaces_.push_back(interface_number);
    spdlog::debug("Interface {} claimed", interface_number);
}

void UsbDevice::releaseInterface(int interface_number) {
    if (!handle_) {
        return;
    }

    int result = libusb_release_interface(handle_, interface_number);
    if (result != LIBUSB_SUCCESS) {
        spdlog::warn("Failed to release interface {}: {}", interface_number,
                     libusb_error_name(result));
    }

    auto it = std::find(claimed_interfaces_.begin(), claimed_interfaces_.end(),
                        interface_number);
    if (it != claimed_interfaces_.end()) {
        claimed_interfaces_.erase(it);
    }
    spdlog::debug("Interface {} released", interface_number);
}

UsbOperation UsbDevice::controlTransfer(uint8_t request_type, uint8_t request,
                                        uint16_t value, uint16_t index,
                                        std::span<uint8_t> data,
                                        unsigned int timeout) {
    ensureOpen();

    auto transfer = std::make_shared<UsbTransfer>();
    transfer->prepareControl(handle_, request_type, request, value, index, data,
                             timeout);

    co_await transfer->submit();

    if (transfer->getStatus() != LIBUSB_TRANSFER_COMPLETED) {
        throw UsbException(LIBUSB_ERROR_IO,
                           "Control transfer failed with status: " +
                               std::to_string(transfer->getStatus()));
    }

    co_return;
}

UsbOperation UsbDevice::bulkWrite(unsigned char endpoint,
                                  std::span<const uint8_t> data,
                                  unsigned int timeout) {
    ensureOpen();

    auto transfer = std::make_shared<UsbTransfer>();
    transfer->prepareBulkWrite(handle_, endpoint, data, timeout);

    co_await transfer->submit();

    if (transfer->getStatus() != LIBUSB_TRANSFER_COMPLETED) {
        throw UsbException(LIBUSB_ERROR_IO,
                           "Bulk write failed with status: " +
                               std::to_string(transfer->getStatus()));
    }

    co_return;
}

UsbOperation UsbDevice::bulkRead(unsigned char endpoint,
                                 std::span<uint8_t> data,
                                 unsigned int timeout) {
    ensureOpen();

    auto transfer = std::make_shared<UsbTransfer>();
    transfer->prepareBulkRead(handle_, endpoint, data, timeout);

    co_await transfer->submit();

    if (transfer->getStatus() != LIBUSB_TRANSFER_COMPLETED) {
        throw UsbException(LIBUSB_ERROR_IO,
                           "Bulk read failed with status: " +
                               std::to_string(transfer->getStatus()));
    }

    co_return;
}

std::string UsbDevice::getDescription() const {
    if (!device_) {
        return "Invalid device";
    }

    libusb_device_descriptor desc;
    int result = libusb_get_device_descriptor(device_, &desc);
    if (result != LIBUSB_SUCCESS) {
        return "Unknown device (error getting descriptor)";
    }

    uint8_t bus = libusb_get_bus_number(device_);
    uint8_t address = libusb_get_device_address(device_);

    std::string manufacturer, product;

    if (handle_) {
        constexpr size_t STRING_DESC_SIZE = 256;
        unsigned char buffer[STRING_DESC_SIZE];

        if (desc.iManufacturer) {
            if (libusb_get_string_descriptor_ascii(handle_, desc.iManufacturer,
                                                   buffer,
                                                   STRING_DESC_SIZE) > 0) {
                manufacturer = reinterpret_cast<char*>(buffer);
            }
        }

        if (desc.iProduct) {
            if (libusb_get_string_descriptor_ascii(
                    handle_, desc.iProduct, buffer, STRING_DESC_SIZE) > 0) {
                product = reinterpret_cast<char*>(buffer);
            }
        }
    }

    std::ostringstream ss;
    ss << "USB Device " << static_cast<int>(bus) << ":"
       << static_cast<int>(address) << " [" << std::hex << std::setw(4)
       << std::setfill('0') << desc.idVendor << ":" << std::hex << std::setw(4)
       << std::setfill('0') << desc.idProduct << std::dec << "]";

    if (!manufacturer.empty() || !product.empty()) {
        ss << " - ";
        if (!manufacturer.empty()) {
            ss << manufacturer;
        }
        if (!manufacturer.empty() && !product.empty()) {
            ss << " ";
        }
        if (!product.empty()) {
            ss << product;
        }
    }

    return ss.str();
}

std::pair<uint16_t, uint16_t> UsbDevice::getIds() const {
    if (!device_) {
        return {0, 0};
    }

    libusb_device_descriptor desc;
    int result = libusb_get_device_descriptor(device_, &desc);
    if (result != LIBUSB_SUCCESS) {
        return {0, 0};
    }

    return {desc.idVendor, desc.idProduct};
}

void UsbDevice::ensureOpen() {
    if (!handle_) {
        throw UsbException(LIBUSB_ERROR_NO_DEVICE, "Device not open");
    }
}

}  // namespace atom::serial
