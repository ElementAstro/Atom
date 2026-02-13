# Atom I/O Module

This directory contains the input/output and file system components for the Atom framework. All public interfaces live in `namespace atom::io`, with async operations under `namespace atom::io::async`.

## Directory Structure

```
atom/io/
├── CMakeLists.txt                          # CMake build configuration
├── xmake.lua                               # XMake build configuration
├── index.hpp                               # Barrel export header (include this for everything)
│
├── core/                                   # Core I/O (namespace atom::io)
│   ├── types.hpp                           # PathLike concept, fs alias, shared types
│   ├── io.hpp                              # Aggregator header for all core headers
│   ├── io.cpp                              # Non-template implementations
│   ├── path_convert.hpp                    # Path conversion (Linux/Windows/normalize)
│   ├── path_utils.hpp                      # Path validation utilities (detail namespace)
│   ├── file_ops.hpp                        # File CRUD: create, copy, move, remove, truncate
│   ├── file_query.hpp                      # File queries: exists, size, type, permissions
│   ├── file_split_merge.hpp                # File split/merge operations
│   ├── directory_ops.hpp                   # Directory CRUD operations
│   ├── directory_walk.hpp                  # Directory traversal (jwalk, fwalk)
│   └── glob.hpp                            # Glob pattern matching
│
├── compression/                            # Compression (namespace atom::io)
│   ├── types.hpp                           # CompressionResult, CompressionOptions
│   ├── compress.hpp                        # Aggregator header
│   ├── utils.hpp                           # CRC32, compression utilities
│   ├── gz_compress.hpp/cpp                 # Gzip compress/decompress via zlib
│   ├── zip_operations.hpp/cpp              # ZIP folder compress/extract via minizip-ng
│   ├── slice_compress.hpp/cpp              # Slice-based compression with manifest
│   ├── data_compress.hpp/cpp               # In-memory data compression
│   └── backup.hpp/cpp                      # Backup/restore + async batch processing
│
├── filesystem/                             # Filesystem utilities (namespace atom::io)
│   ├── file_info.hpp/cpp                   # FileInfo struct + getFileInfo()
│   ├── file_ops.hpp/cpp                    # printFileInfo, deleteFile (deprecated)
│   ├── file_permission.hpp/cpp             # File permission queries
│   ├── file_permission_change.hpp/cpp      # Permission modification
│   ├── task.hpp                            # Task<T> alias → atom::async::Task<T>
│   ├── directory_stack.hpp                 # DirectoryStack class
│   ├── directory_stack_impl.hpp            # DirectoryStack pimpl implementation
│   ├── directory_stack.cpp                 # DirectoryStack core implementation
│   ├── directory_stack_navigation.cpp      # DirectoryStack navigation methods
│   ├── directory_stack_persistence.cpp     # DirectoryStack save/load methods
│   └── pushd.hpp                           # Convenience include for directory stack
│
└── async/                                  # Async I/O (namespace atom::io::async, requires ASIO)
    ├── async_types.hpp                     # AsyncResult, AsyncContext, PathString concept
    ├── async_file.hpp/cpp                  # AsyncFile: read, write, append, delete, exists
    ├── async_directory.hpp/cpp             # AsyncDirectoryOps: create, remove, list
    ├── async_batch.hpp/cpp                 # AsyncBatchOps: batch read/write/delete
    ├── async_stream.hpp/cpp                # AsyncStreamOps: streaming read/write
    ├── async_simd.hpp/cpp                  # SIMD-optimized buffer operations
    ├── async_compressor.hpp/cpp            # Async compression (zlib + ASIO)
    ├── async_decompressor.hpp/cpp          # Async decompression
    ├── async_zip.hpp/cpp                   # Async ZIP operations
    ├── async_glob.hpp/cpp                  # Async glob (namespace atom::io)
    ├── async_compress.hpp                  # Aggregator header
    └── async_io.hpp                        # Aggregator header
```

## Usage

### Barrel Header

Include a single header for all public interfaces:

```cpp
#include "atom/io/index.hpp"
```

Or include individual sub-module headers for fine-grained control:

