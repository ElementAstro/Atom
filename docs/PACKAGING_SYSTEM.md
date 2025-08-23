# Atom Library Packaging System

This document describes the comprehensive packaging and distribution system implemented for the Atom library, providing multi-platform support, modular installation, and automated distribution workflows.

## Overview

The Atom library packaging system provides:

- **Multi-platform support**: Windows, macOS, and Linux
- **Modular installation**: Install only needed components
- **Multiple distribution channels**: Package managers, direct downloads, containers
- **Automated build pipelines**: CI/CD integration with comprehensive testing
- **Portable distributions**: Self-contained, no-installation-required packages
- **Comprehensive validation**: Package integrity and correctness verification

## Architecture

### Core Components

1. **PackagingConfig.cmake**: Advanced CMake packaging configuration
2. **ModularInstall.cmake**: Component-based installation system
3. **Modular Installer**: Python-based component installer with dependency resolution
4. **Build System**: Comprehensive build and package automation
5. **Portable Creator**: Self-contained distribution generator
6. **Package Validator**: Integrity and correctness verification
7. **CI/CD Workflows**: Automated building, testing, and distribution

### Package Types

| Type | Description | Formats | Use Case |
|------|-------------|---------|----------|
| **Source** | Complete source code | `.tar.gz`, `.zip` | Building from source |
| **Binary** | Pre-compiled libraries | `.tar.gz`, `.zip`, `.deb`, `.rpm` | Quick integration |
| **Python** | Python bindings | `.whl` | Python projects |
| **Portable** | Self-contained | `.tar.gz`, `.zip` | No-install usage |
| **Container** | Docker images | Docker Hub | Containerized deployment |

## Quick Start

### For Users

```bash
# Install via package manager (recommended)
vcpkg install atom
# or
pip install atom
# or
sudo apt install libatom-dev

# Install specific components
vcpkg install atom[networking,imaging]
python -m atom_installer install networking imaging

# Use portable version
wget https://github.com/ElementAstro/Atom/releases/latest/download/atom-linux-x64-portable.tar.gz
tar -xzf atom-linux-x64-portable.tar.gz
cd atom-linux-x64-portable
source tools/setup-env.sh
```

### For Developers

```bash
# Build and package everything
python scripts/build-and-package.py --build-type release

# Create specific package formats
python scripts/build-and-package.py --package-formats deb,rpm,zip

# Create portable distribution
python scripts/create-portable.py --source . --output dist

# Validate packages
python scripts/validate-package.py dist/atom-1.0.0-linux-x64.tar.gz
```

## Modular Installation System

### Component Architecture

The Atom library is divided into modular components with clear dependencies:

```
Core Components (always required):
├── error      (error handling)
├── log        (logging utilities)
├── type       (type utilities)
└── utils      (general utilities)

Optional Components:
├── algorithm  (depends: error)
├── async      (depends: error, log)
├── connection (depends: error, log, async)
├── image      (depends: error, log, io)
├── web        (depends: error, log, async, connection)
└── ...
```

### Meta-Packages

Pre-defined component combinations for common use cases:

- **core**: `error`, `log`, `type`, `utils`
- **networking**: `connection`, `web`, `async`
- **imaging**: `image`, `io`, `algorithm`
- **system**: `sysinfo`, `system`, `serial`
- **full**: All components

### Usage Examples

```bash
# Install core components only
atom-installer install core

# Install networking meta-package
atom-installer install networking

# Install specific components (dependencies auto-resolved)
atom-installer install image  # Also installs: error, log, io

# Check what's installed
atom-installer list

# Uninstall components
atom-installer uninstall algorithm
```

## Package Manager Integration

### vcpkg (C++ Package Manager)

```bash
# Basic installation
vcpkg install atom

# With specific features
vcpkg install atom[python,networking,imaging]

# All features
vcpkg install atom[all]
```

