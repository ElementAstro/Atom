# Atom Project Scripts

This directory contains all utility scripts for the Atom project, organized for easy access and maintenance.

## Build Scripts

### Primary Build Scripts
- **`build.sh`** - Enhanced Unix/Linux/macOS build script with comprehensive options
- **`build.bat`** - Enhanced Windows build script with comprehensive options  
- **`build-msvc.bat`** - MSVC-specific Windows build script with Visual Studio integration

### Advanced Build Tools
- **`build-and-package.py`** - Comprehensive build and packaging system
- **`create-distribution.sh`** - Distribution creation script for releases
- **`create-portable.py`** - Portable package creator for standalone distributions

## Development Tools

### Testing and Validation
- **`validate-build-system.py`** - Build system validator and integrity checker
- **`validate-package.py`** - Package validation and verification tool
- **`test-memory-wrapper.ps1`** - Memory testing wrapper for Windows
- **`deploy-test-dlls.ps1`** - DLL deployment for test environments

### Dependency Management
- **`setup_vcpkg.bat`** - vcpkg setup script for Windows
- **`setup_vcpkg.ps1`** - vcpkg setup script for PowerShell
- **`package-manager.sh`** - Package management utilities for multiple platforms

### Project Management
- **`version-manager.sh`** - Version management and release utilities
- **`modular-installer.py`** - Modular component installer

## Documentation
- **`SCRIPT_ORGANIZATION_BACKUP.md`** - Backup documentation of script reorganization
- **`README.md`** - This file

## Usage Examples

### Quick Build
```bash
# Debug build with tests
./scripts/build.sh --debug --tests --run-tests

# Release build with all features
./scripts/build.sh --release --examples --python --docs

# Windows MSVC build
scripts\build-msvc.bat --release --examples --tests
```

### Advanced Operations
```bash
# Validate build system
python scripts/validate-build-system.py --test-builds

# Create distribution packages
./scripts/create-distribution.sh

# Manage project version
./scripts/version-manager.sh bump minor
```

### Development Setup
```bash
# Setup vcpkg dependencies (Windows)
scripts\setup_vcpkg.bat

# Setup vcpkg dependencies (PowerShell)
scripts\setup_vcpkg.ps1

# Install system packages (Linux)
./scripts/package-manager.sh install-deps
```

## Script Categories

| Category | Scripts | Purpose |
|----------|---------|---------|
| **Build** | `build.sh`, `build.bat`, `build-msvc.bat` | Primary build automation |
| **Packaging** | `build-and-package.py`, `create-distribution.sh`, `create-portable.py` | Release and distribution |
| **Testing** | `validate-build-system.py`, `validate-package.py`, `test-memory-wrapper.ps1` | Quality assurance |
| **Dependencies** | `setup_vcpkg.*`, `package-manager.sh` | Dependency management |
| **Utilities** | `version-manager.sh`, `modular-installer.py`, `deploy-test-dlls.ps1` | Project utilities |

## Backward Compatibility

Wrapper scripts are available in the project root for backward compatibility:
- `./build.sh` → `./scripts/build.sh`
- `build.bat` → `scripts\build.bat`
- `build-msvc.bat` → `scripts\build-msvc.bat`

## Contributing

When adding new scripts:
1. Place them in the appropriate category within this directory
2. Update this README with script descriptions
3. Ensure scripts follow project conventions
4. Add appropriate documentation and help text
