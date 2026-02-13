# isort: skip_file
"""
Real-time video processing.

This submodule provides:
- RealtimeProcessor (Processor): Real-time video processing
- CaptureSource: Video capture sources
- ProcessingMode: Processing modes
- FrameInfo: Frame information
- ProcessingStats: Processing statistics
- RealtimeParams (Params): Processing parameters

These classes are exported in the atom_image.realtime submodule by the C++ bindings.
"""

__all__ = [
    "RealtimeProcessor",
    "CaptureSource",
    "ProcessingMode",
    "FrameInfo",
    "ProcessingStats",
    "RealtimeParams",
    "process_video",
    "capture_frames",
    "stream_to_callback",
]

try:
    from atom_image.realtime import (
        CaptureSource,
        FrameInfo,
        ProcessingMode,
        ProcessingStats,
        capture_frames,
        process_video,
        stream_to_callback,
    )
    from atom_image.realtime import Params as RealtimeParams
    from atom_image.realtime import Processor as RealtimeProcessor
except ImportError:
    pass
