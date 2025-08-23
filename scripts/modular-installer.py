#!/usr/bin/env python3
"""
Atom Library Modular Installer
Provides intelligent modular installation with dependency resolution and conflict detection.
"""

import argparse
import json
import os
import sys
import subprocess
import platform
import shutil
import tempfile
import urllib.request
import hashlib
from pathlib import Path
from typing import Dict, List, Set, Optional, Tuple
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class AtomModularInstaller:
    """Advanced modular installer for Atom library components."""

    def __init__(self):
        self.platform = self._detect_platform()
        self.arch = self._detect_architecture()
        self.base_url = "https://github.com/ElementAstro/Atom/releases/latest/download"
        self.install_prefix = self._get_default_install_prefix()
        self.component_registry = {}
        self.installed_components = set()

        # Component dependency mapping
        self.dependencies = {
            'algorithm': ['error'],
            'async': ['error', 'log'],
            'components': ['error', 'log', 'type'],
            'connection': ['error', 'log', 'async'],
            'containers': ['error', 'type'],
            'error': [],
            'image': ['error', 'log', 'io'],
            'io': ['error', 'log'],
            'log': ['error'],
            'memory': ['error'],
            'meta': ['error', 'type'],
            'search': ['error', 'algorithm'],
            'secret': ['error', 'log'],
            'serial': ['error', 'log', 'io'],
            'sysinfo': ['error', 'log'],
            'system': ['error', 'log', 'sysinfo'],
            'type': ['error'],
            'utils': ['error', 'log', 'type'],
            'web': ['error', 'log', 'async', 'connection']
        }

        # Meta-packages for common use cases
        self.meta_packages = {
            'core': ['error', 'log', 'type', 'utils'],
            'networking': ['connection', 'web', 'async'],
            'imaging': ['image', 'io', 'algorithm'],
            'system': ['sysinfo', 'system', 'serial'],
            'full': list(self.dependencies.keys())
        }

        self._load_installed_components()

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
            raise RuntimeError(f"Unsupported platform: {system}")

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
            return 'x64'  # Default fallback

    def _get_default_install_prefix(self) -> Path:
        """Get the default installation prefix for the platform."""
        if self.platform == 'windows':
            return Path(os.environ.get('PROGRAMFILES', 'C:\\Program Files')) / 'Atom'
        else:
            return Path('/usr/local')

    def _load_installed_components(self):
        """Load information about currently installed components."""
        registry_file = self.install_prefix / 'share' / 'atom' / 'installed_components.json'
        if registry_file.exists():
            try:
                with open(registry_file, 'r') as f:
                    data = json.load(f)
                    self.component_registry = data.get('components', {})
                    self.installed_components = set(data.get('installed', []))
            except Exception as e:
                logger.warning(f"Failed to load component registry: {e}")

    def _save_installed_components(self):
        """Save information about installed components."""
        registry_dir = self.install_prefix / 'share' / 'atom'
        registry_dir.mkdir(parents=True, exist_ok=True)

        registry_file = registry_dir / 'installed_components.json'
        data = {
            'components': self.component_registry,
            'installed': list(self.installed_components),
            'platform': self.platform,
            'architecture': self.arch,
            'install_prefix': str(self.install_prefix)
        }

        with open(registry_file, 'w') as f:
            json.dump(data, f, indent=2)

    def resolve_dependencies(self, components: List[str]) -> List[str]:
        """Resolve component dependencies recursively."""
        resolved = set()
        to_process = set(components)

        while to_process:
            component = to_process.pop()
            if component in resolved:
                continue

            if component not in self.dependencies:
                if component in self.meta_packages:
                    # Expand meta-package
                    to_process.update(self.meta_packages[component])
                    continue
                else:
                    raise ValueError(f"Unknown component: {component}")

            resolved.add(component)
            to_process.update(self.dependencies[component])

        # Return in dependency order
        ordered = []
        for component in self.dependencies.keys():
            if component in resolved:
                ordered.append(component)

        return ordered

    def check_conflicts(self, components: List[str]) -> List[str]:
        """Check for potential conflicts with installed components."""
        conflicts = []
        for component in components:
            if component in self.installed_components:
                installed_version = self.component_registry.get(component, {}).get('version', 'unknown')
                conflicts.append(f"{component} (installed: {installed_version})")
        return conflicts

    def download_component(self, component: str, version: str = "latest") -> Path:
        """Download a component package."""
        package_name = f"atom-{component}-{version}-{self.platform}-{self.arch}.tar.gz"
        if self.platform == 'windows':
            package_name = package_name.replace('.tar.gz', '.zip')

        url = f"{self.base_url}/{package_name}"

        # Create temporary download directory
        download_dir = Path(tempfile.mkdtemp(prefix='atom-download-'))
        package_path = download_dir / package_name

        logger.info(f"Downloading {component} from {url}")

        try:
            urllib.request.urlretrieve(url, package_path)
            return package_path
        except Exception as e:
            logger.error(f"Failed to download {component}: {e}")
            raise

    def extract_package(self, package_path: Path, extract_dir: Path):
        """Extract a component package."""
        logger.info(f"Extracting {package_path} to {extract_dir}")

        if package_path.suffix == '.zip':
            shutil.unpack_archive(package_path, extract_dir, 'zip')
        else:
            shutil.unpack_archive(package_path, extract_dir, 'gztar')

    def install_component(self, component: str, version: str = "latest", force: bool = False):
        """Install a single component."""
        if component in self.installed_components and not force:
            logger.info(f"Component {component} is already installed")
            return

        logger.info(f"Installing component: {component}")

        # Download component package
        package_path = self.download_component(component, version)

        try:
            # Extract to temporary directory
            with tempfile.TemporaryDirectory(prefix='atom-extract-') as extract_dir:
                self.extract_package(package_path, Path(extract_dir))

                # Find extracted content
                extracted_content = list(Path(extract_dir).iterdir())
                if len(extracted_content) == 1 and extracted_content[0].is_dir():
                    source_dir = extracted_content[0]
                else:
                    source_dir = Path(extract_dir)

                # Copy files to installation directory
                self._copy_component_files(source_dir, component)

                # Update registry
                self.component_registry[component] = {
                    'version': version,
                    'install_date': str(Path().stat().st_mtime),
                    'files': self._get_component_files(component)
                }
                self.installed_components.add(component)

                logger.info(f"Successfully installed {component}")

        finally:
            # Cleanup download
            if package_path.exists():
                package_path.unlink()
            if package_path.parent.exists():
                shutil.rmtree(package_path.parent)

    def _copy_component_files(self, source_dir: Path, component: str):
        """Copy component files to installation directory."""
        # Copy headers
        include_src = source_dir / 'include' / 'atom' / component
        include_dst = self.install_prefix / 'include' / 'atom' / component
        if include_src.exists():
            include_dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(include_src, include_dst, dirs_exist_ok=True)

        # Copy libraries
        lib_src = source_dir / 'lib'
        lib_dst = self.install_prefix / 'lib'
        if lib_src.exists():
            lib_dst.mkdir(parents=True, exist_ok=True)
            for lib_file in lib_src.glob(f'*atom*{component}*'):
                shutil.copy2(lib_file, lib_dst)

        # Copy CMake config files
        cmake_src = source_dir / 'lib' / 'cmake' / f'atom-{component}'
        cmake_dst = self.install_prefix / 'lib' / 'cmake' / f'atom-{component}'
        if cmake_src.exists():
            cmake_dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(cmake_src, cmake_dst, dirs_exist_ok=True)

    def _get_component_files(self, component: str) -> List[str]:
        """Get list of files installed for a component."""
        files = []

        # Headers
        include_dir = self.install_prefix / 'include' / 'atom' / component
        if include_dir.exists():
            for file_path in include_dir.rglob('*'):
                if file_path.is_file():
                    files.append(str(file_path.relative_to(self.install_prefix)))

        # Libraries
        lib_dir = self.install_prefix / 'lib'
        if lib_dir.exists():
            for lib_file in lib_dir.glob(f'*atom*{component}*'):
                files.append(str(lib_file.relative_to(self.install_prefix)))

        return files

    def uninstall_component(self, component: str, force: bool = False):
        """Uninstall a component."""
        if component not in self.installed_components:
            logger.warning(f"Component {component} is not installed")
            return

        # Check for dependents
        dependents = []
        for installed_comp in self.installed_components:
            if component in self.dependencies.get(installed_comp, []):
                dependents.append(installed_comp)

        if dependents and not force:
            logger.error(f"Cannot uninstall {component}: required by {', '.join(dependents)}")
            logger.info("Use --force to override dependency checks")
            return

        logger.info(f"Uninstalling component: {component}")

        # Remove files
        if component in self.component_registry:
            for file_path in self.component_registry[component].get('files', []):
                full_path = self.install_prefix / file_path
                if full_path.exists():
                    full_path.unlink()

        # Remove from registry
        self.component_registry.pop(component, None)
        self.installed_components.discard(component)

        logger.info(f"Successfully uninstalled {component}")

    def list_components(self, available: bool = False):
        """List installed or available components."""
        if available:
            print("Available components:")
            for component, deps in self.dependencies.items():
                print(f"  {component}: {', '.join(deps) if deps else 'no dependencies'}")

            print("\nMeta-packages:")
            for meta, components in self.meta_packages.items():
                print(f"  {meta}: {', '.join(components)}")
        else:
            print("Installed components:")
            for component in sorted(self.installed_components):
                version = self.component_registry.get(component, {}).get('version', 'unknown')
                print(f"  {component} ({version})")

    def install_components(self, components: List[str], force: bool = False):
        """Install multiple components with dependency resolution."""
        # Resolve dependencies
        resolved_components = self.resolve_dependencies(components)

        # Check conflicts
        conflicts = self.check_conflicts(resolved_components)
        if conflicts and not force:
            logger.error(f"Conflicts detected: {', '.join(conflicts)}")
            logger.info("Use --force to override conflicts")
            return

        logger.info(f"Installing components: {', '.join(resolved_components)}")

        # Install each component
        for component in resolved_components:
            try:
                self.install_component(component, force=force)
            except Exception as e:
                logger.error(f"Failed to install {component}: {e}")
                return

        # Save registry
        self._save_installed_components()
        logger.info("Installation completed successfully")


