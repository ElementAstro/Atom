# Atom Web Examples

This directory contains comprehensive examples demonstrating all features of the Atom Web module. The examples are organized by functionality to mirror the structure of the `atom/web/` module.

## Directory Structure

```text
example/web/
├── README.md                                    # This file
├── CMakeLists.txt                               # Build configuration
├── address/                                     # Address handling examples
│   ├── ipv4_example.cpp                        # IPv4 address operations
│   ├── ipv6_example.cpp                        # IPv6 address operations
│   ├── unix_domain_example.cpp                 # Unix domain socket addresses
│   └── address_factory_example.cpp             # Polymorphic address handling
├── http/                                        # HTTP client examples
│   ├── curl_basic_example.cpp                  # Basic CURL usage
│   ├── curl_comprehensive_example.cpp          # Advanced CURL operations
│   ├── curl_async_example.cpp                  # Async HTTP operations
│   ├── curl_proxy_example.cpp                  # Proxy and HEAD requests
│   ├── downloader_example.cpp                  # Download manager features
│   ├── httpparser_basic_example.cpp            # Basic HTTP header parsing
│   ├── httpparser_comprehensive_example.cpp    # Advanced HTTP parsing
│   └── httpparser_cookies_url_example.cpp      # Cookie and URL parsing
├── mime/                                        # MIME type examples
│   ├── minetype_basic_example.cpp              # Basic MIME type usage
│   └── minetype_comprehensive_example.cpp      # Advanced MIME detection
├── time/                                        # Time management examples
│   ├── time_manager_example.cpp                # System time and NTP sync
│   └── time_utils_example.cpp                  # Time validation utilities
├── utils/                                       # Network utility examples
│   ├── addr_info_example.cpp                   # Address info operations
│   ├── dns_resolution_example.cpp              # DNS resolution
│   ├── ip_validation_example.cpp               # IP address validation
│   ├── network_connectivity_example.cpp        # Internet connectivity check
│   ├── network_utils_example.cpp               # General network utilities
│   ├── port_operations_example.cpp             # Port scanning and management
│   └── socket_operations_example.cpp           # Socket operations
└── integration_example.cpp                     # Component integration
```

## Example Categories

### 1. Address Examples (`address/`)

These examples demonstrate network address handling capabilities:

- **`ipv4_example.cpp`**: IPv4 address parsing, validation, CIDR operations, subnet calculations
- **`ipv6_example.cpp`**: IPv6 address handling, compression, prefix operations
- **`unix_domain_example.cpp`**: Unix domain sockets and Windows named pipes
- **`address_factory_example.cpp`**: Polymorphic address creation and manipulation

### 2. HTTP Examples (`http/`)

These examples show HTTP client functionality:

- **`curl_basic_example.cpp`**: Basic CURL wrapper usage for simple HTTP requests
- **`curl_comprehensive_example.cpp`**: Advanced HTTP operations, authentication, SSL, file uploads
- **`curl_async_example.cpp`**: Asynchronous HTTP operations, parallel requests, speed limiting
- **`curl_proxy_example.cpp`**: Proxy configuration and HEAD request operations
- **`downloader_example.cpp`**: Multi-threaded downloads, progress tracking, task management
- **`httpparser_basic_example.cpp`**: Basic HTTP header parsing and manipulation
- **`httpparser_comprehensive_example.cpp`**: Advanced HTTP parsing, real-world scenarios
- **`httpparser_cookies_url_example.cpp`**: Cookie parsing and URL parameter handling

### 3. MIME Type Examples (`mime/`)

- **`minetype_basic_example.cpp`**: Basic MIME type detection and extension guessing
- **`minetype_comprehensive_example.cpp`**: Advanced MIME detection, content analysis, custom types, JSON/XML export

### 4. Time Management Examples (`time/`)

- **`time_manager_example.cpp`**: System time operations, NTP synchronization, timezone management
- **`time_utils_example.cpp`**: Time validation utilities for datetime and hostname validation

### 5. Network Utility Examples (`utils/`)

- **`addr_info_example.cpp`**: Address information operations, filtering, sorting, comparison
- **`dns_resolution_example.cpp`**: DNS resolution, local IP discovery, DNS caching
- **`ip_validation_example.cpp`**: IPv4 and IPv6 address validation and conversion
- **`network_connectivity_example.cpp`**: Internet connectivity checking and monitoring
- **`network_utils_example.cpp`**: General network utilities and helper functions
- **`port_operations_example.cpp`**: Port scanning, availability checking, process management
- **`socket_operations_example.cpp`**: Socket creation, binding, and configuration

### 6. Integration Examples

- **`integration_example.cpp`**: Demonstrates how different components work together in real-world scenarios

## Building Examples

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler
- libcurl development libraries
- loguru library (optional, for logging)

### Build All Examples

```bash
mkdir build
cd build
cmake ..
make
```

### Build Specific Categories

You can control which categories of examples to build:

```bash
# Build only address examples
cmake -DATOM_EXAMPLE_WEB_BUILD_ADDRESS=ON -DATOM_EXAMPLE_WEB_BUILD_ALL=OFF ..

# Build only HTTP examples
cmake -DATOM_EXAMPLE_WEB_BUILD_HTTP=ON -DATOM_EXAMPLE_WEB_BUILD_ALL=OFF ..

# Build only time examples
cmake -DATOM_EXAMPLE_WEB_BUILD_TIME=ON -DATOM_EXAMPLE_WEB_BUILD_ALL=OFF ..

# Build only utils examples
cmake -DATOM_EXAMPLE_WEB_BUILD_UTILS=ON -DATOM_EXAMPLE_WEB_BUILD_ALL=OFF ..

# Build only integration examples
cmake -DATOM_EXAMPLE_WEB_BUILD_INTEGRATION=ON -DATOM_EXAMPLE_WEB_BUILD_ALL=OFF ..
```

