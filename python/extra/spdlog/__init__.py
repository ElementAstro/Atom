"""
spdlog module for fast C++ logging.

This module provides Python bindings for spdlog, a fast C++ logging library
with support for multiple sinks, async logging, and custom formatting.

Examples:
    Basic usage:

    >>> from atom.extra import spdlog
    >>> # Use spdlog logging functionality
"""

__all__ = []

# Import spdlog module if available
try:
    from .spdlog import *  # noqa: F401, F403

    __all__.extend(["spdlog"])
except ImportError:
    pass
