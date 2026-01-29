#include "atom/algorithm/core/opencl_utils.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(opencl_utils, m) {
    m.doc() = R"pbdoc(
        OpenCL Utilities
        ----------------

        This module provides OpenCL acceleration utilities for compute-intensive
        algorithms. OpenCL support must be enabled at compile time.

        Note: This module requires OpenCL to be available at runtime.
    )pbdoc";

#if ATOM_OPENCL_AVAILABLE
    using namespace atom::algorithm::opencl;

    // Enumerations
    py::enum_<DeviceType>(m, "DeviceType", "OpenCL device types")
        .value("CPU", DeviceType::CPU, "CPU device")
        .value("GPU", DeviceType::GPU, "GPU device")
        .value("ACCELERATOR", DeviceType::ACCELERATOR, "Accelerator device")
        .value("ALL", DeviceType::ALL, "All device types")
        .export_values();

    py::enum_<MemoryFlags>(m, "MemoryFlags", "OpenCL memory allocation flags")
        .value("READ_ONLY", MemoryFlags::READ_ONLY, "Read-only memory")
        .value("WRITE_ONLY", MemoryFlags::WRITE_ONLY, "Write-only memory")
        .value("READ_WRITE", MemoryFlags::READ_WRITE, "Read-write memory")
        .value("USE_HOST_PTR", MemoryFlags::USE_HOST_PTR, "Use host pointer")
        .value("ALLOC_HOST_PTR", MemoryFlags::ALLOC_HOST_PTR,
               "Allocate host pointer")
        .value("COPY_HOST_PTR", MemoryFlags::COPY_HOST_PTR, "Copy host pointer")
        .export_values();

    // DeviceInfo structure
    py::class_<DeviceInfo>(m, "DeviceInfo",
                           "Information about an OpenCL device")
        .def(py::init<>())
        .def_readwrite("name", &DeviceInfo::name, "Device name")
        .def_readwrite("vendor", &DeviceInfo::vendor, "Device vendor")
        .def_readwrite("version", &DeviceInfo::version, "OpenCL version")
        .def_readwrite("type", &DeviceInfo::type, "Device type")
        .def_readwrite("max_compute_units", &DeviceInfo::max_compute_units,
                       "Maximum compute units")
        .def_readwrite("max_work_group_size", &DeviceInfo::max_work_group_size,
                       "Maximum work group size")
        .def_readwrite("global_memory_size", &DeviceInfo::global_memory_size,
                       "Global memory size in bytes")
        .def_readwrite("local_memory_size", &DeviceInfo::local_memory_size,
                       "Local memory size in bytes")
        .def_readwrite("supports_double", &DeviceInfo::supports_double,
                       "Whether double precision is supported")
        .def("__repr__", [](const DeviceInfo& info) {
            return "<DeviceInfo name='" + info.name + "' vendor='" +
                   info.vendor + "'>";
        });

    // Context wrapper - note: we cannot fully bind this due to move-only
    // semantics and OpenCL handle management, but we can expose it for type
    // checking
    py::class_<Context>(m, "Context",
                        "OpenCL context wrapper (move-only resource)")
        .def("valid", &Context::valid, "Check if context is valid");

    // CommandQueue wrapper
    py::class_<CommandQueue>(
        m, "CommandQueue", "OpenCL command queue wrapper (move-only resource)")
        .def("valid", &CommandQueue::valid, "Check if command queue is valid");

    // Buffer wrapper
    py::class_<Buffer>(m, "Buffer",
                       "OpenCL buffer wrapper (move-only resource)")
        .def("valid", &Buffer::valid, "Check if buffer is valid");

    // Kernel wrapper
    py::class_<Kernel>(m, "Kernel",
                       "OpenCL kernel wrapper (move-only resource)")
        .def("valid", &Kernel::valid, "Check if kernel is valid");

    // Platform class - static methods only
    py::class_<Platform>(m, "Platform", "OpenCL platform management utilities")
        .def_static(
            "get_platforms",
            []() {
                auto platforms = Platform::getPlatforms();
                return py::cast(platforms.size());
            },
            "Get number of available OpenCL platforms")
        .def_static(
            "get_device_info",
            [](size_t platform_idx, size_t device_idx) -> DeviceInfo {
                auto platforms = Platform::getPlatforms();
                if (platform_idx >= platforms.size()) {
                    throw std::out_of_range("Platform index out of range");
                }
                auto devices = Platform::getDevices(platforms[platform_idx],
                                                    DeviceType::ALL);
                if (device_idx >= devices.size()) {
                    throw std::out_of_range("Device index out of range");
                }
                return Platform::getDeviceInfo(devices[device_idx]);
            },
            py::arg("platform_idx"), py::arg("device_idx"),
            "Get device information by platform and device indices");

    m.attr("opencl_available") = true;
#else
    m.attr("opencl_available") = false;
    m.def("get_platforms", []() {
        throw std::runtime_error("OpenCL support not enabled at compile time");
    });
#endif
}
