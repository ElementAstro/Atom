"""
cURL module for HTTP client functionality.

This module provides Python bindings for libcurl-based HTTP client features
including session management, caching, connection pooling, cookies, error
handling, multipart uploads, rate limiting, and WebSocket support.

Examples:
    Basic usage:

    >>> from atom.extra import curl
    >>> # Use cURL HTTP client functionality
"""

__all__ = []

# Import cache module if available
try:
    from .cache import *  # noqa: F401, F403

    __all__.extend(["cache"])
except ImportError:
    pass

# Import connection pool module if available
try:
    from .connection_pool import *  # noqa: F401, F403

    __all__.extend(["connection_pool"])
except ImportError:
    pass

# Import cookie module if available
try:
    from .cookie import *  # noqa: F401, F403

    __all__.extend(["cookie"])
except ImportError:
    pass

# Import error module if available
try:
    from .error import *  # noqa: F401, F403

    __all__.extend(["error"])
except ImportError:
    pass

# Import multipart module if available
try:
    from .multipart import *  # noqa: F401, F403

    __all__.extend(["multipart"])
except ImportError:
    pass

# Import rate limiter module if available
try:
    from .rate_limiter import *  # noqa: F401, F403

    __all__.extend(["rate_limiter"])
except ImportError:
    pass

# Import session module if available
try:
    from .session import *  # noqa: F401, F403

    __all__.extend(["session"])
except ImportError:
    pass

# Import WebSocket module if available
try:
    from .websocket import *  # noqa: F401, F403

    __all__.extend(["websocket"])
except ImportError:
    pass

# Import REST client module if available
try:
    from .rest_client import *  # noqa: F401, F403

    __all__.extend(["rest_client"])
except ImportError:
    pass

# Import multi-session module if available
try:
    from .multi_session import *  # noqa: F401, F403

    __all__.extend(["multi_session"])
except ImportError:
    pass
