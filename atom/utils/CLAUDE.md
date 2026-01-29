# atom/utils - General Utilities

[根目录](../../CLAUDE.md) > **utils**

---

## Module Overview

The `atom/utils` module provides a comprehensive collection of utility functions and helper classes for general C++ development. It includes string manipulation, time handling, hashing, UUID generation, cryptographic helpers, CLI utilities, and platform-specific abstractions.

**Key Responsibilities:**

- String manipulation and formatting utilities
- Time and date handling
- Hash generation and verification
- UUID/GUID generation
- Cryptographic helper functions
- Command-line interface utilities
- Platform-specific abstractions

---

## Module Structure

```
atom/utils/
├── utils.hpp              # Main backward compatibility header
├── strings.hpp            # String utilities
├── time.hpp               # Time/date utilities
├── hash.hpp               # Hash functions
├── uuid.hpp               # UUID generation
├── crypto.hpp             # Cryptographic helpers
├── cli.hpp                # CLI argument parsing
├── platform.hpp           # Platform detection
├── algorithms.hpp         # Algorithm utilities
├── math_utils.hpp         # Mathematical utilities
├── file_utils.hpp         # File system utilities
├── path_utils.hpp         # Path manipulation
├── encoding.hpp           # Encoding/decoding (Base64, Hex)
├── format.hpp             # Formatting utilities
└── ... (additional utility headers)
```

---

## Public Interfaces

### String Utilities

```cpp
namespace atom::utils {

// Case conversion
std::string toLower(std::string_view str);
std::string toUpper(std::string_view str);
std::string toTitleCase(std::string_view str);

// Trimming
std::string trimLeft(std::string_view str, std::string_view chars = " \t\n\r");
std::string trimRight(std::string_view str, std::string_view chars = " \t\n\r");
std::string trim(std::string_view str, std::string_view chars = " \t\n\r");

// Splitting and joining
std::vector<std::string> split(std::string_view str, std::string_view delim);
std::string join(const std::vector<std::string>& parts, std::string_view delim);

// Searching and replacing
bool contains(std::string_view str, std::string_view substr, bool case_sensitive = true);
bool startsWith(std::string_view str, std::string_view prefix, bool case_sensitive = true);
bool endsWith(std::string_view str, std::string_view suffix, bool case_sensitive = true);
std::string replace(std::string_view str, std::string_view from, std::string_view to);

// Formatting
std::string format(std::string_view fmt, fmt::format_args args);
std::string padLeft(std::string_view str, size_t width, char pad = ' ');
std::string padRight(std::string_view str, size_t width, char pad = ' ');

// Conversion
template <typename T>
std::string toString(const T& value);

template <typename T>
T fromString(std::string_view str);

// String views
std::string_view substr(std::string_view str, size_t pos, size_t count = std::string_view::npos);

}  // namespace atom::utils
```

### Time Utilities

```cpp
namespace atom::utils {

// Get current time
std::chrono::system_clock::time_point now();
std::string currentTimeString(std::string_view format = "%Y-%m-%d %H:%M:%S");
std::string currentTimeISO8601();

// Time conversion
std::chrono::system_clock::time_point fromTimeT(time_t t);
time_t toTimeT(const std::chrono::system_clock::time_point& tp);

// Time formatting
std::string formatTime(const std::chrono::system_clock::time_point& tp,
                       std::string_view format = "%Y-%m-%d %H:%M:%S");
std::string formatDuration(std::chrono::nanoseconds ns);

// Time parsing
std::optional<std::chrono::system_clock::time_point> parseTime(
    std::string_view time_str,
    std::string_view format = "%Y-%m-%d %H:%M:%S");

// Sleep
template <typename Rep, typename Period>
void sleepFor(std::chrono::duration<Rep, Period> duration);

void sleepUntil(const std::chrono::system_clock::time_point& tp);

}  // namespace atom::utils
```

### Hash Functions

