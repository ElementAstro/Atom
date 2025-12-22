"""
Computer vision algorithms.

This submodule provides:
- ComputerVision: CV algorithms
- FeatureDetectorType: Feature detection methods
- ObjectDetectionModel: Object detection models
- FaceModel: Face detection models
- SegmentationMethod: Segmentation algorithms
- Keypoint, Detection, FaceDetection, FeatureMatch: Result structs

These classes are exported in the atom_image.cv submodule by the C++ bindings.
"""

__all__ = [
    "ComputerVision",
    "FeatureDetectorType",
    "ObjectDetectionModel",
    "FaceModel",
    "SegmentationMethod",
    "Keypoint",
    "Detection",
    "FaceDetection",
    "FeatureMatch",
    "createOptimalComputerVision",
]

try:
    from atom_image.cv import (
        ComputerVision,
        Detection,
        FaceDetection,
        FaceModel,
        FeatureDetectorType,
        FeatureMatch,
        Keypoint,
        ObjectDetectionModel,
        SegmentationMethod,
        createOptimalComputerVision,
    )
except ImportError:
    pass
