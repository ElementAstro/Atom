#ifndef ATOM_SERIAL_TEST_USB_HPP
#define ATOM_SERIAL_TEST_USB_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <future>
#include <thread>

#include "atom/serial/usb.hpp"

using namespace std::chrono_literals;
using namespace atom::serial;
using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

// Mock for libusb functionality
class LibUsbMock {
public:
    static LibUsbMock& instance() {
        static LibUsbMock instance;
        return instance;
    }

    MOCK_METHOD(int, init, (libusb_context**), ());
    MOCK_METHOD(void, exit, (libusb_context*), ());
    MOCK_METHOD(int, handle_events, (libusb_context*), ());
    MOCK_METHOD(ssize_t, get_device_list, (libusb_context*, libusb_device***),
                ());
    MOCK_METHOD(void, free_device_list, (libusb_device**, int), ());
    MOCK_METHOD(int, open, (libusb_device*, libusb_device_handle**), ());
    MOCK_METHOD(void, close, (libusb_device_handle*), ());
    MOCK_METHOD(int, claim_interface, (libusb_device_handle*, int), ());
    MOCK_METHOD(int, release_interface, (libusb_device_handle*, int), ());
    MOCK_METHOD(int, get_device_descriptor,
                (libusb_device*, libusb_device_descriptor*), ());
    MOCK_METHOD(int, hotplug_register_callback,
                (libusb_context*, int, int, int, int, int,
                 libusb_hotplug_callback_fn, void*,
                 libusb_hotplug_callback_handle*),
                ());
    MOCK_METHOD(void, hotplug_deregister_callback,
                (libusb_context*, libusb_hotplug_callback_handle), ());
    MOCK_METHOD(int, submit_transfer, (libusb_transfer*), ());
    MOCK_METHOD(libusb_transfer*, alloc_transfer, (int), ());
    MOCK_METHOD(void, free_transfer, (libusb_transfer*), ());
    MOCK_METHOD(int, has_capability, (uint32_t), ());
    MOCK_METHOD(libusb_device*, ref_device, (libusb_device*), ());
    MOCK_METHOD(void, unref_device, (libusb_device*), ());
};

// Override libusb functions to use our mock
extern "C" {
int libusb_init(libusb_context** ctx) {
    return LibUsbMock::instance().init(ctx);
}

void libusb_exit(libusb_context* ctx) { LibUsbMock::instance().exit(ctx); }

int libusb_handle_events(libusb_context* ctx) {
    return LibUsbMock::instance().handle_events(ctx);
}

ssize_t libusb_get_device_list(libusb_context* ctx, libusb_device*** list) {
    return LibUsbMock::instance().get_device_list(ctx, list);
}

void libusb_free_device_list(libusb_device** list, int unref_devices) {
    LibUsbMock::instance().free_device_list(list, unref_devices);
}

int libusb_open(libusb_device* dev, libusb_device_handle** handle) {
    return LibUsbMock::instance().open(dev, handle);
}

void libusb_close(libusb_device_handle* handle) {
    LibUsbMock::instance().close(handle);
}

int libusb_claim_interface(libusb_device_handle* handle, int interface_number) {
    return LibUsbMock::instance().claim_interface(handle, interface_number);
}

int libusb_release_interface(libusb_device_handle* handle,
                             int interface_number) {
    return LibUsbMock::instance().release_interface(handle, interface_number);
}

int libusb_get_device_descriptor(libusb_device* dev,
                                 libusb_device_descriptor* desc) {
    return LibUsbMock::instance().get_device_descriptor(dev, desc);
}

int libusb_hotplug_register_callback(libusb_context* ctx, int events, int flags,
                                     int vendor_id, int product_id,
                                     int dev_class,
                                     libusb_hotplug_callback_fn cb_fn,
                                     void* user_data,
                                     libusb_hotplug_callback_handle* handle) {
    return LibUsbMock::instance().hotplug_register_callback(
        ctx, events, flags, vendor_id, product_id, dev_class, cb_fn, user_data,
        handle);
}

void libusb_hotplug_deregister_callback(libusb_context* ctx,
                                        libusb_hotplug_callback_handle handle) {
    LibUsbMock::instance().hotplug_deregister_callback(ctx, handle);
}

int libusb_submit_transfer(libusb_transfer* transfer) {
    return LibUsbMock::instance().submit_transfer(transfer);
}

libusb_transfer* libusb_alloc_transfer(int iso_packets) {
    return LibUsbMock::instance().alloc_transfer(iso_packets);
}

void libusb_free_transfer(libusb_transfer* transfer) {
    LibUsbMock::instance().free_transfer(transfer);
}

int libusb_has_capability(uint32_t capability) {
    return LibUsbMock::instance().has_capability(capability);
}

libusb_device* libusb_ref_device(libusb_device* dev) {
    return LibUsbMock::instance().ref_device(dev);
}

void libusb_unref_device(libusb_device* dev) {
    LibUsbMock::instance().unref_device(dev);
}
}

