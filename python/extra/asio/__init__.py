"""
ASIO module for asynchronous I/O operations.

This module provides Python bindings for ASIO-based asynchronous I/O components
including MQTT clients and Server-Sent Events (SSE) support.

Examples:
    Basic usage:

    >>> from atom.extra import asio
    >>> # Use MQTT or SSE functionality
"""

__all__ = []

# Import MQTT module if available
try:
    from .mqtt import *  # noqa: F401, F403

    __all__.extend(["mqtt"])
except ImportError:
    pass

# Import SSE module if available
try:
    from .sse import *  # noqa: F401, F403

    __all__.extend(["sse"])
except ImportError:
    pass
