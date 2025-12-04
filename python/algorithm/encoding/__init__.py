"""Encoding algorithms module."""

__all__ = []

try:
    from .base import *  # noqa: F401, F403

    __all__.append("base")
except ImportError:
    pass
