# atom/serial - Serial Communication Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **serial**

---

## Module Overview

The **atom::serial** module provides cross-platform serial communication capabilities for the Atom framework. It offers a modern C++ interface for serial port operations, including traditional serial ports and Bluetooth serial adapters.

### Key Features

- **Cross-Platform Serial I/O**: Windows, Linux, macOS support
- **Configurable Parameters**: Baud rate, data bits, parity, stop bits, flow control
- **Async Operations**: Non-blocking read/write with futures
- **Bluetooth Serial**: Support for Bluetooth SPP (Serial Port Profile)
- **USB Device Support**: USB serial device detection and communication (optional)
- **Port Enumeration**: Automatic detection of available serial ports
- **Signal Control**: RTS/DTR signal manipulation
- **Timeout Management**: Configurable read/write timeouts

---

## Directory Structure

```
atom/serial/
├── core/              # Core serial port implementation
│   ├── serial_port.hpp
│   ├── serial_port.cpp
│   └── scanner.hpp
│   └── scanner.cpp
├── bluetooth/         # Bluetooth serial support
│   ├── bluetooth_serial.hpp
│   ├── bluetooth_serial.cpp
│   ├── bluetooth_serial_mac.hpp
│   ├── bluetooth_serial_mac.mm (Objective-C++)
│   ├── bluetooth_serial_unix.hpp
│   └── ├── bluetooth_serial_win.hpp
├── platform/          # Platform-specific implementations
│   ├── serial_port_unix.hpp
│   └── serial_port_win.hpp
└── usb/               # USB device support (optional)
    ├── usb.hpp
    └── usb.cpp
```

---

## Core Components

### Serial Port

```cpp
#include "atom/serial/core/serial_port.hpp"

using namespace serial;

// Create serial port
SerialPort port;

// Configure and open
SerialConfig config = SerialConfig::standardConfig(115200);
config.setDataBits(8)
      .setParity(SerialConfig::Parity::None)
      .setStopBits(SerialConfig::StopBits::One)
      .setReadTimeout(std::chrono::milliseconds(1000));

port.open("/dev/ttyUSB0", config);

// Write data
std::string data = "Hello, Serial!";
port.write(data);

// Read data
std::vector<uint8_t> buffer = port.read(1024);

// Async read
auto future = port.asyncReadFuture(1024);
// ... do other work ...
buffer = future.get();

// Close port
port.close();
```

### Port Scanner

```cpp
#include "atom/serial/core/scanner.hpp"

using namespace serial;

// Get available ports
auto ports = SerialPort::getAvailablePorts();

std::cout << "Available ports:\n";
for (const auto& portName : ports) {
    std::cout << "  " << portName << "\n";
}
```

### Bluetooth Serial

```cpp
#include "atom/serial/bluetooth/bluetooth_serial.hpp"

using namespace serial;

// Create Bluetooth serial adapter
BluetoothSerial bt;

// Discover devices
auto devices = bt.discoverDevices();

for (const auto& device : devices) {
    std::cout << "Found: " << device.name
              << " (" << device.address << ")\n";
}

// Connect to device
if (!devices.empty()) {
    bt.connect(devices[0].address);

    // Send/receive data
    bt.write("AT\r\n");
    auto response = bt.read(1024);

    bt.disconnect();
}
```

---

## Public Interfaces

### SerialPort Class

