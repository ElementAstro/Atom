"""
Image processing operations.

This submodule provides comprehensive image processing capabilities organized
into specialized sub-packages accessible via the C++ extension module submodules.

Submodules (accessed via atom_image):
--------------------------------------
- transforms: Geometric transformations (ImageTransform, InterpolationMethod, etc.)
- enhancement: Image enhancement (ImageEnhancement, EnhancementParams, etc.)
- cv: Computer vision (ComputerVision, FeatureDetectorType, etc.)
- gpu: GPU acceleration (GPUImageProcessor, GPUContext, etc.)
- ml: ML processing (MLImageProcessor, MLModelType, etc.)
- realtime: Real-time processing (RealtimeProcessor, CaptureSource, etc.)
- ocr: OCR capabilities (OCREngine, OCRLanguage, etc.) - if available

Main Module Exports:
--------------------
- ImageProcessor: Main image processing class
- ImageFilter: Filter operations
- FilterType: Enum of available filter types
- FilterParams: Filter parameters
- ProcessingOptions: Processing configuration
- StructuringElement: Morphological structuring elements
"""

# All processing-related classes are exported at the atom_image module level
# The submodules (transforms, enhancement, cv, gpu, ml, realtime, ocr) are
# created as submodules of atom_image by the C++ bindings

__all__ = [
    # Main processing classes (exported at atom_image level)
    "ImageProcessor",
    "ImageFilter",
    "FilterType",
    "FilterParams",
    "ProcessingOptions",
    "StructuringElement",
    "createOptimalFilter",
]

# Re-export from atom_image for convenience
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
