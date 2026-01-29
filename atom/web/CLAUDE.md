# atom/web - Web Utilities Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **web**

---

## Module Overview

The **atom::web** module provides web-related utilities for the Atom framework. It offers HTTP client functionality, URL parsing, MIME type handling, network utilities, and time synchronization features.

### Key Features

- **HTTP Client**: Download files with progress tracking and resume support
- **Download Manager**: Queue-based download management with prioritization
- **URL Parsing**: Parse and manipulate URLs
- **MIME Types**: File extension to MIME type mapping
- **Network Utilities**: DNS lookup, IP address handling, port utilities
- **Time Synchronization**: NTP-based time sync
- **Async Operations**: Coroutine-based async I/O (with C++20)

---

## Directory Structure

```
atom/web/
├── http/              # HTTP client functionality
│   ├── curl.hpp
│   ├── curl.cpp
│   ├── downloader.hpp
│   ├── downloader.cpp
│   └── httpparser.hpp
│   └── httpparser.cpp
├── mime/              # MIME type handling
│   └── minetype.hpp
│   └── minetype.cpp
├── address/           # Address subdirectory
│   └── (address handling files)
├── time/              # Time synchronization
│   └── (time sync files)
└── utils/             # Network utilities
    ├── common.hpp
    ├── addr_info.hpp
    ├── addr_info.cpp
    ├── dns.hpp
    ├── dns.cpp
    ├── ip.hpp
    ├── ip.cpp
    ├── network.hpp
    ├── network.cpp
    ├── port.hpp
    ├── port.cpp
    └── socket.hpp
    └── socket.cpp
```

---

## Core Components

### Download Manager

```cpp
#include "atom/web/http/downloader.hpp"

using namespace atom::web;

// Create download manager
DownloadManager manager("tasks.json");

// Configure
DownloadManagerConfig config;
config.maxConcurrentDownloads = 3;
config.maxRetries = 3;
config.enableResume = true;
manager.applyConfig(config);

// Add download
auto taskId = manager.addTask(
    "https://example.com/file.zip",
    "/tmp/file.zip",
    1  // priority
);

// Set callbacks
manager.onDownloadComplete([](const std::string& url, const std::string& filePath) {
    std::cout << "Downloaded: " << filePath << "\n";
});

manager.onProgressUpdate([](const std::string& url, double progress,
                             double speed, std::chrono::seconds eta) {
    std::cout << url << ": " << progress << "% "
              << "(" << speed / 1024 << " KB/s)\n";
});

manager.onError([](const std::string& url, const std::string& error) {
    std::cerr << "Error downloading " << url << ": " << error << "\n";
});

// Start downloads
manager.start();

// Wait for completion
manager.waitForCompletion(std::chrono::minutes(5));

manager.stop();
```

### Simple Download

```cpp
#include "atom/web/http/downloader.hpp"

using namespace atom::web;

// Synchronous download
auto result = download(
    "https://example.com/data.json",
    "/tmp/data.json",
    std::chrono::minutes(5)  // timeout
);

if (result) {
    std::cout << "Downloaded " << result.value() << " bytes\n";
} else {
    std::cerr << "Download failed: "
              << downloadErrorToString(result.error()) << "\n";
}

// Download to memory
auto data = downloadToMemory("https://example.com/config.json");
if (data) {
    std::string content(data->begin(), data->end());
    std::cout << "Content: " << content << "\n";
}
```

### URL Parsing

```cpp
#include "atom/web/address/address.hpp"

using namespace atom::web;

// Parse IPv4 address
IPv4Address ipv4("192.168.1.1");
std::cout << "IP: " << ipv4.toString() << "\n";
std::cout << "Is private: " << ipv4.isPrivate() << "\n";
std::cout << "Is loopback: " << ipv4.isLoopback() << "\n";

// Parse IPv6 address
IPv6Address ipv6("::1");
std::cout << "IP: " << ipv6.toString() << "\n";

// Parse Unix domain socket path
UnixDomainAddress socket("/tmp/my.sock");
std::cout << "Path: " << socket.getPath() << "\n";
```

### MIME Type Detection

```cpp
#include "atom/web/mime/minetype.hpp"

using namespace atom::web;

MimeTypes mime;

// Get MIME type from file extension
std::string mimeType = mime.getMimeTypeFromFile("file.json");
std::cout << "MIME type: " << mimeType << "\n";  // application/json

// Get MIME type from filename
mimeType = mime.getMimeTypeFromFile("image.png");
std::cout << "MIME type: " << mimeType << "\n";  // image/png

// Check if type is common
if (mime.isCommonMimeType("application/json")) {
    std::cout << "Common MIME type\n";
}
```

### DNS Lookup

