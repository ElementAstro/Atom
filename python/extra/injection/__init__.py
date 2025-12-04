"""
injection module for dependency injection.

This module provides Python bindings for dependency injection and
inversion of control (IoC) container functionality.

Examples:
    Basic usage:

    >>> from atom.extra import injection
    >>> # Use dependency injection functionality
"""

__all__ = []

# Import injection module if available
try:
    from .injection import *  # noqa: F401, F403

    __all__.extend(["injection"])
except ImportError:
    pass