// Sample hotplug handler for testing
class TestHotplugHandler {
public:
    MOCK_METHOD(void, onHotplugEvent, (UsbDevice & device, bool arrived), ());
};

// Test fixture for USB context
class UsbContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup expectations for a successful context creation
        EXPECT_CALL(LibUsbMock::instance(), init(_))
            .WillOnce(
                DoAll(SetArgPointee<0>(reinterpret_cast<libusb_context*>(1)),
                      Return(LIBUSB_SUCCESS)));
    }

    void TearDown() override {
        EXPECT_CALL(LibUsbMock::instance(), exit(_))
            .Times(::testing::AnyNumber());
    }
};

// Test fixture for USB device operations
class UsbDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize context and mock device
        EXPECT_CALL(LibUsbMock::instance(), init(_))
            .WillOnce(DoAll(SetArgPointee<0>(reinterpret_cast<libusb_context*>(
                                &mock_context_)),
                            Return(LIBUSB_SUCCESS)));

        mock_device_ = reinterpret_cast<libusb_device*>(1);
        mock_handle_ = reinterpret_cast<libusb_device_handle*>(2);

        // Setup device descriptor expectations
        EXPECT_CALL(LibUsbMock::instance(),
                    get_device_descriptor(mock_device_, _))
            .WillRepeatedly(DoAll(
                [](libusb_device*, libusb_device_descriptor* desc) {
                    desc->idVendor = 0x1234;
                    desc->idProduct = 0x5678;
                    return LIBUSB_SUCCESS;
                },
                Return(LIBUSB_SUCCESS)));

        // Reference device
        EXPECT_CALL(LibUsbMock::instance(), ref_device(mock_device_))
            .WillOnce(::testing::Return(mock_device_));
    }

    void TearDown() override {
        EXPECT_CALL(LibUsbMock::instance(), exit(_))
            .Times(::testing::AnyNumber());
        EXPECT_CALL(LibUsbMock::instance(), unref_device(_))
            .Times(::testing::AnyNumber());
        EXPECT_CALL(LibUsbMock::instance(), close(_))
            .Times(::testing::AnyNumber());
    }

    void expectDeviceOpen() {
        EXPECT_CALL(LibUsbMock::instance(), open(mock_device_, _))
            .WillOnce(
                DoAll(SetArgPointee<1>(mock_handle_), Return(LIBUSB_SUCCESS)));
    }

    int mock_context_;
    libusb_device* mock_device_;
    libusb_device_handle* mock_handle_;
};

// Test fixture for USB transfers
class UsbTransferTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_transfer_ = reinterpret_cast<libusb_transfer*>(1);
        mock_handle_ = reinterpret_cast<libusb_device_handle*>(2);

        EXPECT_CALL(LibUsbMock::instance(), alloc_transfer(0))
            .WillOnce(Return(mock_transfer_));
    }

    void TearDown() override {
        EXPECT_CALL(LibUsbMock::instance(), free_transfer(_))
            .Times(::testing::AnyNumber());
    }

    libusb_transfer* mock_transfer_;
    libusb_device_handle* mock_handle_;
};

