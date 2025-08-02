#!/usr/bin/env python3
"""
Comprehensive installation script for Atom project
Handles cross-platform dependency installation and build optimization
Author: Max Qian
"""

import os
import sys
import subprocess
import platform
import shutil
import argparse
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import json

# Rich imports for better output
try:
    from rich.console import Console
    from rich.panel import Panel
    from rich.table import Table
    from rich.progress import Progress, SpinnerColumn, TextColumn
    from rich.text import Text
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False
    print("Rich not available, using basic output")

class PlatformDetector:
    """Detect platform and system capabilities"""

    def __init__(self):
        self.system = platform.system().lower()
        self.machine = platform.machine().lower()
        self.python_version = platform.python_version()

    @property
    def is_windows(self) -> bool:
        return self.system == 'windows'

    @property
    def is_linux(self) -> bool:
        return self.system == 'linux'

    @property
    def is_macos(self) -> bool:
        return self.system == 'darwin'

    @property
    def is_wsl(self) -> bool:
        """Detect if running in Windows Subsystem for Linux"""
        if not self.is_linux:
            return False
        try:
            with open('/proc/version', 'r') as f:
                return 'microsoft' in f.read().lower()
        except:
            return False

    @property
    def architecture(self) -> str:
        """Get normalized architecture name"""
        if self.machine in ['x86_64', 'amd64']:
            return 'x64'
        elif self.machine in ['i386', 'i686']:
            return 'x86'
        elif self.machine in ['aarch64', 'arm64']:
            return 'arm64'
        elif self.machine.startswith('arm'):
            return 'arm'
        return self.machine

    def get_package_manager(self) -> Optional[str]:
        """Detect available package manager"""
        if self.is_windows:
            if shutil.which('choco'):
                return 'choco'
            elif shutil.which('winget'):
                return 'winget'
            elif shutil.which('scoop'):
                return 'scoop'
        elif self.is_macos:
            if shutil.which('brew'):
                return 'brew'
            elif shutil.which('port'):
                return 'port'
        elif self.is_linux:
            if shutil.which('apt'):
                return 'apt'
            elif shutil.which('yum'):
                return 'yum'
            elif shutil.which('dnf'):
                return 'dnf'
            elif shutil.which('pacman'):
                return 'pacman'
            elif shutil.which('zypper'):
                return 'zypper'
        return None

