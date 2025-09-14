#!/usr/bin/env python3
"""
Atom Library Comprehensive Build and Package System
Automates building, testing, and packaging for multiple platforms and distribution channels.
"""

import argparse
import os
import sys
import subprocess
import platform
import shutil
import json
import tempfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any
import logging
import concurrent.futures
from datetime import datetime

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class AtomBuildSystem:
    """Comprehensive build and packaging system for Atom library."""

    def __init__(self, source_dir: Path, output_dir: Path):
        self.source_dir = Path(source_dir).resolve()
        self.output_dir = Path(output_dir).resolve()
        self.platform = self._detect_platform()
        self.arch = self._detect_architecture()
        self.build_dir = self.source_dir / "build"
        self.dist_dir = self.output_dir / "dist"

        # Build configurations
        self.build_configs = {
            'debug': {
                'cmake_build_type': 'Debug',
                'optimization': False,
                'debug_symbols': True
            },
            'release': {
                'cmake_build_type': 'Release',
                'optimization': True,
                'debug_symbols': False
            },
            'relwithdebinfo': {
                'cmake_build_type': 'RelWithDebInfo',
                'optimization': True,
                'debug_symbols': True
            }
        }

        # Package formats by platform
        self.package_formats = {
            'linux': ['tar.gz', 'deb', 'rpm', 'appimage'],
            'windows': ['zip', 'msi', 'nsis'],
            'macos': ['tar.gz', 'dmg', 'pkg']
        }

        # Distribution channels
        self.distribution_channels = [
            'github-releases',
            'pypi',
            'vcpkg',
            'conan',
            'homebrew',
            'apt',
            'docker'
        ]

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

    def setup_build_environment(self):
        """Setup the build environment."""
        logger.info("Setting up build environment")

        # Create directories
        self.build_dir.mkdir(exist_ok=True)
        self.dist_dir.mkdir(parents=True, exist_ok=True)

        # Install system dependencies
        self._install_system_dependencies()

        # Setup vcpkg if needed
        self._setup_vcpkg()

    def _install_system_dependencies(self):
        """Install system dependencies."""
        logger.info("Installing system dependencies")

        script_path = self.source_dir / "scripts" / "package-manager.sh"
        if script_path.exists():
            try:
                subprocess.run([str(script_path), "install-deps"], check=True)
            except subprocess.CalledProcessError as e:
                logger.warning(f"Failed to install system dependencies: {e}")

    def _setup_vcpkg(self):
        """Setup vcpkg package manager."""
        logger.info("Setting up vcpkg")

        script_path = self.source_dir / "scripts" / "package-manager.sh"
        if script_path.exists():
            try:
                subprocess.run([str(script_path), "setup-vcpkg"], check=True)
            except subprocess.CalledProcessError as e:
                logger.warning(f"Failed to setup vcpkg: {e}")

    def configure_build(self, build_type: str = "release", components: Optional[List[str]] = None,
                       features: Optional[Dict[str, bool]] = None):
        """Configure the build system."""
        logger.info(f"Configuring build (type: {build_type})")

        if build_type not in self.build_configs:
            raise ValueError(f"Unknown build type: {build_type}")

        config = self.build_configs[build_type]

        # Base CMake arguments
        cmake_args = [
            'cmake', '-B', str(self.build_dir), '-S', str(self.source_dir),
            f'-DCMAKE_BUILD_TYPE={str(config["cmake_build_type"])}',
            '-DATOM_BUILD_EXAMPLES=ON',
            '-DATOM_BUILD_TESTS=ON',
            '-DATOM_BUILD_PYTHON_BINDINGS=ON',
            '-DATOM_BUILD_DOCS=ON',
            '-DATOM_INSTALL_MODULAR=ON',
            '-DATOM_INSTALL_COMPONENT_PACKAGES=ON',
        ]

        # Component-specific options
        if components:
            all_components = [
                'algorithm', 'async', 'components', 'connection', 'containers',
                'error', 'image', 'io', 'log', 'memory', 'meta', 'search',
                'secret', 'serial', 'sysinfo', 'system', 'type', 'utils', 'web'
            ]

            for component in all_components:
                enabled = 'ON' if component in components else 'OFF'
                cmake_args.append(f'-DATOM_BUILD_{component.upper()}={enabled}')

        # Feature options
        if features:
            for feature, feature_enabled in features.items():
                cmake_value: str = 'ON' if feature_enabled else 'OFF'
                cmake_args.append(f'-DATOM_USE_{feature.upper()}={cmake_value}')

        # Platform-specific configuration
        if self.platform == 'windows':
            cmake_args.extend(['-G', 'Visual Studio 17 2022', '-A', 'x64'])
        else:
            cmake_args.extend(['-G', 'Ninja'])

        # vcpkg integration
        vcpkg_dir = self.source_dir / "vcpkg"
        if vcpkg_dir.exists():
            toolchain_file = vcpkg_dir / "scripts" / "buildsystems" / "vcpkg.cmake"
            if toolchain_file.exists():
                cmake_args.append(f'-DCMAKE_TOOLCHAIN_FILE={toolchain_file}')

        # Run CMake configuration
        result = subprocess.run(cmake_args, cwd=self.source_dir, capture_output=True, text=True)
        if result.returncode != 0:
            logger.error(f"CMake configuration failed: {result.stderr}")
            raise RuntimeError("Build configuration failed")

        logger.info("Build configured successfully")

    def build_project(self, build_type: str = "release", parallel: bool = True):
        """Build the project."""
        logger.info(f"Building project (type: {build_type})")

        config = self.build_configs[build_type]

        # Build arguments
        build_args: list[str] = [
            'cmake', '--build', str(self.build_dir),
            '--config', str(config['cmake_build_type'])
        ]

        if parallel:
            build_args.append('--parallel')

        # Run build
        result = subprocess.run(build_args, cwd=self.source_dir, capture_output=True, text=True)
        if result.returncode != 0:
            logger.error(f"Build failed: {result.stderr}")
            raise RuntimeError("Build failed")

        logger.info("Project built successfully")

    def run_tests(self):
        """Run the test suite."""
        logger.info("Running tests")

        # Run CTest
        test_args = ['ctest', '--output-on-failure', '--parallel']
        result = subprocess.run(test_args, cwd=self.build_dir, capture_output=True, text=True)

        if result.returncode != 0:
            logger.error(f"Tests failed: {result.stderr}")
            raise RuntimeError("Tests failed")

        logger.info("All tests passed")

    def create_packages(self, formats: Optional[List[str]] = None):
        """Create distribution packages."""
        logger.info("Creating distribution packages")

        if not formats:
            formats = self.package_formats.get(self.platform, ['tar.gz'])

        # Create source distribution
        self._create_source_package()

        # Create binary packages
        for package_format in formats:
            try:
                self._create_package(package_format)
            except Exception as e:
                logger.error(f"Failed to create {package_format} package: {e}")

    def _create_source_package(self):
        """Create source distribution package."""
        logger.info("Creating source package")

        script_path = self.source_dir / "scripts" / "create-distribution.sh"
        if script_path.exists():
            try:
                subprocess.run([str(script_path)], cwd=self.source_dir, check=True)
            except subprocess.CalledProcessError as e:
                logger.error(f"Failed to create source package: {e}")

    def _create_package(self, package_format: str):
        """Create a specific package format."""
        logger.info(f"Creating {package_format} package")

        if package_format in ['deb', 'rpm']:
            # Use package manager script
            script_path = self.source_dir / "scripts" / "package-manager.sh"
            if script_path.exists():
                subprocess.run([str(script_path), f"create-{package_format}"], check=True)

        elif package_format in ['tar.gz', 'zip']:
            # Use CPack
            cpack_args = ['cpack', '-G', 'TGZ' if package_format == 'tar.gz' else 'ZIP']
            subprocess.run(cpack_args, cwd=self.build_dir, check=True)

        elif package_format == 'docker':
            # Create Docker images
            script_path = self.source_dir / "scripts" / "package-manager.sh"
            if script_path.exists():
                subprocess.run([str(script_path), "create-docker"], check=True)

    def create_portable_distribution(self, components: Optional[List[str]] = None):
        """Create portable distribution."""
        logger.info("Creating portable distribution")

        script_path = self.source_dir / "scripts" / "create-portable.py"
        if script_path.exists():
            cmd = [sys.executable, str(script_path), '--source', str(self.source_dir),
                   '--output', str(self.dist_dir)]

            if components:
                cmd.extend(['--components'] + components)

            try:
                subprocess.run(cmd, check=True)
            except subprocess.CalledProcessError as e:
                logger.error(f"Failed to create portable distribution: {e}")

    def publish_packages(self, channels: Optional[List[str]] = None, dry_run: bool = True):
        """Publish packages to distribution channels."""
        if dry_run:
            logger.info("Publishing packages (dry run)")
        else:
            logger.info("Publishing packages")

        if not channels:
            channels = ['github-releases']

        for channel in channels:
            try:
                self._publish_to_channel(channel, dry_run)
            except Exception as e:
                logger.error(f"Failed to publish to {channel}: {e}")

    def _publish_to_channel(self, channel: str, dry_run: bool):
        """Publish to a specific distribution channel."""
        logger.info(f"Publishing to {channel}")

        if channel == 'github-releases':
            self._publish_github_release(dry_run)
        elif channel == 'pypi':
            self._publish_pypi(dry_run)
        elif channel == 'docker':
            self._publish_docker(dry_run)
        else:
            logger.warning(f"Publishing to {channel} not implemented")

    def _publish_github_release(self, dry_run: bool):
        """Publish to GitHub releases."""
        if dry_run:
            logger.info("Would publish to GitHub releases")
            return

        # Implementation would use GitHub API or gh CLI
        logger.info("Publishing to GitHub releases")

    def _publish_pypi(self, dry_run: bool):
        """Publish Python packages to PyPI."""
        if dry_run:
            logger.info("Would publish to PyPI")
            return

        # Build Python wheel first
        subprocess.run([sys.executable, 'setup.py', 'bdist_wheel'],
                      cwd=self.source_dir, check=True)

        # Upload with twine
        subprocess.run(['twine', 'upload', 'dist/*.whl'],
                      cwd=self.source_dir, check=True)

    def _publish_docker(self, dry_run: bool):
        """Publish Docker images."""
        if dry_run:
            logger.info("Would publish Docker images")
            return

        logger.info("Publishing Docker images")

    def generate_build_report(self) -> Dict:
        """Generate a comprehensive build report."""
        report: Dict[str, Any] = {
            'build_info': {
                'timestamp': datetime.now().isoformat(),
                'platform': self.platform,
                'architecture': self.arch,
                'source_dir': str(self.source_dir),
                'build_dir': str(self.build_dir),
                'output_dir': str(self.output_dir)
            },
            'packages': [],
            'artifacts': []
        }

        # Find created packages
        if self.dist_dir.exists():
            for package_file in self.dist_dir.rglob('*'):
                if package_file.is_file():
                    report['packages'].append({
                        'name': package_file.name,
                        'path': str(package_file),
                        'size': package_file.stat().st_size,
                        'type': package_file.suffix
                    })

        return report

    def full_build_and_package(self, build_type: str = "release",
                              components: Optional[List[str]] = None,
                              features: Optional[Dict[str, bool]] = None,
                              package_formats: Optional[List[str]] = None,
                              create_portable: bool = True,
                              run_tests: bool = True) -> Dict:
        """Perform a complete build and packaging workflow."""
        logger.info("Starting full build and package workflow")

        try:
            # Setup environment
            self.setup_build_environment()

            # Configure build
            self.configure_build(build_type, components, features)

            # Build project
            self.build_project(build_type)

            # Run tests
            if run_tests:
                self.run_tests()

            # Create packages
            self.create_packages(package_formats)

            # Create portable distribution
            if create_portable:
                self.create_portable_distribution(components)

            # Generate report
            report = self.generate_build_report()

            logger.info("Build and packaging completed successfully")
            return report

        except Exception as e:
            logger.error(f"Build and packaging failed: {e}")
            raise


