# atom/connection - Networking Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **connection**

---

## Module Overview

The **atom::connection** module provides comprehensive networking and IPC (Inter-Process Communication) capabilities for the Atom framework. It offers both synchronous and asynchronous APIs for various communication protocols.

### Key Features

- **TCP/UDP Sockets**: Network communication with async support
- **FIFO/Named Pipes**: IPC for local process communication
- **Serial/TTY**: Serial port communication
- **SSH Support**: Secure shell connections (optional)
- **Async I/O**: Coroutine-based asynchronous operations (with ASIO)
- **Connection Pooling**: Efficient connection reuse

---

## Directory Structure

```
atom/connection/
├── tcp/               # TCP socket implementations
│   ├── tcpclient.hpp
│   ├── tcpclient.cpp
│   ├── async_tcpclient.hpp
│   └── async_tcpclient.cpp
├── udp/               # UDP socket implementations
│   ├── udpclient.hpp
│   ├── udpclient.cpp
│   ├── udpserver.hpp
│   ├── udpserver.cpp
│   ├── async_udpclient.hpp
│   ├── async_udpclient.cpp
│   ├── async_udpserver.hpp
│   └── async_udpserver.cpp
├── fifo/              # FIFO/Named Pipe implementations
│   ├── fifoclient.hpp
│   ├── fifoclient.cpp
│   ├── fifoserver.hpp
│   ├── fifoserver.cpp
│   ├── async_fifoclient.hpp
│   ├── async_fifoclient.cpp
│   ├── async_fifoserver.hpp
│   └── async_fifoserver.cpp
├── serial/            # Serial/TTY implementations
│   └── ttybase.hpp
├── shared/            # Shared connection utilities
│   ├── sockethub.hpp
│   ├── sockethub.cpp
│   ├── async_sockethub.hpp
│   └── async_sockethub.cpp
└── ssh/               # SSH implementations (optional)
    ├── sshclient.hpp
    ├── sshclient.cpp
    ├── sshserver.hpp
    └── sshserver.cpp
```

---

## Core Components

### TCP Client

#### Synchronous TCP Client

```cpp
#include "atom/connection/tcp/tcpclient.hpp"

using namespace atom::connection;

TcpClient client(TcpClient::Options{
    .ipv6_enabled = false,
    .keep_alive = true,
    .no_delay = true,
    .receive_buffer_size = 8192,
    .send_buffer_size = 8192
});

// Connect to server
auto result = client.connect("example.com", 8080);
if (!result) {
    std::cerr << "Connection failed: " << result.error().message() << "\n";
    return;
}

// Send data
std::string data = "Hello, Server!";
auto sendResult = client.send(std::span<const char>(data));
if (sendResult) {
    std::cout << "Sent " << sendResult.value() << " bytes\n";
}

// Receive data
auto received = client.receive(1024);
if (received) {
    std::string response(received->begin(), received->end());
    std::cout << "Received: " << response << "\n";
}

client.disconnect();
```

#### Asynchronous TCP Client (with ASIO)

```cpp
#include "atom/connection/tcp/async_tcpclient.hpp"

using namespace atom::connection;

AsyncTcpClient client;

// Async connection
auto connectTask = client.connect_async("example.com", 8080);
// ... do other work ...
auto connectResult = co_await connectTask;

// Async send/receive
auto sendTask = client.send_async(data);
auto receiveTask = client.receive_async(1024);
```

### UDP Socket

#### UDP Server

```cpp
#include "atom/connection/udp/udpserver.hpp"

using namespace atom::connection;

UdpSocketHub server;

// Start listening on port 8080
auto result = server.start(8080);
if (!result) {
    std::cerr << "Failed to start UDP server\n";
    return;
}

// Set message handler
server.addMessageHandler([](const std::string& message,
                            const std::string& ip,
                            int port) {
    std::cout << "Received from " << ip << ":" << port << ": " << message << "\n";
});

// Send response
server.sendTo("Response", "192.168.1.100", 9000);

server.stop();
```