class DependencyInstaller:
    """Handle dependency installation across platforms"""

    def __init__(self, platform_detector: PlatformDetector, console=None):
        self.platform = platform_detector
        self.console = console or (Console() if RICH_AVAILABLE else None)
        self.package_manager = platform_detector.get_package_manager()

    def log(self, message: str, style: str = ""):
        """Log message with optional styling"""
        if self.console:
            self.console.print(message, style=style)
        else:
            print(message)

    def run_command(self, cmd: List[str], check: bool = True) -> Tuple[bool, str]:
        """Run command and return success status and output"""
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=check
            )
            return True, result.stdout
        except subprocess.CalledProcessError as e:
            return False, e.stderr

    def install_build_tools(self) -> bool:
        """Install essential build tools"""
        self.log("Installing build tools...", "blue")

        if self.platform.is_windows:
            return self._install_windows_build_tools()
        elif self.platform.is_macos:
            return self._install_macos_build_tools()
        elif self.platform.is_linux:
            return self._install_linux_build_tools()

        return False

    def _install_windows_build_tools(self) -> bool:
        """Install Windows build tools"""
        tools = {
            'choco': [
                'choco install cmake ninja git python3 visualstudio2022buildtools -y'
            ],
            'winget': [
                'winget install Kitware.CMake',
                'winget install Ninja-build.Ninja',
                'winget install Git.Git',
                'winget install Python.Python.3'
            ]
        }

        if self.package_manager in tools:
            for cmd in tools[self.package_manager]:
                success, output = self.run_command(cmd.split(), check=False)
                if not success:
                    self.log(f"Warning: Failed to run {cmd}", "yellow")

        # Check for Visual Studio Build Tools
        vs_paths = [
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional"
        ]

        if not any(Path(p).exists() for p in vs_paths):
            self.log("Visual Studio Build Tools not found. Please install manually.", "red")
            return False

        return True

    def _install_macos_build_tools(self) -> bool:
        """Install macOS build tools"""
        # Install Xcode command line tools
        success, _ = self.run_command(['xcode-select', '--install'], check=False)

        if self.package_manager == 'brew':
            tools = ['cmake', 'ninja', 'ccache', 'python@3.11']
            for tool in tools:
                success, output = self.run_command(['brew', 'install', tool], check=False)
                if not success:
                    self.log(f"Warning: Failed to install {tool}", "yellow")

        return True

    def _install_linux_build_tools(self) -> bool:
        """Install Linux build tools"""
        tools_map = {
            'apt': [
                'sudo apt update',
                'sudo apt install -y build-essential cmake ninja-build git python3 python3-pip python3-dev ccache pkg-config'
            ],
            'yum': [
                'sudo yum groupinstall -y "Development Tools"',
                'sudo yum install -y cmake ninja-build git python3 python3-pip python3-devel ccache pkgconfig'
            ],
            'dnf': [
                'sudo dnf groupinstall -y "Development Tools"',
                'sudo dnf install -y cmake ninja-build git python3 python3-pip python3-devel ccache pkgconf-pkg-config'
            ],
            'pacman': [
                'sudo pacman -S --noconfirm base-devel cmake ninja git python python-pip ccache pkgconf'
            ]
        }

        if self.package_manager in tools_map:
            for cmd in tools_map[self.package_manager]:
                success, output = self.run_command(cmd.split(), check=False)
                if not success:
                    self.log(f"Warning: Failed to run {cmd}", "yellow")

        return True

    def install_python_dependencies(self) -> bool:
        """Install Python dependencies"""
        self.log("Installing Python dependencies...", "blue")

        # Upgrade pip first
        success, _ = self.run_command([sys.executable, '-m', 'pip', 'install', '--upgrade', 'pip'], check=False)

        # Install build dependencies
        build_deps = [
            'setuptools>=68.0.0',
            'wheel>=0.40.0',
            'pybind11>=2.11.0',
            'scikit-build-core>=0.5.0',
            'cmake>=3.21.0',
            'ninja',
            'rich>=14.0.0',
            'loguru>=0.7.3',
            'psutil>=7.0.0',
            'pyyaml>=6.0.2'
        ]

        for dep in build_deps:
            success, output = self.run_command([sys.executable, '-m', 'pip', 'install', dep], check=False)
            if not success:
                self.log(f"Warning: Failed to install {dep}", "yellow")

        return True

    def setup_development_environment(self) -> bool:
        """Setup development environment"""
        self.log("Setting up development environment...", "blue")

        # Create virtual environment if not in one
        if not hasattr(sys, 'real_prefix') and not (hasattr(sys, 'base_prefix') and sys.base_prefix != sys.prefix):
            self.log("Creating virtual environment...", "yellow")
            success, _ = self.run_command([sys.executable, '-m', 'venv', 'venv'], check=False)
            if success:
                self.log("Virtual environment created. Please activate it and run this script again.", "green")
                return False

        # Install development dependencies
        dev_deps = [
            'pytest>=7.0.0',
            'pytest-cov>=4.0.0',
            'black>=22.0.0',
            'isort>=5.10.0',
            'flake8>=5.0.0',
            'mypy>=0.991'
        ]

        for dep in dev_deps:
            success, output = self.run_command([sys.executable, '-m', 'pip', 'install', dep], check=False)
            if not success:
                self.log(f"Warning: Failed to install {dep}", "yellow")

        return True

def main():
    """Main installation function"""
    parser = argparse.ArgumentParser(description="Atom project installation script")
    parser.add_argument('--dev', action='store_true', help='Install development dependencies')
    parser.add_argument('--build-only', action='store_true', help='Only install build dependencies')
    parser.add_argument('--skip-system', action='store_true', help='Skip system package installation')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')

    args = parser.parse_args()

    # Initialize console
    console = Console() if RICH_AVAILABLE else None

    if console:
        console.print(Panel.fit("Atom Project Installation", style="bold blue"))
    else:
        print("=== Atom Project Installation ===")

    # Detect platform
    platform_detector = PlatformDetector()
    installer = DependencyInstaller(platform_detector, console)

    # Show platform information
    if console:
        info_table = Table(title="System Information")
        info_table.add_column("Property", style="cyan")
        info_table.add_column("Value", style="green")

        info_table.add_row("Operating System", platform_detector.system.title())
        info_table.add_row("Architecture", platform_detector.architecture)
        info_table.add_row("Python Version", platform_detector.python_version)
        info_table.add_row("Package Manager", platform_detector.package_manager or "None detected")
        info_table.add_row("WSL", "Yes" if platform_detector.is_wsl else "No")

        console.print(info_table)

    success = True

    # Install system dependencies
    if not args.skip_system:
        success &= installer.install_build_tools()

    # Install Python dependencies
    success &= installer.install_python_dependencies()

    # Setup development environment
    if args.dev:
        success &= installer.setup_development_environment()

    if success:
        installer.log("Installation completed successfully!", "green")
        installer.log("You can now run: python build.py --help", "blue")
    else:
        installer.log("Installation completed with warnings. Check the output above.", "yellow")

    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
