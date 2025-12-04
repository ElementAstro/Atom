"""
Boost module for various Boost library Python bindings.

This module provides Python bindings for selected Boost libraries including
character conversion, locale, math functions, regular expressions, system
utilities, and UUID generation.

Examples:
    Basic usage:

    >>> from atom.extra import boost
    >>> # Use Boost functionality
"""

__all__ = []

# Import character conversion module if available
try:
    from .charconv import *  # noqa: F401, F403

    __all__.extend(["charconv"])
except ImportError:
    pass

# Import locale module if available
try:
    from .locale import *  # noqa: F401, F403

    __all__.extend(["locale"])
except ImportError:
    pass

# Import math module if available
try:
    from .math import *  # noqa: F401, F403

    __all__.extend(["math"])
except ImportError:
    pass

# Import regex module if available
try:
    from .regex import *  # noqa: F401, F403

    __all__.extend(["regex"])
except ImportError:
    pass

# Import system module if available
try:
    from .system import *  # noqa: F401, F403

    __all__.extend(["system"])
except ImportError:
    pass

# Import UUID module if available
try:
    from .uuid import *  # noqa: F401, F403

    __all__.extend(["uuid"])
except ImportError:
    pass
