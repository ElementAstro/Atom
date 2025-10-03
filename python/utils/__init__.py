"""
Atom Utils Module
================

This module provides comprehensive utilities for the Atom package, including:

- **AES Encryption**: Cryptographic utilities with AES encryption and hashing
- **Aligned Memory**: Memory alignment validation and utilities
- **Argument Parsing**: Command-line argument processing with argsview
- **Bit Manipulation**: Bit-level operations and utilities
- **Container Operations**: Advanced container utilities, ranges, and span operations
- **Conversion Utilities**: Type conversion, parsing, and byte serialization
- **Difflib**: Text difference and comparison utilities
- **Error Stack**: Error handling and stack trace utilities
- **LCG Random**: Linear congruential generator for random numbers
- **LINQ**: Language Integrated Query operations for containers
- **Process Management**: Qt-compatible process utilities
- **String Processing**: String conversion, validation, and manipulation
- **Switch Utilities**: String-based switch statement implementations
- **Time Utilities**: Date/time operations and timer functionality
- **UUID Generation**: Universally unique identifier utilities

Examples:
    >>> from atom.utils import aes, bit, container
    >>> # AES encryption
    >>> encrypted = aes.encrypt_string("hello", "password")
    >>>
    >>> # Bit operations
    >>> result = bit.count_set_bits(15)  # Returns 4
    >>>
    >>> # Container operations
    >>> intersection = container.intersection([1, 2, 3], [2, 3, 4])  # [2, 3]

Categories:
    - **Cryptography**: aes
    - **Memory**: aligned
    - **Parsing**: argsview, conversion
    - **Data Structures**: bit, container, linq
    - **Text Processing**: difflib, to_string, valid_string
    - **System**: error_stack, qprocess, qtimer, qtimezone
    - **Utilities**: switch, time, uuid, stopwatcher
"""

# Import all available modules
try:
    from . import aes
    __all__ = ['aes']
except ImportError:
    __all__ = []

# Enhanced modules with new functionality
try:
    from . import aligned
    __all__.append('aligned')
except ImportError:
    pass

try:
    from . import argsview
    __all__.append('argsview')
except ImportError:
    pass

try:
    from . import bit
    __all__.append('bit')
except ImportError:
    pass

try:
    from . import container
    __all__.append('container')
except ImportError:
    pass

try:
    from . import conversion
    __all__.append('conversion')
except ImportError:
    pass

try:
    from . import difflib
    __all__.append('difflib')
except ImportError:
    pass

try:
    from . import error_stack
    __all__.append('error_stack')
except ImportError:
    pass

try:
    from . import lcg
    __all__.append('lcg')
except ImportError:
    pass

try:
    from . import linq
    __all__.append('linq')
except ImportError:
    pass

try:
    from . import qdatetime
    __all__.append('qdatetime')
except ImportError:
    pass

try:
    from . import qprocess
    __all__.append('qprocess')
except ImportError:
    pass

try:
    from . import qtimer
    __all__.append('qtimer')
except ImportError:
    pass

try:
    from . import qtimezone
    __all__.append('qtimezone')
except ImportError:
    pass

try:
    from . import stopwatcher
    __all__.append('stopwatcher')
except ImportError:
    pass

try:
    from . import switch
    __all__.append('switch')
except ImportError:
    pass

try:
    from . import time
    __all__.append('time')
except ImportError:
    pass

try:
    from . import to_string
    __all__.append('to_string')
except ImportError:
    pass

try:
    from . import uuid
    __all__.append('uuid')
except ImportError:
    pass

try:
    from . import valid_string
    __all__.append('valid_string')
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
        'version': __version__,
        'author': __author__,
        'description': __description__,
        'available_modules': get_available_modules(),
        'total_modules': len(__all__),
        'categories': {
            'cryptography': ['aes'],
            'memory': ['aligned'],
            'parsing': ['argsview', 'conversion'],
            'data_structures': ['bit', 'container', 'linq'],
            'text_processing': ['difflib', 'to_string', 'valid_string'],
            'system': ['error_stack', 'qprocess', 'qtimer', 'qtimezone'],
            'utilities': ['switch', 'time', 'uuid', 'stopwatcher']
        }
    }

# Add convenience functions to __all__
__all__.extend(['get_available_modules', 'module_info'])