// Tests for UsbContext
TEST_F(UsbContextTest, ContextCreationAndDestruction) {
    // Context should be created and destroyed properly
    {
        UsbContext context;
        EXPECT_NE(nullptr, context.getNativeContext());
    }
}

TEST_F(UsbContextTest, GetDevices) {
    UsbContext context;

    // Mock device list with 2 devices
    libusb_device* mock_devices[2] = {reinterpret_cast<libusb_device*>(1),
                                      reinterpret_cast<libusb_device*>(2)};
    libusb_device** device_list = mock_devices;

    EXPECT_CALL(LibUsbMock::instance(), get_device_list(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(&device_list), Return(2)));
    EXPECT_CALL(LibUsbMock::instance(), free_device_list(_, _)).Times(1);

    // For each device, we need to mock descriptor retrieval
    EXPECT_CALL(LibUsbMock::instance(), get_device_descriptor(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(
            [](libusb_device*, libusb_device_descriptor* desc) {
                desc->idVendor = 0x1234;
                desc->idProduct = 0x5678;
                return LIBUSB_SUCCESS;
            },
            Return(LIBUSB_SUCCESS)));

    // Reference counting for devices
    EXPECT_CALL(LibUsbMock::instance(), ref_device(_))
        .Times(2)
        .WillRepeatedly(Return(1));

    auto devices = context.getDevices();
    EXPECT_EQ(2, devices.size());
}

TEST_F(UsbContextTest, HotplugDetection) {
    UsbContext context;
    TestHotplugHandler handler;

    // Mock hotplug capability
    EXPECT_CALL(LibUsbMock::instance(), has_capability(LIBUSB_CAP_HAS_HOTPLUG))
        .WillOnce(Return(1));

    // Register hotplug callback
    EXPECT_CALL(LibUsbMock::instance(),
                hotplug_register_callback(_, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<8>(1), Return(LIBUSB_SUCCESS)));

    // Handle events in background thread
    EXPECT_CALL(LibUsbMock::instance(), handle_events(_))
        .WillRepeatedly(Return(LIBUSB_SUCCESS));

    // Start hotplug detection
    context.startHotplugDetection(handler);

    // Give time for thread to start
    std::this_thread::sleep_for(100ms);

    // Stop hotplug detection
    EXPECT_CALL(LibUsbMock::instance(), hotplug_deregister_callback(_, _))
        .Times(1);

    context.stopHotplugDetection();
}

// Tests for UsbDevice
TEST_F(UsbDeviceTest, DeviceCreationAndDestruction) {
    UsbContext context;
    UsbDevice device(context, mock_device_);

    auto ids = device.getIds();
    EXPECT_EQ(0x1234, ids.first);   // Vendor ID
    EXPECT_EQ(0x5678, ids.second);  // Product ID

    // Description should include VID/PID
    std::string description = device.getDescription();
    EXPECT_TRUE(description.find("1234") != std::string::npos);
    EXPECT_TRUE(description.find("5678") != std::string::npos);
}

TEST_F(UsbDeviceTest, DeviceOpenAndClose) {
    UsbContext context;
    UsbDevice device(context, mock_device_);

    // Device open
    expectDeviceOpen();
    device.open();

    // Device close
    EXPECT_CALL(LibUsbMock::instance(), close(mock_handle_)).Times(1);
    device.close();
}

TEST_F(UsbDeviceTest, InterfaceClaimAndRelease) {
    UsbContext context;
    UsbDevice device(context, mock_device_);

    // Open device first
    expectDeviceOpen();
    device.open();

    // Claim interface
    EXPECT_CALL(LibUsbMock::instance(), claim_interface(mock_handle_, 0))
        .WillOnce(Return(LIBUSB_SUCCESS));
    device.claimInterface(0);

    // Release interface
    EXPECT_CALL(LibUsbMock::instance(), release_interface(mock_handle_, 0))
        .WillOnce(Return(LIBUSB_SUCCESS));
    device.releaseInterface(0);
}

TEST_F(UsbDeviceTest, DeviceControlTransfer) {
    UsbContext context;
    UsbDevice device(context, mock_device_);

    // Open device first
    expectDeviceOpen();
    device.open();

    // Setup transfer
    EXPECT_CALL(LibUsbMock::instance(), alloc_transfer(0))
        .WillOnce(Return(reinterpret_cast<libusb_transfer*>(1)));

    // Submit transfer
    EXPECT_CALL(LibUsbMock::instance(), submit_transfer(_))
        .WillOnce([](libusb_transfer* transfer) {
            // Simulate immediate transfer completion
            transfer->callback(transfer);
            return LIBUSB_SUCCESS;
        });

    // Free transfer when done
    EXPECT_CALL(LibUsbMock::instance(), free_transfer(_)).Times(1);

    std::array<uint8_t, 8> buffer = {0};
    auto operation = device.controlTransfer(
        static_cast<uint8_t>(LIBUSB_ENDPOINT_OUT) |
            static_cast<uint8_t>(LIBUSB_REQUEST_TYPE_VENDOR) |
            static_cast<uint8_t>(LIBUSB_RECIPIENT_DEVICE),
        0x01, 0x0002, 0x0003, buffer);
    [[maybe_unused]] auto awaiter = transfer.submit();
}

TEST_F(UsbDeviceTest, BulkReadAndWrite) {
    UsbContext context;
    UsbDevice device(context, mock_device_);

    // Open device first
    expectDeviceOpen();
    device.open();

    // Setup transfer for bulk write
    EXPECT_CALL(LibUsbMock::instance(), alloc_transfer(0))
        .WillOnce(Return(reinterpret_cast<libusb_transfer*>(1)))
        .WillOnce(Return(reinterpret_cast<libusb_transfer*>(2)));

    // Submit transfers
    EXPECT_CALL(LibUsbMock::instance(), submit_transfer(_))
        .Times(2)
        .WillRepeatedly([](libusb_transfer* transfer) {
            // Simulate immediate transfer completion
            transfer->callback(transfer);
            return LIBUSB_SUCCESS;
        });

    // Free transfers when done
    EXPECT_CALL(LibUsbMock::instance(), free_transfer(_)).Times(2);

    // Test bulk write
    std::array<uint8_t, 8> write_data = {1, 2, 3, 4, 5, 6, 7, 8};
    auto write_op = device.bulkWrite(0x81, write_data);

    // Test bulk read
    std::array<uint8_t, 8> read_buffer = {0};
    auto read_op = device.bulkRead(0x01, read_buffer);
}

// Tests for UsbTransfer
TEST_F(UsbTransferTest, TransferCreationAndDestruction) {
    UsbTransfer transfer;
}

TEST_F(UsbTransferTest, ControlTransfer) {
    UsbTransfer transfer;

    std::array<uint8_t, 8> buffer = {0};
    transfer.prepareControl(mock_handle_,
                            LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
                                LIBUSB_RECIPIENT_DEVICE,
                            0x01, 0x0002, 0x0003, buffer, 1000);

    // Submit transfer
    EXPECT_CALL(LibUsbMock::instance(), submit_transfer(mock_transfer_))
        .WillOnce([](libusb_transfer* transfer) {
            // Simulate successful transfer
            transfer->status = LIBUSB_TRANSFER_COMPLETED;
            transfer->actual_length = 8;
            transfer->callback(transfer);
            return LIBUSB_SUCCESS;
        });

    auto awaiter = transfer.submit();

    // After submission, status should be complete and length should be 8
    EXPECT_EQ(LIBUSB_TRANSFER_COMPLETED, transfer.getStatus());
    EXPECT_EQ(8, transfer.getActualLength());
}

TEST_F(UsbTransferTest, BulkTransfers) {
    UsbTransfer write_transfer;
    UsbTransfer read_transfer;

    // Setup transfers
    EXPECT_CALL(LibUsbMock::instance(), alloc_transfer(0))
        .WillOnce(Return(reinterpret_cast<libusb_transfer*>(2)));

    // Write transfer
    std::array<uint8_t, 8> write_data = {1, 2, 3, 4, 5, 6, 7, 8};
    write_transfer.prepareBulkWrite(mock_handle_, 0x01, write_data, 1000);

    // Submit write transfer
    EXPECT_CALL(LibUsbMock::instance(), submit_transfer(_))
        .WillOnce([](libusb_transfer* transfer) {
            // Simulate successful transfer
            transfer->status = LIBUSB_TRANSFER_COMPLETED;
            transfer->actual_length = 8;
            transfer->callback(transfer);
            return LIBUSB_SUCCESS;
        });

    [[maybe_unused]] auto write_awaiter = write_transfer.submit();

    // After submission, status should be complete and length should be 8
    EXPECT_EQ(LIBUSB_TRANSFER_COMPLETED, write_transfer.getStatus());
    EXPECT_EQ(8, write_transfer.getActualLength());

    // Read transfer
    std::array<uint8_t, 8> read_buffer = {0};
    read_transfer.prepareBulkRead(mock_handle_, 0x81, read_buffer, 1000);

    // Submit read transfer
    EXPECT_CALL(LibUsbMock::instance(), submit_transfer(_))
        .WillOnce([](libusb_transfer* transfer) {
            // Simulate successful transfer with data
            transfer->status = LIBUSB_TRANSFER_COMPLETED;
            transfer->actual_length = 8;

            // Fill buffer with sample data
            uint8_t* buffer = transfer->buffer;
            for (int i = 0; i < 8; i++) {
                buffer[i] = i + 1;
            }

            transfer->callback(transfer);
            return LIBUSB_SUCCESS;
        });

    [[maybe_unused]] auto read_awaiter = read_transfer.submit();

    // After submission, status should be complete and buffer should be filled
    EXPECT_EQ(LIBUSB_TRANSFER_COMPLETED, read_transfer.getStatus());
    EXPECT_EQ(8, read_transfer.getActualLength());
}

// Error handling tests
TEST_F(UsbDeviceTest, OpenDeviceFailure) {
    UsbContext context;
    UsbDevice device(context, mock_device_);

    // Simulate open failure
    EXPECT_CALL(LibUsbMock::instance(), open(mock_device_, _))
        .WillOnce(Return(LIBUSB_ERROR_ACCESS));

    EXPECT_THROW(device.open(), UsbException);
}

TEST_F(UsbDeviceTest, ClaimInterfaceFailure) {
    UsbContext context;
    UsbDevice device(context, mock_device_);

    // Open device first
    expectDeviceOpen();
    device.open();

    // Simulate claim interface failure
    EXPECT_CALL(LibUsbMock::instance(), claim_interface(mock_handle_, 0))
        .WillOnce(Return(LIBUSB_ERROR_BUSY));

    EXPECT_THROW(device.claimInterface(0), UsbException);
}

TEST_F(UsbContextTest, HotplugNotSupported) {
    UsbContext context;
    TestHotplugHandler handler;

    // Mock hotplug capability not available
    EXPECT_CALL(LibUsbMock::instance(), has_capability(LIBUSB_CAP_HAS_HOTPLUG))
        .WillOnce(Return(0));

    EXPECT_THROW(context.startHotplugDetection(handler), UsbException);
}

TEST_F(UsbContextTest, HotplugRegistrationFailure) {
    UsbContext context;
    TestHotplugHandler handler;

    // Mock hotplug capability
    EXPECT_CALL(LibUsbMock::instance(), has_capability(LIBUSB_CAP_HAS_HOTPLUG))
        .WillOnce(Return(1));

    // Simulate registration failure
    EXPECT_CALL(LibUsbMock::instance(),
                hotplug_register_callback(_, _, _, _, _, _, _, _, _))
        .WillOnce(Return(LIBUSB_ERROR_NOT_SUPPORTED));

    EXPECT_THROW(context.startHotplugDetection(handler), UsbException);
}

#endif  // ATOM_SERIAL_TEST_USB_HPP
