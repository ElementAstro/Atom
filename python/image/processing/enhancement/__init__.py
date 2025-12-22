"""
Image enhancement operations.

This submodule provides:
- ImageEnhancement: Image enhancement operations
- EnhancementParams: Enhancement parameters
- ColorSpace: Color space types
- HistogramMethod: Histogram equalization methods
- ToneMappingOperator: Tone mapping operators
- ColorCorrectionMethod: Color correction methods

These classes are exported in the atom_image.enhancement submodule by the C++ bindings.
"""

__all__ = [
    "ImageEnhancement",
    "EnhancementParams",
    "ColorSpace",
    "HistogramMethod",
    "ToneMappingOperator",
    "ColorCorrectionMethod",
    "createOptimalEnhancement",
]

try:
    from atom_image.enhancement import (
        ColorCorrectionMethod,
        ColorSpace,
        EnhancementParams,
        HistogramMethod,
        ImageEnhancement,
        ToneMappingOperator,
        createOptimalEnhancement,
    )
except ImportError:
    pass
