"""
Specialized image format support.

This submodule provides:
- FITSFile, HDU, FITSHeader: FITS file support
- FITSErrorCode, FITSFileException: FITS errors
- SERReader (if OpenCV available): SER file support
"""

from atom_image import HDU, FITSErrorCode, FITSFile, FITSFileException, FITSHeader

__all__ = [
    "FITSFile",
    "HDU",
    "FITSHeader",
    "FITSErrorCode",
    "FITSFileException",
]

# Conditional SER format imports (requires OpenCV)
try:
    from atom_image import SERReader, SERWriter

    __all__.extend(["SERReader", "SERWriter"])
except ImportError:
    pass
