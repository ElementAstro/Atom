"""Signal processing algorithms module."""

__all__ = []

try:
    from .convolve import *  # noqa: F401, F403

    __all__.append("convolve")
except ImportError:
    pass
