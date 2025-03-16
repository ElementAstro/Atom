#pragma once

#include <libusb-1.0/libusb.h>
#include <atomic>
#include <concepts>
#include <coroutine>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

// 命名空间
namespace modern_usb {

// 前向声明
class UsbContext;
class UsbDevice;
class UsbTransfer;

// ===== 使用概念(Concepts) =====

// 概念：定义可记录日志的类型
template <typename T>
concept Loggable = requires(T logger, const std::string& message) {
    { logger.debug(message) } -> std::same_as<void>;
    { logger.info(message) } -> std::same_as<void>;
    { logger.warning(message) } -> std::same_as<void>;
    { logger.error(message) } -> std::same_as<void>;
};

// 概念：定义可处理USB热插拔事件的类型
template <typename T>
concept HotplugHandler = requires(T handler, UsbDevice& device, bool arrived) {
    { handler.onHotplugEvent(device, arrived) } -> std::same_as<void>;
};

// ===== 基础设施 =====

// 默认日志记录器
class DefaultLogger {
public:
    void debug(const std::string& message) const {
        std::cout << "[DEBUG] " << message << std::endl;
    }

    void info(const std::string& message) const {
        std::cout << "[INFO] " << message << std::endl;
    }

    void warning(const std::string& message) const {
        std::cout << "[WARNING] " << message << std::endl;
    }

    void error(const std::string& message) const {
        std::cerr << "[ERROR] " << message << std::endl;
    }
};

// USB错误码转换为异常
class UsbException : public std::system_error {
public:
    explicit UsbException(int error_code)
        : std::system_error(error_code, std::system_category(),
                            libusb_error_name(error_code)) {}

    explicit UsbException(int error_code, const std::string& what_arg)
        : std::system_error(error_code, std::system_category(), what_arg) {}
};

// ===== 异步操作支持 =====

// 协程实现的USB操作
struct UsbOperation {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

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

    UsbOperation(handle_type h) : handle(h) {}
    UsbOperation(const UsbOperation&) = delete;
    UsbOperation& operator=(const UsbOperation&) = delete;

    ~UsbOperation() {
        if (handle) {
            handle.destroy();
        }
    }
};

class UsbTransfer {
public:
    explicit UsbTransfer()
        : transfer_(libusb_alloc_transfer(0)),
          completed_(false),
          data_buffer_(nullptr) {
        if (!transfer_) {
            throw UsbException(LIBUSB_ERROR_NO_MEM,
                               "Failed to allocate transfer");
        }
    }

    ~UsbTransfer() {
        if (transfer_) {
            if (!completed_) {
                libusb_cancel_transfer(transfer_);
            }
            libusb_free_transfer(transfer_);
            transfer_ = nullptr;
        }
    }

    // 禁止拷贝
    UsbTransfer(const UsbTransfer&) = delete;
    UsbTransfer& operator=(const UsbTransfer&) = delete;

    // 准备控制传输
    void prepareControl(libusb_device_handle* handle, uint8_t request_type,
                        uint8_t request, uint16_t value, uint16_t index,
                        std::span<uint8_t> data, unsigned int timeout) {
        data_buffer_ = data.data();
        buffer_length_ = data.size();

        libusb_fill_control_setup(setup_buffer_, request_type, request, value,
                                  index, data.size());

        memcpy(setup_buffer_ + LIBUSB_CONTROL_SETUP_SIZE, data.data(),
               data.size());

        libusb_fill_control_transfer(transfer_, handle, setup_buffer_,
                                     &UsbTransfer::transferCallback, this,
                                     timeout);
    }

    // 准备批量传输 - 写入
    void prepareBulkWrite(libusb_device_handle* handle, unsigned char endpoint,
                          std::span<const uint8_t> data, unsigned int timeout) {
        // 为常量数据创建内部副本
        data_copy_.assign(data.begin(), data.end());
        data_buffer_ = data_copy_.data();
        buffer_length_ = data_copy_.size();

        libusb_fill_bulk_transfer(
            transfer_, handle, endpoint,
            const_cast<unsigned char*>(data_buffer_), buffer_length_,
            &UsbTransfer::transferCallback, this, timeout);
    }