```cpp
class SerialPort {
public:
    // Configuration
    class SerialConfig {
    public:
        enum class Parity { None, Odd, Even, Mark, Space };
        enum class StopBits { One, OnePointFive, Two };
        enum class FlowControl { None, Software, Hardware };

        SerialConfig& withBaudRate(int rate);
        SerialConfig& withDataBits(int bits);
        SerialConfig& withParity(Parity p);
        SerialConfig& withStopBits(StopBits sb);
        SerialConfig& withFlowControl(FlowControl flow);
        SerialConfig& withReadTimeout(std::chrono::milliseconds timeout);
        SerialConfig& withWriteTimeout(std::chrono::milliseconds timeout);

        static SerialConfig standardConfig(int baudRate);
    };

    // Construction
    SerialPort();
    ~SerialPort();

    // Lifecycle
    void open(std::string_view portName, const SerialConfig& config = {});
    void close();
    [[nodiscard]] bool isOpen() const;

    // I/O Operations
    std::vector<uint8_t> read(size_t maxBytes);
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout);
    std::string readUntil(char terminator, std::chrono::milliseconds timeout,
                          bool includeTerminator = false);
    std::vector<uint8_t> readUntilSequence(std::span<const uint8_t> sequence,
                                           std::chrono::milliseconds timeout,
                                           bool includeSequence = false);

    size_t write(std::span<const uint8_t> data);
    size_t write(std::string_view data);

    template <Serializable T>
    size_t writeObject(const T& value);

    // Async Operations
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback);
    std::future<std::vector<uint8_t>> asyncReadFuture(size_t maxBytes);
    std::future<size_t> asyncWrite(std::span<const uint8_t> data);
    std::future<size_t> asyncWrite(std::string_view data);

    // Utility
    std::vector<uint8_t> readAvailable();
    void flush();
    void drain();
    [[nodiscard]] size_t available() const;

    // Configuration
    void setConfig(const SerialConfig& config);
    [[nodiscard]] SerialConfig getConfig() const;

    // Signal Control
    void setDTR(bool value);
    void setRTS(bool value);
    [[nodiscard]] bool getCTS() const;
    [[nodiscard]] bool getDSR() const;
    [[nodiscard]] bool getRI() const;
    [[nodiscard]] bool getCD() const;

    // Information
    [[nodiscard]] std::string getPortName() const;
    static std::vector<std::string> getAvailablePorts();

    // Error handling
    std::optional<std::string> tryOpen(std::string_view portName,
                                       const SerialConfig& config = {});
};
```

### BluetoothSerial Class

```cpp
class BluetoothSerial {
public:
    struct DeviceInfo {
        std::string name;
        std::string address;  // MAC address
        bool connected;
    };

    // Device discovery
    std::vector<DeviceInfo> discoverDevices();
    std::vector<DeviceInfo> getPairedDevices();

    // Connection
    bool connect(const std::string& address);
    void disconnect();
    [[nodiscard]] bool isConnected() const;

    // I/O (delegates to SerialPort)
    size_t write(std::span<const uint8_t> data);
    std::vector<uint8_t> read(size_t maxBytes);
};
```

---

## Dependencies

### Required Dependencies

- **atom::error**: Error handling framework
- **atom::log**: Logging framework

### Platform-Specific Dependencies

**Windows:**

- SetupAPI: Device enumeration
- Cfgmgr32: Device configuration
- ws2_32: Windows Sockets

**Linux:**

- libudev: Device detection
- bluez: Bluetooth support (optional)

**macOS:**

- IOKit: Device I/O
- Foundation: Core services
- IOBluetooth: Bluetooth support

### Optional Dependencies

- **libusb-1.0**: USB device support

---

## Build Configuration

### CMake Options

```cmake
# Build serial module
-DBUILD_SERIAL=ON

# USB support requires libusb-1.0
# On Linux: sudo apt-get install libusb-1.0-0-dev
# macOS: brew install libusb
```

### Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Windows (MSVC) | Full | Full feature support |
| Windows (MinGW) | Full | Full feature support |
| Linux | Full | Requires udev dev files |
| macOS | Full | Requires Xcode tools |

---

## Usage Examples

### Serial Communication Loop

```cpp
#include "atom/serial/core/serial_port.hpp"

using namespace serial;

void communicate(const std::string& portName) {
    SerialPort port;

    // Configure
    SerialConfig config = SerialConfig::standardConfig(115200);
    config.setReadTimeout(std::chrono::milliseconds(100));

    // Open port
    port.open(portName, config);
    if (!port.isOpen()) {
        std::cerr << "Failed to open port\n";
        return;
    }

    // Communication loop
    while (true) {
        // Send command
        port.write("GET_STATUS\r\n");

        // Read response
        auto response = port.readUntil('\r', std::chrono::milliseconds(500));
        if (!response.empty()) {
            std::cout << "Response: " << response << "\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    port.close();
}
```

### Async Serial I/O

