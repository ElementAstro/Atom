# Atom Utils Examples

This directory contains comprehensive examples demonstrating all the utilities available in the `atom::utils` namespace. Each example is designed to be complete, functional, and well-documented to help you understand and implement the available features.

## Overview

The Atom Utils library provides a rich set of utilities organized into several categories:

- **Core Utilities**: Basic functionality like switch statements, bit manipulation, and argument parsing
- **Container Utilities**: LINQ-style operations, ranges, and span utilities
- **Conversion Utilities**: Type conversions, string conversions, and serialization
- **Crypto Utilities**: AES encryption, compression, and hash calculations
- **Debug Utilities**: Colored output, error tracking, and debugging tools
- **Format Utilities**: Text diffing, XML processing, and formatting
- **Memory Utilities**: Aligned memory, leak detection, and SIMD operations
- **Process Utilities**: External process management and execution
- **Random Utilities**: Random number generation, UUIDs, and distributions
- **Text Utilities**: String manipulation, UTF conversions, and validation
- **Time Utilities**: Time formatting, timezone handling, and timing operations

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

# Or build specific examples
make utils_aes
make utils_color_print
make utils_integration_examples
```

### Build Options

You can control which examples are built using CMake options:

```bash
# Build all utils examples (default)
cmake -DATOM_EXAMPLE_UTILS_BUILD_ALL=ON ..

# Build only specific examples
cmake -DATOM_EXAMPLE_UTILS_BUILD_ALL=OFF -DATOM_EXAMPLE_UTILS_AES=ON ..
```

## Example Categories

### Core Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `anyutils.cpp` | Any type utilities | Type erasure, safe casting, value extraction |
| `argsview.cpp` | Argument parsing | Command-line argument processing, validation |
| `bit.cpp` | Bit manipulation | Bit operations, masks, endianness |
| `switch.cpp` | String switch statements | Compile-time string switching |

### Container Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `linq.cpp` | LINQ-style operations | Functional programming, data queries |
| `ranges.cpp` | Range algorithms | Range-based operations, transformations |
| `span.cpp` | Span utilities | Memory-safe array views |

### Conversion Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `to_any.cpp` | Any type conversions | Dynamic type conversion |
| `to_byte.cpp` | Byte serialization | Binary data conversion |
| `to_string.cpp` | String serialization | Object to string conversion |
| `text_to_string.cpp` | Text conversions | Comprehensive string formatting |
| `type_convert.cpp` | Type conversions | Safe type casting, bounds checking |

### Crypto Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `aes.cpp` | AES encryption | Encryption/decryption, compression, hashing |

### Debug Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `color_print.cpp` | Colored console output | Terminal colors, formatting, logging |
| `event_stack.cpp` | Error tracking | Error stack management, filtering |
| `print.cpp` | Advanced printing | Formatted output, containers |

### Format Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `difflib.cpp` | Text diffing | Sequence comparison, diff generation |
| `xml.cpp` | XML processing | Parsing, validation, manipulation |

### Memory Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `aligned.cpp` | Aligned memory | Memory alignment validation |
| `leak.cpp` | Memory leak detection | Allocation tracking, debugging |
| `simd_wrapper.cpp` | SIMD operations | Vectorized computations, performance |

### Process Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `qprocess.cpp` | Process management | External process execution, I/O handling |

### Random Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `random.cpp` | Random generation | Various distributions, thread safety |
| `lcg.cpp` | Linear congruential generator | Custom random algorithms |
| `uuid.cpp` | UUID generation | Unique identifier creation |

### Text Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `string.cpp` | String manipulation | Case conversion, splitting, trimming |
| `utf.cpp` | UTF conversions | Unicode handling, encoding conversion |
| `valid_string.cpp` | String validation | Input validation, bracket matching |
| `cstring.cpp` | Compile-time strings | Constexpr string operations |

### Time Utilities

| Example | Description | Key Features |
|---------|-------------|--------------|
| `time.cpp` | Time operations | Formatting, timezone conversion |
| `stopwatcher.cpp` | Performance timing | Benchmarking, lap timing |
| `qdatetime.cpp` | Date/time handling | Advanced date operations |
| `qtimer.cpp` | Timer utilities | Elapsed time measurement |
| `qtimezone.cpp` | Timezone handling | Timezone operations, DST |

### Integration Examples

| Example | Description | Key Features |
|---------|-------------|--------------|
| `integration_examples.cpp` | Combined utilities | Real-world scenarios, multiple utilities |

## Running Examples

After building, you can run any example:

```bash
# Run from the build directory
./utils_aes
./utils_color_print
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

1. Follow the existing naming convention
2. Include comprehensive documentation
3. Demonstrate error handling
4. Add performance considerations
5. Ensure cross-platform compatibility
6. Update this README with your example

## Dependencies

The examples may require additional dependencies:

- spdlog (for logging)
- fmt (for formatting)
- OpenSSL (for crypto operations)
- Boost (optional, for enhanced features)

See the main project documentation for complete dependency information.

## License

These examples are part of the Atom project and follow the same license terms.
