# atom/io - I/O Operations

[根目录](../../CLAUDE.md) > **io**

---

## Module Overview

The `atom/io` module provides comprehensive I/O operations for file handling, compression, globbing, and asynchronous I/O. It offers cross-platform file system utilities, transparent compression support, pattern-based file matching, and high-performance async I/O operations.

**Key Responsibilities:**

- File system operations (read, write, copy, move, delete)
- Compression and decompression (zlib, gzip)
- File pattern matching (glob patterns)
- Asynchronous I/O operations
- File system monitoring and notifications
- Path manipulation and normalization

---

## Module Structure

```
atom/io/
├── io.hpp                 # Main backward compatibility header
├── core/                  # Core I/O operations
│   └── io.hpp             # Core I/O interfaces
├── file/                  # File operations
│   ├── file.hpp           # File I/O
│   ├── file_utils.hpp     # File utilities
│   └── path.hpp           # Path manipulation
├── compression/           # Compression
│   ├── gzip.hpp           # Gzip compression
│   ├── zlib.hpp           # Zlib wrappers
│   └── stream.hpp         # Compressed streams
├── glob/                  # Pattern matching
│   └── glob.hpp           # Glob pattern matching
├── async/                 # Async I/O
│   ├── async_io.hpp       # Async operations
│   └── async_file.hpp     # Async file I/O
└── monitor/               # File monitoring
    └── file_monitor.hpp   # File system events
```

---

## Public Interfaces

### File Operations

```cpp
namespace atom::io {

// File I/O
class File {
public:
    // Open modes
    enum class Mode {
        Read,
        Write,
        Append,
        ReadWrite,
        ReadWriteAppend
    };

    File(std::string_view path, Mode mode);
    ~File();

    // Read operations
    size_t read(void* buffer, size_t size);
    std::vector<std::uint8_t> readAll();
    std::string readText();
    std::vector<std::string> readLines();

    // Write operations
    size_t write(const void* data, size_t size);
    size_t write(std::string_view text);
    void flush();

    // File position
    void seek(int64_t offset, SeekOrigin origin = SeekOrigin::Begin);
    int64_t tell() const;
    int64_t size() const;

    // Status
    bool isOpen() const;
    bool eof() const;

    // Close
    void close();
};

// File utilities
namespace file {

// Basic operations
bool exists(std::string_view path);
bool remove(std::string_view path);
bool rename(std::string_view old_path, std::string_view new_path);
bool copy(std::string_view src, std::string_view dst);
bool move(std::string_view src, std::string_view dst);

// File info
int64_t fileSize(std::string_view path);
std::chrono::system_clock::time_point lastModified(std::string_view path);
FilePermissions permissions(std::string_view path);

// Directory operations
bool createDirectory(std::string_view path);
bool createDirectories(std::string_view path);
bool removeDirectory(std::string_view path);
std::vector<std::string> listDirectory(std::string_view path);

// Temp files
std::string createTempFile(std::string_view prefix = "atom_tmp");
std::string createTempDirectory(std::string_view prefix = "atom_tmp");

}  // namespace file

}  // namespace atom::io
```

### Path Utilities

```cpp
namespace atom::io::path {

// Path manipulation
std::string join(std::initializer_list<std::string_view> parts);
std::string normalize(std::string_view path);
std::string absolute(std::string_view path);
std::string relative(std::string_view path, std::string_view base);

// Path components
std::string filename(std::string_view path);
std::string stem(std::string_view path);  // Without extension
std::string extension(std::string_view path);
std::string parent(std::string_view path);

// Path info
bool isAbsolute(std::string_view path);
bool isRelative(std::string_view path);
bool hasExtension(std::string_view path);

// Path separators
char separator();
std::string separators();

}  // namespace atom::io::path
```

### Compression

