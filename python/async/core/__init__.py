"""Core async primitives module."""

# Note: 'async' is a Python keyword, import as 'async_' or use getattr
from . import future, promise

try:
    import importlib

    async_module = importlib.import_module(".async", __package__)
except ImportError:
    async_module = None

__all__ = ["future", "promise"]
