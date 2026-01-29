"""
Atom I/O Module
===============

This module provides comprehensive I/O functionality for the Atom project including:

Core Modules:
- core_io: Core I/O operations (file/directory manipulation, path utilities)
- core_glob: Pattern matching and glob operations
- file_info: File information and metadata retrieval
- file_permission: File permission management and security utilities

Async Modules:
- asyncio: Asynchronous I/O operations
- compress: Compression and decompression utilities
- dirstack: Directory stack operations

Filesystem Modules:
- glob: Glob pattern matching utilities

All modules provide comprehensive Python bindings for the underlying C++ functionality
with proper exception handling, detailed documentation, and cross-platform support.

Examples:
    >>> import atom.io as io
    >>>
    >>> # Core I/O operations
    >>> io.core_io.create_directory("new_dir")
    >>> files = io.core_io.list_files(".")
    >>>
    >>> # Pattern matching
    >>> python_files = io.core_glob.glob("*.py")
    >>> all_files = io.core_glob.rglob("**/*")
    >>>
    >>> # File information
    >>> info = io.file_info.get_file_info("example.txt")
    >>> print(f"Size: {info.file_size} bytes")
    >>>
    >>> # Permission management
    >>> perms = io.file_permission.get_file_permissions("example.txt")
    >>> print(f"Permissions: {perms}")
"""

# Import all available modules
try:
    from . import core_io

    __all__ = ["core_io"]
except ImportError as e:
    print(f"Warning: Could not import core_io: {e}")
    __all__ = []

try:
    from . import core_glob

    __all__.append("core_glob")
except ImportError as e:
    print(f"Warning: Could not import core_glob: {e}")

try:
    from . import file_info

    __all__.append("file_info")
except ImportError as e:
    print(f"Warning: Could not import file_info: {e}")

try:
    from . import file_permission

    __all__.append("file_permission")
except ImportError as e:
    print(f"Warning: Could not import file_permission: {e}")

# Import existing modules
try:
    from . import asyncio

    __all__.append("asyncio")
except ImportError as e:
    print(f"Warning: Could not import asyncio: {e}")

try:
    from . import compress

    __all__.append("compress")
except ImportError as e:
    print(f"Warning: Could not import compress: {e}")

try:
    from . import dirstack

    __all__.append("dirstack")
except ImportError as e:
    print(f"Warning: Could not import dirstack: {e}")

try:
    from . import glob

    __all__.append("glob")
except ImportError as e:
    print(f"Warning: Could not import glob: {e}")

# Import new modules
try:
    from . import path_utils

    __all__.append("path_utils")
except ImportError as e:
    print(f"Warning: Could not import path_utils: {e}")

try:
    from . import async_glob

    __all__.append("async_glob")
except ImportError as e:
    print(f"Warning: Could not import async_glob: {e}")

try:
    from . import async_compress

    __all__.append("async_compress")
except ImportError as e:
    print(f"Warning: Could not import async_compress: {e}")

# Module metadata
__version__ = "1.0.0"
__author__ = "Atom Project"
__description__ = "Comprehensive I/O functionality for the Atom project"


# Convenience imports for common operations
def get_version():
    """Get the version of the atom.io module."""
    return __version__


def list_available_modules():
    """List all available modules in the atom.io package."""
    return __all__


def get_module_info():
    """Get information about the atom.io module."""
    return {
        "version": __version__,
        "author": __author__,
        "description": __description__,
        "available_modules": __all__,
        "total_modules": len(__all__),
    }