### FIFO/Named Pipes

#### FIFO Server

```cpp
#include "atom/connection/fifo/fifoserver.hpp"

using namespace atom::connection;

FifoServer server("/tmp/my_pipe");

server.setOnDataReceivedCallback([](const std::vector<char>& data) {
    std::string message(data.begin(), data.end());
    std::cout << "Received: " << message << "\n";
});

server.start();

// Server is now listening for clients
```

#### FIFO Client

```cpp
#include "atom/connection/fifo/fifoclient.hpp"

using namespace atom::connection;

FifoClient client;
client.connect("/tmp/my_pipe");

std::string message = "Hello via FIFO!";
client.send(std::span<const char>(message));

client.disconnect();
```

---

## Public Interfaces

### TcpClient Class

```cpp
class TcpClient {
public:
    struct Options {
        bool ipv6_enabled{false};
        bool keep_alive{true};
        bool no_delay{true};
        size_t receive_buffer_size{8192};
        size_t send_buffer_size{8192};
    };

    // Connection
    auto connect(std::string_view host, uint16_t port,
                std::chrono::milliseconds timeout = {})
        -> type::expected<void, std::system_error>;

    void disconnect();
    [[nodiscard]] bool isConnected() const;

    // Data transfer
    auto send(std::span<const char> data)
        -> type::expected<size_t, std::system_error>;
    auto receive(size_t max_size,
                 std::chrono::milliseconds timeout = {})
        -> type::expected<std::vector<char>, std::system_error>;

    // Callbacks
    void setOnConnectedCallback(std::function<void()> callback);
    void setOnDisconnectedCallback(std::function<void()> callback);
    void setOnDataReceivedCallback(
        std::function<void(std::span<const char>)> callback);
    void setOnErrorCallback(
        std::function<void(const std::system_error&)> callback);

    // Async operations
    auto connect_async(std::string_view host, uint16_t port,
                      std::chrono::milliseconds timeout = {})
        -> Task<type::expected<void, std::system_error>>;
};
```

### UdpSocketHub Class

```cpp
class UdpSocketHub {
public:
    using MessageHandler = std::function<void(
        const std::string&, const std::string&, int)>;

    // Lifecycle
    [[nodiscard]] type::expected<void, UdpError> start(uint16_t port);
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

    // Messaging
    [[nodiscard]] type::expected<void, UdpError> sendTo(
        std::string_view message, std::string_view ip, uint16_t port);

    // Handlers
    template <MessageHandlerCallable T>
    void addMessageHandler(T&& handler);
    template <MessageHandlerCallable T>
    void removeMessageHandler(T&& handler);

    // Configuration
    void setBufferSize(std::size_t size) noexcept;
};
```

### FifoServer/FifoClient Classes

```cpp
class FifoServer {
public:
    explicit FifoServer(const std::string& path);

    void start();
    void stop();
    [[nodiscard]] bool isRunning() const;

    void setOnDataReceivedCallback(
        std::function<void(const std::vector<char>&)> callback);
    void setOnClientConnectedCallback(
        std::function<void(const std::string&)> callback);
    void setOnClientDisconnectedCallback(
        std::function<void(const std::string&)> callback);
};

class FifoClient {
public:
    bool connect(const std::string& path);
    void disconnect();
    [[nodiscard]] bool isConnected() const;

    type::expected<size_t, std::system_error> send(
        std::span<const char> data);
    type::expected<std::vector<char>, std::system_error> receive(
        size_t max_size);
};
```

---

## Dependencies

### Required Dependencies

- **atom::async**: Asynchronous primitives
- **atom::error**: Error handling framework
- **atom::type**: Type utilities (expected, noncopyable)

### Optional Dependencies

- **ASIO**: Asynchronous I/O support
- **OpenSSL**: SSL/TLS for secure connections (Windows MSVC)
- **libssh**: SSH support (when ATOM_USE_SSH=ON)

