"""
iconv module for character encoding conversion.

This module provides Python bindings for libiconv-based character encoding
conversion between different character sets.

Examples:
    Basic usage:

    >>> from atom.extra import iconv
    >>> # Use character encoding conversion functionality
"""

__all__ = []

# Import iconv module if available
try:
    from .iconv import *  # noqa: F401, F403

    __all__.extend(["iconv"])
except ImportError:
    pass