### Build Individual Examples

```bash
# Build only IPv4 example
cmake -DATOM_EXAMPLE_WEB_IPV4_EXAMPLE=ON -DATOM_EXAMPLE_WEB_BUILD_ALL=OFF ..
```

## Running Examples

After building, executables will be in the build directory:

```bash
# Run address examples
./web_address_ipv4_example
./web_address_ipv6_example
./web_address_unix_domain_example
./web_address_address_factory_example

# Run HTTP examples
./web_http_curl_basic_example
./web_http_curl_comprehensive_example
./web_http_curl_async_example
./web_http_curl_proxy_example
./web_http_downloader_example
./web_http_httpparser_basic_example
./web_http_httpparser_comprehensive_example
./web_http_httpparser_cookies_url_example

# Run MIME examples
./web_mime_minetype_basic_example
./web_mime_minetype_comprehensive_example

# Run time examples
./web_time_time_manager_example
./web_time_time_utils_example

# Run utils examples
./web_utils_addr_info_example
./web_utils_dns_resolution_example
./web_utils_ip_validation_example
./web_utils_network_connectivity_example
./web_utils_network_utils_example
./web_utils_port_operations_example
./web_utils_socket_operations_example

# Run integration example
./web_integration_example
```

## Example Features

### Address Examples

- **IPv4**: Parsing, validation, CIDR notation, subnet operations, range checking
- **IPv6**: Parsing, compression, prefix operations, special addresses
- **Unix Domain**: Socket paths, named pipes (Windows), validation
- **Factory**: Automatic type detection, polymorphic operations

### HTTP Examples

- **CURL Basic**: Simple HTTP requests for quick start
- **CURL Comprehensive**: All HTTP methods, authentication, SSL, file uploads
- **CURL Async**: Asynchronous operations, parallel requests, speed limiting
- **CURL Proxy**: Proxy configuration, HEAD requests
- **Download Manager**: Multi-threaded downloads, progress callbacks, task control
- **HTTP Parser Basic**: Simple header parsing and manipulation
- **HTTP Parser Comprehensive**: Advanced parsing, multi-value headers, real-world scenarios
- **HTTP Parser Cookies/URL**: Cookie parsing, URL parameter handling

### MIME Examples

- **Basic**: Simple extension-based and content-based detection
- **Comprehensive**: Advanced detection, custom types, JSON/XML export, caching

### Time Examples

- **Time Manager**: System time operations, NTP synchronization, timezone management, admin privileges
- **Time Utils**: DateTime and hostname validation utilities

### Utils Examples

- **Address Info**: getAddrInfo operations, filtering, sorting, comparison, JSON conversion
- **DNS Resolution**: DNS lookup, local IP discovery, DNS caching with TTL
- **IP Validation**: IPv4/IPv6 validation, string conversion
- **Network Connectivity**: Internet connectivity checking, monitoring, performance measurement
- **Network Utils**: General network utility functions
- **Port Operations**: Port scanning, availability checking, process identification and management
- **Socket Operations**: Socket creation, binding, non-blocking mode, connection timeout

### Integration Examples

- **Web Crawler**: Combines network utilities, CURL, HTTP parsing, and MIME detection
- **File Download with Analysis**: Downloads files and analyzes them with MIME detection
- **Network Service Discovery**: Uses address handling and port scanning
- **API Client**: Demonstrates complete API interaction workflow

## Key Features Demonstrated

1. **Network Address Handling**
   - IPv4/IPv6 address parsing and validation
   - CIDR notation support
   - Subnet calculations
   - Cross-platform Unix domain sockets

2. **HTTP Client Operations**
   - All HTTP methods (GET, POST, PUT, DELETE)
   - Authentication (Basic, Bearer token)
   - SSL/TLS configuration
   - File uploads and downloads
   - Asynchronous operations
   - Progress tracking

3. **HTTP Header Processing**
   - Parsing request/response headers
   - Multi-value header handling
   - Header manipulation
   - Real-world scenarios (CORS, redirects, etc.)

4. **MIME Type Detection**
   - Extension-based detection
   - Content-based detection
   - Custom MIME type registration
   - URL parsing

5. **Time Management**
   - System time operations
   - NTP synchronization
   - Timezone management
   - DateTime validation

6. **Network Utilities**
   - Address information operations
   - DNS resolution and caching
   - IP address validation
   - Internet connectivity checking
   - Port scanning and management
   - Socket operations

7. **Component Integration**
   - Combining multiple components
   - Real-world use cases
   - Error handling
   - Resource management

## Error Handling

All examples include comprehensive error handling demonstrating:

- Exception handling for invalid inputs
- Network error recovery
- Resource cleanup
- Graceful degradation

## Logging

Examples use loguru for logging when available. Log files are created in the working directory:

- `*_example.log` - Individual example logs
- Console output for user-friendly information

## Platform Support

Examples are designed to work on:

- Linux (full functionality)
- macOS (full functionality)
- Windows (with platform-specific adaptations for named pipes)

## Contributing

When adding new examples:

1. Follow the existing code structure
2. Include comprehensive error handling
3. Add logging for debugging
4. Update this README
5. Add appropriate CMake configuration
6. Test on multiple platforms

## License

These examples are licensed under GPL3, same as the main Atom project.
