# UDP Connection Module

A comprehensive UDP communication library providing both synchronous and asynchronous implementations for client and server functionality.

## Module Structure

```
atom/connection/udp/
├── udp_common.hpp        # Common types: UdpError, UdpStatistics, SocketOption, etc.
├── udp_platform.hpp      # Platform-specific utilities (Windows/POSIX)
├── multicast_helper.hpp  # Multicast group management helper
├── async_udpclient.hpp   # Asynchronous UDP client (Asio-based)
├── async_udpclient.cpp
├── async_udpserver.hpp   # Asynchronous UDP server (Asio-based)
├── async_udpserver.cpp
├── udpclient.hpp         # Synchronous UDP client (native sockets)
├── udpclient.cpp
├── udpserver.hpp         # Synchronous UDP server (native sockets)
├── udpserver.cpp
└── README.md
```

## Components

### Common Infrastructure

#### `udp_common.hpp`

Shared types and utilities used across all UDP components:

- `UdpError` - Comprehensive error codes for UDP operations
- `UdpStatistics` - Thread-safe packet/byte statistics with atomic counters
- `SocketOption` - Configurable socket options enum
- `RemoteEndpoint` - Host/port endpoint structure
- `SocketConfig` - Socket configuration options struct
- `UdpResult<T>` - Result type using `type::expected`

#### `udp_platform.hpp`

Platform-abstraction layer for socket operations:

- `NetworkInitializer` - RAII wrapper for WSAStartup/WSACleanup
- `setNonBlocking()`, `setBlocking()` - Socket mode configuration
- `isValidIPv4Address()`, `isMulticastAddress()` - Address validation
- `setReceiveTimeout()`, `setSendTimeout()` - Timeout configuration
- `applySocketConfig()` - Apply full socket configuration

#### `multicast_helper.hpp`

Helper class for multicast group management:

- `joinGroup()` / `leaveGroup()` - Join/leave multicast groups
- `setMulticastTTL()` - Configure multicast TTL
- `setMulticastLoopback()` - Enable/disable loopback
- Automatic cleanup of joined groups on destruction

### Asynchronous Components (Asio-based)

#### `AsyncUdpClient` (async_udpclient.hpp)

High-performance asynchronous UDP client using Asio:

```cpp
#include "atom/connection/udp/async_udpclient.hpp"

atom::async::connection::AsyncUdpClient client;
client.bind(12345);
client.send("192.168.1.100", 8080, data);
client.startReceiving(8192);
```

Features:

- Asynchronous send/receive with callbacks
- Batch sending to multiple destinations
- Multicast group support
- Socket option configuration
- Thread-safe statistics

#### `AsyncUdpServer` (async_udpserver.hpp)

Scalable asynchronous UDP server:

```cpp
#include "atom/connection/udp/async_udpserver.hpp"

atom::async::connection::AsyncUdpServer server(4); // 4 worker threads
server.addMessageHandler([](const std::string& msg, const std::string& ip, unsigned short port) {
    // Handle message
});
server.start(8080);
```

Features:

- Multi-threaded I/O processing
- Message handler callbacks
- IP filtering/whitelisting
- Broadcast and multicast support
- Outgoing message queue

### Synchronous Components (Native Sockets)

#### `UdpClient` (udpclient.hpp)

Modern C++20 synchronous UDP client:

```cpp
#include "atom/connection/udp/udpclient.hpp"

atom::connection::UdpClient client(12345);
auto result = client.send({.host = "192.168.1.100", .port = 8080}, data);
if (result) {
    std::cout << "Sent " << *result << " bytes\n";
}
```

Features:

- C++20 coroutine support (`co_await receiveAsync()`)
- `std::span` for zero-copy data handling
- Result types with error codes
- Multicast and broadcast support

#### `UdpSocketHub` (udpserver.hpp)

Synchronous UDP server:

```cpp
#include "atom/connection/udp/udpserver.hpp"

atom::connection::UdpSocketHub server;
server.setMessageHandler([](const std::string& msg, const std::string& ip, int port) {
    // Handle message
});
server.start("0.0.0.0", 8080);
```

## Backward Compatibility

Type aliases are provided for backward compatibility:

- `atom::async::connection::UdpClient` → `AsyncUdpClient`
- `atom::async::connection::UdpSocketHub` → `AsyncUdpServer`

## Dependencies

- **Asio** (header-only) - For async components
- **atom/type/expected.hpp** - Result type
- **atom/log/loguru.hpp** - Logging (optional)

## Platform Support

- Windows (WinSock2)
- Linux (POSIX sockets, epoll)
- macOS (POSIX sockets, kqueue)

## Future Improvements

1. **Phase 2 Migration**: Gradually migrate synchronous components to use common infrastructure
2. **Performance**: Replace busy-wait loops with proper async waiting
3. **IPv6**: Full IPv6 support in all components
4. **Connection Pooling**: For high-throughput scenarios