```cpp
#include "atom/io/core/io.hpp"
#include "atom/io/compression/compress.hpp"
#include "atom/io/filesystem/file_info.hpp"

// Async (only available when ASIO is present)
#include "atom/io/async/async_io.hpp"
```

## Key Components

### Core I/O (core/)

- **File Operations** (`file_ops.hpp`): Templated on `PathLike` — create, copy, move, remove, truncate, symlink
- **File Queries** (`file_query.hpp`): exists, size, type, permissions checks
- **Directory Operations** (`directory_ops.hpp`): create, remove, recursive create/delete
- **Directory Traversal** (`directory_walk.hpp`): `jwalk`, `fwalk` for iterating directory trees
- **File Split/Merge** (`file_split_merge.hpp`): Split large files and merge them back
- **Path Conversion** (`path_convert.hpp`): Linux/Windows path conversion, normalization, validation
- **Glob Matching** (`glob.hpp`): Shell-style pattern matching with `glob()` and recursive `rglob()`

### Compression (compression/)

- **GZip** (`gz_compress.hpp`): Gzip compress/decompress via zlib
- **ZIP** (`zip_operations.hpp`): ZIP folder compress/extract via minizip-ng
- **Slice Compression** (`slice_compress.hpp`): Slice-based compression with manifest support
- **Data Compression** (`data_compress.hpp`): In-memory data compression/decompression
- **Backup** (`backup.hpp`): Backup/restore operations with async batch processing

### Filesystem Utilities (filesystem/)

- **File Information** (`file_info.hpp`): `FileInfo` struct and `getFileInfo()` for metadata retrieval
- **File Permissions** (`file_permission.hpp`, `file_permission_change.hpp`): Cross-platform permission queries and modification
- **Directory Stack** (`directory_stack.hpp`): Push/pop directory operations (pushd/popd) with coroutine and persistence support

### Async I/O (async/, requires ASIO)

- **AsyncFile** (`async_file.hpp`): Asynchronous read, write, append, delete, exists
- **AsyncDirectoryOps** (`async_directory.hpp`): Async directory create, remove, list
- **AsyncBatchOps** (`async_batch.hpp`): Batch read/write/delete operations
- **AsyncStreamOps** (`async_stream.hpp`): Streaming read/write for large files
- **SIMD Operations** (`async_simd.hpp`): SIMD-optimized buffer operations
- **Async Compression** (`async_compressor.hpp`, `async_decompressor.hpp`): Async compress/decompress via zlib + ASIO
- **Async ZIP** (`async_zip.hpp`): Asynchronous ZIP archive operations
- **Async Glob** (`async_glob.hpp`): Asynchronous glob pattern matching

## Dependencies

| Dependency | Required | Purpose |
| --- | --- | --- |
| **spdlog** | Yes | Logging |
| **ZLIB** | Optional | Compression (gzip, data compression) |
| **minizip-ng** | Optional | ZIP archive operations (define `ATOM_IO_NO_MINIZIP` to disable) |
| **ASIO** | Optional | Async I/O (define `ATOM_USE_ASIO` to enable) |
| **TBB** | Optional | Parallel algorithms |
| **atom-async** | Yes | Coroutine `Task<T>` type |
| **atom-containers** | Yes | High-performance `String`, `Vector`, `Map` |
| **atom-error** | Yes | Exception hierarchy |

## Build System

The module supports both CMake and XMake build systems.

### CMake

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

### XMake

```bash
# Basic build
xmake build atom-io

# With optional features
xmake f --use_asio=y --use_minizip=y --use_tbb=y
xmake build atom-io
```

## Features

### Asynchronous Operations

- C++20 coroutine support for modern async programming
- ASIO integration for high-performance I/O
- Thread pool-based execution for CPU-bound operations
- Cancellation support through `AsyncContext`
- SIMD-optimized buffer operations

### Compression

- Multiple compression algorithms (ZLib, GZip)
- Streaming compression for large files
- Archive creation and extraction (ZIP format)
- Slice-based compression with manifest
- In-memory data compression
- Backup/restore with async batch processing

### File System

- Cross-platform file operations templated on `PathLike` concept
- Comprehensive file metadata access
- Permission management with security validation
- Directory traversal and manipulation
- Directory stack with persistence support

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
