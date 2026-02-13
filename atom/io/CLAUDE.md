# atom/io - I/O Operations

[根目录](../../CLAUDE.md) > **io**

---

## Module Overview

The `atom/io` module provides comprehensive I/O operations for file handling, compression, globbing, and asynchronous I/O. All public interfaces live in `namespace atom::io`, with async operations under `namespace atom::io::async`.

**Key Responsibilities:**

- File system operations (read, write, copy, move, delete) via templates constrained by `PathLike` concept
- Compression and decompression (zlib/gzip, ZIP via minizip-ng, slice-based)
- File pattern matching (glob/rglob)
- Asynchronous I/O operations (requires ASIO)
- Directory stack (pushd/popd) with coroutine support
- Path validation and normalization

---

## Module Structure

```
atom/io/
├── io.hpp                        # Backward compat → core/io.hpp
├── compress.hpp                  # Backward compat → compression/compress.hpp
├── glob.hpp                      # Backward compat → core/glob.hpp
├── pushd.hpp                     # Backward compat → filesystem/pushd.hpp
├── file_info.hpp                 # Backward compat → filesystem/file_info.hpp
├── file_permission.hpp           # Backward compat → filesystem/file_permission.hpp
├── async_io.hpp                  # Backward compat → async/async_io.hpp
├── async_compress.hpp            # Backward compat → async/async_compress.hpp
├── async_glob.hpp                # Backward compat → async/async_glob.hpp
│
├── core/                         # Core I/O (namespace atom::io)
│   ├── types.hpp                 # PathLike concept, fs alias, shared types
│   ├── io.hpp                    # Aggregator header for all core headers
│   ├── path_convert.hpp          # Path conversion declarations
│   ├── path_utils.hpp            # Path validation utilities (namespace atom::io::detail)
│   ├── file_ops.hpp              # File CRUD: create, copy, move, remove, truncate
│   ├── file_query.hpp            # File queries: exists, size, type, permissions
│   ├── file_split_merge.hpp      # File split/merge operations
│   ├── directory_ops.hpp         # Directory CRUD operations
│   ├── directory_walk.hpp        # Directory traversal (jwalk, fwalk)
│   ├── glob.hpp                  # Glob pattern matching
│   └── io.cpp                    # Non-template implementations + explicit instantiations
│
├── compression/                  # Compression (namespace atom::io)
│   ├── types.hpp                 # CompressionResult, CompressionOptions
│   ├── compress.hpp              # Aggregator header
│   ├── utils.hpp                 # CRC32, compression utilities
│   ├── gz_compress.hpp/cpp       # Gzip compress/decompress via zlib
│   ├── zip_operations.hpp/cpp    # ZIP folder compress/extract via minizip-ng
│   ├── slice_compress.hpp/cpp    # Slice-based compression with manifest
│   ├── data_compress.hpp/cpp     # In-memory data compression
│   └── backup.hpp/cpp            # Backup/restore + async batch processing
│
├── filesystem/                   # Filesystem utilities (namespace atom::io)
│   ├── file_info.hpp/cpp         # FileInfo struct + getFileInfo()
│   ├── file_ops.hpp/cpp          # printFileInfo, deleteFile (deprecated)
│   ├── file_permission.hpp/cpp   # File permission queries
│   ├── file_permission_change.hpp/cpp  # Permission modification
│   ├── task.hpp                  # Task<T> alias → atom::async::Task<T>
│   ├── directory_stack.hpp       # DirectoryStack class
│   ├── directory_stack_impl.hpp  # DirectoryStack pimpl implementation
│   ├── directory_stack_navigation.cpp
│   ├── directory_stack_persistence.cpp
│   └── pushd.hpp                 # Convenience include for directory stack
│
└── async/                        # Async I/O (namespace atom::io::async, requires ASIO)
    ├── async_types.hpp           # AsyncResult, AsyncContext, PathString concept
    ├── async_file.hpp/cpp        # AsyncFile: read, write, append, delete, exists
    ├── async_directory.hpp/cpp   # AsyncDirectoryOps: create, remove, list
    ├── async_batch.hpp/cpp       # AsyncBatchOps: batch read/write/delete
    ├── async_stream.hpp/cpp      # AsyncStreamOps: streaming read/write
    ├── async_simd.hpp/cpp        # SIMD-optimized buffer operations
    ├── async_compressor.hpp/cpp  # Async compression (zlib + ASIO)
    ├── async_decompressor.hpp/cpp # Async decompression
    ├── async_zip.hpp/cpp         # Async ZIP operations
    ├── async_glob.hpp/cpp        # Async glob (namespace atom::io)
    ├── async_compress.hpp        # Aggregator header
    └── async_io.hpp              # Aggregator header
```

