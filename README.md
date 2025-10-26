# Atom

A comprehensive, modular C++20/C++23 foundational library for astronomical software development. Atom provides a rich collection of high-performance modules for algorithmic operations, image processing, asynchronous programming, networking, system integration, and more.

**Version:** 0.1.0
**License:** GPL-3.0
**Homepage:** <https://github.com/ElementAstro/Atom>

## 🌟 Features

- **18+ Modular Components**: Independently buildable modules with explicit dependency management
- **Cross-Platform Support**: Windows (MSVC), Linux (GCC/Clang), macOS (Clang)
- **Multi-Build System**: CMake (primary) and XMake support with feature parity
- **Python Bindings**: Full pybind11 integration for Python 3.8+
- **High Performance**: SIMD optimization, memory pooling, lock-free data structures
- **Astronomical Focus**: Specialized support for FITS, SER formats, and astronomical image processing
- **Modern C++**: C++20 standard with C++23 support where available

## 📦 Modules

### Core Modules

| Module | Purpose | Key Features |
|--------|---------|--------------|
| **error** | Error handling & stack traces | Comprehensive error context, stack trace generation, error recovery |
| **log** | Logging framework | Async logging, memory-mapped logging, log management |
| **type** | Type utilities & containers | JSON/YAML support, concurrent containers, small vector optimization |
| **meta** | Reflection & metaprogramming | Type introspection, property system, FFI support |
| **utils** | General utilities | String processing, time utilities, cryptography, UUID generation |

### Specialized Modules

| Module | Purpose | Key Features |
|--------|---------|--------------|
| **algorithm** | Algorithms & data structures | Cryptography, compression, signal processing, pathfinding, optimization |
| **async** | Asynchronous programming | Futures, promises, thread pools, message queues, coroutine support |
| **components** | Component system | Memory pooling, lifecycle management, scripting integration (Lua/Python) |
| **connection** | Network communication | TCP/UDP, SSH, FIFO, TTY, async operations, connection pooling |
| **containers** | High-performance containers | Boost containers, lock-free structures, graph algorithms, intrusive containers |
| **image** | Image processing | FITS/SER format support, OCR, format conversion, SIMD optimization |
| **io** | Input/output operations | File operations, compression, glob patterns, async I/O |
| **memory** | Memory management | Memory pools, tracking, shared pointers, ring buffers |
| **search** | Search & caching | LRU cache, TTL cache, SQLite/MySQL database support |
| **secret** | Security & encryption | Password management, encryption, secure storage |
| **serial** | Serial communication | Serial ports, Bluetooth, USB support, cross-platform |
| **sysinfo** | System information | CPU, memory, disk, GPU, battery, network, OS info |
| **system** | System integration | Process management, environment, registry, signals, scheduling |
| **web** | Web utilities | HTTP client, MIME types, URL handling, downloader |

## 🚀 Quick Start

### Prerequisites

- **C++ Compiler**: GCC 11+, Clang 12+, or MSVC 2022+
- **CMake**: 3.21 or later
- **Python**: 3.8+ (for Python bindings)
- **Dependencies**: OpenSSL, loguru, optional: OpenCV, CFITSIO, Tesseract

### Building

#### Using Build Scripts (Recommended)

```bash
# Unix/Linux/macOS
./scripts/build.sh --release --tests --examples

# Windows
scripts\build.bat --release --tests --examples
```

#### Using CMake Presets

```bash
# Configure with preset
cmake --preset release

# Build
cmake --build --preset release -j

# Run tests
ctest --preset default --output-on-failure
```

#### Using CMake Directly

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DATOM_BUILD_TESTS=ON \
  -DATOM_BUILD_EXAMPLES=ON

# Build
cmake --build build -j

# Install
cmake --install build
```

#### Using XMake

```bash
# Configure and build
xmake build -y

# Run tests
xmake test

# Build with options
xmake build -y --build_all=true --build_tests=true
```

### Build Options

Common CMake options:

```cmake
-DATOM_BUILD_ALL=ON                    # Build all modules (default: ON)
-DATOM_BUILD_TESTS=ON                  # Build test suite
-DATOM_BUILD_EXAMPLES=ON                # Build examples
-DATOM_BUILD_PYTHON_BINDINGS=ON        # Build Python bindings
-DATOM_BUILD_DOCS=ON                   # Generate documentation
-DATOM_USE_SSH=ON                      # Enable SSH support
-DATOM_USE_CFITSIO=ON                  # Enable CFITSIO for FITS support
-DBUILD_SHARED_LIBS=ON                 # Build shared libraries
```

Selective module building:

```cmake
-DATOM_BUILD_ALGORITHM=ON
-DATOM_BUILD_ASYNC=ON
-DATOM_BUILD_IMAGE=ON
# ... and so on for each module
```

## 🧪 Testing

### Running Tests

```bash
# Build and run all tests
./scripts/build.sh --debug --tests --run-tests

# Using CMake
cmake --preset debug
cmake --build --preset debug -j
ctest --preset default --output-on-failure

# Run specific test module
ctest -R "algorithm_*" --output-on-failure

# Using XMake
xmake test
```

### Test Framework

- **C++ Tests**: GoogleTest (GTest) framework
- **Python Tests**: pytest with coverage reporting
- **Test Organization**: Tests organized by module under `tests/` directory
- **Coverage**: Configured via `pyproject.toml` for Python tests

## 🐍 Python Bindings

Atom provides comprehensive Python bindings for most modules using pybind11.

### Installation

```bash
# From source with Python bindings
pip install -e .[dev]