    // 准备批量传输 - 读取
    void prepareBulkRead(libusb_device_handle* handle, unsigned char endpoint,
                         std::span<uint8_t> data, unsigned int timeout) {
        data_buffer_ = data.data();
        buffer_length_ = data.size();

        libusb_fill_bulk_transfer(transfer_, handle, endpoint, data.data(),
                                  data.size(), &UsbTransfer::transferCallback,
                                  this, timeout);
    }

    // 提交传输
    struct SubmitAwaiter {
        UsbTransfer& transfer;

        bool await_ready() const noexcept { return transfer.completed_; }

        void await_suspend(std::coroutine_handle<> handle) {
            transfer.completion_handle_ = handle;
            int result = libusb_submit_transfer(transfer.transfer_);
            if (result != LIBUSB_SUCCESS) {
                transfer.completed_ = true;
                transfer.status_ = LIBUSB_TRANSFER_ERROR;
                handle.resume();
            }
        }

        void await_resume() const noexcept {}
    };

    SubmitAwaiter submit() {
        completed_ = false;
        return SubmitAwaiter{*this};
    }

    // 获取传输状态
    libusb_transfer_status getStatus() const { return status_; }

    // 获取传输的字节数
    int getActualLength() const { return actual_length_; }

private:
    libusb_transfer* transfer_;
    bool completed_;
    libusb_transfer_status status_;
    int actual_length_{0};
    unsigned char setup_buffer_[LIBUSB_CONTROL_SETUP_SIZE + 256];
    std::vector<uint8_t> data_copy_;
    const unsigned char* data_buffer_;
    int buffer_length_{0};
    std::coroutine_handle<> completion_handle_;

    static void transferCallback(libusb_transfer* transfer) {
        auto* self = static_cast<UsbTransfer*>(transfer->user_data);
        self->status_ = static_cast<libusb_transfer_status>(transfer->status);
        self->actual_length_ = transfer->actual_length;
        self->completed_ = true;

        if (self->completion_handle_) {
            self->completion_handle_.resume();
        }
    }
};

// ===== USB上下文 =====

// libusb上下文的RAII封装
class UsbContext {
public:
    UsbContext() : hotplug_running_(false) {
        int result = libusb_init(&context_);
        if (result != LIBUSB_SUCCESS) {
            throw UsbException(result, "Failed to initialize libusb context");
        }
    }

    ~UsbContext() {
        stopHotplugDetection();
        libusb_exit(context_);
    }

    // 禁止拷贝
    UsbContext(const UsbContext&) = delete;
    UsbContext& operator=(const UsbContext&) = delete;

    // 获取所有连接的USB设备
    auto getDevices() {
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

    // 开始热插拔检测
    template <HotplugHandler H>
    void startHotplugDetection(H& handler) {
        if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
            throw UsbException(LIBUSB_ERROR_NOT_SUPPORTED,
                               "Hotplug not supported on this platform");
        }

        if (hotplug_running_) {
            return;
        }

        // 保存处理器实例
        hotplug_handler_ = [&handler](UsbDevice& device, bool arrived) {
            handler.onHotplugEvent(device, arrived);
        };

        // 注册回调
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
                }
                return 0;  // 继续监听事件
            },
            this, &hotplug_handle_);

        if (result != LIBUSB_SUCCESS) {
            throw UsbException(result, "Failed to register hotplug callback");
        }

        // 启动事件处理线程
        hotplug_running_ = true;
        hotplug_thread_ = std::thread([this]() {
            try {
                while (hotplug_running_) {
                    libusb_handle_events(context_);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } catch (const std::exception& ex) {
            }
        });
    }

    // 停止热插拔检测
    void stopHotplugDetection() {
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

    // 获取原生libusb上下文
    libusb_context* getNativeContext() const { return context_; }

private:
    libusb_context* context_{nullptr};

    // 热插拔支持
    std::atomic<bool> hotplug_running_;
    std::thread hotplug_thread_;
    libusb_hotplug_callback_handle hotplug_handle_{-1};
    std::function<void(UsbDevice&, bool)> hotplug_handler_;

    // 友元类
    friend class UsbDevice;
    friend class UsbTransfer;
};