### Platform-Specific Libraries

**Windows:**

- ws2_32, mswsock

**All Platforms:**

- threading libraries (pthread/Linux, Windows threads)

---

## Build Options

### CMake Options

```cmake
# Build the connection module
-DBUILD_CONNECTION=ON

# Enable SSH support (requires libssh)
-DATOM_USE_SSH=ON

# ASIO is automatically detected if available
# For MSYS2/MinGW, ensure asio is installed via pacman
```

### ASIO Configuration

The module automatically detects ASIO in the following locations:

- Via CMake's find_package(asio)
- Standalone asio.hpp in /mingw64/include (MSYS2)
- Custom CMAKE_PREFIX_PATH locations

---

## Usage Examples

### TCP Echo Server

```cpp
#include "atom/connection/tcp/async_tcpclient.hpp"
#include <iostream>

using namespace atom::connection;

class EchoServer {
public:
    void start(uint16_t port) {
        // Server implementation would go here
        // This is a conceptual example
    }
};
```

### UDP Broadcast

```cpp
#include "atom/connection/udp/udpclient.hpp"

using namespace atom::connection;

UdpClient client;
client.enableBroadcast(true);
client.bind(0); // Any available port

// Send broadcast
std::string message = "Broadcast message";
client.sendTo(message, "255.255.255.255", 8080);
```

### Connection Pool Pattern

```cpp
#include "atom/connection/shared/sockethub.hpp"

using namespace atom::connection;

SocketHub hub;

// Create multiple connections
for (int i = 0; i < 5; ++i) {
    hub.createConnection("example.com", 8080);
}

// Use connections from pool
auto conn = hub.acquire();
// ... use connection ...
hub.release(conn);
```

---

## Error Handling

### Expected<T, E> Pattern

The module uses `type::expected<T, E>` for error handling:

```cpp
auto result = client.connect("example.com", 8080);

if (result) {
    // Success - result.value() is available
    std::cout << "Connected successfully\n";
} else {
    // Error - result.error() contains the error
    std::cerr << "Connection failed: "
              << result.error().message() << "\n";
}
```

### Error Codes

**TCP Errors:**

- Socket creation failures
- Connection timeouts
- DNS resolution failures
- Send/receive failures

**UDP Errors:**

- `UdpError::SocketCreationFailed`
- `UdpError::BindFailed`
- `UdpError::SendFailed`
- `UdpError::InvalidAddress`

---

## Testing

### Test Organization

Tests are located in `tests/connection/`:

- `test_tcp.cpp`: TCP socket tests
- `test_udp.cpp`: UDP socket tests
- `test_fifo.cpp`: FIFO tests
- `test_serial.cpp`: Serial port tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run connection tests
ctest -R connection_ --output-on-failure
```

---

## Performance Considerations

### TCP Optimization

```cpp
// Disable Nagle's algorithm for low latency
TcpClient::Options options;
options.no_delay = true;

// Increase buffer sizes for high throughput
options.receive_buffer_size = 64 * 1024;  // 64KB
options.send_buffer_size = 64 * 1024;
```

### Async Operations

When using ASIO, prefer async operations for I/O-bound tasks:

```cpp
// Async send is non-blocking
auto task = client.send_async(data);
// ... do other work ...
auto result = co_await task;
```

---

## Security Considerations

### Data Validation

- Always validate received data size
- Use timeouts to prevent hanging connections
- Implement rate limiting for servers

### Secure Connections

- Use SSH module for encrypted communication
- Consider TLS/SSL for TCP (via OpenSSL)
- Validate peer certificates in production

---

## Related Modules

- **atom::async**: Async primitives and executors
- **atom::io**: Lower-level I/O operations
- **atom::system**: System-level networking
- **atom::serial**: Serial communication

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented TCP, UDP, FIFO components
- Added usage examples and error handling patterns

---

**Maintained By:** Atom Framework Team