**Features available:**
- `core`, `algorithm`, `async`, `components`, `connection`, `containers`
- `image`, `io`, `memory`, `meta`, `search`, `secret`, `serial`
- `sysinfo`, `system`, `web`
- `networking`, `imaging`, `full` (meta-packages)
- `python`, `examples`, `tests`, `docs`
- `boost-lockfree`, `boost-graph`, `boost-intrusive`
- `cfitsio`, `ssh`, `readline`

### Conan (C++ Package Manager)

```python
# conanfile.txt
[requires]
atom/1.0.0

[options]
atom:with_networking=True
atom:with_imaging=True
atom:with_python=True
```

```bash
# Install with Conan
conan install . --build=missing
```

### System Package Managers

#### Ubuntu/Debian
```bash
# Add repository
curl -fsSL https://packages.elementastro.org/gpg | sudo apt-key add -
echo "deb https://packages.elementastro.org/ubuntu $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/atom.list

# Install
sudo apt update
sudo apt install libatom-dev

# Install specific components
sudo apt install libatom-core-dev libatom-imaging-dev
```

#### macOS (Homebrew)
```bash
# Install
brew install atom

# With features
brew install atom --with-python --with-imaging
```

#### Windows (Chocolatey)
```powershell
# Install
choco install atom

# With features
choco install atom --params "/Features:python,imaging,networking"
```

## Portable Distribution

### Features

- **Self-contained**: All dependencies included
- **No installation required**: Run from any location
- **Cross-platform**: Windows, macOS, Linux
- **Environment setup**: Automatic path configuration
- **Development ready**: Headers, libraries, tools included

### Structure

```
atom-portable/
├── bin/           # Executables and tools
├── lib/           # Libraries and dependencies
├── include/       # Header files
├── share/         # Data files and documentation
├── tools/         # Environment setup scripts
│   ├── setup-env.sh    # Unix environment setup
│   ├── setup-env.bat   # Windows environment setup
│   ├── dev-env.sh      # Unix development shell
│   └── dev-env.bat     # Windows development shell
└── README.md      # Usage instructions
```

### Usage

```bash
# Extract portable distribution
tar -xzf atom-1.0.0-linux-x64-portable.tar.gz
cd atom-1.0.0-linux-x64-portable

# Setup environment
source tools/setup-env.sh

# Start development environment
tools/dev-env.sh

# Use in CMake project
export CMAKE_PREFIX_PATH=$PWD:$CMAKE_PREFIX_PATH
```

## Build System

### Comprehensive Build Script

The `build-and-package.py` script provides a complete build and packaging solution:

```bash
# Full build and package workflow
python scripts/build-and-package.py \
    --build-type release \
    --components algorithm,async,connection \
    --package-formats tar.gz,deb,rpm \
    --verbose

# Create portable distribution
python scripts/build-and-package.py \
    --build-type release \
    --no-tests \
    --package-formats portable

# Publish to distribution channels
python scripts/build-and-package.py \
    --build-type release \
    --publish github-releases,pypi \
    --dry-run
```

### Build Configuration

| Option | Description | Default |
|--------|-------------|---------|
| `--build-type` | Build configuration | `release` |
| `--components` | Specific components to build | All |
| `--package-formats` | Package formats to create | Platform default |
| `--no-tests` | Skip running tests | `False` |
| `--no-portable` | Skip portable distribution | `False` |
| `--publish` | Distribution channels | None |
| `--dry-run` | Dry run for publishing | `False` |

## CI/CD Integration

### GitHub Actions Workflows

1. **release.yml**: Comprehensive release workflow
2. **packaging.yml**: Advanced packaging for all platforms
3. **ci.yml**: Continuous integration testing

### Automated Features

- **Multi-platform builds**: Linux, Windows, macOS
- **Component testing**: Individual component validation
- **Package validation**: Integrity and correctness checks
- **Automated publishing**: GitHub Releases, PyPI, Docker Hub
- **Registry updates**: vcpkg, Conan, Homebrew PRs

