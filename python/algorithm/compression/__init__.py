"""Compression algorithms module."""

__all__ = []

try:
    from .huffman import *  # noqa: F401, F403

    __all__.append("huffman")
except ImportError:
    pass

try:
    from .matrix_compress import *  # noqa: F401, F403

    __all__.append("matrix_compress")
except ImportError:
    pass
