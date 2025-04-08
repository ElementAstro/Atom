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
        if (!completed_) {
            libusb_cancel_transfer(transfer_);
        }
        libusb_free_transfer(transfer_);
        transfer_ = nullptr;
    }
}

void UsbTransfer::prepareControl(libusb_device_handle* handle,
                                 uint8_t request_type, uint8_t request,
                                 uint16_t value, uint16_t index,
                                 std::span<uint8_t> data,
                                 unsigned int timeout) {
    data_buffer_ = data.data();
    buffer_length_ = data.size();

    libusb_fill_control_setup(setup_buffer_, request_type, request, value,
                              index, data.size());

    memcpy(setup_buffer_ + LIBUSB_CONTROL_SETUP_SIZE, data.data(), data.size());

    libusb_fill_control_transfer(transfer_, handle, setup_buffer_,
                                 &UsbTransfer::transferCallback, this, timeout);
}

void UsbTransfer::prepareBulkWrite(libusb_device_handle* handle,
                                   unsigned char endpoint,
                                   std::span<const uint8_t> data,
                                   unsigned int timeout) {
    data_copy_.assign(data.begin(), data.end());
    data_buffer_ = data_copy_.data();
    buffer_length_ = data_copy_.size();

    libusb_fill_bulk_transfer(
        transfer_, handle, endpoint, const_cast<unsigned char*>(data_buffer_),
        buffer_length_, &UsbTransfer::transferCallback, this, timeout);
}

void UsbTransfer::prepareBulkRead(libusb_device_handle* handle,
                                  unsigned char endpoint,
                                  std::span<uint8_t> data,
                                  unsigned int timeout) {
    data_buffer_ = data.data();
    buffer_length_ = data.size();

    libusb_fill_bulk_transfer(transfer_, handle, endpoint, data.data(),
                              data.size(), &UsbTransfer::transferCallback, this,
                              timeout);
}

UsbTransfer::SubmitAwaiter UsbTransfer::submit() {
    completed_ = false;
    return SubmitAwaiter{*this};
}

libusb_transfer_status UsbTransfer::getStatus() const { return status_; }

int UsbTransfer::getActualLength() const { return actual_length_; }

void UsbTransfer::transferCallback(libusb_transfer* transfer) {
    auto* self = static_cast<UsbTransfer*>(transfer->user_data);
    self->status_ = static_cast<libusb_transfer_status>(transfer->status);
    self->actual_length_ = transfer->actual_length;
    self->completed_ = true;

    if (self->completion_handle_) {
        self->completion_handle_.resume();
    }
}

UsbContext::UsbContext() : hotplug_running_(false) {
    int result = libusb_init(&context_);
    if (result != LIBUSB_SUCCESS) {
        throw UsbException(result, "Failed to initialize libusb context");
    }
}

UsbContext::~UsbContext() {
    stopHotplugDetection();
    libusb_exit(context_);
}

auto UsbContext::getDevices() -> std::vector<std::shared_ptr<UsbDevice>> {
    libusb_device** device_list;
    ssize_t count = libusb_get_device_list(context_, &device_list);

    if (count < 0) {
        throw UsbException(static_cast<int>(count),
                           "Failed to get device list");
    }

    std::vector<std::shared_ptr<UsbDevice>> devices;
    for (ssize_t i = 0; i < count; ++i) {
        try {
            devices.push_back(
                std::make_shared<UsbDevice>(*this, device_list[i]));
        } catch (const UsbException& ex) {
        }
    }

    libusb_free_device_list(device_list, 1);
    return devices;
}

void UsbContext::stopHotplugDetection() {
    if (!hotplug_running_) {
        return;
    }

    hotplug_running_ = false;

    if (hotplug_thread_.joinable()) {
        hotplug_thread_.join();
    }

    if (hotplug_handle_ != -1) {
        libusb_hotplug_deregister_callback(context_, hotplug_handle_);
        hotplug_handle_ = -1;
    }
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
        device_ = nullptr;
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
}

void UsbDevice::close() {
    if (handle_) {
        libusb_close(handle_);
        handle_ = nullptr;
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
}

void UsbDevice::releaseInterface(int interface_number) {
    if (!handle_) {
        return;
    }

    int result = libusb_release_interface(handle_, interface_number);
    if (result != LIBUSB_SUCCESS) {
    }

    claimed_interfaces_.erase(
        std::remove(claimed_interfaces_.begin(), claimed_interfaces_.end(),
                    interface_number),
        claimed_interfaces_.end());
}

UsbOperation UsbDevice::controlTransfer(uint8_t request_type, uint8_t request,
                                        uint16_t value, uint16_t index,
                                        std::span<uint8_t> data,
                                        unsigned int timeout) {
    ensureOpen();

    auto transfer = std::make_shared<UsbTransfer>(context_);
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

    auto transfer = std::make_shared<UsbTransfer>(context_);
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

    auto transfer = std::make_shared<UsbTransfer>(context_);
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

    char manufacturer[256] = {0};
    char product[256] = {0};

    if (handle_) {
        if (desc.iManufacturer) {
            libusb_get_string_descriptor_ascii(
                handle_, desc.iManufacturer,
                reinterpret_cast<unsigned char*>(manufacturer),
                sizeof(manufacturer) - 1);
        }

        if (desc.iProduct) {
            libusb_get_string_descriptor_ascii(
                handle_, desc.iProduct,
                reinterpret_cast<unsigned char*>(product), sizeof(product) - 1);
        }
    }

    std::stringstream ss;
    ss << "USB Device " << static_cast<int>(bus) << ":"
       << static_cast<int>(address) << " [" << std::hex << std::setw(4)
       << std::setfill('0') << desc.idVendor << ":" << std::hex << std::setw(4)
       << std::setfill('0') << desc.idProduct << std::dec << "]";

    if (manufacturer[0] || product[0]) {
        ss << " - ";
        if (manufacturer[0]) {
            ss << manufacturer;
        }
        if (manufacturer[0] && product[0]) {
            ss << " ";
        }
        if (product[0]) {
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
