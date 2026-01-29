"""Optimization algorithms module."""

__all__ = []

try:
    from .annealing import *  # noqa: F401, F403

    __all__.append("annealing")
except ImportError:
    pass

try:
    from .pathfinding import *  # noqa: F401, F403

    __all__.append("pathfinding")
except ImportError:
    pass