# Or build and install
./scripts/build.sh --python --release
```

### Usage

```python
import atom

# Example: Using algorithm module
from atom.algorithm import hash_functions

# Example: Using async module
from atom.async import Promise, Future

# Example: Using system module
from atom.system import get_cpu_info, get_memory_info
```

### Available Python Modules

- `atom.algorithm` - Algorithm and cryptographic functions
- `atom.async` - Asynchronous programming primitives
- `atom.connection` - Network communication
- `atom.error` - Error handling
- `atom.io` - Input/output operations
- `atom.search` - Search and caching
- `atom.sysinfo` - System information
- `atom.system` - System integration
- `atom.type` - Type utilities
- `atom.utils` - General utilities
- `atom.web` - Web utilities

## 📚 Documentation

### Building Documentation

```bash
# Generate Doxygen documentation (C++)
doxygen Doxyfile

# Generate Sphinx documentation (Python)
sphinx-build -b html docs docs/_build

# Using build script
./scripts/build.sh --docs
```

### Documentation Locations

- **C++ API**: `docs/_build/html/` (after Doxygen generation)
- **Python API**: `docs/_build/html/` (after Sphinx generation)
- **Module READMEs**: Each module has a `README.md` with specific documentation
- **Examples**: Comprehensive examples in `example/` directory

## 🏗️ Project Structure

```text
atom/
├── algorithm/          # Algorithms and cryptography
├── async/              # Asynchronous programming
├── components/         # Component system
├── connection/         # Network communication
├── containers/         # High-performance containers
├── error/              # Error handling
├── image/              # Image processing
├── io/                 # Input/output operations
├── log/                # Logging framework
├── memory/             # Memory management
├── meta/               # Reflection and metaprogramming
├── search/             # Search and caching
├── secret/             # Security and encryption
├── serial/             # Serial communication
├── sysinfo/            # System information
├── system/             # System integration
├── type/               # Type utilities
├── utils/              # General utilities
└── web/                # Web utilities

cmake/                  # CMake modules and configuration
example/                # Comprehensive examples
python/                 # Python bindings
tests/                  # Test suite
scripts/                # Build and utility scripts
docs/                   # Documentation
```

## 🔧 Development

### Build System Architecture

- **Primary**: CMake 3.21+ with presets
- **Secondary**: XMake for alternative builds
- **Dependency Management**: vcpkg integration (optional)
- **Module Dependencies**: Explicit dependency graph in `cmake/module_dependencies.cmake`

### Build Presets

Available CMake presets:

- `debug` - Debug build with symbols
- `release` - Optimized release build
- `relwithdebinfo` - Release with debug info
- Platform-specific: `debug-msys2`, `release-vs`, etc.

### Coding Standards

- **C++ Standard**: C++20 (C++23 where available)
- **Code Style**: 4-space indentation, 80-column guide
- **Formatting**: clang-format (see `.clang-format`)
- **Naming**: camelCase for variables/functions, PascalCase for classes
- **Documentation**: Doxygen comments for public APIs

### Pre-commit Hooks

```bash
# Install pre-commit hooks
pre-commit install

# Run manually
pre-commit run -a
```

## 📋 Dependencies

### Required

- **loguru**: Logging framework
- **OpenSSL**: Cryptographic operations

### Optional

- **OpenCV**: Image processing (for image module)
- **CFITSIO**: FITS file format support (for image module)
- **Tesseract**: OCR capabilities (for image module)
- **Boost**: High-performance data structures (for containers module)
- **ASIO**: Asynchronous I/O (for connection module)
- **pybind11**: Python bindings (for Python support)

### Development

- **GoogleTest**: Unit testing framework
- **Sphinx**: Documentation generation
- **pytest**: Python testing framework
- **Black/isort/Ruff**: Python code formatting and linting

## 🛠️ Common Tasks

### Building a Single Module

```bash
cmake -B build -DATOM_BUILD_ALGORITHM=ON -DATOM_BUILD_TESTS=ON
cmake --build build --target atom-algorithm
```

### Running Specific Tests

```bash
cd build
ctest -R "algorithm_*" --output-on-failure
```

### Building with All Features

```bash
./scripts/build.sh --release --python --examples --tests --docs --package
```

### Cleaning Build Artifacts

```bash
./scripts/build.sh --clean
# or
rm -rf build build-msvc
```

## 🤝 Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Workflow

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests and linting
5. Submit a pull request

### Code Quality

- All code must pass `pre-commit` checks
- Tests must pass for all modules
- Documentation must be updated for API changes
- Follow the coding standards in [STYLE_OF_CODE.md](STYLE_OF_CODE.md)

## 📄 License

Atom is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.

## 🔗 Resources

- **GitHub**: <https://github.com/ElementAstro/Atom>
- **Issues**: <https://github.com/ElementAstro/Atom/issues>
- **Documentation**: See `docs/` directory
- **Examples**: See `example/` directory

## 📞 Support

For issues, questions, or suggestions:

1. Check existing [issues](https://github.com/ElementAstro/Atom/issues)
2. Review [documentation](docs/)
3. Create a new issue with detailed information
4. See [SECURITY.md](SECURITY.md) for security-related concerns

## 🎯 Roadmap

- Enhanced GPU acceleration for image processing
- Additional astronomical format support
- Performance optimizations for large-scale data processing
- Extended Python API coverage
- Improved documentation and tutorials

---

## Built with ❤️

For the astronomical software community