```cpp
#include "atom/web/utils/dns.hpp"

using namespace atom::web;

// DNS lookup
auto addresses = DNS::lookup("example.com");
for (const auto& addr : addresses) {
    std::cout << "Address: " << addr << "\n";
}

// Reverse DNS lookup
auto hostname = DNS::reverseLookup("93.184.216.34");
if (hostname) {
    std::cout << "Hostname: " << *hostname << "\n";
}
```

---

## Public Interfaces

### DownloadManager Class

```cpp
class DownloadManager {
public:
    // Configuration
    struct DownloadManagerConfig {
        size_t maxConcurrentDownloads{3};
        size_t maxRetries{3};
        std::chrono::seconds retryDelay{5};
        size_t chunkSize{1024 * 1024};  // 1MB
        bool enableResume{true};
        std::string tempDirectory;
        size_t maxDownloadSpeed{0};  // 0 = unlimited
    };

    struct DownloadTaskInfo {
        std::string taskId;
        std::string url;
        std::string filePath;
        DownloadStatus status;
        size_t downloadedBytes{0};
        size_t totalBytes{0};
        double progress{0.0};
        double speedBytesPerSec{0.0};
        std::chrono::seconds estimatedTimeRemaining{0};
        std::string errorMessage;
        int retryCount{0};
        int priority{0};
    };

    explicit DownloadManager(const std::string& task_file);

    // Task management
    auto addTask(std::string_view url, std::string_view filePath,
                 int priority = 0) -> std::string;
    auto removeTask(std::string_view url) -> bool;
    auto removeTaskById(std::string_view taskId) -> bool;

    // Task control
    void resumeTask(size_t index);
    auto pauseTask(std::string_view url) -> bool;
    auto resumeTaskByUrl(std::string_view url) -> bool;
    auto cancelTask(std::string_view url) -> bool;

    void pauseAll();
    void resumeAll();
    void cancelAll();

    // Progress
    auto getProgress(std::string_view url) const -> double;
    auto getTaskInfo(std::string_view url) const -> std::optional<DownloadTaskInfo>;
    auto getAllTaskInfo() const -> std::vector<DownloadTaskInfo>;

    // Callbacks
    void onDownloadComplete(std::function<void(const std::string&, const std::string&)> callback);
    void onProgressUpdate(std::function<void(const std::string&, double, double,
                                             std::chrono::seconds)> callback);
    void onError(std::function<void(const std::string&, const std::string&)> callback);
    void onStatusChange(std::function<void(const std::string&, DownloadStatus, DownloadStatus)> callback);

    // Lifecycle
    void start();
    void stop();
    bool isRunning() const;

    // Configuration
    void applyConfig(const DownloadManagerConfig& config);
    auto getConfig() const -> DownloadManagerConfig;

    // Batch operations
    auto addTasks(std::span<const std::pair<std::string, std::string>> tasks,
                  int priority = 0) -> std::vector<std::string>;
    auto saveTasks(std::string_view filePath) const -> bool;
    auto loadTasks(std::string_view filePath) -> bool;
};
```

### Address Classes

```cpp
class IPv4Address {
public:
    explicit IPv4Address(std::string_view address);
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool isPrivate() const;
    [[nodiscard]] bool isLoopback() const;
    [[nodiscard]] bool isMulticast() const;
};

class IPv6Address {
public:
    explicit IPv6Address(std::string_view address);
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool isPrivate() const;
    [[nodiscard]] bool isLoopback() const;
    [[nodiscard]] bool isLinkLocal() const;
};

class UnixDomainAddress {
public:
    explicit UnixDomainAddress(std::string_view path);
    [[nodiscard]] std::string getPath() const;
};
```

### MimeTypes Class

```cpp
class MimeTypes {
public:
    [[nodiscard]] std::string getMimeTypeFromFile(std::string_view filename) const;
    [[nodiscard]] std::string getExtensionFromMimeType(std::string_view mimeType) const;
    [[nodiscard]] bool isCommonMimeType(std::string_view mimeType) const;
    [[nodiscard]] std::vector<std::string> getCommonMimeTypes() const;
};
```

---

## Dependencies

### Required Dependencies

- **atom::utils**: Utility functions
- **atom::io**: I/O operations
- **atom::system**: System utilities
- **atom::log**: Logging framework
- **atom::type**: Type utilities

### Optional Dependencies

- **CURL**: HTTP client functionality
- **fmt**: Enhanced formatting
- **spdlog**: Enhanced logging

### Platform-Specific Libraries

**Windows:**

- wsock32, ws2_32: Windows Sockets

---

## Build Configuration

### CMake Options

```cmake
# Build web module
-DBUILD_WEB=ON

# CURL is automatically detected if available
# On Linux: sudo apt-get install libcurl4-openssl-dev
# On macOS: brew install curl
# On Windows: via vcpkg
```