def main():
    parser = argparse.ArgumentParser(description='Atom Library Modular Installer')
    parser.add_argument('--prefix', type=Path, help='Installation prefix')
    parser.add_argument('--force', action='store_true', help='Force installation/uninstallation')

    subparsers = parser.add_subparsers(dest='command', help='Available commands')

    # Install command
    install_parser = subparsers.add_parser('install', help='Install components')
    install_parser.add_argument('components', nargs='+', help='Components to install')
    install_parser.add_argument('--version', default='latest', help='Version to install')

    # Uninstall command
    uninstall_parser = subparsers.add_parser('uninstall', help='Uninstall components')
    uninstall_parser.add_argument('components', nargs='+', help='Components to uninstall')

    # List command
    list_parser = subparsers.add_parser('list', help='List components')
    list_parser.add_argument('--available', action='store_true', help='List available components')

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return

    installer = AtomModularInstaller()

    if args.prefix:
        installer.install_prefix = args.prefix

    try:
        if args.command == 'install':
            installer.install_components(args.components, force=args.force)
        elif args.command == 'uninstall':
            for component in args.components:
                installer.uninstall_component(component, force=args.force)
            installer._save_installed_components()
        elif args.command == 'list':
            installer.list_components(available=args.available)

    except Exception as e:
        logger.error(f"Command failed: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
