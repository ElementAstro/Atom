"""
Beast module for HTTP and WebSocket support.

This module provides Python bindings for Boost.Beast-based HTTP and WebSocket
functionality, including HTTP client/server utilities and WebSocket support.

Examples:
    Basic usage:

    >>> from atom.extra import beast
    >>> # Use HTTP or WebSocket functionality
"""

__all__ = []

# Import HTTP module if available
try:
    from .http import *  # noqa: F401, F403

    __all__.extend(["http"])
except ImportError:
    pass

# Import HTTP utilities if available
try:
    from .http_utils import *  # noqa: F401, F403

    __all__.extend(["http_utils"])
except ImportError:
    pass

# Import WebSocket module if available
try:
    from .ws import *  # noqa: F401, F403

    __all__.extend(["ws"])
except ImportError:
    pass