### Feature Detection

```bash
# Check CURL detection
cmake -B build
# Look for: "CURL found - HTTP client features enabled"
#          or "CURL not found - HTTP client features will be disabled"
```

---

## Usage Examples

### Batch Downloads

```cpp
#include "atom/web/http/downloader.hpp"

using namespace atom::web;

DownloadManager manager("tasks.json");

// Add multiple downloads
std::vector<std::pair<std::string, std::string>> downloads = {
    {"https://example.com/file1.zip", "/tmp/file1.zip"},
    {"https://example.com/file2.zip", "/tmp/file2.zip"},
    {"https://example.com/file3.zip", "/tmp/file3.zip"}
};

auto taskIds = manager.addTasks(downloads, 1);  // priority 1

manager.start();
manager.waitForCompletion(std::chrono::hours(1));
manager.stop();
```

### Checksum Verification

```cpp
#include "atom/web/http/downloader.hpp"

using namespace atom::web;

DownloadManager manager("tasks.json");

auto taskId = manager.addTask("https://example.com/file.iso", "/tmp/file.iso");

// Set SHA256 checksum verification
manager.setChecksum(taskId, ChecksumType::SHA256,
                   "a1b2c3d4e5f6...");

manager.start();
```

### URL Manipulation

```cpp
#include "atom/web/utils/ip.hpp"

using namespace atom::web;

// Validate IP address
if (IP::isValidIPv4("192.168.1.1")) {
    std::cout << "Valid IPv4 address\n";
}

// Get local IP addresses
auto localIPs = IP::getLocalAddresses();
for (const auto& ip : localIPs) {
    std::cout << "Local IP: " << ip << "\n";
}
```

---

## Download Status

### Status Values

| Status | Description |
|--------|-------------|
| `Pending` | Task is queued but not started |
| `Downloading` | Task is actively downloading |
| `Paused` | Task is paused |
| `Completed` | Task completed successfully |
| `Failed` | Task failed with an error |
| `Cancelled` | Task was cancelled by user |
| `Verifying` | Task is verifying checksum |

---

## MIME Type Examples

### Common MIME Types

| File Extension | MIME Type |
|---------------|-----------|
| `.json` | `application/json` |
| `.xml` | `application/xml` |
| `.html` | `text/html` |
| `.css` | `text/css` |
| `.js` | `application/javascript` |
| `.png` | `image/png` |
| `.jpg`, `.jpeg` | `image/jpeg` |
| `.gif` | `image/gif` |
| `.svg` | `image/svg+xml` |
| `.pdf` | `application/pdf` |
| `.zip` | `application/zip` |
| `.tar` | `application/x-tar` |
| `.gz` | `application/gzip` |

---

## Performance Considerations

### Concurrent Downloads

```cpp
// Good: Reasonable concurrent limit
config.maxConcurrentDownloads = 3;

// Bad: Too many concurrent downloads
config.maxConcurrentDownloads = 100;  // May saturate bandwidth
```

### Chunk Size

```cpp
// Good: Balanced chunk size
config.chunkSize = 1024 * 1024;  // 1MB

// For fast connections
config.chunkSize = 4 * 1024 * 1024;  // 4MB

// For slow connections
config.chunkSize = 256 * 1024;  // 256KB
```

---

## Testing

### Test Organization

Tests are located in `tests/web/`:

- `test_downloader.cpp`: Download manager tests
- `test_mime.cpp`: MIME type tests
- `test_address.cpp`: Address parsing tests
- `test_dns.cpp`: DNS lookup tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run web tests
ctest -R web_ --output-on-failure
```

---

## Best Practices

### Error Handling

```cpp
auto result = download(url, filePath, timeout);
if (!result) {
    switch (result.error()) {
        case DownloadError::NetworkError:
            std::cerr << "Network error\n";
            break;
        case DownloadError::Timeout:
            std::cerr << "Download timed out\n";
            break;
        case DownloadError::FileError:
            std::cerr << "File I/O error\n";
            break;
        default:
            std::cerr << "Unknown error\n";
    }
}
```

### Resource Management

```cpp
// Good: RAII with automatic cleanup
{
    DownloadManager manager("tasks.json");
    manager.start();
    // ... use manager ...
} // Automatically stopped

// Bad: Manual cleanup
DownloadManager* manager = new DownloadManager("tasks.json");
manager->start();
// ... forget to stop ...
delete manager;  // May not stop cleanly
```

---

## Related Modules

- **atom::connection**: Alternative network communication
- **atom::io**: File I/O for downloaded content
- **atom::system**: System time operations

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented HTTP client, download manager, utilities
- Added usage examples and best practices

---

**Maintained By:** Atom Framework Team
