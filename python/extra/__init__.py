"""
Extra module for additional third-party library bindings.

This module provides Python bindings for various third-party libraries and
utilities including ASIO (async I/O), Beast (HTTP/WebSocket), Boost libraries,
cURL (HTTP client), dotenv (environment variables), iconv (character encoding),
inicpp (INI files), dependency injection, pugixml (XML parsing), spdlog (logging),
and libuv (async event loop).

Available submodules:
    - asio: ASIO-based async I/O (MQTT, SSE)
    - beast: Boost.Beast HTTP and WebSocket
    - boost: Selected Boost library bindings
    - curl: libcurl HTTP client features
    - dotenv: Environment variable loading from .env files
    - iconv: Character encoding conversion
    - inicpp: INI file parsing
    - injection: Dependency injection framework
    - pugixml: XML document parsing and manipulation
    - spdlog: Fast C++ logging library
    - uv: libuv async event loop

Examples:
    Basic usage:

    >>> from atom.extra import dotenv
    >>> result = dotenv.Dotenv.quick_load(".env")

    >>> from atom.extra import curl
    >>> # Use cURL HTTP client functionality
"""

__all__ = []

# Import ASIO submodule if available
try:
    from . import asio

    __all__.append("asio")
except ImportError:
    pass

# Import Beast submodule if available
try:
    from . import beast

    __all__.append("beast")
except ImportError:
    pass

# Import Boost submodule if available
try:
    from . import boost

    __all__.append("boost")
except ImportError:
    pass

# Import cURL submodule if available
try:
    from . import curl

    __all__.append("curl")
except ImportError:
    pass

# Import dotenv submodule if available
try:
    from . import dotenv

    __all__.append("dotenv")
except ImportError:
    pass

# Import iconv submodule if available
try:
    from . import iconv

    __all__.append("iconv")
except ImportError:
    pass

# Import inicpp submodule if available
try:
    from . import inicpp

    __all__.append("inicpp")
except ImportError:
    pass

# Import injection submodule if available
try:
    from . import injection

    __all__.append("injection")
except ImportError:
    pass

# Import pugixml submodule if available
try:
    from . import pugixml

    __all__.append("pugixml")
except ImportError:
    pass

# Import spdlog submodule if available
try:
    from . import spdlog

    __all__.append("spdlog")
except ImportError:
    pass

# Import uv submodule if available
try:
    from . import uv

    __all__.append("uv")
except ImportError:
    pass
