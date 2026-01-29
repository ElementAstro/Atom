"""
Image metadata handling.

This submodule provides:
- ExifParser: EXIF data parser
- ExifData: EXIF metadata container
- GpsCoordinate: GPS coordinate representation
- ExifException: EXIF parsing errors
- Convenience functions: extract_exif, has_exif, get_gps_coordinates, get_camera_info
"""

from atom_image import (
    ExifData,
    ExifException,
    ExifParser,
    GpsCoordinate,
    extract_exif,
    get_camera_info,
    get_gps_coordinates,
    has_exif,
)

__all__ = [
    "ExifParser",
    "ExifData",
    "GpsCoordinate",
    "ExifException",
    "extract_exif",
    "has_exif",
    "get_gps_coordinates",
    "get_camera_info",
]
