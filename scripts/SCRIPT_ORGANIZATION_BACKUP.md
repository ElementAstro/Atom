# Script Organization Backup

## Original Script Locations (Before Organization)

### Root Directory Scripts
- `build.sh` - Enhanced Unix/Linux/macOS build script (645 lines)
- `build.bat` - Enhanced Windows build script (268 lines)
- `build-msvc.bat` - MSVC-specific Windows build script (396 lines)
- `setup.py` - Python package setup script (272 lines) - **KEEPING IN ROOT**
- `conanfile.py` - Conan package configuration (369 lines) - **KEEPING IN ROOT**

### Scripts Already in scripts/ Directory
- `build-and-package.py` - Comprehensive build and packaging system
- `create-distribution.sh` - Distribution creation script
- `create-portable.py` - Portable package creator
- `deploy-test-dlls.ps1` - DLL deployment for tests
- `modular-installer.py` - Modular component installer
- `package-manager.sh` - Package management utilities
- `setup_vcpkg.bat` - vcpkg setup script for Windows
- `setup_vcpkg.ps1` - vcpkg setup script for PowerShell
- `test-memory-wrapper.ps1` - Memory testing wrapper
- `validate-build-system.py` - Build system validator
- `validate-package.py` - Package validator
- `version-manager.sh` - Version management utilities

### Scripts in example/ Directory (Staying in Place)
- `run_tests.sh` - Shell test runner for examples
- `run_tests.bat` - Windows batch test runner for examples
- `run_tests.py` - Python test runner for examples
- `run_tests.ps1` - PowerShell test runner for examples

## Organization Plan

### Scripts to Move to scripts/
1. `build.sh` → `scripts/build.sh`
2. `build.bat` → `scripts/build.bat`
3. `build-msvc.bat` → `scripts/build-msvc.bat`

### Scripts Staying in Root
- `setup.py` - Standard Python package setup location
- `conanfile.py` - Standard Conan package configuration location

### Backward Compatibility
- Create wrapper scripts in root directory that call the moved scripts
- Maintain all original command-line interfaces and functionality

## Rationale for Keeping Some Scripts in Root

- `setup.py`: Standard Python packaging convention requires this in project root
- `conanfile.py`: Standard Conan packaging convention requires this in project root
- These are package configuration files, not utility scripts

## Date: $(Get-Date)
## Performed by: The Augster