```cpp
namespace atom::utils {

// Hash types
enum class HashType {
    MD5,
    SHA1,
    SHA256,
    SHA512,
    CRC32,
    FNV1a32,
    FNV1a64
};

// Hash generation
std::string hash(std::string_view data, HashType type = HashType::SHA256);
std::string hashFile(std::string_view filepath, HashType type = HashType::SHA256);

// Stream hashing
class Hasher {
public:
    explicit Hasher(HashType type);
    void update(std::string_view data);
    void update(const void* data, size_t size);
    std::string final();
    void reset();
};

// Hash verification
bool verifyHash(std::string_view data, std::string_view expected_hash, HashType type);
bool verifyFileHash(std::string_view filepath, std::string_view expected_hash, HashType type);

}  // namespace atom::utils
```

### UUID Generation

```cpp
namespace atom::utils {

// UUID types
enum class UUIDVersion {
    NIL,        // 00000000-0000-0000-0000-000000000000
    V4,         // Random UUID
    V3,         // MD5-based
    V5,         // SHA1-based
    TimeBased   // Time-based (v1)
};

class UUID {
public:
    UUID();  // Generate random V4 UUID
    explicit UUID(std::string_view uuid_str);

    // Conversion
    std::string toString() const;
    std::string toBytes() const;
    static UUID fromString(std::string_view str);
    static UUID fromBytes(std::string_view bytes);

    // Properties
    bool isNil() const;
    UUIDVersion version() const;
    std::variant_type variant() const;

    // Comparison
    auto operator<=>(const UUID&) const = default;
};

// Generation
UUID generateUUID(UUIDVersion version = UUIDVersion::V4);
UUID generateUUIDv3(std::string_view namespace_uuid, std::string_view name);
UUID generateUUIDv5(std::string_view namespace_uuid, std::string_view name);

// Namespace UUIDs (for v3/v5)
namespace NamespaceUUID {
    constexpr UUID DNS = "6ba7b810-9dad-11d1-80b4-00c04fd430c8";
    constexpr UUID URL = "6ba7b811-9dad-11d1-80b4-00c04fd430c8";
    constexpr UUID OID = "6ba7b812-9dad-11d1-80b4-00c04fd430c8";
    constexpr UUID X500 = "6ba7b814-9dad-11d1-80b4-00c04fd430c8";
}

}  // namespace atom::utils
```

### Cryptographic Helpers

```cpp
namespace atom::utils {

// Encoding/Decoding
std::string base64Encode(std::string_view data);
std::string base64Decode(std::string_view encoded);
std::string hexEncode(std::string_view data);
std::string hexDecode(std::string_view hex);

// Secure random
std::vector<std::uint8_t> secureRandom(size_t count);
int secureRandomInt(int min, int max);

// Password hashing (bcrypt/PBKDF2)
std::string hashPassword(std::string_view password);
bool verifyPassword(std::string_view password, std::string_view hash);

// HMAC
std::string hmacSHA256(std::string_view data, std::string_view key);
std::string hmacSHA512(std::string_view data, std::string_view key);

// Key derivation
std::string deriveKey(std::string_view password,
                      std::string_view salt,
                      size_t key_length = 32);

}  // namespace atom::utils
```

### CLI Utilities

```cpp
namespace atom::utils {

class CommandLine {
public:
    CommandLine(std::string_view description,
                std::string_view version = "1.0.0");

    // Add options
    void addOption(std::string_view name,
                   std::string_view description,
                   bool required = false,
                   std::string_view default_value = "");

    void addFlag(std::string_view name,
                 std::string_view description);

    void addPositional(std::string_view name,
                       std::string_view description);

    // Parsing
    bool parse(int argc, char* argv[]);
    bool parse(std::string_view args);

    // Access
    bool has(std::string_view name) const;
    std::string get(std::string_view name) const;
    int getInt(std::string_view name) const;
    bool getFlag(std::string_view name) const;
    std::vector<std::string> getPositionals() const;

    // Help
    void printHelp() const;
    std::string getHelp() const;
};

}  // namespace atom::utils
```

---

## Dependencies

### Required Dependencies

- **atom-error** - Error handling
- **atom-type** - Type utilities

### Optional Dependencies

- **OpenSSL** - For cryptographic operations (hash, crypto, secure random)

---

## Usage Examples

### String Manipulation

