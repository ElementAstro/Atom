"""
Image filtering operations.

This submodule provides:
- ImageProcessor: Main image processing class
- ImageFilter: Filter operations
- FilterType: Enum of available filter types
- FilterParams: Filter parameters
- ProcessingOptions: Processing configuration
- StructuringElement: Morphological structuring elements

These classes are exported at the atom_image module level by the C++ bindings.
"""

__all__ = [
    "ImageProcessor",
    "ImageFilter",
    "FilterType",
    "FilterParams",
    "ProcessingOptions",
    "StructuringElement",
    "createOptimalFilter",
]

try:
    from atom_image import (
        FilterParams,
        FilterType,
        ImageFilter,
        ImageProcessor,
        ProcessingOptions,
        StructuringElement,
        createOptimalFilter,
    )
except ImportError:
    pass
