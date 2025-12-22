"""
Atom Image Processing Module
=============================

This module provides comprehensive image processing capabilities organized
into submodules that mirror the C++ library structure:

Submodules:
-----------
- **core**: Core image data structures (Blob, ImageMetadata, exceptions)
- **io**: Image loading/saving (ImageLoader, ImageSaver, FormatDetector)
- **formats**: Specialized formats (FITS, SER, advanced formats)
- **metadata**: EXIF metadata handling (ExifParser, ExifData)
- **processing**: Image processing operations
  - **filters**: Filter operations (ImageProcessor, ImageFilter)
  - **transforms**: Geometric transformations (ImageTransform)
  - **enhancement**: Image enhancement (ImageEnhancement)
  - **cv**: Computer vision algorithms (ComputerVision)
  - **gpu**: GPU-accelerated processing (GPUImageProcessor)
  - **ml**: Machine learning processing (MLImageProcessor)
  - **realtime**: Real-time video processing (RealtimeProcessor)
  - **ocr**: OCR capabilities (OCREngine) - if available

Example:
--------
>>> from atom.image import Blob, ImageLoader, ImageProcessor, FilterType
>>>
>>> # Load an image
>>> loader = ImageLoader()
>>> result = loader.loadFromFile("image.jpg")
>>> if result.success:
>>>     img = result.image
>>>
>>>     # Process the image
>>>     processor = ImageProcessor()
>>>     resized = processor.resize(img, 800, 600)
>>>     filtered = processor.applyFilter(resized, FilterType.GAUSSIAN_BLUR)
>>>
>>>     # Save the result
>>>     saver = ImageSaver()
>>>     saver.saveToFile(filtered, "output.jpg")

Submodule Usage:
----------------
>>> from atom.image.core import Blob, ImageMetadata
>>> from atom.image.io import ImageLoader, ImageSaver
>>> from atom.image.processing.transforms import ImageTransform
>>> from atom.image.processing.cv import ComputerVision
"""

# Import submodules
from . import core, formats, io, metadata, processing

# Import main classes and functions from the C++ module for convenience
try:
    # Re-export commonly used classes at top level
    from atom_image import *  # noqa: F401, F403
    from atom_image import (  # Version/Features; Core; Exceptions; I/O; Processing
        Blob,
        BlobMode,
        Features,
        FilterType,
        FormatDetector,
        FormatException,
        ImageException,
        ImageFormat,
        ImageLoader,
        ImageMetadata,
        ImageProcessor,
        ImageSaver,
        LoadException,
        LoadOptions,
        LoadResult,
        ProcessingException,
        ProcessingOptions,
        SaveException,
        SaveOptions,
        SaveResult,
        StructuringElement,
        Version,
        get_features,
        get_version,
    )
except ImportError as e:
    import warnings

    warnings.warn(f"Failed to import atom_image C++ module: {e}", stacklevel=2)

__version__ = "1.0.0"
__author__ = "Atom Framework Team"

__all__ = [
    # Submodules
    "core",
    "io",
    "formats",
    "metadata",
    "processing",
    # Version/Features
    "Version",
    "Features",
    "get_version",
    "get_features",
    # Core
    "Blob",
    "BlobMode",
    "ImageMetadata",
    "ImageException",
    "FormatException",
    "LoadException",
    "SaveException",
    "ProcessingException",
    # I/O
    "ImageLoader",
    "ImageSaver",
    "FormatDetector",
    "ImageFormat",
    "LoadOptions",
    "SaveOptions",
    "LoadResult",
    "SaveResult",
    # Processing
    "ImageProcessor",
    "FilterType",
    "ProcessingOptions",
    "StructuringElement",
]