---

## Key Types (core/types.hpp)

```cpp
namespace atom::io {

namespace fs = std::filesystem;

// High-performance containers from atom/containers
using atom::containers::String;
using atom::containers::Vector;

// Concept: types convertible to a filesystem path
template <typename T>
concept PathLike = std::convertible_to<T, fs::path> ||
                   std::convertible_to<T, std::string> ||
                   std::convertible_to<T, std::string_view> ||
                   std::convertible_to<T, const char*>;

// Alias for PathLike (used in async interfaces)
template <typename T>
concept PathString = PathLike<T>;

// Options for recursive directory creation/deletion
struct CreateDirectoriesOptions { ... };

enum class PathType { NOT_EXISTS, REGULAR_FILE, DIRECTORY, SYMLINK, OTHER };
enum class FileOption { PATH, NAME };

}  // namespace atom::io
```

---

## Public Interfaces

### File Operations (core/file_ops.hpp)

All file operations are templated on `PathLike`:

```cpp
namespace atom::io {

template <PathLike P> bool isFileExists(const P& path);
template <PathLike P> auto fileSize(const P& path) -> std::uintmax_t;
template <PathLike P> bool removeFile(const P& path);
template <PathLike P> bool truncateFile(const P& path, std::streamsize size);
template <PathLike P1, PathLike P2> bool copyFile(const P1& src, const P2& dst);
template <PathLike P1, PathLike P2> bool moveFile(const P1& src, const P2& dst);
template <PathLike P1, PathLike P2> bool renameFile(const P1& old_p, const P2& new_p);
template <PathLike P1, PathLike P2> bool createSymlink(const P1& target, const P2& link);

}
```

### Directory Operations (core/directory_ops.hpp)

```cpp
namespace atom::io {

template <PathLike P> bool isFolderExists(const P& path);
template <PathLike P> bool isFolderEmpty(const P& path);
template <PathLike P> bool createDirectory(const P& path);
template <PathLike P> bool removeDirectory(const P& path);
void createDirectoriesRecursive(std::string_view root_path,
                                 const Vector<String>& subdirs,
                                 const CreateDirectoriesOptions& opts);
void deleteDirectoriesRecursive(std::string_view root_path,
                                 const Vector<String>& subdirs,
                                 const CreateDirectoriesOptions& opts);

}
```

### Path Utilities

```cpp
// core/path_convert.hpp (namespace atom::io)
auto convertToLinuxPath(std::string_view path) -> std::string;
auto convertToWindowsPath(std::string_view path) -> std::string;
auto normPath(std::string_view path) -> std::string;
auto isFolderNameValid(std::string_view name) -> bool;
auto isFileNameValid(std::string_view name) -> bool;
auto getExecutableNameFromPath(std::string_view path) -> std::string;

// core/path_utils.hpp (namespace atom::io::detail)
bool validatePath(std::string_view path) noexcept;
bool isFolderNameValid(std::string_view name) noexcept;
bool isFileNameValid(std::string_view name) noexcept;
bool isValidPath(const fs::path& path) noexcept;
bool validatePermissions(std::string_view path, bool write = false) noexcept;
```

### Glob Pattern Matching (core/glob.hpp)

```cpp
namespace atom::io {

auto glob(const String& pattern) -> Vector<fs::path>;
auto glob(const Vector<String>& patterns) -> Vector<fs::path>;
auto rglob(const String& pattern) -> Vector<fs::path>;  // recursive

}
```

### Compression (compression/)

```cpp
namespace atom::io {

struct CompressionResult { bool success; std::string error_message; ... };
struct CompressionOptions { int compression_level = 6; size_t chunk_size = 32768; ... };

CompressionResult compressFile(std::string_view path, std::string_view output, const CompressionOptions&);
CompressionResult decompressFile(std::string_view path, std::string_view output, const CompressionOptions&);
CompressionResult compressFolder(std::string_view folder, std::string_view output, const CompressionOptions&);
CompressionResult extractZip(std::string_view zip, std::string_view output);

}
```