```cpp
namespace atom::io::compression {

// Gzip compression
std::vector<uint8_t> gzipCompress(std::span<const uint8_t> data, int level = 6);
std::vector<uint8_t> gzipDecompress(std::span<const uint8_t> data);

// Zlib compression
std::vector<uint8_t> zlibCompress(std::span<const uint8_t> data, int level = 6);
std::vector<uint8_t> zlibDecompress(std::span<const uint8_t> data);

// Compression stream
class CompressionStream {
public:
    enum class Format {
        Gzip,
        Zlib,
        Deflate
    };

    CompressionStream(Format format, int level = 6);

    void write(std::span<const uint8_t> data);
    std::vector<uint8_t> finish();

    void reset();
};

// Decompression stream
class DecompressionStream {
public:
    enum class Format {
        Gzip,
        Zlib,
        Deflate,
        AutoDetect
    };

    explicit DecompressionStream(Format format = Format::AutoDetect);

    void write(std::span<const uint8_t> data);
    std::vector<uint8_t> finish();

    void reset();
};

}  // namespace atom::io::compression
```

### Glob Pattern Matching

```cpp
namespace atom::io::glob {

// Pattern matching
std::vector<std::string> match(std::string_view pattern);
std::vector<std::string> match(std::string_view pattern, std::string_view root);

// Glob patterns
// *      - Matches any sequence of characters
// ?      - Matches any single character
// [abc]  - Matches any character in brackets
// [!abc] - Matches any character NOT in brackets
// **     - Matches zero or more directories

// Recursive matching
std::vector<std::string> matchRecursive(std::string_view pattern);

// Case-insensitive matching
std::vector<std::string> matchCaseInsensitive(std::string_view pattern);

}  // namespace atom::io::glob
```

### Async I/O

```cpp
namespace atom::io::async {

// Async file operations
class AsyncFile {
public:
    using Callback = std::function<void(std::exception_ptr, size_t)>;

    AsyncFile(std::string_view path, File::Mode mode);

    // Async read
    void read(void* buffer, size_t size, Callback callback);
    atom::async::Future<size_t> readAsync(void* buffer, size_t size);

    // Async write
    void write(const void* data, size_t size, Callback callback);
    atom::async::Future<size_t> writeAsync(const void* data, size_t size);

    // Async flush
    void flush(Callback callback);
    atom::async::Future<void> flushAsync();
};

// High-level async operations
namespace async_file {

atom::async::Future<std::vector<uint8_t>> readFile(std::string_view path);
atom::async::Future<std::string> readTextFile(std::string_view path);
atom::async::Future<void> writeFile(std::string_view path,
                                     std::span<const uint8_t> data);
atom::async::Future<void> writeTextFile(std::string_view path,
                                        std::string_view text);

}  // namespace atom::io::async_file

}  // namespace atom::io::async
```

### File Monitoring

```cpp
namespace atom::io::monitor {

// File change events
enum class EventType {
    Created,
    Modified,
    Deleted,
    Renamed
};

struct FileEvent {
    std::string path;
    EventType type;
    std::chrono::system_clock::time_point timestamp;
};

// File monitor
class FileMonitor {
public:
    using Callback = std::function<void(const FileEvent&)>;

    FileMonitor();
    ~FileMonitor();

    // Watch files/directories
    void watch(std::string_view path, Callback callback);
    void watchRecursive(std::string_view path, Callback callback);

    // Event types to watch
    void setEventTypeMask(std::initializer_list<EventType> types);

    // Control
    void start();
    void stop();
    bool isRunning() const;
};

}  // namespace atom::io::monitor
```

---

## Dependencies

### Required Dependencies

- **atom-async** - Async primitives
- **atom-utils** - Utility functions
- **ZLIB** - Compression support

---

## Usage Examples

### Basic File Operations