// ===== USB设备 =====

class UsbDevice {
public:
    UsbDevice(UsbContext& context, libusb_device* device)
        : context_(context), device_(device), handle_(nullptr) {
        if (device_) {
            libusb_ref_device(device_);
        }
    }

    ~UsbDevice() {
        close();
        if (device_) {
            libusb_unref_device(device_);
            device_ = nullptr;
        }
    }

    // 禁止拷贝
    UsbDevice(const UsbDevice&) = delete;
    UsbDevice& operator=(const UsbDevice&) = delete;

    // 打开设备
    void open() {
        if (!device_) {
            throw UsbException(LIBUSB_ERROR_NO_DEVICE, "Invalid device");
        }

        if (handle_) {
            return;  // 已经打开
        }

        int result = libusb_open(device_, &handle_);
        if (result != LIBUSB_SUCCESS) {
            throw UsbException(result, "Failed to open device");
        }
    }

    // 关闭设备
    void close() {
        if (handle_) {
            libusb_close(handle_);
            handle_ = nullptr;
        }
    }

    // 声明设备接口
    void claimInterface(int interface_number) {
        ensureOpen();

        int result = libusb_claim_interface(handle_, interface_number);
        if (result != LIBUSB_SUCCESS) {
            throw UsbException(result, "Failed to claim interface " +
                                           std::to_string(interface_number));
        }

        claimed_interfaces_.push_back(interface_number);
    }

    // 释放设备接口
    void releaseInterface(int interface_number) {
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

    // 控制传输
    UsbOperation controlTransfer(uint8_t request_type, uint8_t request,
                                 uint16_t value, uint16_t index,
                                 std::span<uint8_t> data,
                                 unsigned int timeout = 1000) {
        ensureOpen();

        auto transfer = std::make_shared<UsbTransfer>(context_);
        transfer->prepareControl(handle_, request_type, request, value, index,
                                 data, timeout);

        co_await transfer->submit();

        if (transfer->getStatus() != LIBUSB_TRANSFER_COMPLETED) {
            throw UsbException(LIBUSB_ERROR_IO,
                               "Control transfer failed with status: " +
                                   std::to_string(transfer->getStatus()));
        }

        co_return;
    }

    // 批量传输 - 写入
    UsbOperation bulkWrite(unsigned char endpoint,
                           std::span<const uint8_t> data,
                           unsigned int timeout = 1000) {
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

    // 批量传输 - 读取
    UsbOperation bulkRead(unsigned char endpoint, std::span<uint8_t> data,
                          unsigned int timeout = 1000) {
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

    // 获取设备描述
    std::string getDescription() const {
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
                    reinterpret_cast<unsigned char*>(product),
                    sizeof(product) - 1);
            }
        }

        std::stringstream ss;
        ss << "USB Device " << static_cast<int>(bus) << ":"
           << static_cast<int>(address) << " [" << std::hex << std::setw(4)
           << std::setfill('0') << desc.idVendor << ":" << std::hex
           << std::setw(4) << std::setfill('0') << desc.idProduct << std::dec
           << "]";

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

    // 获取设备ID信息
    std::pair<uint16_t, uint16_t> getIds() const {
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

private:
    UsbContext& context_;
    libusb_device* device_;
    libusb_device_handle* handle_;
    std::vector<int> claimed_interfaces_;

    // 确保设备已打开
    void ensureOpen() {
        if (!handle_) {
            throw UsbException(LIBUSB_ERROR_NO_DEVICE, "Device not open");
        }
    }

    friend class UsbTransfer;
};

// ===== USB传输 =====

}  // namespace modern_usb
