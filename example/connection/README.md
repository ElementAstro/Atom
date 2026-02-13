# Atom Connection Examples

This directory contains comprehensive examples demonstrating the advanced networking capabilities of the Atom Connection library. These examples showcase various communication patterns, protocols, and advanced features for building robust networked applications.

## Overview

The Atom Connection library provides high-performance, cross-platform networking components with both synchronous and asynchronous APIs. These examples demonstrate real-world usage patterns and best practices.

## Features Demonstrated

### Core Networking Protocols

- **TCP**: Reliable, connection-oriented communication
- **UDP**: Fast, connectionless communication with multicast/broadcast support
- **FIFO**: Named pipe communication for inter-process communication
- **SSH**: Secure shell communication with authentication
- **TTY**: Serial communication for hardware interfaces

### Advanced Features

- **Asynchronous Operations**: Non-blocking I/O with callbacks and futures
- **Connection Pooling**: Efficient resource management
- **Load Balancing**: Distribute traffic across multiple endpoints
- **Statistics Monitoring**: Real-time performance tracking
- **Error Handling**: Comprehensive error recovery mechanisms
- **Session Management**: Client tracking and lifecycle management

## Examples Overview

### TCP Examples

#### `tcpclient.cpp` - Enhanced TCP Client

**Features:**

- Basic and advanced TCP client operations
- SSL/TLS support configuration
- Connection pooling and reuse
- Performance monitoring with latency tracking
- Multiple message patterns (JSON, XML, Binary)
- Error handling and recovery patterns
- Statistics tracking

**Usage:**

```bash
./connection_tcpclient
```

#### `async_tcpclient.cpp` - Asynchronous TCP Client

**Features:**

- Future-based async operations
- Promise-based response handling
- Concurrent connection management
- Message queue processing
- Advanced callback patterns
- Performance statistics

**Usage:**

```bash
./connection_async_tcpclient
```

### UDP Examples

#### `udpclient.cpp` - Enhanced UDP Client

**Features:**

- Basic UDP communication with enhanced error handling
- Multicast and broadcast messaging
- Network discovery patterns
- Load balancing across multiple servers
- Advanced socket options configuration
- Performance monitoring and statistics

**Usage:**

```bash
./connection_udpclient
```

#### `udpserver.cpp` - Enhanced UDP Server

**Features:**

- Advanced message routing and filtering
- Client session management
- High-performance server patterns
- Real-time statistics monitoring
- Multi-handler message processing

**Usage:**

```bash
./connection_udpserver
```

#### `async_udpclient.cpp` - Asynchronous UDP Client

**Features:**

- Future-based async operations
- Concurrent message processing
- Performance testing and load analysis
- Message queuing and buffering
- Advanced error handling

**Usage:**

```bash
./connection_async_udpclient
```

#### `async_udpserver.cpp` - Asynchronous UDP Server

**Features:**

- Multi-handler message routing
- Performance monitoring and load testing
- Client session management
- Real-time statistics tracking

**Usage:**

```bash
./connection_async_udpserver
```

### Socket Hub Examples

#### `sockethub.cpp` - Enhanced Socket Hub

**Features:**

- Advanced client connection management
- Multi-threaded client simulation
- Performance testing and load analysis
- Connection lifecycle management
- Real-time statistics monitoring

**Usage:**

```bash
./connection_sockethub
```

#### `async_sockethub.cpp` - Asynchronous Socket Hub

**Features:**

- Advanced async message handling
- Client broadcasting and selective communication
- Performance optimization patterns
- Error handling and recovery

**Usage:**

```bash
./connection_async_sockethub
```

### FIFO Examples

#### `fifoclient.cpp` - FIFO Client

**Features:**

- Named pipe communication
- Cross-platform compatibility
- Error handling and recovery

**Usage:**

```bash
./connection_fifoclient
```

#### `fifoserver.cpp` - FIFO Server

**Features:**

- Named pipe server implementation
- Message handling and processing

**Usage:**

```bash
./connection_fifoserver
```

### SSH Examples

#### `sshclient.cpp` - SSH Client

**Features:**

- Secure shell client implementation
- Authentication methods
- Command execution and file transfer

**Usage:**

```bash
./connection_sshclient
```

#### `sshserver.cpp` - SSH Server

**Features:**

- SSH server implementation
- User authentication and session management

**Usage:**

```bash
./connection_sshserver
```

### TTY Examples

#### `ttybase.cpp` - TTY Communication

**Features:**

- Serial communication patterns
- Hardware interface communication
- Cross-platform TTY handling

**Usage:**

```bash
./connection_ttybase
```

## Building the Examples

### Prerequisites

- CMake 3.10 or higher
- C++20 compatible compiler
- Atom Connection library
- Platform-specific dependencies (see below)

### Platform-Specific Dependencies

#### Windows

