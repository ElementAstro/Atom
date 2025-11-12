# Atom Utils Examples

This directory contains comprehensive examples demonstrating all the utilities available in the `atom::utils` namespace. Each example is designed to be complete, functional, and well-documented to help you understand and implement the available features.

## Directory Structure

The examples are organized into subdirectories that mirror the structure of `atom/utils/`:

```text
example/utils/
├── container/          # Container utilities (LINQ, ranges, span)
├── conversion/         # Type conversion and serialization
├── core/              # Core utilities (anyutils, argsview, bit, switch)
├── crypto/            # Cryptographic operations (AES, hashing)
├── debug/             # Debugging utilities (color print, error stack)
├── format/            # Formatting utilities (difflib, XML)
├── memory/            # Memory utilities (aligned, leak detection, SIMD)
├── process/           # Process management
├── random/            # Random number generation (LCG, UUID)
├── text/              # Text processing (string, UTF, validation)
├── time/              # Time and date utilities
└── integration_examples.cpp  # Cross-module integration examples
```

## Overview

The Atom Utils library provides a rich set of utilities organized into several categories:

- **Container Utilities** (`container/`): LINQ-style operations, ranges, and span utilities
- **Conversion Utilities** (`conversion/`): Type conversions, string conversions, and serialization
- **Core Utilities** (`core/`): Basic functionality like switch statements, bit manipulation, and argument parsing
- **Crypto Utilities** (`crypto/`): AES encryption, compression, and hash calculations
- **Debug Utilities** (`debug/`): Colored output, error tracking, and debugging tools
- **Format Utilities** (`format/`): Text diffing, XML processing, and formatting
- **Memory Utilities** (`memory/`): Aligned memory, leak detection, and SIMD operations
- **Process Utilities** (`process/`): External process management and execution
- **Random Utilities** (`random/`): Random number generation, UUIDs, and distributions
- **Text Utilities** (`text/`): String manipulation, UTF conversions, and validation
- **Time Utilities** (`time/`): Time formatting, timezone handling, and timing operations

## Building the Examples

### Prerequisites

- CMake 3.10 or higher
- C++20 compatible compiler
- Atom library dependencies (see main project README)

### Build Instructions

```bash
# From the project root directory
mkdir build && cd build
cmake ..
make

# Or build specific examples by category and name
make utils_crypto_aes_example
make utils_debug_color_print_example
make utils_integration_examples
```

### Build Options

You can control which examples are built using CMake options:

```bash
# Build all utils examples (default)
cmake -DATOM_EXAMPLE_UTILS_BUILD_ALL=ON ..

# Build only specific examples (category_examplename format)
cmake -DATOM_EXAMPLE_UTILS_BUILD_ALL=OFF \
      -DATOM_EXAMPLE_UTILS_CRYPTO_AES_EXAMPLE=ON \
      -DATOM_EXAMPLE_UTILS_DEBUG_COLOR_PRINT_EXAMPLE=ON ..
```

## Example Categories

### Container Utilities (`container/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `container_example.cpp` | General container operations | Set operations, transformations, filtering |
| `linq_example.cpp` | LINQ-style operations | Functional programming, data queries, aggregations |
| `ranges_example.cpp` | Range algorithms | Range-based operations, transformations, generators |
| `span_example.cpp` | Span utilities | Memory-safe array views, span operations |

### Conversion Utilities (`conversion/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `convert_example.cpp` | Windows string conversions | LPWSTR, LPSTR, wide character conversions |
| `to_any_example.cpp` | Parser and type conversions | Dynamic type conversion, literal parsing |
| `to_byte_example.cpp` | Byte serialization (original) | Binary data conversion, serialization |
| `to_byte_serialization_example.cpp` | Byte serialization (extended) | Advanced serialization patterns |

### Core Utilities (`core/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `anyutils_example.cpp` | Any type utilities | Type erasure, safe casting, value extraction |
| `argsview_example.cpp` | Argument parsing | Command-line argument processing, validation |
| `bit_example.cpp` | Bit manipulation | Bit operations, masks, endianness, SIMD |
| `switch_example.cpp` | String switch statements | String-based switching, pattern matching |

### Crypto Utilities (`crypto/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `aes_example.cpp` | AES encryption | Encryption/decryption, compression, SHA hashing |

### Debug Utilities (`debug/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `color_print_example.cpp` | Colored console output | Terminal colors, formatting, logging |
| `error_stack_example.cpp` | Error tracking | Error stack management, filtering, categorization |
| `print_example.cpp` | Advanced printing | Formatted output, containers, performance timing |

