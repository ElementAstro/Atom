#!/usr/bin/env python3
"""
Setup script for Atom Python bindings
"""

import platform
import subprocess
import sys
from pathlib import Path

from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import find_packages, setup

# Project information
PROJECT_NAME = "atom"
PROJECT_VERSION = "0.1.0"
PROJECT_DESCRIPTION = "Foundational library for astronomical software"
PROJECT_URL = "https://github.com/ElementAstro/Atom"
AUTHOR = "Max Qian"
AUTHOR_EMAIL = "max@example.com"


# Read long description from README
def read_long_description():
    readme_path = Path(__file__).parent / "README.md"
    if readme_path.exists():
        with open(readme_path, encoding="utf-8") as f:
            return f.read()
    return PROJECT_DESCRIPTION


# Get version from git or VERSION file
def get_version():
    # Try to get version from git
    try:
        result = subprocess.run(
            ["git", "describe", "--tags", "--always", "--dirty"],
            capture_output=True,
            text=True,
            check=True,
        )
        version = result.stdout.strip()
        if version.startswith("v"):
            version = version[1:]
        return version
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # Try to read from VERSION file
    version_file = Path(__file__).parent / "VERSION"
    if version_file.exists():
        with open(version_file) as f:
            return f.read().strip()

    return PROJECT_VERSION


# Platform-specific configuration
def get_platform_config():
    config = {
        "libraries": [],
        "library_dirs": [],
        "include_dirs": [],
        "define_macros": [],
        "extra_compile_args": [],
        "extra_link_args": [],
    }

    system = platform.system().lower()

    if system == "linux":
        config["libraries"].extend(["ssl", "crypto", "z", "sqlite3", "pthread"])
        config["extra_compile_args"].extend(["-std=c++20", "-fPIC"])

    elif system == "darwin":  # macOS
        config["libraries"].extend(["ssl", "crypto", "z", "sqlite3"])
        config["extra_compile_args"].extend(["-std=c++20", "-stdlib=libc++"])
        config["extra_link_args"].extend(["-stdlib=libc++"])

    elif system == "windows":
        config["libraries"].extend(["ws2_32", "crypt32"])
        config["define_macros"].extend(
            [("WIN32_LEAN_AND_MEAN", None), ("NOMINMAX", None)]
        )
        config["extra_compile_args"].extend(["/std:c++20"])

    return config


# Find source files
def find_source_files():
    source_files = []
    python_dir = Path(__file__).parent / "python"

    if python_dir.exists():
        # Find all .cpp files in python directory
        for cpp_file in python_dir.rglob("*.cpp"):
            source_files.append(str(cpp_file))

    return source_files


# Create extensions
def create_extensions():
    platform_config = get_platform_config()
    source_files = find_source_files()

    if not source_files:
        print("Warning: No Python binding source files found")
        return []

    # Base include directories
    include_dirs = [
        str(Path(__file__).parent / "atom"),
        str(Path(__file__).parent / "python"),
        str(Path(__file__).parent),
    ]
    include_dirs.extend(platform_config["include_dirs"])

    # Create extension
    ext = Pybind11Extension(
        "atom._core",
        source_files,
        include_dirs=include_dirs,
        libraries=platform_config["libraries"],
        library_dirs=platform_config["library_dirs"],
        define_macros=platform_config["define_macros"],
        extra_compile_args=platform_config["extra_compile_args"],
        extra_link_args=platform_config["extra_link_args"],
        cxx_std=20,
    )

    return [ext]


# Custom build command
class CustomBuildExt(build_ext):
    """Custom build extension to handle special requirements"""

    def build_extensions(self):
        # Check for required system libraries
        self.check_system_dependencies()

        # Build extensions
        super().build_extensions()

    def check_system_dependencies(self):
        """Check for required system dependencies"""
        system = platform.system().lower()

        if system == "linux":
            # Check for required development packages
            required_packages = ["libssl-dev", "zlib1g-dev", "libsqlite3-dev"]
            print(
                f"Note: Ensure these packages are installed: {', '.join(required_packages)}"
            )

        elif system == "darwin":
            print("Note: Ensure OpenSSL, zlib, and SQLite3 are available via Homebrew")

        elif system == "windows":
            print("Note: Ensure vcpkg dependencies are available")


# Package data
def get_package_data():
    package_data = {
        "atom": [
            "*.pyi",  # Type stubs
            "py.typed",  # PEP 561 marker
        ]
    }
    return package_data


# Entry points
def get_entry_points():
    return {
        "console_scripts": [
            "atom-info=atom.cli:info_command",
        ],
    }


# Requirements
def get_requirements():
    requirements = [
        "numpy>=1.20.0",
        "typing-extensions>=4.0.0",
    ]

    return requirements


def get_extras_require():
    return {
        "dev": [
            "pytest>=6.0",
            "pytest-cov>=2.0",
            "black>=21.0",
            "isort>=5.0",
            "mypy>=0.900",
            "sphinx>=4.0",
            "sphinx-rtd-theme>=1.0",
        ],
        "test": [
            "pytest>=6.0",
            "pytest-cov>=2.0",
            "numpy>=1.20.0",
        ],
        "docs": [
            "sphinx>=4.0",
            "sphinx-rtd-theme>=1.0",
            "myst-parser>=0.15",
        ],
    }


# Main setup
def main():
    # Get version
    version = get_version()
    print(f"Building Atom Python bindings version {version}")

    # Create extensions
    extensions = create_extensions()

    setup(
        name=PROJECT_NAME,
        version=version,
        author=AUTHOR,
        author_email=AUTHOR_EMAIL,
        description=PROJECT_DESCRIPTION,
        long_description=read_long_description(),
        long_description_content_type="text/markdown",
        url=PROJECT_URL,
        project_urls={
            "Bug Tracker": f"{PROJECT_URL}/issues",
            "Documentation": f"{PROJECT_URL}#readme",
            "Source Code": PROJECT_URL,
        },
        packages=find_packages(where="python"),
        package_dir={"": "python"},
        package_data=get_package_data(),
        ext_modules=extensions,
        cmdclass={"build_ext": CustomBuildExt},
        zip_safe=False,
        python_requires=">=3.8",
        install_requires=get_requirements(),
        extras_require=get_extras_require(),
        entry_points=get_entry_points(),
        classifiers=[
            "Development Status :: 3 - Alpha",
            "Intended Audience :: Developers",
            "Intended Audience :: Science/Research",
            "License :: OSI Approved :: GNU General Public License v3 (GPLv3)",
            "Operating System :: OS Independent",
            "Programming Language :: C++",
            "Programming Language :: Python :: 3",
            "Programming Language :: Python :: 3.8",
            "Programming Language :: Python :: 3.9",
            "Programming Language :: Python :: 3.10",
            "Programming Language :: Python :: 3.11",
            "Topic :: Scientific/Engineering",
            "Topic :: Scientific/Engineering :: Astronomy",
            "Topic :: Software Development :: Libraries :: Python Modules",
        ],
        keywords="astronomy, astrophysics, c++, python, bindings",
        include_package_data=True,
    )


if __name__ == "__main__":
    main()
