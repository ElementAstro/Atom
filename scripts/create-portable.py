#!/usr/bin/env python3
"""
Atom Library Portable Distribution Creator
Creates self-contained, portable distributions that don't require installation.
"""

import argparse
import os
import sys
import shutil
import subprocess
import platform
import tempfile
import json
from pathlib import Path
from typing import Dict, List, Set, Optional
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class AtomPortableCreator:
    """Creates portable distributions of the Atom library."""

    def __init__(self, source_dir: Path, output_dir: Path):
        self.source_dir = Path(source_dir).resolve()
        self.output_dir = Path(output_dir).resolve()
        self.platform = self._detect_platform()
        self.arch = self._detect_architecture()
        self.build_dir = self.source_dir / "build"

        # Portable distribution structure
        self.portable_structure = {
            'bin': 'Executable files and tools',
            'lib': 'Libraries and shared objects',
            'include': 'Header files',
            'share': 'Data files and documentation',
            'examples': 'Example code and applications',
            'tools': 'Utility scripts and tools'
        }

        # Component mapping for selective packaging
        self.component_files = {
            'algorithm': ['libalgorithm*', 'algorithm/'],
            'async': ['libasync*', 'async/'],
            'components': ['libcomponents*', 'components/'],
            'connection': ['libconnection*', 'connection/'],
            'containers': ['libcontainers*', 'containers/'],
            'error': ['liberror*', 'error/'],
            'image': ['libimage*', 'image/'],
            'io': ['libio*', 'io/'],
            'log': ['liblog*', 'log/'],
            'memory': ['libmemory*', 'memory/'],
            'meta': ['libmeta*', 'meta/'],
            'search': ['libsearch*', 'search/'],
            'secret': ['libsecret*', 'secret/'],
            'serial': ['libserial*', 'serial/'],
            'sysinfo': ['libsysinfo*', 'sysinfo/'],
            'system': ['libsystem*', 'system/'],
            'type': ['libtype*', 'type/'],
            'utils': ['libutils*', 'utils/'],
            'web': ['libweb*', 'web/']
        }

    def _detect_platform(self) -> str:
        """Detect the current platform."""
        system = platform.system().lower()
        if system == 'windows':
            return 'windows'
        elif system == 'darwin':
            return 'macos'
        elif system == 'linux':
            return 'linux'
        else:
            return 'unknown'

    def _detect_architecture(self) -> str:
        """Detect the current architecture."""
        machine = platform.machine().lower()
        if machine in ['x86_64', 'amd64']:
            return 'x64'
        elif machine in ['i386', 'i686']:
            return 'x86'
        elif machine in ['aarch64', 'arm64']:
            return 'arm64'
        else:
            return 'x64'

    def build_project(self, build_type: str = "Release", components: Optional[List[str]] = None):
        """Build the project for portable distribution."""
        logger.info(f"Building project for portable distribution (type: {build_type})")

        # Create build directory
        self.build_dir.mkdir(exist_ok=True)

        # Configure CMake
        cmake_args = [
            'cmake', '-B', str(self.build_dir), '-S', str(self.source_dir),
            f'-DCMAKE_BUILD_TYPE={build_type}',
            '-DATOM_BUILD_EXAMPLES=ON',
            '-DATOM_BUILD_TESTS=OFF',
            '-DATOM_BUILD_PYTHON_BINDINGS=ON',
            '-DBUILD_SHARED_LIBS=OFF',  # Static linking for portability
            '-DCMAKE_INSTALL_PREFIX=/portable',  # Temporary install prefix
        ]

        # Add component-specific options
        if components:
            for component in self.component_files.keys():
                enabled = 'ON' if component in components else 'OFF'
                cmake_args.append(f'-DATOM_BUILD_{component.upper()}={enabled}')

        # Platform-specific configuration
        if self.platform == 'windows':
            cmake_args.extend(['-G', 'Visual Studio 17 2022', '-A', 'x64'])
        else:
            cmake_args.extend(['-G', 'Ninja'])

        # Run CMake configuration
        result = subprocess.run(cmake_args, cwd=self.source_dir, capture_output=True, text=True)
        if result.returncode != 0:
            logger.error(f"CMake configuration failed: {result.stderr}")
            raise RuntimeError("Build configuration failed")

        # Build the project
        build_args = ['cmake', '--build', str(self.build_dir), '--config', build_type, '--parallel']
        result = subprocess.run(build_args, cwd=self.source_dir, capture_output=True, text=True)
        if result.returncode != 0:
            logger.error(f"Build failed: {result.stderr}")
            raise RuntimeError("Build failed")

        logger.info("Project built successfully")

    def create_portable_structure(self, portable_dir: Path):
        """Create the portable distribution directory structure."""
        logger.info(f"Creating portable structure in {portable_dir}")

        # Create main directories
        for dir_name, description in self.portable_structure.items():
            dir_path = portable_dir / dir_name
            dir_path.mkdir(parents=True, exist_ok=True)

            # Create description file
            desc_file = dir_path / 'README.txt'
            with open(desc_file, 'w') as f:
                f.write(f"{dir_name.upper()} Directory\n")
                f.write("=" * (len(dir_name) + 10) + "\n\n")
                f.write(f"{description}\n")

    def copy_build_artifacts(self, portable_dir: Path, components: Optional[List[str]] = None):
        """Copy build artifacts to portable distribution."""
        logger.info("Copying build artifacts")

        # Install to temporary directory first
        temp_install = Path(tempfile.mkdtemp(prefix='atom-install-'))
        try:
            install_args = [
                'cmake', '--install', str(self.build_dir),
                '--prefix', str(temp_install),
                '--config', 'Release'
            ]

            result = subprocess.run(install_args, cwd=self.source_dir, capture_output=True, text=True)
            if result.returncode != 0:
                logger.error(f"Installation failed: {result.stderr}")
                raise RuntimeError("Installation failed")

            # Copy files to portable structure
            self._copy_installed_files(temp_install, portable_dir, components)

        finally:
            # Cleanup temporary install directory
            if temp_install.exists():
                shutil.rmtree(temp_install)

    def _copy_installed_files(self, install_dir: Path, portable_dir: Path, components: Optional[List[str]]):
        """Copy installed files to portable directory."""
        # Copy libraries
        lib_src = install_dir / 'lib'
        lib_dst = portable_dir / 'lib'
        if lib_src.exists():
            self._copy_selective_files(lib_src, lib_dst, components, 'lib')

        # Copy headers
        include_src = install_dir / 'include'
        include_dst = portable_dir / 'include'
        if include_src.exists():
            self._copy_selective_files(include_src, include_dst, components, 'include')

        # Copy binaries
        bin_src = install_dir / 'bin'
        bin_dst = portable_dir / 'bin'
        if bin_src.exists():
            shutil.copytree(bin_src, bin_dst, dirs_exist_ok=True)

        # Copy share files
        share_src = install_dir / 'share'
        share_dst = portable_dir / 'share'
        if share_src.exists():
            shutil.copytree(share_src, share_dst, dirs_exist_ok=True)

    def _copy_selective_files(self, src_dir: Path, dst_dir: Path, components: Optional[List[str]], file_type: str):
        """Copy files selectively based on components."""
        dst_dir.mkdir(parents=True, exist_ok=True)

        if not components:
            # Copy everything if no specific components requested
            shutil.copytree(src_dir, dst_dir, dirs_exist_ok=True)
            return

        # Copy component-specific files
        for component in components:
            if component not in self.component_files:
                continue

            patterns = self.component_files[component]
            for pattern in patterns:
                if file_type == 'lib':
                    # Handle library files
                    for lib_file in src_dir.glob(pattern):
                        if lib_file.is_file():
                            shutil.copy2(lib_file, dst_dir)
                elif file_type == 'include':
                    # Handle header directories
                    if '/' in pattern:
                        header_dir = src_dir / 'atom' / pattern
                        if header_dir.exists():
                            dst_component_dir = dst_dir / 'atom' / pattern
                            shutil.copytree(header_dir, dst_component_dir, dirs_exist_ok=True)

    def copy_dependencies(self, portable_dir: Path):
        """Copy system dependencies for portability."""
        logger.info("Copying system dependencies")

        if self.platform == 'linux':
            self._copy_linux_dependencies(portable_dir)
        elif self.platform == 'windows':
            self._copy_windows_dependencies(portable_dir)
        elif self.platform == 'macos':
            self._copy_macos_dependencies(portable_dir)

    def _copy_linux_dependencies(self, portable_dir: Path):
        """Copy Linux system dependencies."""
        lib_dir = portable_dir / 'lib'

        # Find and copy shared library dependencies
        for lib_file in lib_dir.glob('*.so*'):
            try:
                # Use ldd to find dependencies
                result = subprocess.run(['ldd', str(lib_file)], capture_output=True, text=True)
                if result.returncode == 0:
                    for line in result.stdout.split('\n'):
                        if '=>' in line and '/lib' in line:
                            parts = line.strip().split('=>')
                            if len(parts) == 2:
                                dep_path = parts[1].strip().split()[0]
                                if Path(dep_path).exists():
                                    dep_name = Path(dep_path).name
                                    if not (lib_dir / dep_name).exists():
                                        shutil.copy2(dep_path, lib_dir)
            except Exception as e:
                logger.warning(f"Failed to process dependencies for {lib_file}: {e}")

    def _copy_windows_dependencies(self, portable_dir: Path):
        """Copy Windows system dependencies."""
        bin_dir = portable_dir / 'bin'

        # Common Windows runtime libraries
        common_dlls = [
            'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll',
            'api-ms-win-crt-runtime-l1-1-0.dll'
        ]

        # Search for DLLs in system directories
        system_dirs = [
            Path(os.environ.get('WINDIR', 'C:\\Windows')) / 'System32',
            Path(os.environ.get('WINDIR', 'C:\\Windows')) / 'SysWOW64'
        ]

        for dll_name in common_dlls:
            for system_dir in system_dirs:
                dll_path = system_dir / dll_name
                if dll_path.exists():
                    shutil.copy2(dll_path, bin_dir)
                    break

    def _copy_macos_dependencies(self, portable_dir: Path):
        """Copy macOS system dependencies."""
        lib_dir = portable_dir / 'lib'

        # Use otool to find dependencies
        for lib_file in lib_dir.glob('*.dylib'):
            try:
                result = subprocess.run(['otool', '-L', str(lib_file)], capture_output=True, text=True)
                if result.returncode == 0:
                    for line in result.stdout.split('\n')[1:]:  # Skip first line
                        if line.strip() and '/usr/local' in line:
                            dep_path = line.strip().split()[0]
                            if Path(dep_path).exists():
                                dep_name = Path(dep_path).name
                                if not (lib_dir / dep_name).exists():
                                    shutil.copy2(dep_path, lib_dir)
            except Exception as e:
                logger.warning(f"Failed to process dependencies for {lib_file}: {e}")

    def create_launcher_scripts(self, portable_dir: Path):
        """Create launcher scripts for the portable distribution."""
        logger.info("Creating launcher scripts")

        tools_dir = portable_dir / 'tools'
        tools_dir.mkdir(exist_ok=True)

        if self.platform == 'windows':
            self._create_windows_launchers(tools_dir, portable_dir)
        else:
            self._create_unix_launchers(tools_dir, portable_dir)

    def _create_windows_launchers(self, tools_dir: Path, portable_dir: Path):
        """Create Windows launcher scripts."""
        # Environment setup script
        env_script = tools_dir / 'setup-env.bat'
        with open(env_script, 'w') as f:
            f.write('@echo off\n')
            f.write('set ATOM_PORTABLE_ROOT=%~dp0..\n')
            f.write('set PATH=%ATOM_PORTABLE_ROOT%\\bin;%PATH%\n')
            f.write('set CMAKE_PREFIX_PATH=%ATOM_PORTABLE_ROOT%;%CMAKE_PREFIX_PATH%\n')
            f.write('echo Atom portable environment configured\n')
            f.write('echo ATOM_PORTABLE_ROOT=%ATOM_PORTABLE_ROOT%\n')

        # Development environment launcher
        dev_script = tools_dir / 'dev-env.bat'
        with open(dev_script, 'w') as f:
            f.write('@echo off\n')
            f.write('call "%~dp0setup-env.bat"\n')
            f.write('echo Starting development environment...\n')
            f.write('cmd /k\n')

    def _create_unix_launchers(self, tools_dir: Path, portable_dir: Path):
        """Create Unix launcher scripts."""
        # Environment setup script
        env_script = tools_dir / 'setup-env.sh'
        with open(env_script, 'w') as f:
            f.write('#!/bin/bash\n')
            f.write('export ATOM_PORTABLE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"\n')
            f.write('export PATH="$ATOM_PORTABLE_ROOT/bin:$PATH"\n')
            f.write('export LD_LIBRARY_PATH="$ATOM_PORTABLE_ROOT/lib:$LD_LIBRARY_PATH"\n')
            f.write('export CMAKE_PREFIX_PATH="$ATOM_PORTABLE_ROOT:$CMAKE_PREFIX_PATH"\n')
            f.write('echo "Atom portable environment configured"\n')
            f.write('echo "ATOM_PORTABLE_ROOT=$ATOM_PORTABLE_ROOT"\n')

        # Make executable
        env_script.chmod(0o755)

        # Development environment launcher
        dev_script = tools_dir / 'dev-env.sh'
        with open(dev_script, 'w') as f:
            f.write('#!/bin/bash\n')
            f.write('source "$(dirname "${BASH_SOURCE[0]}")/setup-env.sh"\n')
            f.write('echo "Starting development environment..."\n')
            f.write('exec "$SHELL"\n')

        # Make executable
        dev_script.chmod(0o755)

    def create_documentation(self, portable_dir: Path):
        """Create documentation for the portable distribution."""
        logger.info("Creating documentation")

        # Create main README
        readme_file = portable_dir / 'README.md'
        with open(readme_file, 'w') as f:
            f.write(f"# Atom Library Portable Distribution\n\n")
            f.write(f"Version: {self._get_version()}\n")
            f.write(f"Platform: {self.platform}-{self.arch}\n")
            f.write(f"Build Date: {self._get_build_date()}\n\n")
            f.write("## Quick Start\n\n")

            if self.platform == 'windows':
                f.write("1. Run `tools\\setup-env.bat` to configure environment\n")
                f.write("2. Use `tools\\dev-env.bat` for development shell\n")
            else:
                f.write("1. Run `source tools/setup-env.sh` to configure environment\n")
                f.write("2. Use `tools/dev-env.sh` for development shell\n")

            f.write("\n## Directory Structure\n\n")
            for dir_name, description in self.portable_structure.items():
                f.write(f"- `{dir_name}/`: {description}\n")

            f.write("\n## Usage\n\n")
            f.write("### CMake Integration\n\n")
            f.write("```cmake\n")
            f.write("find_package(atom REQUIRED)\n")
            f.write("target_link_libraries(your_target PRIVATE atom::atom)\n")
            f.write("```\n\n")
            f.write("### Environment Variables\n\n")
            f.write("- `ATOM_PORTABLE_ROOT`: Root directory of portable installation\n")
            f.write("- `CMAKE_PREFIX_PATH`: Includes portable root for CMake discovery\n")
            f.write("- `PATH`: Includes portable bin directory\n")
            if self.platform != 'windows':
                f.write("- `LD_LIBRARY_PATH`: Includes portable lib directory\n")

    def _get_version(self) -> str:
        """Get project version."""
        try:
            # Try to get version from git
            result = subprocess.run(
                ['git', 'describe', '--tags', '--always', '--dirty'],
                cwd=self.source_dir, capture_output=True, text=True
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except:
            pass
        return "1.0.0"

    def _get_build_date(self) -> str:
        """Get current build date."""
        from datetime import datetime
        return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    def create_portable_package(self, components: Optional[List[str]] = None,
                              build_type: str = "Release") -> Path:
        """Create a complete portable package."""
        logger.info("Creating portable package")

        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # Create portable distribution directory
        package_name = f"atom-{self._get_version()}-{self.platform}-{self.arch}-portable"
        portable_dir = self.output_dir / package_name

        if portable_dir.exists():
            shutil.rmtree(portable_dir)

        try:
            # Build the project
            self.build_project(build_type, components)

            # Create portable structure
            self.create_portable_structure(portable_dir)

            # Copy build artifacts
            self.copy_build_artifacts(portable_dir, components)

            # Copy dependencies
            self.copy_dependencies(portable_dir)

            # Create launcher scripts
            self.create_launcher_scripts(portable_dir)

            # Create documentation
            self.create_documentation(portable_dir)

            # Create archive
            archive_path = self._create_archive(portable_dir)

            logger.info(f"Portable package created: {archive_path}")
            return archive_path

        except Exception as e:
            logger.error(f"Failed to create portable package: {e}")
            if portable_dir.exists():
                shutil.rmtree(portable_dir)
            raise

    def _create_archive(self, portable_dir: Path) -> Path:
        """Create archive of portable distribution."""
        if self.platform == 'windows':
            archive_path = portable_dir.with_suffix('.zip')
            shutil.make_archive(str(portable_dir), 'zip', portable_dir.parent, portable_dir.name)
        else:
            archive_path = portable_dir.with_suffix('.tar.gz')
            shutil.make_archive(str(portable_dir), 'gztar', portable_dir.parent, portable_dir.name)

        return archive_path


def main():
    parser = argparse.ArgumentParser(description='Create portable Atom library distribution')
    parser.add_argument('--source', type=Path, default=Path.cwd(),
                       help='Source directory (default: current directory)')
    parser.add_argument('--output', type=Path, default=Path.cwd() / 'dist',
                       help='Output directory (default: ./dist)')
    parser.add_argument('--components', nargs='*',
                       help='Specific components to include (default: all)')
    parser.add_argument('--build-type', choices=['Debug', 'Release'], default='Release',
                       help='Build type (default: Release)')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Enable verbose logging')

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    try:
        creator = AtomPortableCreator(args.source, args.output)
        archive_path = creator.create_portable_package(args.components, args.build_type)
        print(f"Portable package created: {archive_path}")

    except Exception as e:
        logger.error(f"Failed to create portable package: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
