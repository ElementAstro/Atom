"""
GPU-accelerated image processing.

This submodule provides:
- GPUImageProcessor (ImageProcessor): GPU-accelerated processing
- GPUContext (Context): GPU execution context
- GPUBuffer (Buffer): GPU memory buffer
- GPUBackend (Backend): GPU backend types (CUDA, OpenCL, Vulkan, etc.)
- GPUMemoryType (MemoryType): GPU memory types
- GPUDeviceInfo (DeviceInfo): Device information

These classes are exported in the atom_image.gpu submodule by the C++ bindings.
"""

__all__ = [
    "GPUImageProcessor",
    "GPUContext",
    "GPUBuffer",
    "GPUBackend",
    "GPUMemoryType",
    "GPUDeviceInfo",
    "createOptimalGPUProcessor",
]

try:
    from atom_image.gpu import Backend as GPUBackend
    from atom_image.gpu import Buffer as GPUBuffer
    from atom_image.gpu import Context as GPUContext
    from atom_image.gpu import DeviceInfo as GPUDeviceInfo
    from atom_image.gpu import ImageProcessor as GPUImageProcessor
    from atom_image.gpu import MemoryType as GPUMemoryType
    from atom_image.gpu import createOptimalGPUProcessor
except ImportError:
    pass
