# Atom I/O Examples

This directory contains comprehensive examples demonstrating all the functionality available in the `atom/io` module. The examples are organized into categories and showcase real-world usage patterns, best practices, and advanced techniques.

## Directory Structure

```
example/io/
├── core/                    # Core I/O functionality examples
├── async/                   # Asynchronous I/O operations
├── compression/             # File and archive compression
├── filesystem/              # Filesystem utilities and operations
├── integration_examples.cpp # Advanced integration examples
├── CMakeLists.txt          # Build configuration
└── README.md               # This file
```

## Example Categories

### Core I/O Examples (`core/`)

#### `basic_file_operations.cpp`

Demonstrates fundamental file and directory operations:

- File creation, copying, moving, and deletion
- Directory creation, moving, and removal
- File existence and validation checks
- File size operations and metadata
- Symbolic link operations
- Cross-platform path handling

#### `file_splitting.cpp`

Shows file splitting and merging capabilities:

- File splitting into multiple chunks
- Calculating optimal chunk sizes
- Merging split files back together
- Quick split/merge operations
- File integrity verification

#### `directory_traversal.cpp`

Covers directory traversal and file analysis:

- Directory walking with callbacks
- JSON-based directory structure export
- File classification by extension
- Executable file searching
- File type checking and analysis
- Line counting in text files

#### `glob_patterns.cpp`

Basic glob pattern matching:

- Basic glob pattern matching with wildcards
- Recursive glob patterns using **
- Multiple pattern matching
- Directory-only filtering
- Cross-platform path handling

#### `advanced_glob_patterns.cpp`

Advanced glob operations and performance optimization:

- Complex glob pattern syntax
- Performance optimization techniques
- Pattern caching and compilation
- Large directory tree handling
- Parallel glob processing

#### `path_utils_example.cpp`

Path validation and security utilities:

- Comprehensive path validation for security and format compliance
- File and folder name validation
- Permission validation for read/write operations
- Platform-specific validation (Windows reserved names, invalid characters)
- Path traversal detection and prevention
- Null byte injection detection

### Async I/O Examples (`async/`)

#### `basic_async_io.cpp`

Introduction to asynchronous I/O:

- Creating async contexts for cancellation support
- Asynchronous file writing with error handling
- Asynchronous file reading with callbacks
- Asynchronous file deletion
- Context cancellation and cleanup

#### `advanced_async_operations.cpp`

Advanced async operations:

- Batch file operations with parallel processing
- Timeout handling for async operations
- File copying and moving operations
- Directory operations (create, remove, list)
- Error handling and recovery patterns

#### `coroutine_operations.cpp`

C++20 coroutine-based async I/O:

- Coroutine-based file reading and writing
- Sequential async operations with co_await
- Error handling in coroutines
- Directory listing with coroutines
- Combining multiple async operations

#### `async_glob_operations.cpp`

Asynchronous glob pattern matching:

- Asynchronous glob pattern matching with callbacks
- Coroutine-based glob operations
- Performance comparison between sync and async
- Error handling in async glob operations

### Compression Examples (`compression/`)

#### `basic_compression.cpp`

Fundamental compression operations:

- File compression using GZip
- ZIP archive creation and manipulation
- Listing ZIP contents
- File existence checking in archives
- Archive size retrieval

#### `advanced_compression.cpp`

Advanced compression features:

- Advanced compression options and levels
- Streaming compression for large files
- Compression ratio analysis
- Memory usage optimization
- Performance benchmarking

#### `async_compression.cpp`

Asynchronous compression operations:

- Asynchronous file compression with callbacks
- Parallel compression of multiple files
- Progress monitoring for large file compression
- Performance comparison with synchronous compression

### Filesystem Examples (`filesystem/`)

#### `directory_stack.cpp`

Directory stack operations (pushd/popd):

- Basic pushd/popd operations
- Directory stack management
- Stack persistence (save/load)
- Error handling for invalid directories
- Cross-platform directory handling

#### `file_permissions.cpp`

File permission management:

- Reading and displaying file permissions
- Changing file permissions with validation
- Comparing file and process permissions
- Cross-platform permission handling
- Permission string formatting and parsing

#### `file_information.cpp`

File information retrieval:

- Retrieving detailed file information and metadata
- Displaying file properties in formatted manner
- Handling different file types
- Cross-platform file information handling
- Batch file information processing

### Integration Examples

#### `integration_examples.cpp`

Real-world integration scenarios:

- Integration between different atom/io modules
- Comprehensive file processing workflows
- Error handling patterns and recovery
- Performance optimization strategies
- Batch processing with progress monitoring

## Building the Examples

### Prerequisites

- CMake 3.20 or later
- C++20 compatible compiler
- Atom library dependencies

### Build Instructions

```bash
# From the project root directory
mkdir build && cd build
cmake ..
make io_examples

# Or build specific categories
make io_core_examples
make io_async_examples
make io_compression_examples
make io_filesystem_examples
make io_integration_examples
```

### Build Options

You can control which examples are built using CMake options:

```bash
cmake -DATOM_EXAMPLE_IO_BUILD_CORE=ON \
      -DATOM_EXAMPLE_IO_BUILD_ASYNC=OFF \
      -DATOM_EXAMPLE_IO_BUILD_COMPRESSION=ON \
      ..
```

Available options:

- `ATOM_EXAMPLE_IO_BUILD_ALL` - Build all examples (default: ON)
- `ATOM_EXAMPLE_IO_BUILD_CORE` - Build core examples (default: ON)
- `ATOM_EXAMPLE_IO_BUILD_ASYNC` - Build async examples (default: ON)
- `ATOM_EXAMPLE_IO_BUILD_COMPRESSION` - Build compression examples (default: ON)
- `ATOM_EXAMPLE_IO_BUILD_FILESYSTEM` - Build filesystem examples (default: ON)
- `ATOM_EXAMPLE_IO_BUILD_INTEGRATION` - Build integration examples (default: ON)

## Running the Examples

After building, executables will be located in:

```
build/examples/io/core/
build/examples/io/async/
build/examples/io/compression/
build/examples/io/filesystem/
build/examples/io/
```

Each example is self-contained and includes:

- Comprehensive documentation and comments
- Test data creation and cleanup
- Error handling demonstrations
- Performance measurements where applicable

## Example Features

### Common Patterns

All examples demonstrate:

- ✅ **Proper error handling** - Comprehensive exception handling and graceful degradation
- ✅ **Resource cleanup** - Automatic cleanup of test files and directories
- ✅ **Cross-platform compatibility** - Works on Windows, Linux, and macOS
- ✅ **Performance monitoring** - Timing measurements for operations
- ✅ **Best practices** - Modern C++ patterns and atom/io best practices

### Educational Value

Each example includes:

- **Detailed comments** explaining the purpose and usage of each operation
- **Multiple scenarios** covering common use cases and edge cases
- **Performance comparisons** between different approaches
- **Real-world applications** showing practical usage patterns

## Contributing

When adding new examples:

1. Follow the existing directory structure
2. Include comprehensive documentation and comments
3. Add proper error handling and cleanup
4. Update the CMakeLists.txt file
5. Update this README with the new example description

## License

These examples are part of the Atom project and follow the same license terms.
