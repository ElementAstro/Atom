# Atom Web Examples

This directory contains comprehensive examples demonstrating all features of the Atom Web module. The examples are organized by functionality and complexity level.

## Directory Structure

```
example/web/
├── README.md                           # This file
├── CMakeLists.txt                      # Build configuration
├── address/                            # Address handling examples
│   ├── ipv4_example.cpp               # IPv4 address operations
│   ├── ipv6_example.cpp               # IPv6 address operations
│   ├── unix_domain_example.cpp        # Unix domain socket addresses
│   └── address_factory_example.cpp    # Polymorphic address handling
├── http/                               # HTTP client examples
│   ├── downloader_example.cpp         # Download manager features
│   ├── curl_comprehensive_example.cpp # Advanced CURL operations
│   └── httpparser_comprehensive_example.cpp # HTTP header parsing
├── mime/                               # MIME type examples
│   └── minetype_comprehensive_example.cpp # MIME type detection
├── integration_example.cpp            # Component integration
├── curl.cpp                           # Basic CURL example (legacy)
├── httpparser.cpp                     # Basic HTTP parser (legacy)
├── minetype.cpp                       # Basic MIME type (legacy)
├── time.cpp                           # Time manager example
└── utils.cpp                          # Network utilities example
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

- **`downloader_example.cpp`**: Multi-threaded downloads, progress tracking, task management
- **`curl_comprehensive_example.cpp`**: Advanced HTTP operations, authentication, SSL, async requests
- **`httpparser_comprehensive_example.cpp`**: HTTP header parsing, real-world scenarios

### 3. MIME Type Examples (`mime/`)

- **`minetype_comprehensive_example.cpp`**: MIME type detection, content analysis, custom types

### 4. Integration Examples

- **`integration_example.cpp`**: Demonstrates how different components work together

### 5. Basic Examples (Legacy)

- **`curl.cpp`**: Basic CURL wrapper usage
- **`httpparser.cpp`**: Basic HTTP header parsing
- **`minetype.cpp`**: Basic MIME type detection
- **`time.cpp`**: Time manager operations
- **`utils.cpp`**: Network utility functions

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
cmake -DATOM_EXAMPLE_WEB_BUILD_ADDRESS=ON -DATOM_EXAMPLE_WEB_BUILD_HTTP=OFF ..

# Build only HTTP examples
cmake -DATOM_EXAMPLE_WEB_BUILD_HTTP=ON -DATOM_EXAMPLE_WEB_BUILD_ADDRESS=OFF ..

# Build only integration examples
cmake -DATOM_EXAMPLE_WEB_BUILD_INTEGRATION=ON -DATOM_EXAMPLE_WEB_BUILD_BASIC=OFF ..
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
./web_http_downloader_example
./web_http_curl_comprehensive_example
./web_http_httpparser_comprehensive_example

# Run MIME examples
./web_mime_minetype_comprehensive_example

# Run integration example
./web_integration_example

# Run basic examples
./web_curl
./web_httpparser
./web_minetype
./web_time
./web_utils
```

## Example Features

### Address Examples

- **IPv4**: Parsing, validation, CIDR notation, subnet operations, range checking
- **IPv6**: Parsing, compression, prefix operations, special addresses
- **Unix Domain**: Socket paths, named pipes (Windows), validation
- **Factory**: Automatic type detection, polymorphic operations

### HTTP Examples

- **Download Manager**: Multi-threaded downloads, progress callbacks, task control
- **CURL Comprehensive**: All HTTP methods, authentication, SSL, file uploads, async operations
- **HTTP Parser**: Header parsing, multi-value headers, real-world scenarios

### MIME Examples

- **Comprehensive**: Extension-based detection, content-based detection, custom types

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

5. **Network Utilities**
   - DNS resolution
   - Port scanning
   - Connectivity testing
   - Process identification

6. **Component Integration**
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
