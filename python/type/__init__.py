"""
Type module for advanced type handling and validation.

This module provides Python bindings for type utilities including expected/result
types, JSON schema validation, high-performance robin_hood hash maps, and
trackable object functionality.

Available modules:
    - expected: Expected<T, E> type for error handling (similar to Rust Result)
    - json_schema: JSON schema validation and manipulation
    - robin_hood: High-performance hash map implementation
    - trackable: Object tracking and monitoring functionality

Examples:
    Basic usage:

    >>> from atom import type
    >>> # Use type utilities
"""

__all__ = []

# Expected/Result type
try:
    from . import expected  # noqa: F401

    __all__.append("expected")
except ImportError:
    pass

# JSON Schema support
try:
    from . import json_schema  # noqa: F401

    __all__.append("json_schema")
except ImportError:
    pass

# Robin Hood hash map
try:
    from . import robin_hood  # noqa: F401

    __all__.append("robin_hood")
except ImportError:
    pass

# Trackable objects
try:
    from . import trackable  # noqa: F401

    __all__.append("trackable")
except ImportError:
    pass
