"""Hash algorithms module."""

__all__ = []

try:
    from .hash import *  # noqa: F401, F403

    __all__.append("hash")
except ImportError:
    pass

try:
    from .mhash import *  # noqa: F401, F403

    __all__.append("mhash")
except ImportError:
    pass