def main():
    parser = argparse.ArgumentParser(description='Atom Library Build and Package System')
    parser.add_argument('--source', type=Path, default=Path.cwd(),
                       help='Source directory (default: current directory)')
    parser.add_argument('--output', type=Path, default=Path.cwd() / 'dist',
                       help='Output directory (default: ./dist)')
    parser.add_argument('--build-type', choices=['debug', 'release', 'relwithdebinfo'],
                       default='release', help='Build type (default: release)')
    parser.add_argument('--components', nargs='*',
                       help='Specific components to build (default: all)')
    parser.add_argument('--package-formats', nargs='*',
                       help='Package formats to create')
    parser.add_argument('--no-tests', action='store_true',
                       help='Skip running tests')
    parser.add_argument('--no-portable', action='store_true',
                       help='Skip creating portable distribution')
    parser.add_argument('--publish', nargs='*',
                       help='Publish to distribution channels')
    parser.add_argument('--dry-run', action='store_true',
                       help='Dry run for publishing')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Enable verbose logging')

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    try:
        build_system = AtomBuildSystem(args.source, args.output)

        # Perform full build and package
        report = build_system.full_build_and_package(
            build_type=args.build_type,
            components=args.components,
            package_formats=args.package_formats,
            create_portable=not args.no_portable,
            run_tests=not args.no_tests
        )

        # Publish if requested
        if args.publish:
            build_system.publish_packages(args.publish, args.dry_run)

        # Print report
        print("\nBuild Report:")
        print(json.dumps(report, indent=2))

    except Exception as e:
        logger.error(f"Build system failed: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
