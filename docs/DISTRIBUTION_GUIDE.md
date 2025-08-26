# Atom Library Distribution Guide

This guide covers the comprehensive distribution system for the Atom library, including package creation, distribution channels, and deployment strategies.

## Table of Contents

- [Overview](#overview)
- [Distribution Formats](#distribution-formats)
- [Package Creation](#package-creation)
- [Distribution Channels](#distribution-channels)
- [Installation Methods](#installation-methods)
- [Version Management](#version-management)
- [Cross-Platform Compatibility](#cross-platform-compatibility)

## Overview

The Atom library supports multiple distribution formats and channels to accommodate different use cases and platforms:

- **Source distributions**: For building from source
- **Binary packages**: Pre-compiled libraries and headers
- **Python wheels**: For Python integration
- **System packages**: Native package manager integration
- **Container images**: Docker/Podman support
- **vcpkg ports**: C++ package manager integration

## Distribution Formats

### Source Distribution

**Format**: `.tar.gz`, `.zip`
**Contents**: Complete source code with build scripts
**Use case**: Building from source, development

```bash
# Create source distribution
./scripts/create-distribution.sh
```

**Contents**:

- Source code (`atom/`, `python/`, `example/`)
- Build configuration (`CMakeLists.txt`, `xmake.lua`)
- Documentation (`README.md`, `docs/`)
- Build scripts (`build.sh`, `build.bat`)
- Package configuration (`vcpkg.json`, `pyproject.toml`)

### Binary Distribution

**Format**: `.tar.gz`, `.zip`
**Contents**: Pre-compiled libraries and headers
**Use case**: Quick integration, production deployment

**Structure**:

```
atom-1.0.0-linux-x64/
├── include/
│   └── atom/
├── lib/
│   ├── libatom.a
│   └── cmake/atom/
├── bin/
│   └── atom-tools
└── share/
    └── doc/
```

### Python Wheels

**Format**: `.whl`
**Contents**: Python bindings and native extensions
**Use case**: Python projects, scientific computing

```bash
# Install from wheel
pip install atom-1.0.0-cp311-cp311-linux_x86_64.whl
```

### System Packages

#### Debian/Ubuntu (.deb)

```bash
# Install
sudo dpkg -i libatom-dev_1.0.0-1_amd64.deb
sudo apt-get install -f
```

#### Red Hat/CentOS (.rpm)

```bash
# Build RPM
rpmbuild -ba atom.spec

# Install
sudo rpm -i atom-1.0.0-1.x86_64.rpm
```

#### Arch Linux (PKGBUILD)

```bash
# Build and install
makepkg -si
```

## Package Creation

### Automated Creation

Use the distribution script for comprehensive package creation:

```bash
# Create all distribution formats
./scripts/create-distribution.sh

# Create specific formats
./scripts/package-manager.sh create-deb
./scripts/package-manager.sh create-rpm
```

### Manual Creation

#### Source Package

```bash
# Create source archive
git archive --format=tar.gz --prefix=atom-1.0.0/ HEAD > atom-1.0.0-source.tar.gz
```

#### Binary Package

```bash
# Build and install to temporary directory
cmake -B build -DCMAKE_INSTALL_PREFIX=/tmp/atom-install
cmake --build build
cmake --install build

# Create archive
tar -czf atom-1.0.0-linux-x64.tar.gz -C /tmp atom-install
```

#### Python Wheel

```bash
# Build wheel
python -m build --wheel

# Build for multiple platforms
python -m cibuildwheel --platform linux
```

### Package Validation

Validate packages before distribution:

```bash
# Test installation
tar -xzf atom-1.0.0-linux-x64.tar.gz
export CMAKE_PREFIX_PATH=$PWD/atom-1.0.0-linux-x64

# Test CMake integration
echo 'find_package(atom REQUIRED)' > test.cmake
cmake -P test.cmake

# Test Python wheel
pip install atom-1.0.0-*.whl
python -c "import atom; print(atom.__version__)"
```

## Distribution Channels

### GitHub Releases

**Automatic**: Triggered by git tags
**Manual**: Using GitHub web interface or API

```bash
# Create release with GitHub CLI
gh release create v1.0.0 \
    --title "Release 1.0.0" \
    --notes-file CHANGELOG.md \
    dist/*
```

### Python Package Index (PyPI)

**Automatic**: Via GitHub Actions on release
**Manual**: Using twine

```bash
# Upload to PyPI
python -m twine upload dist/*.whl

# Upload to Test PyPI
python -m twine upload --repository testpypi dist/*.whl
```

### vcpkg Registry

**Process**:

1. Create port files (`ports/atom/`)
2. Submit PR to vcpkg registry
3. Automatic integration after approval

```bash
# Test vcpkg port locally
vcpkg install atom --overlay-ports=./ports
```

### Package Managers

#### Homebrew (macOS)

```ruby
# Formula template
class Atom < Formula
  desc "Foundational library for astronomical software"
  homepage "https://github.com/ElementAstro/Atom"
  url "https://github.com/ElementAstro/Atom/archive/v1.0.0.tar.gz"
  sha256 "..."

  depends_on "cmake"
  depends_on "openssl"
  depends_on "sqlite"

  def install
    system "cmake", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end
end
```

#### Conan

```python
# conanfile.py
from conans import ConanFile, CMake

class AtomConan(ConanFile):
    name = "atom"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    requires = "openssl/1.1.1", "zlib/1.2.11", "sqlite3/3.39.0"

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
```

### Container Registries

#### Docker Hub

```dockerfile
# Dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libatom-dev
```

```bash
# Build and push
docker build -t atom/atom:1.0.0 .
docker push atom/atom:1.0.0
```

## Installation Methods

### From Source

```bash
# Download and extract
wget https://github.com/ElementAstro/Atom/archive/v1.0.0.tar.gz
tar -xzf v1.0.0.tar.gz
cd Atom-1.0.0

# Build and install
./build.sh --release --install-deps
sudo cmake --install build
```

### Binary Package

```bash
# Download and extract
wget https://github.com/ElementAstro/Atom/releases/download/v1.0.0/atom-1.0.0-linux-x64.tar.gz
tar -xzf atom-1.0.0-linux-x64.tar.gz

# Set environment
export CMAKE_PREFIX_PATH=$PWD/atom-1.0.0-linux-x64:$CMAKE_PREFIX_PATH
```

### Package Managers

```bash
# vcpkg
vcpkg install atom

# Conan
conan install atom/1.0.0@

# Homebrew (macOS)
brew install atom

# APT (Ubuntu/Debian)
sudo apt-get install libatom-dev

# DNF (Fedora)
sudo dnf install atom-devel
```

### Python Package

```bash
# From PyPI
pip install atom

# From wheel
pip install atom-1.0.0-cp311-cp311-linux_x86_64.whl

# Development install
pip install -e .
```

## Version Management

### Semantic Versioning

The project follows [Semantic Versioning](https://semver.org/):

- **MAJOR**: Incompatible API changes
- **MINOR**: Backward-compatible functionality
- **PATCH**: Backward-compatible bug fixes

### Version Tools

```bash
# Check current version
./scripts/version-manager.sh current

# Bump version
./scripts/version-manager.sh bump minor

# Create release
./scripts/version-manager.sh release patch
```

### Version Synchronization

Versions are synchronized across:

- `CMakeLists.txt`
- `xmake.lua`
- `vcpkg.json`
- `pyproject.toml`
- `setup.py`
- Git tags

## Cross-Platform Compatibility

### Platform Support Matrix

| Platform | Architecture | CMake | XMake | Python | Status |
|----------|-------------|-------|-------|--------|--------|
| Linux    | x86_64      | ✅    | ✅    | ✅     | Full   |
| Linux    | ARM64       | ✅    | ✅    | ✅     | Full   |
| Windows  | x86_64      | ✅    | ✅    | ✅     | Full   |
| Windows  | ARM64       | ✅    | ⚠️    | ✅     | Partial|
| macOS    | x86_64      | ✅    | ✅    | ✅     | Full   |
| macOS    | ARM64       | ✅    | ✅    | ✅     | Full   |

### Compatibility Testing

```bash
# Test on multiple platforms
docker run --rm -v $PWD:/src ubuntu:20.04 /src/build.sh --test
docker run --rm -v $PWD:/src ubuntu:22.04 /src/build.sh --test
docker run --rm -v $PWD:/src centos:8 /src/build.sh --test
```

### Distribution Optimization

#### Size Optimization

- Strip debug symbols from release builds
- Use static linking where appropriate
- Compress archives with maximum compression
- Remove unnecessary files

#### Performance Optimization

- Enable link-time optimization (LTO)
- Use profile-guided optimization (PGO)
- Optimize for target architecture
- Bundle frequently used dependencies

## Quality Assurance

### Package Testing

```bash
# Automated testing
./scripts/test-distribution.sh

# Manual verification
./scripts/verify-package.sh atom-1.0.0-linux-x64.tar.gz
```

### Checksums and Signatures

```bash
# Generate checksums
sha256sum atom-1.0.0-* > checksums.sha256
md5sum atom-1.0.0-* > checksums.md5

# GPG signatures
gpg --detach-sign --armor atom-1.0.0-source.tar.gz
```

### Security Scanning

```bash
# Scan for vulnerabilities
trivy fs --security-checks vuln .

# License compliance
licensee detect
```

## Troubleshooting

### Common Issues

#### Package Size Too Large

- Enable compression
- Remove debug symbols
- Exclude unnecessary files

#### Missing Dependencies

- Update dependency lists
- Test on clean systems
- Document system requirements

#### Installation Failures

- Check file permissions
- Verify checksums
- Test installation scripts

### Getting Help

- Check distribution logs
- Review package contents
- Test on target systems
- Contact maintainers

For more information, see:

- [Build Guide](BUILD_GUIDE.md)
- [CI/CD Guide](CI_CD_GUIDE.md)
- [Development Guide](DEVELOPMENT_GUIDE.md)