### Directory Stack (filesystem/directory_stack.hpp)

```cpp
namespace atom::io {

class DirectoryStack {
public:
    template <PathLike P> void asyncPushd(const P& dir, handler);
    template <PathLike P> auto pushd(const P& dir) -> Task<void>;
    void asyncPopd(handler);
    auto popd() -> Task<void>;
    auto peek() const -> fs::path;
    auto dirs() const noexcept -> Vector<fs::path>;
    auto size() const noexcept -> size_t;
    auto isEmpty() const noexcept -> bool;
    auto saveStackToFile(const String& filename) -> Task<void>;
    auto loadStackFromFile(const String& filename) -> Task<void>;
};

}
```

### Async I/O (async/, requires ATOM_USE_ASIO)

```cpp
namespace atom::io::async {

template <typename T>
struct AsyncResult {
    static AsyncResult success_result(T value);
    static AsyncResult error_result(std::string msg);
    bool is_success() const;
    T& value();
    const std::string& error() const;
};

class AsyncFile {
    template <PathString T> void asyncRead(T&& path, std::function<void(AsyncResult<std::string>)>);
    template <PathString T> void asyncWrite(T&& path, std::span<const char>, std::function<void(AsyncResult<void>)>);
    template <PathString T> void asyncAppend(T&& path, std::span<const char>, std::function<void(AsyncResult<void>)>);
    template <PathString T> void asyncDelete(T&& path, std::function<void(AsyncResult<void>)>);
    template <PathString T> void asyncExists(T&& path, std::function<void(AsyncResult<bool>)>);
};

class AsyncDirectoryOps { ... };
class AsyncBatchOps { ... };
class AsyncStreamOps { ... };

}
```

---

## Dependencies

- **atom-async** — Coroutine Task type (`atom::async::Task<T>`)
- **atom-containers** — High-performance `String`, `Vector`, `Map`
- **atom-error** — Exception hierarchy (`THROW_*` macros)
- **spdlog** — Logging
- **ZLIB** — Compression (required)
- **minizip-ng** — ZIP operations (optional, define `ATOM_IO_NO_MINIZIP` to disable)
- **ASIO** — Async I/O (optional, define `ATOM_USE_ASIO` to enable)
- **TBB** — Parallel algorithms (optional)

---

## Build Options

```cmake
add_library(atom-io STATIC ...)
add_library(atom::io ALIAS atom-io)
target_link_libraries(atom-io PRIVATE spdlog::spdlog ZLIB::ZLIB atom-async)

# Optional: ASIO for async I/O
if(asio_FOUND)
    target_compile_definitions(atom-io PUBLIC ASIO_STANDALONE ATOM_USE_ASIO)
    target_link_libraries(atom-io PUBLIC asio::asio)
endif()
```

---

## Usage Examples

### File Operations

```cpp
#include "atom/io/core/io.hpp"

using namespace atom::io;

if (isFileExists("data.txt")) {
    auto size = fileSize("data.txt");
    copyFile("data.txt", "backup.txt");
}

createDirectory("output");
auto type = checkPathType("output");
```

### Glob Pattern Matching

```cpp
#include "atom/io/core/glob.hpp"

auto files = atom::io::glob("*.cpp");
auto all_headers = atom::io::rglob("**/*.hpp");
```

### Compression

```cpp
#include "atom/io/compression/compress.hpp"

atom::io::CompressionOptions opts;
opts.compression_level = 6;
auto result = atom::io::compressFile("data.bin", "output/", opts);
if (!result.success) {
    spdlog::error("Compression failed: {}", result.error_message);
}
```

---

## Change Log

### 2025-07-22

- **Phase 1**: Consolidated `core/types.hpp` as single source for `namespace fs`, `PathLike`/`PathString` concepts, container aliases
- **Phase 2**: Replaced duplicate `filesystem/task.hpp` with alias to `atom::async::Task<T>`
- **Phase 3**: Deprecated `getFileSize()` (use `fileSize()`), `deleteFile()` (use `removeFile()`)
- **Phase 4**: Renamed `atom::async::io` → `atom::io::async`, `path_utils` → `detail`
- **Phase 5**: Reduced excessive `spdlog::info()` entry/exit logging
- **Phase 7**: Deduplicated ASIO source lists in CMake, fixed install headers preserving subdirectory structure

### 2025-01-15

- Initial module documentation created
