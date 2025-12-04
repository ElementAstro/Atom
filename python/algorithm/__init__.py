"""
Algorithm module providing comprehensive algorithmic operations.

This module provides Python bindings for various algorithmic operations including
compression, cryptography, hashing, encoding, graphics, mathematical operations,
optimization algorithms, signal processing, and utility functions.

Submodules:
    - compression: Matrix compression and Huffman coding
    - core: Core algorithms including SIMD, OpenCL utilities, and Rust numeric
    - crypto: Cryptographic algorithms (Blowfish, MD5, SHA1, TEA)
    - encoding: Base encoding/decoding
    - graphics: Image operations, flood fill, Perlin/Simplex noise
    - hash: Hashing utilities
    - math: Mathematical operations (BigNumber, fractions, matrices, GPU math)
    - optimization: Optimization algorithms (pathfinding, simulated annealing)
    - signal: Signal processing (convolution)
    - utils: Utility functions (error calibration, fnmatch, snowflake, UUID, weights)

Examples:
    Basic usage:

    >>> from atom import algorithm
    >>> # Use algorithm functionality from submodules
"""

__all__ = []

# Compression algorithms
try:
    from .compression.huffman import *

    __all__.append("huffman")
except ImportError:
    pass

try:
    from .compression.matrix_compress import *

    __all__.append("matrix_compress")
except ImportError:
    pass

# Core algorithms
try:
    from .core.algorithm import *

    __all__.append("algorithm")
except ImportError:
    pass

try:
    from .core.opencl_utils import *

    __all__.append("opencl_utils")
except ImportError:
    pass

try:
    from .core.rust_numeric import *

    __all__.append("rust_numeric")
except ImportError:
    pass

try:
    from .core.simd_utils import *

    __all__.append("simd_utils")
except ImportError:
    pass

# Cryptographic algorithms
try:
    from .crypto.blowfish import *

    __all__.append("blowfish")
except ImportError:
    pass

try:
    from .crypto.md5 import *

    __all__.append("md5")
except ImportError:
    pass

try:
    from .crypto.sha1 import *

    __all__.append("sha1")
except ImportError:
    pass

try:
    from .crypto.tea import *

    __all__.append("tea")
except ImportError:
    pass

# Encoding algorithms
try:
    from .encoding.base import *

    __all__.append("base")
except ImportError:
    pass

# Graphics algorithms
try:
    from .graphics.flood import *

    __all__.append("flood")
except ImportError:
    pass

try:
    from .graphics.flood_fill import *

    __all__.append("flood_fill")
except ImportError:
    pass

try:
    from .graphics.image_ops import *

    __all__.append("image_ops")
except ImportError:
    pass

try:
    from .graphics.perlin import *

    __all__.append("perlin")
except ImportError:
    pass

try:
    from .graphics.simplex import *

    __all__.append("simplex")
except ImportError:
    pass

# Hash algorithms
try:
    from .hash.hash import *

    __all__.append("hash")
except ImportError:
    pass

try:
    from .hash.mhash import *

    __all__.append("mhash")
except ImportError:
    pass

# Mathematical operations
try:
    from .math.bignumber import *

    __all__.append("bignumber")
except ImportError:
    pass

try:
    from .math.fraction import *

    __all__.append("fraction")
except ImportError:
    pass

try:
    from .math.gpu_math import *

    __all__.append("gpu_math")
except ImportError:
    pass

try:
    from .math.math import *

    __all__.append("math")
except ImportError:
    pass

try:
    from .math.matrix import *

    __all__.append("matrix")
except ImportError:
    pass

# Optimization algorithms
try:
    from .optimization.annealing import *

    __all__.append("annealing")
except ImportError:
    pass

try:
    from .optimization.pathfinding import *

    __all__.append("pathfinding")
except ImportError:
    pass

# Signal processing
try:
    from .signal.convolve import *

    __all__.append("convolve")
except ImportError:
    pass

# Utility functions
try:
    from .utils.error_calibration import *

    __all__.append("error_calibration")
except ImportError:
    pass

try:
    from .utils.fnmatch import *

    __all__.append("fnmatch")
except ImportError:
    pass

try:
    from .utils.snowflake import *

    __all__.append("snowflake")
except ImportError:
    pass

try:
    from .utils.uuid import *

    __all__.append("uuid")
except ImportError:
    pass

try:
    from .utils.weight import *

    __all__.append("weight")
except ImportError:
    pass