### Format Utilities (`format/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `difflib_example.cpp` | Text diffing | Sequence comparison, diff generation, algorithms |
| `xml_example.cpp` | XML processing | Parsing, validation, manipulation |

### Memory Utilities (`memory/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `aligned_example.cpp` | Aligned memory | Memory alignment validation |
| `leak_example.cpp` | Memory leak detection | Allocation tracking, debugging (VLD integration) |
| `simd_wrapper_example.cpp` | SIMD operations | Vectorized computations, cross-platform SIMD |

### Process Utilities (`process/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `qprocess_example.cpp` | Process management | External process execution, I/O handling, callbacks |

### Random Utilities (`random/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `lcg_example.cpp` | Linear congruential generator | Custom random algorithms, distributions |
| `random_example.cpp` | Random generation | Various distributions, thread safety, engines |
| `uuid_example.cpp` | UUID generation | Unique identifier creation, string conversion |

### Text Utilities (`text/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `cstring_example.cpp` | Compile-time strings | Constexpr string operations |
| `string_example.cpp` | String manipulation | Case conversion, splitting, trimming, URL encoding |
| `to_string_example.cpp` | String conversions | Object to string conversion, formatting |
| `utf_example.cpp` | UTF conversions | Unicode handling, UTF-8/16/32 conversion |
| `valid_string_example.cpp` | String validation | Input validation, bracket matching |

### Time Utilities (`time/`)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `qdatetime_example.cpp` | Date/time handling | Advanced date operations, parsing, formatting |
| `qtimer_example.cpp` | Timer utilities | Elapsed time measurement, intervals |
| `qtimezone_example.cpp` | Timezone handling | Timezone operations, DST, conversions |
| `stopwatcher_example.cpp` | Performance timing | Benchmarking, lap timing, statistics |
| `time_example.cpp` | Time operations | Formatting, timezone conversion, timestamps |

### Integration Examples (root directory)

| Example | Description | Key Features |
|---------|-------------|--------------|
| `integration_examples.cpp` | Combined utilities | Real-world scenarios, multiple utilities working together |

## Running Examples

After building, you can run any example. Note the naming convention: `utils_<category>_<example_name>`:

```bash
# Run from the build directory

# Container examples
./utils_container_linq_example
./utils_container_ranges_example
./utils_container_span_example

# Crypto examples
./utils_crypto_aes_example

# Debug examples
./utils_debug_color_print_example
./utils_debug_error_stack_example

# Integration example (no category prefix)
./utils_integration_examples
```

## Key Features Demonstrated

### Error Handling

Most examples demonstrate proper error handling patterns:

- Exception safety
- Error reporting
- Graceful degradation
- Resource cleanup

### Performance Considerations

Examples include performance demonstrations:

- Timing measurements
- Memory usage optimization
- Algorithm complexity analysis
- Benchmarking techniques

### Real-world Usage

Examples show practical applications:

- Log processing
- Data serialization
- Configuration management
- Network data handling
- File I/O operations

### Cross-platform Compatibility

Examples are designed to work across platforms:

- Windows-specific adaptations
- Unix/Linux compatibility
- Portable code patterns

## Best Practices

The examples demonstrate several best practices:

1. **RAII**: Proper resource management
2. **Exception Safety**: Strong exception guarantees
3. **Type Safety**: Compile-time type checking
4. **Performance**: Efficient algorithms and data structures
5. **Readability**: Clear, well-documented code
6. **Testability**: Examples that can be easily verified

## Contributing

When adding new examples:

1. **Follow the directory structure**: Place examples in the appropriate subdirectory matching `atom/utils/` organization
2. **Follow naming conventions**: Use descriptive names with `_example.cpp` suffix (e.g., `aes_example.cpp`, not `advanced_aes.cpp`)
3. **Include comprehensive documentation**: Add file headers and inline comments explaining all features
4. **Demonstrate error handling**: Show proper exception handling and error recovery
5. **Add performance considerations**: Include timing measurements where relevant
6. **Ensure cross-platform compatibility**: Test on multiple platforms
7. **Update this README**: Add your example to the appropriate category table
8. **Update CMakeLists.txt**: The build system automatically picks up new `.cpp` files in subdirectories

## Dependencies

The examples may require additional dependencies:

- spdlog (for logging)
- fmt (for formatting)
- OpenSSL (for crypto operations)
- Boost (optional, for enhanced features)

See the main project documentation for complete dependency information.

## License

These examples are part of the Atom project and follow the same license terms.
