"""
pugixml module for XML parsing and manipulation.

This module provides Python bindings for pugixml-based XML document parsing,
manipulation, and querying with XPath support.

Examples:
    Basic usage:

    >>> from atom.extra import pugixml
    >>> # Use XML parsing and manipulation functionality
"""

__all__ = []

# Import pugixml module if available
try:
    from .pugixml import *  # noqa: F401, F403

    __all__.extend(["pugixml"])
except ImportError:
    pass
