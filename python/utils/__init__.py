"""
Atom Utils Module
================

This module provides comprehensive utilities for the Atom package, including:

- **AES Encryption**: Cryptographic utilities with AES encryption and hashing
- **Aligned Memory**: Memory alignment validation and utilities
- **Argument Parsing**: Command-line argument processing with argsview
- **Bit Manipulation**: Bit-level operations and utilities
- **Container Operations**: Advanced container utilities, ranges, and span operations
- **Conversion Utilities**: Type conversion, parsing, and byte serialization (including Windows-specific conversions)
- **Difflib**: Text difference and comparison utilities
- **Error Stack**: Error handling and stack trace utilities
- **LCG Random**: Linear congruential generator for random numbers
- **LINQ**: Language Integrated Query operations for containers
- **Print Utilities**: Formatted printing, logging, progress bars, and performance timing
- **Process Management**: Qt-compatible process utilities
- **Random Generation**: Random number and string generation utilities
- **String Processing**: String conversion, validation, and manipulation
- **Switch Utilities**: String-based switch statement implementations
- **Time Utilities**: Date/time operations and timer functionality
- **UTF Encoding**: UTF-8, UTF-16, UTF-32 encoding conversions
- **UUID Generation**: Universally unique identifier utilities
- **XML Processing**: XML reading and writing utilities

Examples:
    >>> from atom.utils import aes, bit, container, string, random
    >>> # AES encryption
    >>> encrypted = aes.encrypt_string("hello", "password")
    >>>
    >>> # Bit operations
    >>> result = bit.count_set_bits(15)  # Returns 4
    >>>
    >>> # Container operations
    >>> intersection = container.intersection([1, 2, 3], [2, 3, 4])  # [2, 3]
    >>>
    >>> # String manipulation
    >>> camel = string.to_camel_case("hello_world")  # "helloWorld"
    >>>
    >>> # Random generation
    >>> rng = random.RandomInt(1, 100)
    >>> value = rng.generate()

Categories:
    - **Cryptography**: aes
    - **Memory**: aligned
    - **Parsing**: argsview, conversion, convert
    - **Data Structures**: bit, container, linq
    - **Text Processing**: difflib, string, to_string, utf, valid_string
    - **System**: error_stack, print, qprocess, qtimer, qtimezone
    - **Utilities**: random, switch, time, uuid, stopwatcher, xml
"""

# Import all available modules
try:
    from . import aes

    __all__ = ["aes"]
except ImportError:
    __all__ = []

# Enhanced modules with new functionality
try:
    from . import aligned

    __all__.append("aligned")
except ImportError:
    pass

try:
    from . import argsview

    __all__.append("argsview")
except ImportError:
    pass

try:
    from . import bit

    __all__.append("bit")
except ImportError:
    pass

try:
    from . import container

    __all__.append("container")
except ImportError:
    pass

try:
    from . import conversion

    __all__.append("conversion")
except ImportError:
    pass

try:
    from . import difflib

    __all__.append("difflib")
except ImportError:
    pass

try:
    from . import error_stack

    __all__.append("error_stack")
except ImportError:
    pass

try:
    from . import lcg

    __all__.append("lcg")
except ImportError:
    pass

try:
    from . import linq

    __all__.append("linq")
except ImportError:
    pass

try:
    from . import qdatetime

    __all__.append("qdatetime")
except ImportError:
    pass

try:
    from . import qprocess

    __all__.append("qprocess")
except ImportError:
    pass

try:
    from . import qtimer

    __all__.append("qtimer")
except ImportError:
    pass

try:
    from . import qtimezone

    __all__.append("qtimezone")
except ImportError:
    pass

try:
    from . import stopwatcher

    __all__.append("stopwatcher")
except ImportError:
    pass

try:
    from . import switch

    __all__.append("switch")
except ImportError:
    pass

try:
    from . import time

    __all__.append("time")
except ImportError:
    pass

try:
    from . import to_string

    __all__.append("to_string")
except ImportError:
    pass

try:
    from . import uuid

    __all__.append("uuid")
except ImportError:
    pass

try:
    from . import valid_string

    __all__.append("valid_string")
except ImportError:
    pass

try:
    from . import string

    __all__.append("string")
except ImportError:
    pass

try:
    from . import utf

    __all__.append("utf")
except ImportError:
    pass

try:
    from . import random

    __all__.append("random")
except ImportError:
    pass

try:
    from . import print

    __all__.append("print")
except ImportError:
    pass

try:
    from . import xml

    __all__.append("xml")
except ImportError:
    pass

try:
    from . import convert

    __all__.append("convert")
except ImportError:
    pass

# New modules added for complete coverage
try:
    from . import anyutils

    __all__.append("anyutils")
except ImportError:
    pass

try:
    from . import cstring

    __all__.append("cstring")
except ImportError:
    pass

try:
    from . import color_print

    __all__.append("color_print")
except ImportError:
    pass

try:
    from . import ranges

    __all__.append("ranges")
except ImportError:
    pass

try:
    from . import to_byte

    __all__.append("to_byte")
except ImportError:
    pass

# Module information
__version__ = "1.0.0"
__author__ = "Atom Development Team"
__description__ = "Comprehensive utilities for the Atom package"


# Convenience imports for commonly used functionality
def get_available_modules():
    """
    Get a list of all available utility modules.

    Returns:
        List[str]: Names of all successfully imported modules.

    Examples:
        >>> from atom.utils import get_available_modules
        >>> modules = get_available_modules()
        >>> print(f"Available modules: {', '.join(modules)}")
    """
    return sorted(__all__)


def module_info():
    """
    Get information about the utils module and its submodules.

    Returns:
        Dict[str, Any]: Information about the module including version,
                       available submodules, and descriptions.
    """
    return {
        "version": __version__,
        "author": __author__,
        "description": __description__,
        "available_modules": get_available_modules(),
        "total_modules": len(__all__),
        "categories": {
            "cryptography": ["aes"],
            "memory": ["aligned"],
            "parsing": ["argsview", "conversion", "convert"],
            "data_structures": ["bit", "container", "linq", "ranges"],
            "text_processing": [
                "difflib",
                "string",
                "to_string",
                "utf",
                "valid_string",
                "cstring",
            ],
            "system": [
                "error_stack",
                "print",
                "qprocess",
                "qtimer",
                "qtimezone",
                "color_print",
            ],
            "utilities": ["random", "switch", "time", "uuid", "stopwatcher", "xml"],
            "serialization": ["anyutils"],
            "conversion": ["to_byte"],
        },
    }


# Add convenience functions to __all__
__all__.extend(["get_available_modules", "module_info"])
