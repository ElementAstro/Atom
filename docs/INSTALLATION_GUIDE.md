# Atom Library Installation Guide

This comprehensive guide covers all installation methods and platforms supported by the Atom library, including modular installation options and troubleshooting.

## Table of Contents

- [Quick Start](#quick-start)
- [Installation Methods](#installation-methods)
- [Platform-Specific Instructions](#platform-specific-instructions)
- [Modular Installation](#modular-installation)
- [Package Managers](#package-managers)
- [Portable Distribution](#portable-distribution)
- [Building from Source](#building-from-source)
- [Troubleshooting](#troubleshooting)

## Quick Start

### For Most Users (Recommended)

```bash
# Using vcpkg (cross-platform)
vcpkg install atom

# Using pip (Python users)
pip install atom

# Using package managers
# Ubuntu/Debian
sudo apt install libatom-dev

# macOS
brew install atom

# Windows (Chocolatey)
choco install atom
```

### For Developers

```bash
# Clone and build from source
git clone https://github.com/ElementAstro/Atom.git
cd Atom
./scripts/build-and-package.py --build-type release
```

## Installation Methods

### 1. Package Managers (Recommended)

Package managers provide the easiest installation experience with automatic dependency management.

#### vcpkg (Cross-platform C++ Package Manager)

```bash
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh  # Linux/macOS
# or
./vcpkg/bootstrap-vcpkg.bat  # Windows

# Install Atom
./vcpkg/vcpkg install atom

# With specific features
./vcpkg/vcpkg install atom[python,networking,imaging]
```

#### Conan (C++ Package Manager)

```bash
# Install Conan
pip install conan

# Add Atom to your conanfile.txt
echo "atom/1.0.0" >> conanfile.txt

# Or install directly
conan install atom/1.0.0@
```

#### Python Package Index (PyPI)

```bash
# Install Python bindings
pip install atom

# With specific extras
pip install atom[imaging,networking]

# Development installation
pip install -e .[dev]
```

### 2. System Package Managers

#### Ubuntu/Debian

```bash
# Add repository (if needed)
curl -fsSL https://packages.elementastro.org/gpg | sudo apt-key add -
echo "deb https://packages.elementastro.org/ubuntu $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/atom.list

# Install
sudo apt update
sudo apt install libatom-dev

# Install specific components
sudo apt install libatom-core-dev libatom-imaging-dev
```

#### CentOS/RHEL/Fedora

```bash
# Add repository
sudo dnf config-manager --add-repo https://packages.elementastro.org/rpm/atom.repo

# Install
sudo dnf install atom-devel

# Or using yum
sudo yum install atom-devel
```

#### Arch Linux

```bash
# Using AUR helper (yay)
yay -S atom

# Manual installation
git clone https://aur.archlinux.org/atom.git
cd atom
makepkg -si
```

#### macOS (Homebrew)

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install Atom
brew install atom

# Install with specific features
brew install atom --with-python --with-imaging
```

#### Windows (Chocolatey)

```powershell
# Install Chocolatey if not already installed
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

# Install Atom
choco install atom

# Install with specific features
choco install atom --params "/Features:python,imaging,networking"
```

### 3. Direct Downloads

Download pre-built packages from [GitHub Releases](https://github.com/ElementAstro/Atom/releases):

- **Windows**: `.zip`, `.msi`, `.exe` installers
- **macOS**: `.tar.gz`, `.dmg`, `.pkg` packages
- **Linux**: `.tar.gz`, `.deb`, `.rpm`, `.appimage` packages

## Platform-Specific Instructions

### Windows

#### Prerequisites
- Windows 10 or later (x64)
- Visual Studio 2019 or later (for building from source)
- CMake 3.21 or later

#### Installation Options

1. **MSI Installer (Recommended)**
   ```powershell
   # Download and run the MSI installer
   Invoke-WebRequest -Uri "https://github.com/ElementAstro/Atom/releases/latest/download/atom-windows-x64.msi" -OutFile "atom-installer.msi"
   Start-Process msiexec.exe -ArgumentList "/i atom-installer.msi /quiet" -Wait
   ```

2. **Portable ZIP**
   ```powershell
   # Download and extract portable version
   Invoke-WebRequest -Uri "https://github.com/ElementAstro/Atom/releases/latest/download/atom-windows-x64-portable.zip" -OutFile "atom-portable.zip"
   Expand-Archive -Path "atom-portable.zip" -DestinationPath "C:\atom"
   
   # Add to PATH
   $env:PATH += ";C:\atom\bin"
   ```

### macOS

#### Prerequisites
- macOS 10.15 (Catalina) or later
- Xcode Command Line Tools

#### Installation Options

1. **Homebrew (Recommended)**
   ```bash
   brew install atom
   ```

2. **DMG Installer**
   ```bash
   # Download and mount DMG
   curl -L -o atom-installer.dmg "https://github.com/ElementAstro/Atom/releases/latest/download/atom-macos-x64.dmg"
   hdiutil mount atom-installer.dmg
   # Follow installer instructions
   ```

3. **PKG Installer**
   ```bash
   # Download and install PKG
   curl -L -o atom-installer.pkg "https://github.com/ElementAstro/Atom/releases/latest/download/atom-macos-x64.pkg"
   sudo installer -pkg atom-installer.pkg -target /
   ```

### Linux

#### Prerequisites
- GCC 9+ or Clang 10+
- CMake 3.21+
- Standard development tools

#### Installation Options

1. **Package Manager (Recommended)**
   ```bash
   # Ubuntu/Debian
   sudo apt install libatom-dev
   
   # CentOS/RHEL/Fedora
   sudo dnf install atom-devel
   
   # Arch Linux
   yay -S atom
   ```

2. **AppImage (Universal)**
   ```bash
   # Download and run AppImage
   wget "https://github.com/ElementAstro/Atom/releases/latest/download/atom-linux-x64.AppImage"
   chmod +x atom-linux-x64.AppImage
   ./atom-linux-x64.AppImage
   ```

3. **Tarball**
   ```bash
   # Download and extract
   wget "https://github.com/ElementAstro/Atom/releases/latest/download/atom-linux-x64.tar.gz"
   tar -xzf atom-linux-x64.tar.gz
   cd atom-linux-x64
   
   # Install
   sudo cp -r * /usr/local/
   sudo ldconfig
   ```

## Modular Installation

Atom supports modular installation, allowing you to install only the components you need.

### Available Components

- **Core**: `error`, `log`, `type`, `utils` (always required)
- **Algorithm**: `algorithm` - Algorithm utilities and data structures
- **Async**: `async` - Asynchronous programming utilities
- **Components**: `components` - Component system framework
- **Connection**: `connection` - Network and IPC utilities
- **Containers**: `containers` - Advanced container data structures
- **Image**: `image` - Image processing and FITS support
- **I/O**: `io` - Input/output utilities
- **Memory**: `memory` - Memory management utilities
- **Meta**: `meta` - Metaprogramming utilities
- **Search**: `search` - Search algorithms and indexing
- **Secret**: `secret` - Cryptographic utilities
- **Serial**: `serial` - Serial communication
- **System Info**: `sysinfo` - System information and monitoring
- **System**: `system` - System utilities and process management
- **Web**: `web` - Web server and HTTP utilities

### Meta-Packages

- **Networking**: `connection`, `web`, `async`
- **Imaging**: `image`, `io`, `algorithm`
- **System**: `sysinfo`, `system`, `serial`
- **Full**: All components

### Using the Modular Installer

```bash
# Install the modular installer
pip install atom-installer

# List available components
atom-installer list --available

# Install specific components
atom-installer install algorithm async connection

# Install meta-package
atom-installer install networking

# Install with dependencies automatically resolved
atom-installer install image  # Also installs: error, log, io

# Check installed components
atom-installer list

# Uninstall components
atom-installer uninstall algorithm
```

### vcpkg Modular Installation

```bash
# Install core only
vcpkg install atom[core]

# Install specific components
vcpkg install atom[algorithm,async,connection]

# Install meta-packages
vcpkg install atom[networking]
vcpkg install atom[imaging]

# Install everything
vcpkg install atom[full]
```

### CMake Integration

```cmake
# Find specific components
find_package(atom REQUIRED COMPONENTS algorithm async connection)

# Link to specific components
target_link_libraries(your_target PRIVATE 
    atom::algorithm 
    atom::async 
    atom::connection
)

# Or link to main library (includes all installed components)
target_link_libraries(your_target PRIVATE atom::atom)
```

## Portable Distribution

Portable distributions don't require installation and can be run from any location.

### Download Portable Version

```bash
# Linux
wget "https://github.com/ElementAstro/Atom/releases/latest/download/atom-linux-x64-portable.tar.gz"
tar -xzf atom-linux-x64-portable.tar.gz
cd atom-linux-x64-portable

# Windows
# Download atom-windows-x64-portable.zip and extract

# macOS
curl -L -o atom-portable.tar.gz "https://github.com/ElementAstro/Atom/releases/latest/download/atom-macos-x64-portable.tar.gz"
tar -xzf atom-portable.tar.gz
cd atom-macos-x64-portable
```

### Using Portable Distribution

```bash
# Linux/macOS - Setup environment
source tools/setup-env.sh

# Windows - Setup environment
tools\setup-env.bat

# Start development environment
# Linux/macOS
tools/dev-env.sh

# Windows
tools\dev-env.bat
```

### Portable Directory Structure

```
atom-portable/
├── bin/           # Executables and tools
├── lib/           # Libraries
├── include/       # Header files
├── share/         # Data files and documentation
├── tools/         # Environment setup scripts
└── README.md      # Usage instructions
```

## Building from Source

### Prerequisites

#### All Platforms
- CMake 3.21 or later
- C++20 compatible compiler
- Git

#### Platform-Specific
- **Windows**: Visual Studio 2019+ or MinGW-w64
- **macOS**: Xcode Command Line Tools
- **Linux**: GCC 9+ or Clang 10+

### Quick Build

```bash
# Clone repository
git clone https://github.com/ElementAstro/Atom.git
cd Atom

# Install dependencies and build
./scripts/build-and-package.py --build-type release

# Or use the traditional approach
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Advanced Build Options

```bash
# Build with specific components
./scripts/build-and-package.py \
    --build-type release \
    --components algorithm async connection \
    --package-formats tar.gz

# Build with all features
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DATOM_BUILD_FULL=ON \
    -DATOM_BUILD_PYTHON_BINDINGS=ON \
    -DATOM_BUILD_EXAMPLES=ON \
    -DATOM_BUILD_TESTS=ON

cmake --build build --parallel
cmake --install build
```

### Build Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `ATOM_BUILD_FULL` | Build all components | `ON` |
| `ATOM_BUILD_PYTHON_BINDINGS` | Build Python bindings | `OFF` |
| `ATOM_BUILD_EXAMPLES` | Build examples | `OFF` |
| `ATOM_BUILD_TESTS` | Build tests | `OFF` |
| `ATOM_BUILD_DOCS` | Build documentation | `OFF` |
| `ATOM_INSTALL_MODULAR` | Enable modular installation | `ON` |
| `BUILD_SHARED_LIBS` | Build shared libraries | `OFF` |

## Troubleshooting

### Common Issues

#### 1. CMake Configuration Fails

**Problem**: CMake cannot find dependencies
```
CMake Error: Could not find OpenSSL
```

**Solution**:
```bash
# Install dependencies first
./scripts/package-manager.sh install-deps

# Or specify paths manually
cmake -B build -S . \
    -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl \
    -DCMAKE_PREFIX_PATH="/usr/local"
```

#### 2. Build Fails with C++20 Errors

**Problem**: Compiler doesn't support C++20
```
error: 'std::format' is not a member of 'std'
```

**Solution**:
```bash
# Update compiler
# Ubuntu
sudo apt install gcc-11 g++-11
export CC=gcc-11 CXX=g++-11

# macOS
xcode-select --install

# Windows
# Install Visual Studio 2019 or later
```

#### 3. Python Bindings Import Error

**Problem**: Cannot import atom module
```python
ImportError: No module named 'atom'
```

**Solution**:
```bash
# Ensure Python bindings were built
cmake -B build -S . -DATOM_BUILD_PYTHON_BINDINGS=ON
cmake --build build

# Install Python package
pip install -e .

# Or set PYTHONPATH
export PYTHONPATH=$PWD/build:$PYTHONPATH
```

#### 4. Missing System Libraries

**Problem**: Runtime library not found
```
error while loading shared libraries: libatom.so.1: cannot open shared object file
```

**Solution**:
```bash
# Update library cache
sudo ldconfig

# Or set LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# For portable installation
source tools/setup-env.sh
```

#### 5. vcpkg Integration Issues

**Problem**: vcpkg packages not found
```
CMake Error: Could not find package configuration file
```

**Solution**:
```bash
# Ensure vcpkg toolchain is used
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Or use vcpkg integrate
./vcpkg/vcpkg integrate install
```

### Getting Help

1. **Check Documentation**: [https://elementastro.github.io/Atom/](https://elementastro.github.io/Atom/)
2. **GitHub Issues**: [https://github.com/ElementAstro/Atom/issues](https://github.com/ElementAstro/Atom/issues)
3. **Discussions**: [https://github.com/ElementAstro/Atom/discussions](https://github.com/ElementAstro/Atom/discussions)
4. **Email Support**: max@example.com

### Diagnostic Information

When reporting issues, please include:

```bash
# System information
uname -a
cmake --version
gcc --version  # or clang --version

# Atom installation info
atom-installer list  # if using modular installer
pkg-config --modversion atom  # if using system packages

# Build log
cmake -B build -S . --verbose
cmake --build build --verbose
```

## Next Steps

After installation:

1. **Verify Installation**: Run the examples in `examples/`
2. **Read Documentation**: Check `docs/` for API documentation
3. **Join Community**: Participate in GitHub discussions
4. **Contribute**: See `CONTRIBUTING.md` for contribution guidelines

For more detailed information, see:
- [API Documentation](API_REFERENCE.md)
- [Examples Guide](EXAMPLES_GUIDE.md)
- [Development Guide](DEVELOPMENT_GUIDE.md)
- [Distribution Guide](DISTRIBUTION_GUIDE.md)
