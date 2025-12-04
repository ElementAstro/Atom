"""
inicpp module for INI file parsing and manipulation.

This module provides Python bindings for INI file reading, writing, and
manipulation using the inicpp library.

Examples:
    Basic usage:

    >>> from atom.extra import inicpp
    >>> # Use INI file parsing functionality
"""

__all__ = []

# Import inicpp module if available
try:
    from .inicpp import *  # noqa: F401, F403

    __all__.extend(["inicpp"])
except ImportError:
    pass
