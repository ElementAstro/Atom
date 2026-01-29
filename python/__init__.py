"""
Atom - A comprehensive C++ library with Python bindings.

Atom is a foundational library for astronomical software development and general-purpose
computing, providing modules for algorithmic operations, async programming, networking,
image processing, system integration, and more.

Available modules:
    - algorithm: Algorithmic operations (compression, crypto, graphics, math, optimization)
    - async: Asynchronous programming primitives and concurrency utilities
    - connection: Network communication (TCP, UDP, SSH, FIFO)
    - error: Comprehensive error handling and stack trace system
    - extra: Third-party library bindings (ASIO, Boost, cURL, dotenv, etc.)
    - image: Image processing and computer vision (OpenCV integration)
    - io: Input/output operations and file system utilities
    - search: Database and caching utilities (MySQL, SQLite, LRU, TTL)
    - secret: Secure storage and password management
    - sysinfo: System information (CPU, memory, GPU, disk, network)
    - system: System-level integration (processes, signals, clipboard, etc.)
    - type: Advanced type handling (expected/result, JSON schema, robin hood)
    - utils: General utility functions and helpers
    - web: HTTP client and web-related utilities

Examples:
    Basic usage:

    >>> import atom
    >>> # Use modules from atom package
    >>> from atom import utils
    >>> from atom import algorithm
    >>> from atom.extra import dotenv

Installation:
    The Atom package is built using CMake with pybind11. To build:

    $ ./scripts/build.sh --python --release
    $ cmake --build build --target install

Requirements:
    - Python 3.8+
    - Various C++ dependencies (OpenSSL, OpenCV, spdlog, etc.)

For more information, see: https://github.com/ElementAstro/Atom
"""

__version__ = "0.0.1"
__author__ = "Max Qian"
__all__ = []

# Import core modules if available
_modules = [
    "algorithm",
    "async",
    "connection",
    "error",
    "extra",
    "image",
    "io",
    "search",
    "secret",
    "sysinfo",
    "system",
    "type",
    "utils",
    "web",
]

for _module in _modules:
    try:
        exec(f"from . import {_module}")
        __all__.append(_module)
    except ImportError:
        # Module not compiled or dependencies not available
        pass

# Clean up temporary variables
del _modules, _module
