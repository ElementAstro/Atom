"""
Machine learning-based image processing.

This submodule provides:
- MLImageProcessor (ImageProcessor): ML-based processing
- MLModelType (ModelType): Supported model types
- MLBackend (Backend): ML inference backends
- MLParams (Params): Processing parameters
- MLResult (Result): Processing results

These classes are exported in the atom_image.ml submodule by the C++ bindings.
"""

__all__ = [
    "MLImageProcessor",
    "MLModelType",
    "MLBackend",
    "MLParams",
    "MLResult",
    "createOptimalMLProcessor",
]

try:
    from atom_image.ml import Backend as MLBackend
    from atom_image.ml import ImageProcessor as MLImageProcessor
    from atom_image.ml import ModelType as MLModelType
    from atom_image.ml import Params as MLParams
    from atom_image.ml import Result as MLResult
    from atom_image.ml import createOptimalMLProcessor
except ImportError:
    pass
