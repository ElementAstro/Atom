# Atom

A comprehensive, modular C++20/C++23 foundational library for astronomical software development. Atom provides a rich collection of high-performance modules for algorithmic operations, image processing, asynchronous programming, networking, system integration, and more.

**Version:** 0.1.0
**License:** GPL-3.0
**Homepage:** <https://github.com/ElementAstro/Atom>

## 🌟 Features

- **Modular Core (18+ domains)**: Each module can be enabled/disabled independently with explicit dependencies
- **Cross-Platform**: Windows (MSVC/MSYS2 MinGW64), Linux (GCC/Clang), macOS (Clang)
- **Primary Build: CMake** with presets for Ninja, Makefiles, MSVC, and MSYS2 MinGW64; **XMake** supported
- **Python Bindings (pybind11)**: Optional bindings for major modules (Python 3.8+)
- **Performance-Oriented**: SIMD where available, memory pooling, lock-free queues, tuned allocators
- **Astronomy-Friendly**: FITS/SER helpers and image utilities for astro workflows
- **Modern C++**: Targets C++20, uses C++23 features when the toolchain supports them

## 📦 Modules

### Core Modules

| Module | Purpose | Key Capabilities |
|--------|---------|------------------|
| **error** | Error handling | Error contexts, stack traces, recovery helpers |
| **log** | Logging | Async logging, rotation, memory-mapped sinks |
| **type** | Type & containers | Variant/any helpers, small-vector, JSON/YAML helpers |
| **meta** | Reflection & meta | Type traits, property helpers, light FFI utilities |
| **utils** | General utilities | Strings/time, hashing, UUIDs, crypto helpers, helpers for CLI/process |

### Specialized Modules

| Module | Purpose | Key Capabilities |
|--------|---------|------------------|
| **algorithm** | Algorithms & data structures | Compression, crypto primitives, hashing, filters, pathfinding |
| **async** | Async primitives | Futures/promises, executors, workers, messaging |
| **components** | Component system | Pools, lifecycle management, lightweight ECS-like utilities |
| **connection** | Networking & IPC | TCP/UDP, FIFO/TTY helpers, async sockets, pooling |
| **containers** | Extra containers | Lock-free queues, intrusive/graph helpers (Boost optional) |
| **image** | Image helpers | FITS/SER helpers, basic transforms, optional OCR/OpenCV |
| **io** | I/O utilities | File ops, compression, globbing, async I/O helpers |
| **memory** | Memory tooling | Pools, arenas, tracking, custom allocators |
| **search** | Caches & search | LRU/TTL caches, pluggable storage (SQLite/MySQL optional) |
| **secret** | Security helpers | Password/crypto helpers, secure storage utilities |
| **serial** | Serial comms | Serial ports and adapters with cross-platform helpers |
| **sysinfo** | System info | CPU/mem/disk/GPU/network/system introspection |
| **system** | System integration | Process management, env/registry, scheduling, signals |
| **web** | Web utilities | HTTP client, MIME helpers, URL tools, downloaders |

## 🚀 Quick Start

### Prerequisites

- **C++ Compiler**: GCC 11+/Clang 12+/MSVC 2022+ (C++20; C++23 used where supported)
- **CMake**: 3.21+
- **Python**: 3.8+ if building bindings/tests
- **Core deps**: spdlog (compiled), OpenSSL
- **Optional**: OpenCV/CFITSIO/Tesseract (image), Boost (containers/graph), ASIO or system ASIO (connection), pybind11 (Python bindings)

### Building

#### Using Build Scripts

```bash
# Unix/Linux/macOS
./scripts/build.sh --release --tests --examples

# Windows
scripts\build.bat --release --tests --examples
```

#### Using CMake Presets (recommended)

```bash
# Configure (choose one)
cmake --preset debug           # or release / relwithdebinfo
cmake --preset debug-msys2     # MSYS2 MinGW64
cmake --preset debug-vs        # MSVC

# Build
cmake --build --preset debug -j

# Run tests (if enabled)
ctest --preset default --output-on-failure
```

#### Using CMake Directly

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DATOM_BUILD_TESTS=ON \
  -DATOM_BUILD_EXAMPLES=ON \
  -DATOM_BUILD_PYTHON_BINDINGS=ON

# Build
cmake --build build -j

# Install
cmake --install build
```

#### Using XMake

```bash
xmake build -y
xmake test
# With options
xmake build -y --build_all=true --build_tests=true
```

### Build Options

Common CMake options:

```cmake
-DATOM_BUILD_ALL=ON                    # Build all modules (default ON)
-DATOM_BUILD_TESTS=ON                  # Build C++ tests
-DATOM_BUILD_EXAMPLES=ON               # Build examples
-DATOM_BUILD_PYTHON_BINDINGS=ON        # Build pybind11 bindings
-DATOM_BUILD_DOCS=ON                   # Build docs (Doxygen/Sphinx)
-DATOM_USE_SSH=ON                      # Enable SSH support in connection
-DATOM_USE_CFITSIO=ON                  # FITS support for image
-DATOM_USE_BOOST=ON                    # Enable Boost-based containers/graph
-DBUILD_SHARED_LIBS=ON                 # Build shared libs
```

Per-module toggles (all default to `ATOM_BUILD_ALL`):

```cmake
-DATOM_BUILD_ALGORITHM=ON
-DATOM_BUILD_ASYNC=ON
-DATOM_BUILD_COMPONENTS=ON
-DATOM_BUILD_CONNECTION=ON
-DATOM_BUILD_CONTAINERS=ON
-DATOM_BUILD_ERROR=ON
-DATOM_BUILD_IMAGE=ON
-DATOM_BUILD_IO=ON
-DATOM_BUILD_LOG=ON
-DATOM_BUILD_MEMORY=ON
-DATOM_BUILD_META=ON
-DATOM_BUILD_SEARCH=ON
-DATOM_BUILD_SECRET=ON
-DATOM_BUILD_SERIAL=ON
-DATOM_BUILD_SYSINFO=ON
-DATOM_BUILD_SYSTEM=ON
-DATOM_BUILD_TYPE=ON
-DATOM_BUILD_UTILS=ON
-DATOM_BUILD_WEB=ON
```

## 🧪 Testing

### Running Tests

```bash
# Build + run all C++ tests (CMake preset)
cmake --preset debug -DATOM_BUILD_TESTS=ON
cmake --build --preset debug -j
ctest --preset default --output-on-failure

# Run specific test module
ctest -R "algorithm_*" --output-on-failure

# Python tests
pip install -e .[dev]
pytest -q

# Using scripts
./scripts/build.sh --debug --tests --run-tests
```

### Test Framework

- **C++**: GoogleTest via CTest presets
- **Python**: pytest (coverage configured in `pyproject.toml`)
- **Layout**: Tests organized by module under `tests/`

## 🐍 Python Bindings

Atom offers optional pybind11 bindings for major modules.

### Installation

```bash
pip install -e .[dev]           # editable install with dev extras
# or via build script
./scripts/build.sh --python --release
```

### Usage

```python
import atom
from atom.algorithm import hash_functions
from atom.async import Promise, Future
from atom.system import get_cpu_info, get_memory_info
```

### Available Python Modules

- `atom.algorithm`, `atom.async`, `atom.connection`, `atom.error`, `atom.io`,
  `atom.search`, `atom.sysinfo`, `atom.system`, `atom.type`, `atom.utils`,
  `atom.web` (availability depends on build options)

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

- **spdlog**: Logging framework
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
