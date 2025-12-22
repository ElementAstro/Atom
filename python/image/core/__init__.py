"""
Core image data structures and exceptions.

This submodule provides:
- Blob: Image data container with pixel data access
- ImageMetadata: Image metadata container
- BlobMode: Memory management modes
- Exception classes for image operations
"""

from atom_image import (  # Exceptions
    Blob,
    BlobMode,
    CodecException,
    ConfigurationException,
    FormatException,
    ImageException,
    ImageMetadata,
    IOException,
    LoadException,
    MemoryException,
    MetadataKeys,
    OutOfBoundsException,
    ProcessingException,
    SaveException,
    TimeoutException,
    UnsupportedOperationException,
    ValidationException,
)

__all__ = [
    "Blob",
    "BlobMode",
    "ImageMetadata",
    "MetadataKeys",
    "ImageException",
    "FormatException",
    "LoadException",
    "SaveException",
    "ProcessingException",
    "MemoryException",
    "ValidationException",
    "IOException",
    "CodecException",
    "OutOfBoundsException",
    "UnsupportedOperationException",
    "TimeoutException",
    "ConfigurationException",
]
