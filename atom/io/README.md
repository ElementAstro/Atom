# Atom I/O Module

This directory contains the input/output and file system components for the Atom framework.

## Directory Structure

The I/O module has been refactored to follow a clean, organized structure:

```
atom/io/
├── CMakeLists.txt              # CMake build configuration
├── xmake.lua                   # XMake build configuration
├── README.md                   # This file
├── [compatibility headers]     # Backward compatibility headers (deprecated)
├── async/                      # Asynchronous I/O operations
│   ├── async_compress.hpp     # Asynchronous compression operations
│   ├── async_compress.cpp     # Async compression implementation
│   ├── async_glob.hpp         # Asynchronous glob pattern matching
│   ├── async_glob.cpp         # Async glob implementation
│   ├── async_io.hpp           # Asynchronous file I/O operations
│   └── async_io.cpp           # Async I/O implementation
├── compression/                # Compression and decompression
│   ├── compress.hpp           # Compression utilities (ZLib, MiniZip-ng)
│   └── compress.cpp           # Compression implementation
├── filesystem/                 # File system operations
│   ├── file_info.hpp          # File information and metadata
│   ├── file_info.cpp          # File info implementation
│   ├── file_permission.hpp    # File permission management
│   ├── file_permission.cpp    # File permission implementation
│   ├── pushd.hpp              # Directory stack operations
│   └── pushd.cpp              # Directory stack implementation
└── core/                       # Core I/O functionality
    ├── io.hpp                 # Core I/O operations and utilities
    ├── io.cpp                 # Core I/O implementation
    └── glob.hpp               # Glob pattern matching (header-only)
```

## Backward Compatibility

All existing header file paths continue to work without modification. The root-level headers are now compatibility headers that forward to the new locations:

- `async_compress.hpp` → `async/async_compress.hpp`
- `async_glob.hpp` → `async/async_glob.hpp`
- `async_io.hpp` → `async/async_io.hpp`
- `compress.hpp` → `compression/compress.hpp`
- `file_info.hpp` → `filesystem/file_info.hpp`
- `file_permission.hpp` → `filesystem/file_permission.hpp`
- `pushd.hpp` → `filesystem/pushd.hpp`
- `io.hpp` → `core/io.hpp`
- `glob.hpp` → `core/glob.hpp`

## Migration Guide

### For New Code

Use the new structured paths:

```cpp
#include "atom/io/async/async_io.hpp"
#include "atom/io/compression/compress.hpp"
#include "atom/io/filesystem/file_permission.hpp"
```

### For Existing Code

No changes required! Existing includes will continue to work:

```cpp
#include "atom/io/async_io.hpp"        // Still works
#include "atom/io/compress.hpp"        // Still works
#include "atom/io/file_permission.hpp" // Still works
```

## Key Components

### Asynchronous I/O Operations

- **AsyncFile**: High-performance asynchronous file operations with C++20 coroutine support
- **AsyncGlob**: Asynchronous glob pattern matching with callback-based and coroutine interfaces
- **AsyncCompress**: Asynchronous compression and decompression operations

### Compression Support

- **ZLib Integration**: High-performance compression using ZLib
- **MiniZip-ng Support**: Advanced ZIP archive operations
- **Streaming Compression**: Memory-efficient streaming compression/decompression

### File System Operations

- **File Information**: Comprehensive file metadata and information retrieval
- **Permission Management**: Cross-platform file permission handling
- **Directory Stack**: Push/pop directory operations (pushd/popd functionality)

### Core I/O Functionality

- **File Operations**: Basic file read/write operations with error handling
- **Glob Matching**: Shell-style pattern matching with recursive support
- **Path Utilities**: Cross-platform path manipulation and validation

## Build System

The module supports both CMake and XMake build systems. The build files have been updated to reflect the new directory structure while maintaining compatibility.

### Dependencies

- **Core**: C++20 compiler support, loguru (logging)
- **Compression**: ZLib, MiniZip-ng for compression operations
- **Async**: ASIO (optional) for enhanced asynchronous operations
- **Threading**: TBB (Intel Threading Building Blocks)
- **Platform**: Platform-specific libraries (Windows: ws2_32, wsock32)

## Features

### Asynchronous Operations

- C++20 coroutine support for modern async programming
- ASIO integration for high-performance I/O
- Thread pool-based execution for CPU-bound operations
- Cancellation support through AsyncContext

### Compression

- Multiple compression algorithms (ZLib, GZip)
- Streaming compression for large files
- Archive creation and extraction (ZIP format)
- Progress callbacks and error handling

### File System

- Cross-platform file operations
- Comprehensive file metadata access
- Permission management with security validation
- Directory traversal and manipulation

### Pattern Matching

- Shell-style glob patterns (\*, ?, [], etc.)
- Recursive directory matching (\*\*)
- High-performance pattern compilation and caching
- Parallel processing for large directory trees

## Performance Features

- Memory-efficient streaming operations
- Parallel processing using thread pools
- Pattern caching for repeated glob operations
- SIMD optimizations where available
- Zero-copy operations where possible

## Notes

This refactoring maintains 100% backward compatibility while providing a cleaner, more maintainable codebase structure that follows established patterns from other Atom modules. The organization separates concerns clearly: async operations, compression, filesystem operations, and core I/O functionality are now in dedicated subdirectories for better maintainability and discoverability.
