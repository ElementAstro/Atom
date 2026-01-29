"""
Geometric transformation operations.

This submodule provides:
- ImageTransform: Geometric transformations
- InterpolationMethod: Interpolation types
- BorderMode: Border handling modes
- Point2D, Rectangle: Geometry primitives

These classes are exported in the atom_image.transforms submodule by the C++ bindings.
"""

__all__ = [
    "ImageTransform",
    "InterpolationMethod",
    "BorderMode",
    "Point2D",
    "Rectangle",
    "createOptimalTransform",
]

try:
    from atom_image.transforms import (
        BorderMode,
        ImageTransform,
        InterpolationMethod,
        Point2D,
        Rectangle,
        createOptimalTransform,
    )
except ImportError:
    pass
