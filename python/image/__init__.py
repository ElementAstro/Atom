"""
Atom Image Processing Module
=============================

This module provides comprehensive image processing capabilities including:
- Core image data structures (Blob)
- Image I/O operations (loading, saving, format detection)
- Image processing (filters, transforms, enhancement)
- Format support (FITS, SER, standard formats)
- Metadata handling (EXIF)
- Computer vision operations
- GPU acceleration (when available)
- OCR capabilities (when available)

Submodules:
-----------
- core: Core image data structures and utilities
- io: Image loading and saving operations
- processing: Image processing algorithms
- formats: Specialized format handlers (FITS, SER)
- metadata: Metadata extraction and manipulation

Example:
--------
>>> from atom_image import Blob, ImageLoader, ImageProcessor
>>>
>>> # Load an image
>>> loader = ImageLoader()
>>> result = loader.load_from_file("image.jpg")
>>> if result.success:
>>>     img = result.image_data
>>>
>>>     # Process the image
>>>     processor = ImageProcessor()
>>>     resized = processor.resize(img, 800, 600)
>>>     filtered = processor.apply_filter(resized, FilterType.GAUSSIAN_BLUR)
>>>
>>>     # Save the result
>>>     filtered.save("output.jpg")
"""

# Import main classes and functions from the C++ module
try:
    from atom_image import *  # noqa: F401, F403
except ImportError as e:
    import warnings

    warnings.warn(f"Failed to import atom_image C++ module: {e}", stacklevel=2)

__version__ = "1.0.0"
__author__ = "Atom Framework Team"
