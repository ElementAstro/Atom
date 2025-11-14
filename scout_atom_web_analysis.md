# Atom Web Module Architecture Analysis

## Directory Structure

```
atom/web/
├── address/
│   ├── address.hpp          (Base class and exceptions)
│   ├── address.cpp
│   ├── ipv4.hpp             (IPv4 address implementation)
│   ├── ipv4.cpp
│   ├── ipv6.hpp             (IPv6 address implementation)
│   ├── ipv6.cpp
│   ├── unix_domain.hpp      (Unix domain socket implementation)
│   ├── unix_domain.cpp
│   └── main.hpp             (Unified interface header)
├── http/
│   ├── curl.hpp             (libcurl wrapper)
│   ├── curl.cpp
│   ├── downloader.hpp       (Download manager)
│   ├── downloader.cpp
│   ├── httpparser.hpp       (HTTP protocol parser)
│   └── httpparser.cpp
├── mime/
│   ├── minetype.hpp         (MIME type detection)
│   └── minetype.cpp
├── time/
│   ├── time_manager.hpp     (Time management interface)
│   ├── time_manager.cpp
│   ├── time_manager_impl.hpp (Implementation details)
│   ├── time_manager_impl.cpp
│   ├── time_utils.hpp       (Time utilities)
│   ├── time_utils.cpp
│   └── time_error.hpp       (Error definitions)
├── utils/
│   ├── common.hpp           (Common type definitions)
│   ├── addr_info.hpp        (Address info utilities)
│   ├── addr_info.cpp
│   ├── dns.hpp              (DNS resolution)
│   ├── dns.cpp
│   ├── ip.hpp               (IP validation utilities)
│   ├── ip.cpp
│   ├── network.hpp          (Network connectivity)
│   ├── network.cpp
│   ├── port.hpp             (Port utilities)
│   ├── port.cpp
│   ├── socket.hpp           (Socket operations)
│   └── socket.cpp
├── address.hpp              (Facade header)
├── curl.hpp                 (Deprecated compatibility header)
├── downloader.hpp           (Deprecated compatibility header)
├── httpparser.hpp           (Deprecated compatibility header)
├── minetype.hpp             (Deprecated compatibility header)
├── time.hpp                 (Compatibility header)
└── utils.hpp                (Facade header)
```

## Module Organization

The web module is organized into 5 submodules, each providing specific functionality:

