/**
 * @file usb.hpp
 * @brief USB device communication interface using libusb
 *
 * This header provides a C++ wrapper around libusb-1.0 for USB device
 * communication, featuring asynchronous operations using C++20 coroutines and
 * hotplug detection.
 */

#ifndef ATOM_SERIAL_USB_HPP
#define ATOM_SERIAL_USB_HPP

#include <libusb-1.0/libusb.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <concepts>
#include <coroutine>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace atom::serial {

class UsbContext;
class UsbDevice;
class UsbTransfer;

/**
 * @brief Concept defining requirements for hotplug event handlers
 *
 * Any type used as a hotplug handler must implement onHotplugEvent method
 * that takes a UsbDevice reference and arrival/departure flag.
 */
template <typename T>
concept HotplugHandler = requires(T handler, UsbDevice& device, bool arrived) {
    { handler.onHotplugEvent(device, arrived) } -> std::same_as<void>;
};

/**
 * @brief Exception class for USB-related errors
 *
 * Wraps libusb error codes with descriptive messages and integrates
 * with the C++ standard error system.
 */
class UsbException : public std::system_error {
public:
    /**
     * @brief Construct with libusb error code
     * @param error_code The libusb error code
     */
    explicit UsbException(int error_code)
        : std::system_error(error_code, std::system_category(),
                            libusb_error_name(error_code)) {}

    /**
     * @brief Construct with error code and custom message
     * @param error_code The libusb error code
     * @param what_arg Custom error description
     */
    explicit UsbException(int error_code, const std::string& what_arg)
        : std::system_error(error_code, std::system_category(), what_arg) {}
};

/**
 * @brief Coroutine operation for asynchronous USB transfers
 *
 * Represents an asynchronous USB operation that can be awaited.
 * The coroutine suspends until the transfer completes.
 */