- Winsock2 (ws2_32.lib, wsock32.lib)
- SetupAPI (setupapi.lib) for TTY examples

#### Linux/Unix

- pthread library
- Standard POSIX libraries

#### Optional Dependencies

- libssh (for SSH examples)
- OpenSSL (for SSL/TLS support)

### Build Configuration

```bash
# Build all examples
cmake -DATOM_EXAMPLE_CONNECTION_BUILD_ALL=ON ..
make connection_examples_all

# Build specific examples
cmake -DATOM_EXAMPLE_CONNECTION_TCPCLIENT=ON ..
make connection_tcpclient

# Enable verbose output
cmake -DATOM_EXAMPLE_CONNECTION_VERBOSE=ON ..

# Enable test targets
cmake -DATOM_EXAMPLE_CONNECTION_ENABLE_TESTS=ON ..
```

### Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `ATOM_EXAMPLE_CONNECTION_BUILD_ALL` | Build all connection examples | ON |
| `ATOM_EXAMPLE_CONNECTION_ENABLE_TESTS` | Enable test targets | OFF |
| `ATOM_EXAMPLE_CONNECTION_VERBOSE` | Enable verbose build output | OFF |
| `ATOM_EXAMPLE_CONNECTION_<NAME>` | Build specific example | Follows BUILD_ALL |

## Testing the Examples

### TCP Examples

1. Start a TCP server (e.g., `connection_sockethub`)
2. Run TCP client examples to connect and communicate

### UDP Examples

1. Start a UDP server (e.g., `connection_udpserver`)
2. Run UDP client examples to send messages

### Testing with External Tools

#### TCP Testing

```bash
# Test with telnet
telnet localhost 8080

# Test with netcat
nc localhost 8080
```

#### UDP Testing

```bash
# Test with netcat UDP mode
nc -u localhost 8080
```

## Performance Considerations

### Optimization Tips

1. **Buffer Sizes**: Adjust buffer sizes based on your use case
2. **Thread Pools**: Use appropriate thread pool sizes for async operations
3. **Connection Pooling**: Reuse connections when possible
4. **Statistics**: Monitor performance metrics for optimization

### Benchmarking

Each example includes built-in performance monitoring:

- Message throughput (messages/second)
- Bandwidth utilization (bytes/second)
- Connection latency
- Error rates
- Resource utilization

## Troubleshooting

### Common Issues

#### Build Issues

- Ensure all dependencies are installed
- Check CMake configuration options
- Verify compiler C++20 support

#### Runtime Issues

- Check firewall settings for network examples
- Verify port availability
- Check platform-specific permissions

#### Performance Issues

- Monitor system resources
- Adjust buffer sizes
- Check network configuration

### Debug Mode

Build with debug flags for detailed logging:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

## Advanced Usage Patterns

### Custom Message Protocols

Examples demonstrate how to implement custom message protocols with:

- Message framing and parsing
- Protocol versioning
- Error detection and correction

### Load Balancing

UDP client examples show load balancing patterns:

- Round-robin distribution
- Health checking
- Failover mechanisms

### Security Considerations

- SSL/TLS configuration examples
- Authentication patterns
- Secure communication best practices

## Contributing

When adding new examples:

1. Follow the established naming conventions
2. Include comprehensive error handling
3. Add performance monitoring
4. Update this README
5. Add appropriate CMake configuration

## Example Output

### TCP Client Example Output

```
[12:34:56.789] [INFO] [Main] Starting Enhanced TCP Client Examples
[12:34:56.790] [SUCCESS] [Example1] Connected successfully (latency: 1234 μs)
[12:34:56.791] [SUCCESS] [Example1] Sent message 1: 25 bytes (latency: 567 μs)
[12:34:56.792] [INFO] [Stats] Messages/sec sent: 1000
[12:34:56.793] [INFO] [Stats] Success rate: 98.5%
```

### UDP Server Example Output

```
[12:34:56.789] [INFO] [Main] Starting Enhanced UDP Server Examples
[12:34:56.790] [SUCCESS] [Example1] UDP server started on port 8080
[12:34:56.791] [INFO] [Router] Processing message from 127.0.0.1:12345: Hello Server
[12:34:56.792] [INFO] [SessionMgr] New client session: 127.0.0.1:12345
[12:34:56.793] [INFO] [Stats] Total messages: 1000, Success rate: 99.2%
```

## Code Examples

### Basic TCP Client Usage

```cpp
#include "atom/connection/tcpclient.hpp"

// Create client with optimized options
atom::connection::TcpClient::Options options{};
options.keep_alive = true;
options.no_delay = true;
atom::connection::TcpClient client(options);

// Set up callbacks
client.setOnConnectedCallback([]() {
    std::cout << "Connected!" << std::endl;
});

client.setOnDataReceivedCallback([](std::span<const char> data) {
    std::string message(data.begin(), data.end());
    std::cout << "Received: " << message << std::endl;
});

// Connect and communicate
if (client.connect("127.0.0.1", 8080, std::chrono::seconds(5)).has_value()) {
    std::string message = "Hello, Server!";
    std::span<const char> data(message.data(), message.size());
    client.send(data);

    client.startReceiving(1024);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    client.stopReceiving();
}
```