```cpp
#include "atom/utils/strings.hpp"

void exampleStrings() {
    std::string text = "  Hello, World!  ";

    // Trim
    auto trimmed = atom::utils::trim(text);
    ATOM_INFO("Trimmed: '{}'", trimmed);  // 'Hello, World!'

    // Case conversion
    auto lower = atom::utils::toLower(text);
    auto upper = atom::utils::toUpper(text);

    // Split
    auto parts = atom::utils::split("a,b,c,d", ",");
    for (const auto& part : parts) {
        ATOM_INFO("Part: {}", part);
    }

    // Join
    auto joined = atom::utils::join(parts, "|");
    ATOM_INFO("Joined: {}", joined);  // "a|b|c|d"

    // Replace
    auto replaced = atom::utils::replace("Hello World", "World", "C++");
    ATOM_INFO("Replaced: {}", replaced);  // "Hello C++"
}
```

### Time Handling

```cpp
#include "atom/utils/time.hpp"

void exampleTime() {
    // Current time as string
    auto time_str = atom::utils::currentTimeString();
    ATOM_INFO("Current time: {}", time_str);

    // ISO 8601 format
    auto iso_time = atom::utils::currentTimeISO8601();
    ATOM_INFO("ISO 8601: {}", iso_time);

    // Format duration
    auto start = std::chrono::steady_clock::now();
    // ... do some work
    auto end = std::chrono::steady_clock::now();
    auto duration = atom::utils::formatDuration(end - start);
    ATOM_INFO("Duration: {}", duration);
}
```

### Hash Generation

```cpp
#include "atom/utils/hash.hpp"

void exampleHash() {
    std::string data = "Important data";

    // Generate SHA-256 hash
    auto sha256_hash = atom::utils::hash(data, atom::utils::HashType::SHA256);
    ATOM_INFO("SHA-256: {}", sha256_hash);

    // Hash a file
    auto file_hash = atom::utils::hashFile("document.pdf", atom::utils::HashType::SHA256);
    ATOM_INFO("File hash: {}", file_hash);

    // Verify hash
    bool valid = atom::utils::verifyHash(data, expected_hash,
                                          atom::utils::HashType::SHA256);
    if (valid) {
        ATOM_INFO("Hash verified!");
    }

    // Stream hashing
    atom::utils::Hasher hasher(atom::utils::HashType::SHA256);
    hasher.update("Part 1 ");
    hasher.update("Part 2 ");
    hasher.update("Part 3");
    auto final_hash = hasher.final();
    ATOM_INFO("Stream hash: {}", final_hash);
}
```

### UUID Generation

```cpp
#include "atom/utils/uuid.hpp"

void exampleUUID() {
    // Generate random UUID (v4)
    auto uuid = atom::utils::generateUUID();
    ATOM_INFO("UUID: {}", uuid.toString());
    // Output: "550e8400-e29b-41d4-a716-446655440000"

    // Generate UUID v5 (SHA1-based)
    auto uuid_v5 = atom::utils::generateUUIDv5(
        atom::utils::NamespaceUUID::DNS,
        "example.com"
    );
    ATOM_INFO("UUID v5: {}", uuid_v5.toString());

    // Parse UUID
    auto parsed = atom::utils::UUID::fromString("550e8400-e29b-41d4-a716-446655440000");

    // Check properties
    if (!parsed.isNil()) {
        ATOM_INFO("Version: {}", static_cast<int>(parsed.version()));
    }
}
```

### Cryptographic Operations

```cpp
#include "atom/utils/crypto.hpp"

void exampleCrypto() {
    std::string data = "Sensitive data";

    // Base64 encoding
    auto encoded = atom::utils::base64Encode(data);
    ATOM_INFO("Base64: {}", encoded);

    auto decoded = atom::utils::base64Decode(encoded);
    ATOM_INFO("Decoded: {}", decoded);

    // Hex encoding
    auto hex = atom::utils::hexEncode(data);
    ATOM_INFO("Hex: {}", hex);

    // Password hashing
    std::string password = "secure_password_123";
    auto password_hash = atom::utils::hashPassword(password);
    ATOM_INFO("Password hash: {}", password_hash);

    // Password verification
    bool valid = atom::utils::verifyPassword(password, password_hash);
    ATOM_INFO("Password valid: {}", valid);

    // HMAC
    auto hmac = atom::utils::hmacSHA256("message", "secret_key");
    ATOM_INFO("HMAC: {}", hmac);
}
```