1. **address/**: Network address handling (IPv4, IPv6, Unix domain sockets)
2. **http/**: HTTP protocol handling (CURL, downloader, parser)
3. **mime/**: MIME type detection and management
4. **time/**: System time management and NTP synchronization
5. **utils/**: Network utilities (DNS, IP validation, port scanning, sockets)

Facade headers at the module root (`address.hpp`, `utils.hpp`) aggregate submodule exports.

## Complete Public API Inventory

### Address Module

#### Base Exception Classes (address/address.hpp)

```cpp
class AddressException : public std::runtime_error
// Custom base exception for address operations

class InvalidAddressFormat : public AddressException
// Thrown when address format is invalid

class AddressRangeError : public AddressException
// Thrown when address range operations fail
```

#### Address Base Class (address/address.hpp)

```cpp
class Address
```

**Constructor Methods:**

- `Address()` = default
- `~Address()` virtual = default
- Copy/move constructors and assignment operators (defaulted)

**Virtual Methods (to be overridden):**

- `virtual auto parse(std::string_view address) -> bool`
- `virtual void printAddressType() const`
- `virtual auto isInRange(std::string_view start, std::string_view end) -> bool`
- `[[nodiscard]] virtual auto toBinary() const -> std::string`
- `[[nodiscard]] virtual auto isEqual(const Address& other) const -> bool`
- `[[nodiscard]] virtual auto getType() const -> std::string_view`
- `[[nodiscard]] virtual auto getNetworkAddress(std::string_view mask) const -> std::string`
- `[[nodiscard]] virtual auto getBroadcastAddress(std::string_view mask) const -> std::string`
- `[[nodiscard]] virtual auto isSameSubnet(const Address& other, std::string_view mask) const -> bool`
- `[[nodiscard]] virtual auto toHex() const -> std::string`

**Public Methods:**

- `[[nodiscard]] auto getAddress() const -> std::string_view`
- `static auto createFromString(std::string_view addressStr) -> std::unique_ptr<Address>` (Factory method)

#### IPv4 Class (address/ipv4.hpp)

Inherits from `Address`

**Constructors:**

- `IPv4()` = default
- `explicit IPv4(std::string_view address)` throws InvalidAddressFormat

**Override Methods:**

- `auto parse(std::string_view address) -> bool override`
- `void printAddressType() const override`
- `auto isInRange(std::string_view start, std::string_view end) -> bool override`
- `[[nodiscard]] auto toBinary() const -> std::string override`
- `[[nodiscard]] auto isEqual(const Address& other) const -> bool override`
- `[[nodiscard]] auto getType() const -> std::string_view override`
- `[[nodiscard]] auto getNetworkAddress(std::string_view mask) const -> std::string override`
- `[[nodiscard]] auto getBroadcastAddress(std::string_view mask) const -> std::string override`
- `[[nodiscard]] auto isSameSubnet(const Address& other, std::string_view mask) const -> bool override`
- `[[nodiscard]] auto toHex() const -> std::string override`

**CIDR Support:**

- `auto parseCIDR(std::string_view cidr) -> bool`
- `[[nodiscard]] static auto getPrefixLength(std::string_view cidr) -> std::optional<int>`

**Private Methods:**

- `[[nodiscard]] auto ipToInteger(std::string_view ipAddr) const -> uint32_t`
- `[[nodiscard]] auto integerToIp(uint32_t ipAddr) const -> std::string`
- `[[nodiscard]] static auto isValidIPv4(std::string_view address) -> bool`

**Member Variables:**

- `uint32_t ipValue{0}`

#### IPv6 Class (address/ipv6.hpp)

Inherits from `Address`

**Constructors:**

- `IPv6()` = default
- `explicit IPv6(std::string_view address)` throws InvalidAddressFormat

**Override Methods:** (Same as IPv4 but for IPv6)

- All base `Address` virtual methods overridden
- `auto parseCIDR(std::string_view cidr) -> bool`
- `[[nodiscard]] static auto getPrefixLength(std::string_view cidr) -> std::optional<int>`
- `[[nodiscard]] static auto isValidIPv6(std::string_view address) -> bool`

**Private Methods:**

- `[[nodiscard]] auto ipToArray(std::string_view ipAddr) const -> std::array<uint16_t, 8>`
- `[[nodiscard]] auto arrayToIp(const std::array<uint16_t, 8>& segments) const -> std::string`
- `void applyPrefixMask(int prefixLength)`

**Member Variables:**

- `std::array<uint16_t, 8> ipSegments{}`

#### UnixDomain Class (address/unix_domain.hpp)

Inherits from `Address`

**Constructors:**

- `UnixDomain()` = default
- `explicit UnixDomain(std::string_view path)` throws InvalidAddressFormat

**Override Methods:**

- `auto parse(std::string_view path) -> bool override`
- `void printAddressType() const override`
- `auto isInRange(std::string_view start, std::string_view end) -> bool override` (lexicographical)
- `[[nodiscard]] auto toBinary() const -> std::string override`
- `[[nodiscard]] auto isEqual(const Address& other) const -> bool override`
- `[[nodiscard]] auto getType() const -> std::string_view override`
- `[[nodiscard]] auto getNetworkAddress(std::string_view mask) const -> std::string override` (directory)
- `[[nodiscard]] auto getBroadcastAddress(std::string_view mask) const -> std::string override` (wildcard)
- `[[nodiscard]] auto isSameSubnet(const Address& other, std::string_view mask) const -> bool override` (same directory)
- `[[nodiscard]] auto toHex() const -> std::string override`

**Static Methods:**

- `[[nodiscard]] static auto isValidPath(std::string_view path) -> bool`

**Private Methods:**

- `[[nodiscard]] static auto fastIsValidPath(std::string_view path) -> bool`
- `[[nodiscard]] static auto getDirectoryPath(std::string_view path) -> std::string`

#### Address Module Utility (address/main.hpp)

```cpp
inline auto createAddress(std::string_view addressString)
// Convenience function wrapping Address::createFromString()
```

---

### HTTP Module

#### CurlWrapper Class (http/curl.hpp)

**Design Pattern:** Non-copyable, no move semantics

**Constructors/Destructors:**

- `CurlWrapper()`
- `~CurlWrapper()`
- Copy/move operations deleted

**Configuration Methods (fluent interface):**

- `auto setUrl(const std::string &url) -> CurlWrapper &`
- `auto setRequestMethod(const std::string &method) -> CurlWrapper &`
- `auto addHeader(const std::string &key, const std::string &value) -> CurlWrapper &`
- `auto setOnErrorCallback(std::function<void(CURLcode)> callback) -> CurlWrapper &`
- `auto setOnResponseCallback(std::function<void(const std::string &)> callback) -> CurlWrapper &`
- `auto setTimeout(long timeout) -> CurlWrapper &`
- `auto setFollowLocation(bool follow) -> CurlWrapper &`
- `auto setRequestBody(const std::string &data) -> CurlWrapper &`
- `auto setUploadFile(const std::string &filePath) -> CurlWrapper &`
- `auto setProxy(const std::string &proxy) -> CurlWrapper &`
- `auto setSSLOptions(bool verifyPeer, bool verifyHost) -> CurlWrapper &`
- `auto setMaxDownloadSpeed(size_t speed) -> CurlWrapper &`

**Execution Methods:**

- `auto perform() -> std::string` (synchronous)
- `auto performAsync() -> CurlWrapper &` (asynchronous)
- `void waitAll()` (wait for async to complete)

**Internal:**

- PIMPL pattern with `class Impl` and `std::unique_ptr<Impl> pImpl_`

#### DownloadManager Class (http/downloader.hpp)

**Design Pattern:** PIMPL idiom, non-copyable, move-compatible

**Constructors:**

- `explicit DownloadManager(const std::string& task_file)` throws std::runtime_error
- `~DownloadManager()`
- Copy operations deleted
- Move operations defaulted

**Task Management:**

- `void addTask(const std::string& url, const std::string& filepath, int priority = 0)` throws std::invalid_argument
- `bool removeTask(size_t index)`
- `void pauseTask(size_t index)`
- `void resumeTask(size_t index)`
- `void cancelTask(size_t index)`

**Control Methods:**

- `void start(size_t thread_count = std::thread::hardware_concurrency(), size_t download_speed = 0)` throws std::runtime_error
- `void stop()`
- `void setThreadCount(size_t thread_count)`
- `void setMaxRetries(size_t retries)`

**Status Queries:**

- `size_t getDownloadedBytes(size_t index) const`
- `size_t getTotalBytes(size_t index) const`
- `double getProgress(size_t index) const` (0.0-100.0, -1.0 if unknown)
- `size_t getActiveTaskCount() const`
- `size_t getTotalTaskCount() const`
- `bool isRunning() const`

**Callbacks:**

- `void onDownloadComplete(const std::function<void(size_t, bool)>& callback)` (index, success)
- `void onProgressUpdate(const std::function<void(size_t, double)>& callback)` (index, percentage)
- `void onError(const std::function<void(size_t, const std::string&)>& callback)` (index, message)

**Internal:**

- PIMPL pattern with `class Impl` and `std::unique_ptr<Impl> impl_`

#### HTTP Enumerations and Structures (http/httpparser.hpp)

```cpp
enum class HttpMethod {
    GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH, TRACE, CONNECT, UNKNOWN
};

enum class HttpVersion {
    HTTP_1_0, HTTP_1_1, HTTP_2_0, HTTP_3_0, UNKNOWN
};

struct HttpStatus {
    int code;
    std::string description;
    // Static factory methods for common status codes
    static HttpStatus OK();
    static HttpStatus Created();
    static HttpStatus Accepted();
    static HttpStatus NoContent();
    static HttpStatus MovedPermanently();
    static HttpStatus Found();
    static HttpStatus BadRequest();
    static HttpStatus Unauthorized();
    static HttpStatus Forbidden();
    static HttpStatus NotFound();
    static HttpStatus MethodNotAllowed();
    static HttpStatus InternalServerError();
    static HttpStatus NotImplemented();
    static HttpStatus BadGateway();
    static HttpStatus ServiceUnavailable();
};

struct Cookie {
    std::string name;
    std::string value;
    std::optional<std::chrono::system_clock::time_point> expires;
    std::optional<int> maxAge;
    std::optional<std::string> domain;
    std::optional<std::string> path;
    bool secure;
    bool httpOnly;
    std::optional<std::string> sameSite;
};
```

#### HttpHeaderParser Class (http/httpparser.hpp)

**Design Pattern:** PIMPL idiom with shared_ptr

**Constructors:**

- `HttpHeaderParser()`
- `~HttpHeaderParser()`

**Parsing Methods:**

- `void parseHeaders(const std::string& rawHeaders)`
- `bool parseRequest(const std::string& rawRequest)`
- `bool parseResponse(const std::string& rawResponse)`

**Header Manipulation:**

- `void setHeaderValue(const std::string& key, const std::string& value)`
- `void setHeaders(const std::map<std::string, std::vector<std::string>>& headers)`
- `void addHeaderValue(const std::string& key, const std::string& value)`
- `[[nodiscard]] auto getHeaderValues(const std::string& key) const -> std::optional<std::vector<std::string>>`
- `[[nodiscard]] auto getHeaderValue(const std::string& key) const -> std::optional<std::string>`
- `void removeHeader(const std::string& key)`
- `[[nodiscard]] auto getAllHeaders() const -> std::map<std::string, std::vector<std::string>>`
- `[[nodiscard]] auto hasHeader(const std::string& key) const -> bool`
- `void clearHeaders()`

**Cookie Management:**

- `void addCookie(const Cookie& cookie)`
- `[[nodiscard]] std::map<std::string, std::string> parseCookies(const std::string& cookieStr) const`
- `[[nodiscard]] std::vector<Cookie> getAllCookies() const`
- `[[nodiscard]] std::optional<Cookie> getCookie(const std::string& name) const`
- `void removeCookie(const std::string& name)`

**URL and Query Parsing:**

- `[[nodiscard]] std::map<std::string, std::string> parseUrlParameters(const std::string& url) const`

**HTTP Method Management:**

- `void setMethod(HttpMethod method)`
- `[[nodiscard]] HttpMethod getMethod() const`
- `[[nodiscard]] static HttpMethod stringToMethod(const std::string& methodStr)`
- `[[nodiscard]] static std::string methodToString(HttpMethod method)`

**HTTP Status Management:**

- `void setStatus(const HttpStatus& status)`
- `[[nodiscard]] HttpStatus getStatus() const`

**HTTP Version Management:**

- `void setVersion(HttpVersion version)`
- `[[nodiscard]] HttpVersion getVersion() const`

**Path Management:**

- `void setPath(const std::string& path)`
- `[[nodiscard]] std::string getPath() const`

**Body Management:**

- `void setBody(const std::string& body)`
- `[[nodiscard]] std::string getBody() const`

**Building Methods:**

- `[[nodiscard]] std::string buildRequest() const`
- `[[nodiscard]] std::string buildResponse() const`

**URL Encoding/Decoding:**

- `[[nodiscard]] static std::string urlEncode(const std::string& str)`
- `[[nodiscard]] static std::string urlDecode(const std::string& str)`

**Internal:**

- PIMPL pattern with `class HttpHeaderParserImpl` and `std::shared_ptr<HttpHeaderParserImpl> impl_`

---

### MIME Module

#### MIME Type Structures and Concepts (mime/minetype.hpp)

```cpp
class MimeTypeException : public std::runtime_error
// Exception for MIME type operations

template <typename T>
concept PathLike = requires(T a) {
    { std::string(a) } -> std::convertible_to<std::string>;
};
// Concept for path-like types

struct MimeTypeConfig {
    bool lenient;                           // Lenient MIME detection
    bool useCache;                          // Enable caching
    size_t cacheSize;                       // Max cache entries (default 1000)
    bool enableDeepScanning;                // Deep content scanning
    std::string defaultType;                // Default MIME type
};
```

#### MimeTypes Class (mime/minetype.hpp)

**Design Pattern:** PIMPL idiom

**Constructors:**

- `explicit MimeTypes(std::span<const std::string> knownFiles, bool lenient = false)` throws MimeTypeException
- `explicit MimeTypes(std::span<const std::string> knownFiles, const MimeTypeConfig& config)` throws MimeTypeException
- `~MimeTypes()`

**File I/O:**

- `void readJson(const std::string& jsonFile)` throws MimeTypeException
- `void readXml(const std::string& xmlFile)` throws MimeTypeException
- `void exportToJson(const std::string& jsonFile) const` throws MimeTypeException
- `void exportToXml(const std::string& xmlFile) const` throws MimeTypeException

**MIME Type Guessing:**

- `std::pair<std::optional<std::string>, std::optional<std::string>> guessType(const std::string& url) const` (returns type and charset)
- `std::optional<std::string> guessExtension(const std::string& mimeType) const`
- `std::vector<std::string> guessAllExtensions(const std::string& mimeType) const`
- `template <PathLike T> std::optional<std::string> guessTypeByContent(const T& filePath) const` throws MimeTypeException

**Type Management:**

- `void addType(const std::string& mimeType, const std::string& extension)` throws MimeTypeException
- `void addTypesBatch(std::span<const std::pair<std::string, std::string>> types)`
- `void listAllTypes() const`

**Queries:**

- `bool hasMimeType(const std::string& mimeType) const`
- `bool hasExtension(const std::string& extension) const`

**Configuration:**

- `void updateConfig(const MimeTypeConfig& config)`
- `MimeTypeConfig getConfig() const`
- `void clearCache()`

**Internal:**

- PIMPL pattern with `class Impl` and `std::unique_ptr<Impl> pImpl`

---

### Time Module

#### Time Error Definition (time/time_error.hpp)

```cpp
enum class TimeError {
    None,
    InvalidParameter,
    PermissionDenied,
    NetworkError,
    SystemError,
    TimeoutError,
    NotSupported
};

namespace detail {
    class time_error_category : public std::error_category
    // Custom error category implementation
}

inline std::error_code make_error_code(TimeError e)
// Error code factory function
```

#### Time Utilities (time/time_utils.hpp)

**Constants:**

```cpp
constexpr int MIN_VALID_YEAR = 1970;
constexpr int MAX_VALID_YEAR = 2038;
constexpr int MIN_VALID_MONTH = 1;
constexpr int MAX_VALID_MONTH = 12;
constexpr int MIN_VALID_DAY = 1;
constexpr int MAX_VALID_DAY = 31;
constexpr int MIN_VALID_HOUR = 0;
constexpr int MAX_VALID_HOUR = 23;
constexpr int MIN_VALID_MINUTE = 0;
constexpr int MAX_VALID_MINUTE = 59;
constexpr int MIN_VALID_SECOND = 0;
constexpr int MAX_VALID_SECOND = 59;
constexpr int NTP_PACKET_SIZE = 48;
constexpr uint16_t NTP_PORT = 123;
constexpr uint32_t NTP_DELTA = 2208988800UL;
```

**Validation Functions:**

- `bool validateDateTime(int year, int month, int day, int hour, int minute, int second)`
- `bool validateHostname(std::string_view hostname)`

#### TimeManager Class (time/time_manager.hpp)

**Design Pattern:** PIMPL idiom with move semantics

**Constructors:**

- `TimeManager()` throws std::runtime_error
- `~TimeManager()`
- Copy operations deleted
- Move operations defaulted

**System Time Operations:**

- `auto getSystemTime() -> std::time_t` throws std::system_error
- `auto getSystemTimePoint() -> std::chrono::system_clock::time_point` throws std::system_error
- `auto setSystemTime(int year, int month, int day, int hour, int minute, int second) -> std::error_code`

**Timezone Management:**

- `auto setSystemTimezone(std::string_view timezone) -> std::error_code`

**Time Synchronization:**

- `auto syncTimeFromRTC() -> std::error_code` (Real-Time Clock)
- `auto getNtpTime(std::string_view hostname, std::chrono::milliseconds timeout = std::chrono::seconds(5)) -> std::optional<std::time_t>` (cached)

**Privileges:**

- `bool hasAdminPrivileges() const`

**Testing Support:**

- `void setImpl(std::unique_ptr<TimeManagerImpl> impl)`

**Internal:**

- PIMPL pattern with forward-declared `TimeManagerImpl` and `std::unique_ptr<TimeManagerImpl> impl_`

---

### Utils Module

#### Common Types (utils/common.hpp)

```cpp
template <typename T>
concept PortNumber = std::integral<T> && requires(T port) {
    { port >= 0 && port <= 65535 } -> std::same_as<bool>;
};
// Concept ensuring integral types represent valid ports (0-65535)
```

#### Address Info Utilities (utils/addr_info.hpp)

```cpp
// Uses platform-specific addrinfo structures

auto dumpAddrInfo(
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>& dst,
    const struct addrinfo* src) -> int
// Copy address info structures
// Throws: std::invalid_argument, std::runtime_error

auto addrInfoToString(const struct addrinfo* addrInfo, bool jsonFormat = false) -> std::string
// Convert addrinfo to string/JSON
// Throws: std::invalid_argument

auto getAddrInfo(const std::string& hostname, const std::string& service)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>
// Get address info for hostname and service
// Throws: std::runtime_error, std::invalid_argument

auto compareAddrInfo(const struct addrinfo* addrInfo1,
                     const struct addrinfo* addrInfo2) -> bool
// Compare address info structures
// Throws: std::invalid_argument

auto filterAddrInfo(const struct addrinfo* addrInfo, int family)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>
// Filter address info by family
// Throws: std::invalid_argument

auto sortAddrInfo(const struct addrinfo* addrInfo)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>
// Sort address info structures
// Throws: std::invalid_argument
```

#### DNS Utilities (utils/dns.hpp)

```cpp
void setDNSCacheTTL(std::chrono::seconds ttlSeconds)
// Set cache time-to-live for DNS entries

auto getIPAddresses(const std::string& hostname) -> std::vector<std::string>
// Resolve hostname to IP addresses (with caching)

auto getLocalIPAddresses() -> std::vector<std::string>
// Get all local IP addresses

void clearDNSCacheExpiredEntries()
// Clean up expired DNS cache entries
```

#### IP Utilities (utils/ip.hpp)

```cpp
auto isValidIPv4(const std::string& ipAddress) -> bool
// Validate IPv4 address format

auto isValidIPv6(const std::string& ipAddress) -> bool
// Validate IPv6 address format

auto ipToString(const struct sockaddr* addr, char* strBuf, size_t bufSize) -> bool
// Convert sockaddr structure to string representation
```

#### Network Utilities (utils/network.hpp)

```cpp
auto checkInternetConnectivity() -> bool
// Check if device has active internet connectivity
```

#### Port Utilities (utils/port.hpp)

Uses `PortNumber` concept from `common.hpp`

```cpp
// All these functions take PortNumber concept parameters:

auto isPortInUse(PortNumber auto port) -> bool
// Check if port is in use
// Throws: std::invalid_argument, std::runtime_error

auto isPortInUseAsync(PortNumber auto port) -> std::future<bool>
// Asynchronously check if port is in use

auto getProcessIDOnPort(PortNumber auto port) -> std::optional<int>
// Get process ID running on port
// Throws: std::invalid_argument, std::runtime_error

auto checkAndKillProgramOnPort(PortNumber auto port) -> bool
// Check and terminate program on port
// Throws: std::invalid_argument, std::runtime_error, std::system_error

auto scanPort(const std::string& host, uint16_t port,
              std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) -> bool
// Scan single port on host

auto scanPortRange(
    const std::string& host, uint16_t startPort, uint16_t endPort,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    -> std::vector<uint16_t>
// Scan range of ports on host

auto scanPortRangeAsync(
    const std::string& host, uint16_t startPort, uint16_t endPort,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    -> std::future<std::vector<uint16_t>>
// Asynchronously scan port range
```

#### Socket Utilities (utils/socket.hpp)

```cpp
auto initializeWindowsSocketAPI() -> bool
// Initialize Windows socket subsystem
// Throws: std::runtime_error

auto createSocket() -> int
// Create socket file descriptor (-1 on error)
// Throws: std::runtime_error

auto bindSocket(int sockfd, uint16_t port) -> bool
// Bind socket to port

auto setSocketNonBlocking(int sockfd) -> bool
// Set socket to non-blocking mode

auto connectWithTimeout(int sockfd, const struct sockaddr* addr,
                        socklen_t addrlen,
                        std::chrono::milliseconds timeout) -> bool
// Connect with timeout
```

---

## Architectural Patterns

### 1. PIMPL (Pointer to Implementation)

Used extensively to hide implementation details and improve compile times:

- `CurlWrapper`: Uses `class Impl` with `std::unique_ptr<Impl> pImpl_`
- `DownloadManager`: Uses `class Impl` with `std::unique_ptr<Impl> impl_`
- `HttpHeaderParser`: Uses `class HttpHeaderParserImpl` with `std::shared_ptr<HttpHeaderParserImpl> impl_`
- `MimeTypes`: Uses `class Impl` with `std::unique_ptr<Impl> pImpl`
- `TimeManager`: Uses `TimeManagerImpl` with `std::unique_ptr<TimeManagerImpl> impl_`

### 2. Factory Pattern

- `Address::createFromString()`: Static factory method that creates appropriate address subclass
- `createAddress()`: Wrapper convenience function
- `HttpStatus`: Static factory methods for common status codes

### 3. Polymorphism

- `Address` base class with three concrete implementations: `IPv4`, `IPv6`, `UnixDomain`
- Virtual method overrides for network address operations

### 4. Fluent Interface

- `CurlWrapper`: All configuration methods return `CurlWrapper&` for method chaining

### 5. Template Specialization

- `MimeTypes::guessTypeByContent<PathLike T>()`: Template method accepting path-like types

### 6. C++20 Concepts

- `PathLike`: Concept for path convertible types in MIME module
- `PortNumber`: Concept for integral types 0-65535 in utils module

### 7. Error Handling

- Custom exception hierarchies (AddressException, MimeTypeException, TimeError enum)
- `std::optional` for values that may not exist
- `std::error_code` for system error reporting
- `std::future` for asynchronous operations with error handling

### 8. Non-copyable/Move-only Design

- `CurlWrapper`: Explicitly deleted copy/move to prevent unsafe copying of curl handles
- `DownloadManager`: Non-copyable but move-compatible
- `TimeManager`: Non-copyable with move semantics

---

## Component Relationships and Dependencies

```
address/
├─ address.hpp (base exception, factory pattern, polymorphism)
├─ ipv4.hpp (extends Address)
├─ ipv6.hpp (extends Address)
├─ unix_domain.hpp (extends Address)
└─ main.hpp (aggregates all, provides createAddress())

http/
├─ curl.hpp (direct libcurl wrapper, fluent config)
├─ downloader.hpp (uses curl for downloads, async support)
└─ httpparser.hpp (protocol parsing, cookie/URL handling)

mime/
└─ minetype.hpp (standalone, JSON/XML I/O, caching)

time/
├─ time_error.hpp (error codes)
├─ time_utils.hpp (validation constants, NTP constants)
└─ time_manager.hpp (depends on TimeManagerImpl, uses error codes)

utils/
├─ common.hpp (PortNumber concept)
├─ addr_info.hpp (system addrinfo structure utilities)
├─ dns.hpp (hostname resolution, caching)
├─ ip.hpp (IP validation)
├─ network.hpp (connectivity check)
├─ port.hpp (uses PortNumber concept, socket operations)
└─ socket.hpp (low-level socket API)
```

**No cross-module dependencies** between address, http, mime, and time modules. Utils module is standalone utilities.

---

## Compilation Flags and Compatibility

### Platform-Specific Includes

**address/address.hpp, utils/socket.hpp:**

```cpp
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#endif
```

**utils/addr_info.hpp:**

```cpp
#if defined(__linux__) || defined(__APPLE__)
#include <netdb.h>
#elif defined(_WIN32)
#include <ws2tcpip.h>
#endif
```

**http/downloader.hpp:**

```cpp
#ifdef USE_ASIO
#include <asio.hpp>
#endif
```

**httpparser.hpp (Windows compatibility):**

```cpp
#ifdef _WIN32
#ifdef DELETE
#undef DELETE  // Undefine Windows macro to avoid conflict with enum value
#endif
#endif
```

### C++ Features Used

- C++20 concepts (`PathLike`, `PortNumber`)
- Modern type traits and template specialization
- `std::optional`, `std::variant` (where applicable)
- `std::unique_ptr`, `std::shared_ptr` for PIMPL
- Structured bindings for error handling
- `std::span` for non-owning view types
- `std::chrono` for time operations
- Function objects and lambdas for callbacks

---

## Summary Statistics

- **5 submodules** (address, http, mime, time, utils)
- **4 major classes with PIMPL pattern** (CurlWrapper, DownloadManager, HttpHeaderParser, MimeTypes, TimeManager)
- **3 address type classes** (IPv4, IPv6, UnixDomain) inheriting from Address
- **15+ exception and error types** (AddressException hierarchy, MimeTypeException, TimeError enum)
- **35+ public free functions** (utils module)
- **3 C++20 concepts** (PathLike, PortNumber)
- **50+ public methods** in major classes
- **Cross-platform support** (Windows, Linux, macOS)
- **Async/concurrent operations** (async HTTP requests, port scanning, callbacks)
- **Facade headers** (address.hpp, utils.hpp) for convenience