struct UsbOperation {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    /**
     * @brief Promise type for the UsbOperation coroutine
     */
    struct promise_type {
        UsbOperation get_return_object() {
            return UsbOperation{handle_type::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { exception_ptr = std::current_exception(); }

        std::exception_ptr exception_ptr;
    };

    handle_type handle;

    explicit UsbOperation(handle_type h) : handle(h) {}
    UsbOperation(const UsbOperation&) = delete;
    UsbOperation& operator=(const UsbOperation&) = delete;
    UsbOperation(UsbOperation&& other) noexcept
        : handle(std::exchange(other.handle, {})) {}
    UsbOperation& operator=(UsbOperation&& other) noexcept {
        if (this != &other) {
            if (handle)
                handle.destroy();
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }

    ~UsbOperation() {
        if (handle) {
            handle.destroy();
        }
    }
};

/**
 * @brief Wrapper for libusb transfer operations
 *
 * Manages USB transfers with support for control, bulk read and write
 * operations. Provides asynchronous operation through coroutines.
 */
class UsbTransfer {
public:
    explicit UsbTransfer();
    ~UsbTransfer();

    UsbTransfer(const UsbTransfer&) = delete;
    UsbTransfer& operator=(const UsbTransfer&) = delete;

    /**
     * @brief Prepares a control transfer
     * @param handle Device handle
     * @param request_type Request type field
     * @param request Request field
     * @param value Value field
     * @param index Index field
     * @param data Data buffer
     * @param timeout Timeout in milliseconds
     */
    void prepareControl(libusb_device_handle* handle, uint8_t request_type,
                        uint8_t request, uint16_t value, uint16_t index,
                        std::span<uint8_t> data, unsigned int timeout);

    /**
     * @brief Prepares a bulk write transfer
     * @param handle Device handle
     * @param endpoint Endpoint address
     * @param data Data to write
     * @param timeout Timeout in milliseconds
     */
    void prepareBulkWrite(libusb_device_handle* handle, unsigned char endpoint,
                          std::span<const uint8_t> data, unsigned int timeout);

    /**
     * @brief Prepares a bulk read transfer
     * @param handle Device handle
     * @param endpoint Endpoint address
     * @param data Buffer for read data
     * @param timeout Timeout in milliseconds
     */
    void prepareBulkRead(libusb_device_handle* handle, unsigned char endpoint,
                         std::span<uint8_t> data, unsigned int timeout);

    /**
     * @brief Awaiter for transfer submission
     */
    struct SubmitAwaiter {
        UsbTransfer& transfer;

        bool await_ready() const noexcept { return transfer.completed_.load(); }

        void await_suspend(std::coroutine_handle<> handle) {
            transfer.completion_handle_ = handle;
            int result = libusb_submit_transfer(transfer.transfer_);
            if (result != LIBUSB_SUCCESS) {
                transfer.completed_.store(true);
                transfer.status_ = LIBUSB_TRANSFER_ERROR;
                handle.resume();
            }
        }

        void await_resume() const noexcept {}
    };

    /**
     * @brief Submits the transfer for execution
     * @return Awaiter for the transfer operation
     */
    SubmitAwaiter submit();

    /**
     * @brief Gets the transfer status
     * @return libusb_transfer_status value
     */
    libusb_transfer_status getStatus() const;

    /**
     * @brief Gets the actual number of bytes transferred
     * @return Number of bytes actually transferred
     */
    int getActualLength() const;

private:
    libusb_transfer* transfer_;
    std::atomic<bool> completed_;
    libusb_transfer_status status_;
    std::atomic<int> actual_length_{0};
    unsigned char setup_buffer_[LIBUSB_CONTROL_SETUP_SIZE + 256];
    std::vector<uint8_t> data_copy_;
    const unsigned char* data_buffer_;
    int buffer_length_{0};
    std::coroutine_handle<> completion_handle_;

    static void transferCallback(libusb_transfer* transfer);
};

/**
 * @brief Manages the USB context and hotplug detection
 *
 * Wraps libusb context operations and provides hotplug detection capabilities.
 */
class UsbContext {
public:
    UsbContext();
    ~UsbContext();

    UsbContext(const UsbContext&) = delete;
    UsbContext& operator=(const UsbContext&) = delete;

    /**
     * @brief Gets list of available USB devices
     * @return Vector of shared pointers to USB devices
     */
    std::vector<std::shared_ptr<UsbDevice>> getDevices();

    /**
     * @brief Starts hotplug detection with the given handler
     * @tparam H Handler type satisfying HotplugHandler concept
     * @param handler Reference to the handler object
     * @throws UsbException if hotplug is not supported or registration fails
     */
    template <HotplugHandler H>
    void startHotplugDetection(H& handler) {
        if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
            throw UsbException(LIBUSB_ERROR_NOT_SUPPORTED,
                               "Hotplug not supported on this platform");
        }

        if (hotplug_running_.load()) {
            return;
        }

        hotplug_handler_ = [&handler](UsbDevice& device, bool arrived) {
            handler.onHotplugEvent(device, arrived);
        };

        int result = libusb_hotplug_register_callback(
            context_,
            static_cast<libusb_hotplug_event>(
                LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
            LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY,
            LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
            []([[maybe_unused]] libusb_context* ctx, libusb_device* device,
               libusb_hotplug_event event, void* user_data) -> int {
                auto* self = static_cast<UsbContext*>(user_data);
                try {
                    auto dev = std::make_shared<UsbDevice>(*self, device);
                    bool arrived =
                        (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED);
                    self->hotplug_handler_(*dev, arrived);
                } catch (const std::exception& ex) {
                    spdlog::error("Hotplug callback error: {}", ex.what());
                }
                return 0;
            },
            this, &hotplug_handle_);

        if (result != LIBUSB_SUCCESS) {
            throw UsbException(result, "Failed to register hotplug callback");
        }

        hotplug_running_.store(true);
        hotplug_thread_ = std::thread([this]() {
            try {
                while (hotplug_running_.load()) {
                    libusb_handle_events(context_);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } catch (const std::exception& ex) {
                spdlog::error("Hotplug thread error: {}", ex.what());
            }
        });
    }

    /**
     * @brief Stops hotplug detection
     */
    void stopHotplugDetection();

    /**
     * @brief Gets the native libusb context
     * @return Pointer to libusb_context
     */
    libusb_context* getNativeContext() const;

private:
    libusb_context* context_{nullptr};
    std::atomic<bool> hotplug_running_;
    std::thread hotplug_thread_;
    libusb_hotplug_callback_handle hotplug_handle_{-1};
    std::function<void(UsbDevice&, bool)> hotplug_handler_;

    friend class UsbDevice;
    friend class UsbTransfer;
};

/**
 * @brief Represents a USB device
 *
 * Provides methods for device control, bulk transfers, and interface
 * management.
 */
class UsbDevice {
public:
    /**
     * @brief Construct from libusb device
     * @param context USB context
     * @param device libusb device pointer
     */
    UsbDevice(UsbContext& context, libusb_device* device);
    ~UsbDevice();

    UsbDevice(const UsbDevice&) = delete;
    UsbDevice& operator=(const UsbDevice&) = delete;

    /**
     * @brief Opens the device for communication
     */
    void open();

    /**
     * @brief Closes the device
     */
    void close();

    /**
     * @brief Claims an interface on the device
     * @param interface_number Interface number to claim
     */
    void claimInterface(int interface_number);

    /**
     * @brief Releases a claimed interface
     * @param interface_number Interface number to release
     */
    void releaseInterface(int interface_number);

    /**
     * @brief Performs a control transfer
     * @param request_type Request type field
     * @param request Request field
     * @param value Value field
     * @param index Index field
     * @param data Data buffer
     * @param timeout Timeout in milliseconds (default: 1000)
     * @return UsbOperation coroutine for asynchronous operation
     */
    UsbOperation controlTransfer(uint8_t request_type, uint8_t request,
                                 uint16_t value, uint16_t index,
                                 std::span<uint8_t> data,
                                 unsigned int timeout = 1000);

    /**
     * @brief Performs a bulk write
     * @param endpoint Endpoint address
     * @param data Data to write
     * @param timeout Timeout in milliseconds (default: 1000)
     * @return UsbOperation coroutine for asynchronous operation
     */
    UsbOperation bulkWrite(unsigned char endpoint,
                           std::span<const uint8_t> data,
                           unsigned int timeout = 1000);

    /**
     * @brief Performs a bulk read
     * @param endpoint Endpoint address
     * @param data Buffer for read data
     * @param timeout Timeout in milliseconds (default: 1000)
     * @return UsbOperation coroutine for asynchronous operation
     */
    UsbOperation bulkRead(unsigned char endpoint, std::span<uint8_t> data,
                          unsigned int timeout = 1000);

    /**
     * @brief Gets a description of the device
     * @return String describing the device
     */
    std::string getDescription() const;

    /**
     * @brief Gets the vendor and product IDs
     * @return Pair of vendor ID and product ID
     */
    std::pair<uint16_t, uint16_t> getIds() const;

private:
    UsbContext& context_;
    libusb_device* device_;
    libusb_device_handle* handle_;
    std::vector<int> claimed_interfaces_;

    void ensureOpen();

    friend class UsbTransfer;
};

}  // namespace atom::serial

#endif  // ATOM_SERIAL_USB_HPP