```cpp
#include "atom/io/file/file.hpp"
#include "atom/io/file/path.hpp"

void exampleFileOps() {
    using namespace atom::io;

    // Check if file exists
    if (file::exists("data.txt")) {
        // Get file size
        auto size = file::fileSize("data.txt");
        ATOM_INFO("File size: {} bytes", size);
    }

    // Read entire file
    File file("data.txt", File::Mode::Read);
    if (file.isOpen()) {
        auto content = file.readAll();
        ATOM_INFO("Read {} bytes", content.size());
        file.close();
    }

    // Write file
    File out("output.txt", File::Mode::Write);
    out.write("Hello, World!");
    out.flush();
    out.close();

    // Copy file
    file::copy("output.txt", "backup.txt");
}
```

### Path Manipulation

```cpp
#include "atom/io/file/path.hpp"

void examplePaths() {
    using namespace atom::io::path;

    // Join paths
    auto full = join({"home", "user", "documents", "file.txt"});
    ATOM_INFO("Full path: {}", full);

    // Normalize path
    auto normalized = normalize("home/user/../user/./documents");
    ATOM_INFO("Normalized: {}", normalized);

    // Get path components
    std::string path = "/home/user/documents/file.txt";
    ATOM_INFO("Filename: {}", filename(path));     // "file.txt"
    ATOM_INFO("Stem: {}", stem(path));             // "file"
    ATOM_INFO("Extension: {}", extension(path));   // ".txt"
    ATOM_INFO("Parent: {}", parent(path));         // "/home/user/documents"

    // Check path type
    bool is_abs = isAbsolute(path);
    ATOM_INFO("Is absolute: {}", is_abs);
}
```

### Compression

```cpp
#include "atom/io/compression/gzip.hpp"

void exampleCompression() {
    using namespace atom::io::compression;

    // Compress data
    std::string data = "Hello, World! This is some data to compress.";
    std::vector<uint8_t> compressed = gzipCompress(
        std::span(reinterpret_cast<const uint8_t*>(data.data()), data.size())
    );
    ATOM_INFO("Original: {} bytes, Compressed: {} bytes",
              data.size(), compressed.size());

    // Decompress data
    std::vector<uint8_t> decompressed = gzipDecompress(compressed);
    std::string decoded(decompressed.begin(), decompressed.end());
    ATOM_INFO("Decompressed: {}", decoded);
}
```

### Glob Pattern Matching

```cpp
#include "atom/io/glob/glob.hpp"

void exampleGlob() {
    using namespace atom::io::glob;

    // Find all .txt files
    auto txt_files = match("*.txt");
    ATOM_INFO("Found {} .txt files", txt_files.size());

    // Find all .cpp files in src directory
    auto cpp_files = match("src/**/*.cpp");
    for (const auto& file : cpp_files) {
        ATOM_INFO("Found C++ file: {}", file);
    }

    // Find all image files
    auto images = match("images/*.{jpg,png,gif}");
    for (const auto& img : images) {
        ATOM_INFO("Found image: {}", img);
    }
}
```

### Async File I/O

```cpp
#include "atom/io/async/async_file.hpp"

void exampleAsyncIO() {
    using namespace atom::io::async;

    // Async read
    auto future = async_file::readTextFile("data.txt");
    future.then([](const std::string& content) {
        ATOM_INFO("Read {} characters", content.size());
    }).except([](std::exception_ptr eptr) {
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            ATOM_ERROR("Read failed: {}", e.what());
        }
    });

    // Wait for completion
    future.wait();

    // Async write
    auto write_future = async_file::writeTextFile(
        "output.txt",
        "Async write content"
    );
    write_future.wait();
}
```

### File Monitoring

```cpp
#include "atom/io/monitor/file_monitor.hpp"

void exampleFileMonitor() {
    using namespace atom::io::monitor;

    FileMonitor monitor;

    // Watch for file changes
    monitor.watch("data.txt", [](const FileEvent& event) {
        switch (event.type) {
            case EventType::Modified:
                ATOM_INFO("File modified: {}", event.path);
                break;
            case EventType::Created:
                ATOM_INFO("File created: {}", event.path);
                break;
            case EventType::Deleted:
                ATOM_INFO("File deleted: {}", event.path);
                break;
        }
    });

    monitor.start();

    // Keep monitoring...
    std::this_thread::sleep_for(std::chrono::seconds(30));

    monitor.stop();
}
```