### Async UDP Client Usage

```cpp
#include "atom/connection/async_udpclient.hpp"

using namespace atom::async::connection;

UdpClient client;
if (client.bind(12345)) {
    // Set up async callbacks
    client.setOnDataReceivedCallback([](const std::vector<char>& data,
                                        const std::string& host, int port) {
        std::string message(data.begin(), data.end());
        std::cout << "Received from " << host << ":" << port
                  << " - " << message << std::endl;
    });

    client.startReceiving(1024);

    // Send message
    std::vector<char> message = {'H', 'e', 'l', 'l', 'o'};
    client.send("127.0.0.1", 8080, message);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    client.stopReceiving();
}
```

## Integration Examples

### TCP Server with UDP Discovery

```cpp
// Start UDP discovery service
UdpSocketHub discoveryServer;
discoveryServer.addMessageHandler([](const std::string& message,
                                    const std::string& ip, unsigned short port) {
    if (message == "DISCOVER_TCP_SERVER") {
        // Respond with TCP server information
        // Implementation details in examples
    }
});
discoveryServer.start(9999);

// Start main TCP server
SocketHub tcpServer;
tcpServer.addHandler([](std::string_view message) {
    // Handle TCP messages
});
tcpServer.start(8080);
```

### Load Balanced UDP Client

```cpp
std::vector<std::string> servers = {"server1:8080", "server2:8080", "server3:8080"};
size_t currentServer = 0;

for (int i = 0; i < 100; ++i) {
    // Round-robin load balancing
    auto [host, port] = parseEndpoint(servers[currentServer]);

    UdpClient client;
    if (client.send(host, port, message)) {
        std::cout << "Sent to " << servers[currentServer] << std::endl;
    }

    currentServer = (currentServer + 1) % servers.size();
}
```

## Performance Tuning Guide

### Buffer Size Optimization

- **Small Messages (<1KB)**: Use 1-4KB buffers
- **Medium Messages (1-10KB)**: Use 8-16KB buffers
- **Large Messages (>10KB)**: Use 32-64KB buffers
- **Streaming Data**: Use 64KB+ buffers

### Thread Pool Configuration

- **CPU-bound tasks**: threads = CPU cores
- **I/O-bound tasks**: threads = 2-4x CPU cores
- **Mixed workloads**: Start with 2x CPU cores, tune based on metrics

### Network Optimization

- Enable TCP_NODELAY for low-latency applications
- Use SO_REUSEADDR for server sockets
- Configure appropriate SO_RCVBUF and SO_SNDBUF sizes
- Consider TCP_CORK/TCP_NOPUSH for bulk transfers

## Monitoring and Metrics

### Built-in Statistics

All examples include comprehensive statistics:

- Connection counts and rates
- Message throughput and latency
- Error rates and types
- Bandwidth utilization
- Session duration and lifecycle

### Custom Metrics

Examples show how to implement:

- Application-specific counters
- Performance histograms
- Real-time dashboards
- Alerting thresholds

## Security Best Practices

### Network Security

- Use SSL/TLS for sensitive data
- Implement proper authentication
- Validate all input data
- Use secure random number generation
- Implement rate limiting

### Code Security

- Bounds checking for all buffers
- Input sanitization and validation
- Resource cleanup and RAII patterns
- Exception safety guarantees

## Platform-Specific Notes

### Windows

- Winsock initialization handled automatically
- IOCP used for high-performance async I/O
- Named pipes for FIFO communication
- COM port access for TTY

### Linux

- epoll used for async I/O multiplexing
- Unix domain sockets for IPC
- Standard POSIX TTY interfaces
- systemd integration examples

### macOS

- kqueue for async I/O
- BSD socket extensions
- Framework integration patterns

## Deployment Considerations

### Docker Deployment

```dockerfile
FROM ubuntu:20.04
RUN apt-get update && apt-get install -y libatomic1
COPY connection_* /usr/local/bin/
EXPOSE 8080 8081 9999
CMD ["connection_sockethub"]
```

### Service Configuration

Examples include systemd service files and configuration templates for production deployment.

### Scaling Patterns

- Horizontal scaling with load balancers
- Vertical scaling with thread pools
- Database connection pooling
- Caching strategies

## License

These examples are part of the Atom project and follow the same licensing terms.

## Support and Community

- **Documentation**: Full API documentation available
- **Issues**: Report bugs and feature requests on GitHub
- **Discussions**: Community forum for questions and best practices
- **Contributing**: Pull requests welcome with comprehensive tests