### Workflow Triggers

```yaml
# Manual trigger with options
workflow_dispatch:
  inputs:
    components:
      description: 'Components to include'
      required: false
    create_portable:
      description: 'Create portable distribution'
      type: boolean
      default: true

# Automatic on tags
push:
  tags:
    - 'v*'
```

## Package Validation

### Validation Tool

The `validate-package.py` script provides comprehensive package validation:

```bash
# Validate package
python scripts/validate-package.py atom-1.0.0-linux-x64.tar.gz

# JSON output
python scripts/validate-package.py --json atom-1.0.0-linux-x64.tar.gz

# Verbose output
python scripts/validate-package.py --verbose atom-1.0.0-linux-x64.tar.gz
```

### Validation Checks

1. **Structure Check**: Required files and directories
2. **Dependency Check**: Dependency information and validity
3. **Integrity Check**: File corruption and checksums
4. **Installation Test**: CMake integration and import tests

### Validation Results

```
ATOM PACKAGE VALIDATION REPORT
============================================================

Package Information:
  Name: atom-1.0.0-linux-x64.tar.gz
  Type: binary
  Size: 15,234,567 bytes
  Format: .tar.gz
  Checksum: a1b2c3d4e5f6...

Validation Results:
  Structure Check: ✓ PASS
  Dependency Check: ✓ PASS
  Integrity Check: ✓ PASS
  Installation Test: ✓ PASS

Overall Status: ✓ PASSED
```

## Distribution Channels

### Supported Channels

1. **GitHub Releases**: Automatic release creation
2. **PyPI**: Python package distribution
3. **Docker Hub**: Container image registry
4. **vcpkg Registry**: C++ package manager
5. **Conan Center**: C++ package repository
6. **Homebrew**: macOS package manager
7. **APT Repository**: Debian/Ubuntu packages
8. **RPM Repository**: Red Hat/CentOS packages

### Publishing Workflow

```bash
# Automated publishing (CI/CD)
# Triggered on git tags or manual workflow dispatch

# Manual publishing
python scripts/build-and-package.py \
    --publish github-releases,pypi,docker \
    --dry-run  # Remove for actual publishing
```

## Troubleshooting

### Common Issues

1. **Build Failures**
   - Check system dependencies: `./scripts/package-manager.sh install-deps`
   - Verify compiler version: C++20 required
   - Update vcpkg: `git pull` in vcpkg directory

2. **Package Validation Failures**
   - Check package integrity: `python scripts/validate-package.py package.tar.gz`
   - Verify dependencies: Ensure all required libraries are included
   - Test installation: Extract and test manually

3. **Modular Installation Issues**
   - Check component dependencies: `atom-installer list --available`
   - Resolve conflicts: `atom-installer install --force`
   - Verify installation: `atom-installer list`

### Getting Help

1. **Documentation**: [Installation Guide](INSTALLATION_GUIDE.md)
2. **GitHub Issues**: [Report problems](https://github.com/ElementAstro/Atom/issues)
3. **Discussions**: [Community support](https://github.com/ElementAstro/Atom/discussions)

## Development

### Adding New Components

1. **Register Component**:
   ```cmake
   atom_register_component(newcomponent
       DESCRIPTION "New component description"
       VERSION ${PROJECT_VERSION}
       DEPENDS error log
   )
   ```

2. **Update Packaging**:
   - Add to `vcpkg.json` features
   - Update `conanfile.py` options
   - Add to modular installer mapping

3. **Test Integration**:
   ```bash
   python scripts/build-and-package.py --components newcomponent
   python scripts/validate-package.py dist/atom-newcomponent-*.tar.gz
   ```

### Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines and [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md) for development setup.

## License

The packaging system is part of the Atom library and is licensed under GPL-3.0. See [LICENSE](../LICENSE) for details.