---

## Testing

The module does not currently have dedicated unit tests. Tests should be added in `tests/io/`:

### Test Structure

```
tests/io/
├── CMakeLists.txt
├── test_file.cpp          # File operation tests
├── test_path.cpp          # Path utility tests
├── test_compression.cpp   # Compression tests
├── test_glob.cpp          # Glob pattern tests
├── test_async_io.cpp      # Async I/O tests
└── test_monitor.cpp       # File monitor tests
```

---

## Build Options

```cmake
# Create library
add_library(atom-io INTERFACE)
add_library(atom::io ALIAS atom-io)

# Required dependencies
find_package(ZLIB REQUIRED)
target_link_libraries(atom-io PUBLIC atom-async atom-utils ZLIB::ZLIB)

# Optional ASIO for async I/O
find_package(asio QUIET)
if(asio_FOUND)
    target_link_libraries(atom-io PUBLIC asio::asio)
    target_compile_definitions(atom-io PRIVATE ATOM_USE_ASIO)
endif()
```

---

## Platform-Specific Features

### Windows

- UNC path support (`\\server\share\path`)
- Drive letters and current directory per drive
- Case-insensitive path comparison
- Registry-based file associations

### Linux

- Symlink handling and creation
- Permission bits and mode_t
- Device files and special files
- Extended attributes

### macOS

- Resource fork handling
- Bundle structure support
- Alias resolution
- Extended attributes

---

## Common Patterns

### Safe File Reading with Exception Handling

```cpp
std::string readFileSafely(std::string_view path) {
    try {
        atom::io::File file(path, atom::io::File::Mode::Read);
        if (!file.isOpen()) {
            THROW_FAIL_TO_OPEN_FILE(std::string(path));
        }
        return file.readText();
    } catch (const atom::error::Exception& e) {
        ATOM_ERROR("Failed to read file {}: {}", path, e.what());
        throw;
    }
}
```

### Atomic File Writing

```cpp
void writeAtomically(std::string_view path, std::string_view content) {
    auto temp_path = atom::io::file::createTempFile();
    {
        atom::io::File file(temp_path, atom::io::File::Mode::Write);
        file.write(content);
        file.flush();
    }
    atom::io::file::move(temp_path, path);
}
```

### Directory Traversal with Filters

```cpp
void findFilesByExtension(std::string_view root,
                          std::string_view extension) {
    auto pattern = atom::io::path::join({root, "**", "*"} + extension);
    auto files = atom::io::glob::matchRecursive(pattern);

    for (const auto& file : files) {
        ATOM_INFO("Found: {}", file);
    }
}
```

---

## Performance Considerations

### Buffer Size

- Default buffer size: 8KB
- Large sequential reads: Use larger buffers (64KB - 1MB)
- Small random reads: Use smaller buffers (4KB - 8KB)

### Compression Level

- 0-3: Fastest, larger files (real-time)
- 6: Balanced (default)
- 9: Smallest files, slower (archival)

### Async I/O

- Use for I/O-bound operations
- Avoid for small files (< 4KB)
- Batch operations when possible

---

## See Also

- [atom/async](../async/CLAUDE.md) - Async primitives
- [atom/utils](../utils/CLAUDE.md) - Utility functions
- [atom/connection](../connection/CLAUDE.md) - Networking (uses async I/O)

---

## Change Log

### 2025-01-15

- Initial module documentation created
- Documented File, Path, Compression, Glob, Async I/O, and File Monitor
- Added usage examples for all major components
- Documented platform-specific features and performance considerations