```cpp
#include "atom/serial/core/serial_port.hpp"
#include <future>

using namespace serial;

void asyncCommunication() {
    SerialPort port;
    port.open("/dev/ttyUSB0", SerialConfig::standardConfig(9600));

    // Async write
    auto writeFuture = port.asyncWrite("Command\r\n");

    // Async read
    auto readFuture = port.asyncReadFuture(1024);

    // Do other work while waiting
    std::cout << "Waiting for response...\n";

    // Get results
    writeFuture.wait();
    auto bytesWritten = writeFuture.get();

    auto data = readFuture.get();
    std::string response(data.begin(), data.end());

    port.close();
}
```

### Bluetooth Device Discovery

```cpp
#include "atom/serial/bluetooth/bluetooth_serial.hpp"

using namespace serial;

void findBluetoothDevices() {
    BluetoothSerial bt;

    std::cout << "Scanning for devices...\n";
    auto devices = bt.discoverDevices();

    for (const auto& device : devices) {
        std::cout << "Found: " << device.name
                  << " (" << device.address << ")\n";
    }
}
```

---

## Common Baud Rates

Standard baud rates supported by most serial hardware:

| Baud Rate | Typical Use |
|-----------|-------------|
| 9600 | Default, legacy devices |
| 19200 | Industrial equipment |
| 38400 | Some GPS devices |
| 57600 | Some microcontrollers |
| 115200 | Common for modern devices |
| 230400 | High-speed communication |
| 460800 | Very high-speed |
| 921600 | Maximum for many adapters |

---

## Port Naming Conventions

### Windows

- `COM1`, `COM2`, etc.
- Maximum port number is typically 256

### Linux

- `/dev/ttyS0`, `/dev/ttyS1` (hardware serial)
- `/dev/ttyUSB0`, `/dev/ttyUSB1` (USB-serial adapters)
- `/dev/ttyACM0`, `/dev/ttyACM1` (CDC-ACM devices)

### macOS

- `/dev/tty.usbserial-*` (USB-serial)
- `/dev/cu.usbserial-*` (callout devices)

---

## Error Handling

### Exception Types

```cpp
try {
    port.open("/dev/ttyUSB0", config);
} catch (const SerialConfigException& e) {
    std::cerr << "Configuration error: " << e.what() << "\n";
} catch (const SerialIOException& e) {
    std::cerr << "I/O error: " << e.what() << "\n";
} catch (const SerialTimeoutException& e) {
    std::cerr << "Timeout: " << e.what() << "\n";
} catch (const SerialException& e) {
    std::cerr << "Serial error: " << e.what() << "\n";
}
```

---

## Testing

### Test Organization

Tests are located in `tests/serial/`:

- `test_serial_port.cpp`: Serial port tests
- `test_config.cpp`: Configuration tests
- `test_async.cpp`: Async I/O tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run serial tests
ctest -R serial_ --output-on-failure
```

**Note:** Some tests may require actual serial hardware connected.

---

## Best Practices

### Resource Management

```cpp
// Good: RAII with automatic cleanup
{
    SerialPort port;
    port.open("/dev/ttyUSB0", config);
    // ... use port ...
} // Automatically closed

// Bad: Manual cleanup
SerialPort* port = new SerialPort();
port->open("/dev/ttyUSB0", config);
// ... use port ...
delete port;  // Easy to forget!
```

### Error Handling

```cpp
// Good: Use tryOpen for non-blocking
auto error = port.tryOpen("/dev/ttyUSB0", config);
if (error) {
    std::cerr << "Failed: " << *error << "\n";
    return;
}

// Bad: Exceptions in destructor
SerialPort port;
try {
    port.open("/dev/ttyUSB0", config);
} catch (...) {
    // Don't let exceptions escape destructor!
}
```

### Timeouts

```cpp
// Good: Always set timeouts
config.setReadTimeout(std::chrono::milliseconds(1000));
config.setWriteTimeout(std::chrono::milliseconds(5000));

// Bad: Infinite timeout
// Can cause application to hang!
```

---

## Related Modules

- **atom::connection**: Network communication alternatives
- **atom::system**: System-level device information
- **atom::async**: Async primitives for serial I/O

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented serial port, Bluetooth, USB components
- Added usage examples and platform-specific notes

---

**Maintained By:** Atom Framework Team
