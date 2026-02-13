"""
Atom I/O Module
===============

This module provides comprehensive I/O functionality for the Atom project including:

Core Modules:
- core_io: Core I/O operations (file/directory manipulation, path utilities,
  file classification, recursive directory creation/removal)
- core_glob: Pattern matching and glob operations
- file_info: File information and metadata retrieval
- file_permission: File permission management and security utilities
- path_utils: Path validation and utility functions

Async Modules:
- asyncio: Asynchronous I/O operations (AsyncFile, AsyncDirectory,
  AsyncDirectoryOps, AsyncBatchOps, AsyncStreamOps)
- async_compress: Asynchronous compression/decompression with ASIO
  (SingleFileCompressor/Decompressor, DirectoryCompressor/Decompressor,
  ZIP operations)
- async_glob: Asynchronous glob pattern matching

Compression Modules:
- compress: Comprehensive compression/decompression (GZ, ZIP, slice-based,
  in-memory data, backup/restore, async compressors)

Filesystem Modules:
- glob: Glob pattern matching utilities
- dirstack: Directory stack operations (pushd/popd)

All modules provide comprehensive Python bindings for the underlying C++ functionality
with proper exception handling, detailed documentation, and cross-platform support.

Examples:
    >>> import atom.io as io
    >>>
    >>> # Core I/O operations
    >>> io.core_io.create_directory("new_dir")
    >>> io.core_io.classify_files(".")
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
    >>>
    >>> # Compression
    >>> io.compress.compress_gz("data.txt", "data.txt.gz")
    >>> io.compress.compress_folder("my_dir", "archive.zip")
    >>> io.compress.create_backup("important", "backup", compress=True)
"""

import logging

_logger = logging.getLogger(__name__)

# Import all available modules
__all__ = []

_MODULES = [
    "core_io",
    "core_glob",
    "file_info",
    "file_permission",
    "path_utils",
    "asyncio",
    "compress",
    "dirstack",
    "glob",
    "async_glob",
    "async_compress",
]

for _mod_name in _MODULES:
    try:
        _mod = __import__(f"{__name__}.{_mod_name}", fromlist=[_mod_name])
        globals()[_mod_name] = _mod
        __all__.append(_mod_name)
    except ImportError as e:
        _logger.debug("Could not import %s: %s", _mod_name, e)

# Module metadata
__version__ = "2.0.0"
__author__ = "Atom Project"
__description__ = "Comprehensive I/O functionality for the Atom project"


def get_version():
    """Get the version of the atom.io module."""
    return __version__


def list_available_modules():
    """List all available modules in the atom.io package."""
    return list(__all__)


def get_module_info():
    """Get information about the atom.io module."""
    return {
        "version": __version__,
        "author": __author__,
        "description": __description__,
        "available_modules": list(__all__),
        "total_modules": len(__all__),
    }