### CLI Argument Parsing

```cpp
#include "atom/utils/cli.hpp"

int main(int argc, char* argv[]) {
    atom::utils::CommandLine cli("My Application", "1.0.0");

    cli.addOption("input", "Input file path", true);
    cli.addOption("output", "Output file path", true, "output.txt");
    cli.addOption("threads", "Number of threads", false, "4");
    cli.addFlag("verbose", "Enable verbose output");
    cli.addFlag("help", "Show this help message");

    if (!cli.parse(argc, argv)) {
        cli.printHelp();
        return 1;
    }

    if (cli.getFlag("help")) {
        cli.printHelp();
        return 0;
    }

    auto input = cli.get("input");
    auto output = cli.get("output");
    int threads = cli.getInt("threads");
    bool verbose = cli.getFlag("verbose");

    ATOM_INFO("Processing {} -> {} with {} threads", input, output, threads);

    return 0;
}
```

---

## Testing

The module does not currently have dedicated unit tests. Tests should be added in `tests/utils/`:

### Test Structure

```
tests/utils/
├── CMakeLists.txt
├── test_strings.cpp       # String utility tests
├── test_time.cpp          # Time utility tests
├── test_hash.cpp          # Hash function tests
├── test_uuid.cpp          # UUID generation tests
├── test_crypto.cpp        # Cryptographic tests
├── test_cli.cpp          # CLI parsing tests
└── test_encoding.cpp      # Encoding/decoding tests
```

---

## Build Options

```cmake
# Create library (header-only for most utilities)
add_library(atom-utils INTERFACE)
add_library(atom::utils ALIAS atom-utils)

# Optional OpenSSL for crypto functions
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_compile_definitions(atom-utils PRIVATE ATOM_USE_OPENSSL)
    target_link_libraries(atom-utils PUBLIC OpenSSL::Crypto)
    message(STATUS "atom-utils: OpenSSL enabled for crypto functions")
else()
    message(WARNING "atom-utils: OpenSSL not found, crypto functions disabled")
endif()

# Link required modules
target_link_libraries(atom-utils PUBLIC atom-error atom-type)
```

---

## Platform-Specific Features

### Windows

- UTF-16 string conversion utilities
- Windows registry helpers
- Windows path handling

### Linux

- POSIX file permission utilities
- System configuration file parsers
- Signal handling helpers

### macOS

- Apple-specific path handling
- plist utilities
- Notification Center helpers

---

## Common Patterns

### Safe String Conversion

```cpp
template <typename T>
std::string safeToString(const T& value) {
    try {
        return atom::utils::toString(value);
    } catch (const std::exception& e) {
        ATOM_ERROR("Conversion failed: {}", e.what());
        return "<conversion error>";
    }
}
```

### Retry with Exponential Backoff

```cpp
template <typename F>
auto retryWithBackoff(F&& func, int max_attempts = 3)
    -> decltype(func()) {
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        try {
            return func();
        } catch (const std::exception& e) {
            if (attempt == max_attempts - 1) throw;
            auto delay = std::chrono::milliseconds(100 * (1 << attempt));
            atom::utils::sleepFor(delay);
        }
    }
}
```

### Secure Password Input

```cpp
std::string readPasswordSecurely() {
    std::cout << "Enter password: ";
    std::string password;
#ifdef _WIN32
    // Windows-specific secure input
#else
    // POSIX-specific secure input (no echo)
#endif
    return atom::utils::trim(password);
}
```

---

## See Also

- [atom/error](../error/CLAUDE.md) - Error handling
- [atom/type](../type/CLAUDE.md) - Type utilities
- [atom/algorithm](../algorithm/CLAUDE.md) - Algorithm module (uses utils)

---

## Change Log

### 2025-01-15

- Initial module documentation created
- Documented string, time, hash, UUID, crypto, and CLI utilities
- Added usage examples for all major components
- Documented platform-specific features
