#include "atom/sysinfo/cpu.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <thread>

namespace py = pybind11;
using namespace atom::system;
using namespace pybind11::literals;

PYBIND11_MODULE(cpu, m) {
    m.doc() = "CPU information and monitoring module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // CPU Architecture enum
    py::enum_<CpuArchitecture>(m, "CpuArchitecture",
                               "Enumeration of CPU architectures")
        .value("UNKNOWN", CpuArchitecture::UNKNOWN, "Unknown CPU architecture")
        .value("X86", CpuArchitecture::X86, "32-bit x86 architecture")
        .value("X86_64", CpuArchitecture::X86_64, "64-bit x86 architecture")
        .value("ARM", CpuArchitecture::ARM, "32-bit ARM architecture")
        .value("ARM64", CpuArchitecture::ARM64, "64-bit ARM architecture")
        .value("POWERPC", CpuArchitecture::POWERPC, "PowerPC architecture")
        .value("MIPS", CpuArchitecture::MIPS, "MIPS architecture")
        .value("RISC_V", CpuArchitecture::RISC_V, "RISC-V architecture")
        .export_values();

    // CPU Vendor enum
    py::enum_<CpuVendor>(m, "CpuVendor", "Enumeration of CPU vendors")
        .value("UNKNOWN", CpuVendor::UNKNOWN, "Unknown CPU vendor")
        .value("INTEL", CpuVendor::INTEL, "Intel Corporation")
        .value("AMD", CpuVendor::AMD, "Advanced Micro Devices")
        .value("ARM", CpuVendor::ARM, "ARM Holdings")
        .value("APPLE", CpuVendor::APPLE, "Apple Inc.")
        .value("QUALCOMM", CpuVendor::QUALCOMM, "Qualcomm Inc.")
        .value("IBM", CpuVendor::IBM, "International Business Machines")
        .value("MEDIATEK", CpuVendor::MEDIATEK, "MediaTek Inc.")
        .value("SAMSUNG", CpuVendor::SAMSUNG, "Samsung Electronics")
        .value("OTHER", CpuVendor::OTHER, "Other CPU vendor")
        .export_values();

    // CpuFeatureSupport enum
    py::enum_<CpuFeatureSupport>(m, "CpuFeatureSupport",
                                 "CPU feature support status")
        .value("UNKNOWN", CpuFeatureSupport::UNKNOWN, "Unknown support status")
        .value("SUPPORTED", CpuFeatureSupport::SUPPORTED,
               "Feature is supported")
        .value("NOT_SUPPORTED", CpuFeatureSupport::NOT_SUPPORTED,
               "Feature is not supported")
        .export_values();

    // CpuCoreInfo structure
    py::class_<CpuCoreInfo>(m, "CpuCoreInfo",
                            R"(Information about a specific CPU core.

This class provides detailed information about a specific CPU core, including
its frequency, temperature, and utilization.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Get overall CPU info
    >>> info = cpu.get_cpu_info()
    >>> # Access information about the first core
    >>> if info.cores:
    ...     core = info.cores[0]
    ...     print(f"Core {core.id} frequency: {core.current_frequency} GHz")
    ...     print(f"Core {core.id} temperature: {core.temperature}°C")
    ...     print(f"Core {core.id} usage: {core.usage}%")
)")
        .def(py::init<>(), "Constructs a new CpuCoreInfo object.")
        .def_readwrite("id", &CpuCoreInfo::id, "Core ID number")
        .def_readwrite("current_frequency", &CpuCoreInfo::currentFrequency,
                       "Current frequency in GHz")
        .def_readwrite("max_frequency", &CpuCoreInfo::maxFrequency,
                       "Maximum frequency in GHz")
        .def_readwrite("min_frequency", &CpuCoreInfo::minFrequency,
                       "Minimum frequency in GHz")
        .def_readwrite("temperature", &CpuCoreInfo::temperature,
                       "Temperature in Celsius")
        .def_readwrite("usage", &CpuCoreInfo::usage,
                       "Usage percentage (0-100%)")
        .def_readwrite("governor", &CpuCoreInfo::governor,
                       "CPU frequency governor (Linux)")
        .def("__repr__", [](const CpuCoreInfo& info) {
            return "<CpuCoreInfo id=" + std::to_string(info.id) +
                   " freq=" + std::to_string(info.currentFrequency) + "GHz" +
                   " temp=" + std::to_string(info.temperature) + "°C" +
                   " usage=" + std::to_string(info.usage) + "%>";
        });

    // CacheSizes structure
    py::class_<CacheSizes>(m, "CacheSizes",
                           R"(CPU cache size information.

This class provides information about the sizes and characteristics of the
various CPU caches.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Get cache information
    >>> cache_info = cpu.get_cache_sizes()
    >>> print(f"L1 Data Cache: {cache_info.l1d / 1024} KB")
    >>> print(f"L2 Cache: {cache_info.l2 / 1024} KB")
    >>> print(f"L3 Cache: {cache_info.l3 / 1024 / 1024} MB")
)")
        .def(py::init<>(), "Constructs a new CacheSizes object.")
        .def_readwrite("l1d", &CacheSizes::l1d, "L1 data cache size in bytes")
        .def_readwrite("l1i", &CacheSizes::l1i,
                       "L1 instruction cache size in bytes")
        .def_readwrite("l2", &CacheSizes::l2, "L2 cache size in bytes")
        .def_readwrite("l3", &CacheSizes::l3, "L3 cache size in bytes")
        .def_readwrite("l1d_line_size", &CacheSizes::l1d_line_size,
                       "L1 data cache line size")
        .def_readwrite("l1i_line_size", &CacheSizes::l1i_line_size,
                       "L1 instruction cache line size")
        .def_readwrite("l2_line_size", &CacheSizes::l2_line_size,
                       "L2 cache line size")
        .def_readwrite("l3_line_size", &CacheSizes::l3_line_size,
                       "L3 cache line size")
        .def_readwrite("l1d_associativity", &CacheSizes::l1d_associativity,
                       "L1 data cache associativity")
        .def_readwrite("l1i_associativity", &CacheSizes::l1i_associativity,
                       "L1 instruction cache associativity")
        .def_readwrite("l2_associativity", &CacheSizes::l2_associativity,
                       "L2 cache associativity")
        .def_readwrite("l3_associativity", &CacheSizes::l3_associativity,
                       "L3 cache associativity")
        .def("__repr__", [](const CacheSizes& cache) {
            return "<CacheSizes L1d=" + std::to_string(cache.l1d / 1024) +
                   "KB" + " L1i=" + std::to_string(cache.l1i / 1024) + "KB" +
                   " L2=" + std::to_string(cache.l2 / 1024) + "KB" +
                   " L3=" + std::to_string(cache.l3 / 1024 / 1024) + "MB>";
        });

    // LoadAverage structure
    py::class_<LoadAverage>(m, "LoadAverage",
                            R"(System load average information.

This class provides information about system load averages over different time periods.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Get system load averages
    >>> load = cpu.get_cpu_load_average()
    >>> print(f"1-minute load average: {load.one_minute}")
    >>> print(f"5-minute load average: {load.five_minutes}")
    >>> print(f"15-minute load average: {load.fifteen_minutes}")
)")
        .def(py::init<>(), "Constructs a new LoadAverage object.")
        .def_readwrite("one_minute", &LoadAverage::oneMinute,
                       "1-minute load average")
        .def_readwrite("five_minutes", &LoadAverage::fiveMinutes,
                       "5-minute load average")
        .def_readwrite("fifteen_minutes", &LoadAverage::fifteenMinutes,
                       "15-minute load average")
        .def("__repr__", [](const LoadAverage& load) {
            return "<LoadAverage 1min=" + std::to_string(load.oneMinute) +
                   " 5min=" + std::to_string(load.fiveMinutes) +
                   " 15min=" + std::to_string(load.fifteenMinutes) + ">";
        });

    // CpuPowerInfo structure
    py::class_<CpuPowerInfo>(m, "CpuPowerInfo",
                             R"(CPU power consumption information.

This class provides information about the CPU's power consumption and thermal characteristics.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Get CPU power information
    >>> power = cpu.get_cpu_power_info()
    >>> print(f"Current power consumption: {power.current_watts} watts")
    >>> print(f"Maximum TDP: {power.max_tdp} watts")
)")
        .def(py::init<>(), "Constructs a new CpuPowerInfo object.")
        .def_readwrite("current_watts", &CpuPowerInfo::currentWatts,
                       "Current power consumption in watts")
        .def_readwrite("max_tdp", &CpuPowerInfo::maxTDP,
                       "Maximum thermal design power in watts")
        .def_readwrite("energy_impact", &CpuPowerInfo::energyImpact,
                       "Energy impact (where supported)")
        .def("__repr__", [](const CpuPowerInfo& power) {
            return "<CpuPowerInfo current=" +
                   std::to_string(power.currentWatts) + "W" +
                   " max_tdp=" + std::to_string(power.maxTDP) + "W>";
        });

    // CpuInfo structure
    py::class_<CpuInfo>(m, "CpuInfo",
                        R"(Comprehensive CPU information.

This class provides detailed information about the CPU, including model, architecture,
cores, cache sizes, and more.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Get comprehensive CPU information
    >>> info = cpu.get_cpu_info()
    >>> print(f"CPU Model: {info.model}")
    >>> print(f"Vendor: {cpu.cpu_vendor_to_string(info.vendor)}")
    >>> print(f"Architecture: {cpu.cpu_architecture_to_string(info.architecture)}")
    >>> print(f"Cores: {info.num_physical_cores} physical, {info.num_logical_cores} logical")
    >>> print(f"Base frequency: {info.base_frequency} GHz")
    >>> print(f"Current usage: {info.usage}%")
    >>> print(f"Current temperature: {info.temperature}°C")
)")
        .def(py::init<>(), "Constructs a new CpuInfo object.")
        .def_readwrite("model", &CpuInfo::model, "CPU model name")
        .def_readwrite("identifier", &CpuInfo::identifier, "CPU identifier")
        .def_readwrite("architecture", &CpuInfo::architecture,
                       "CPU architecture")
        .def_readwrite("vendor", &CpuInfo::vendor, "CPU vendor")
        .def_readwrite("num_physical_packages", &CpuInfo::numPhysicalPackages,
                       "Number of physical CPU packages")
        .def_readwrite("num_physical_cores", &CpuInfo::numPhysicalCores,
                       "Number of physical CPU cores")
        .def_readwrite("num_logical_cores", &CpuInfo::numLogicalCores,
                       "Number of logical CPU cores (threads)")
        .def_readwrite("base_frequency", &CpuInfo::baseFrequency,
                       "Base frequency in GHz")
        .def_readwrite("max_frequency", &CpuInfo::maxFrequency,
                       "Maximum turbo frequency in GHz")
        .def_readwrite("socket_type", &CpuInfo::socketType, "CPU socket type")
        .def_readwrite("temperature", &CpuInfo::temperature,
                       "Current temperature in Celsius")
        .def_readwrite("usage", &CpuInfo::usage, "Current usage percentage")
        .def_readwrite("caches", &CpuInfo::caches, "Cache sizes")
        .def_readwrite("power", &CpuInfo::power, "Power information")
        .def_readwrite("flags", &CpuInfo::flags, "CPU feature flags")
        .def_readwrite("cores", &CpuInfo::cores, "Per-core information")
        .def_readwrite("load_average", &CpuInfo::loadAverage,
                       "System load average")
        .def_readwrite("instruction_set", &CpuInfo::instructionSet,
                       "Instruction set")
        .def_readwrite("stepping", &CpuInfo::stepping, "CPU stepping")
        .def_readwrite("family", &CpuInfo::family, "CPU family")
        .def_readwrite("model_id", &CpuInfo::model_id, "CPU model ID")
        .def("__repr__", [](const CpuInfo& info) {
            return "<CpuInfo model='" + info.model + "'" +
                   " cores=" + std::to_string(info.numPhysicalCores) + "p/" +
                   std::to_string(info.numLogicalCores) + "l" +
                   " freq=" + std::to_string(info.baseFrequency) + "GHz>";
        });

    // CPU functions
    m.def("get_current_cpu_usage", &getCurrentCpuUsage,
          R"(Get the current CPU usage percentage.

Returns:
    Float representing the current CPU usage as a percentage (0.0 to 100.0).

Examples:
    >>> from atom.sysinfo import cpu
    >>> usage = cpu.get_current_cpu_usage()
    >>> print(f"Current CPU usage: {usage:.1f}%")
)");

    m.def("get_per_core_cpu_usage", &getPerCoreCpuUsage,
          R"(Get per-core CPU usage percentages.

Returns:
    List of floats representing each core's usage percentage.

Examples:
    >>> from atom.sysinfo import cpu
    >>> core_usage = cpu.get_per_core_cpu_usage()
    >>> for i, usage in enumerate(core_usage):
    ...     print(f"Core {i} usage: {usage:.1f}%")
)");

    m.def("get_current_cpu_temperature", &getCurrentCpuTemperature,
          R"(Get the current CPU temperature.

Returns:
    Float representing the CPU temperature in degrees Celsius.

Examples:
    >>> from atom.sysinfo import cpu
    >>> temp = cpu.get_current_cpu_temperature()
    >>> print(f"Current CPU temperature: {temp:.1f}°C")
)");

    m.def("get_per_core_cpu_temperature", &getPerCoreCpuTemperature,
          R"(Get per-core CPU temperatures.

Returns:
    List of floats representing each core's temperature in degrees Celsius.

Examples:
    >>> from atom.sysinfo import cpu
    >>> core_temps = cpu.get_per_core_cpu_temperature()
    >>> for i, temp in enumerate(core_temps):
    ...     print(f"Core {i} temperature: {temp:.1f}°C")
)");

    m.def("get_cpu_model", &getCPUModel,
          R"(Get the CPU model name.

Returns:
    String representing the CPU model name.

Examples:
    >>> from atom.sysinfo import cpu
    >>> model = cpu.get_cpu_model()
    >>> print(f"CPU model: {model}")
)");

    m.def("get_processor_identifier", &getProcessorIdentifier,
          R"(Get the CPU identifier.

Returns:
    String representing the CPU identifier.

Examples:
    >>> from atom.sysinfo import cpu
    >>> identifier = cpu.get_processor_identifier()
    >>> print(f"CPU identifier: {identifier}")
)");

    m.def("get_processor_frequency", &getProcessorFrequency,
          R"(Get the current CPU frequency.

Returns:
    Double representing the CPU frequency in gigahertz (GHz).

Examples:
    >>> from atom.sysinfo import cpu
    >>> freq = cpu.get_processor_frequency()
    >>> print(f"Current CPU frequency: {freq:.2f} GHz")
)");

    m.def("get_min_processor_frequency", &getMinProcessorFrequency,
          R"(Get the minimum CPU frequency.

Returns:
    Double representing the minimum CPU frequency in gigahertz (GHz).

Examples:
    >>> from atom.sysinfo import cpu
    >>> min_freq = cpu.get_min_processor_frequency()
    >>> print(f"Minimum CPU frequency: {min_freq:.2f} GHz")
)");

    m.def("get_max_processor_frequency", &getMaxProcessorFrequency,
          R"(Get the maximum CPU frequency.

Returns:
    Double representing the maximum CPU frequency in gigahertz (GHz).

Examples:
    >>> from atom.sysinfo import cpu
    >>> max_freq = cpu.get_max_processor_frequency()
    >>> print(f"Maximum CPU frequency: {max_freq:.2f} GHz")
)");

    m.def("get_per_core_frequencies", &getPerCoreFrequencies,
          R"(Get per-core CPU frequencies.

Returns:
    List of doubles representing each core's current frequency in GHz.

Examples:
    >>> from atom.sysinfo import cpu
    >>> core_freqs = cpu.get_per_core_frequencies()
    >>> for i, freq in enumerate(core_freqs):
    ...     print(f"Core {i} frequency: {freq:.2f} GHz")
)");

    m.def("get_number_of_physical_packages", &getNumberOfPhysicalPackages,
          R"(Get the number of physical CPU packages.

Returns:
    Integer representing the number of physical CPU packages.

Examples:
    >>> from atom.sysinfo import cpu
    >>> packages = cpu.get_number_of_physical_packages()
    >>> print(f"Number of physical CPU packages: {packages}")
)");

    m.def("get_number_of_physical_cores", &getNumberOfPhysicalCores,
          R"(Get the number of physical CPU cores.

Returns:
    Integer representing the total number of physical CPU cores.

Examples:
    >>> from atom.sysinfo import cpu
    >>> cores = cpu.get_number_of_physical_cores()
    >>> print(f"Number of physical CPU cores: {cores}")
)");

    m.def("get_number_of_logical_cores", &getNumberOfLogicalCores,
          R"(Get the number of logical CPUs (cores).

Returns:
    Integer representing the total number of logical CPUs (cores).

Examples:
    >>> from atom.sysinfo import cpu
    >>> logical_cores = cpu.get_number_of_logical_cores()
    >>> print(f"Number of logical CPU cores: {logical_cores}")
)");

    m.def("get_cache_sizes", &getCacheSizes,
          R"(Get the sizes of the CPU caches (L1, L2, L3).

Returns:
    CacheSizes structure containing the sizes of the L1, L2, and L3 caches.

Examples:
    >>> from atom.sysinfo import cpu
    >>> cache = cpu.get_cache_sizes()
    >>> print(f"L1 data cache: {cache.l1d / 1024} KB")
    >>> print(f"L2 cache: {cache.l2 / 1024} KB")
    >>> print(f"L3 cache: {cache.l3 / (1024 * 1024)} MB")
)");

    m.def("get_cpu_load_average", &getCpuLoadAverage,
          R"(Get the CPU load average.

Returns:
    LoadAverage structure with 1, 5, and 15-minute load averages.

Examples:
    >>> from atom.sysinfo import cpu
    >>> load = cpu.get_cpu_load_average()
    >>> print(f"1-minute load average: {load.one_minute:.2f}")
    >>> print(f"5-minute load average: {load.five_minutes:.2f}")
    >>> print(f"15-minute load average: {load.fifteen_minutes:.2f}")
)");

    m.def("get_cpu_power_info", &getCpuPowerInfo,
          R"(Get CPU power consumption information.

Returns:
    CpuPowerInfo structure with power consumption details.

Examples:
    >>> from atom.sysinfo import cpu
    >>> power = cpu.get_cpu_power_info()
    >>> print(f"Current power consumption: {power.current_watts:.2f} watts")
    >>> print(f"Maximum TDP: {power.max_tdp:.2f} watts")
)");

    m.def("get_cpu_feature_flags", &getCpuFeatureFlags,
          R"(Get all CPU feature flags.

Returns:
    List of strings representing all CPU feature flags.

Examples:
    >>> from atom.sysinfo import cpu
    >>> flags = cpu.get_cpu_feature_flags()
    >>> print("CPU supports the following features:")
    >>> for flag in flags:
    ...     print(f"- {flag}")
)");

    m.def("is_cpu_feature_supported", &isCpuFeatureSupported,
          py::arg("feature"),
          R"(Check if a specific CPU feature is supported.

Args:
    feature: The name of the feature to check.

Returns:
    CpuFeatureSupport enum indicating if the feature is supported.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Check if CPU supports AVX2
    >>> support = cpu.is_cpu_feature_supported("avx2")
    >>> if support == cpu.CpuFeatureSupport.SUPPORTED:
    ...     print("CPU supports AVX2")
    ... elif support == cpu.CpuFeatureSupport.NOT_SUPPORTED:
    ...     print("CPU does not support AVX2")
    ... else:
    ...     print("Could not determine AVX2 support")
)");

    m.def("get_cpu_architecture", &getCpuArchitecture,
          R"(Get the CPU architecture.

Returns:
    CpuArchitecture enum representing the CPU architecture.

Examples:
    >>> from atom.sysinfo import cpu
    >>> arch = cpu.get_cpu_architecture()
    >>> print(f"CPU architecture: {cpu.cpu_architecture_to_string(arch)}")
)");

    m.def("get_cpu_vendor", &getCpuVendor,
          R"(Get the CPU vendor.

Returns:
    CpuVendor enum representing the CPU vendor.

Examples:
    >>> from atom.sysinfo import cpu
    >>> vendor = cpu.get_cpu_vendor()
    >>> print(f"CPU vendor: {cpu.cpu_vendor_to_string(vendor)}")
)");

    m.def("get_cpu_socket_type", &getCpuSocketType,
          R"(Get the CPU socket type.

Returns:
    String representing the CPU socket type.

Examples:
    >>> from atom.sysinfo import cpu
    >>> socket = cpu.get_cpu_socket_type()
    >>> print(f"CPU socket type: {socket}")
)");

    m.def("get_cpu_scaling_governor", &getCpuScalingGovernor,
          R"(Get the CPU scaling governor (Linux) or power mode (Windows/macOS).

Returns:
    String representing the current CPU scaling governor or power mode.

Examples:
    >>> from atom.sysinfo import cpu
    >>> governor = cpu.get_cpu_scaling_governor()
    >>> print(f"CPU scaling governor: {governor}")
)");

    m.def("get_per_core_scaling_governors", &getPerCoreScalingGovernors,
          R"(Get per-core CPU scaling governors (Linux only).

Returns:
    List of strings representing each core's scaling governor.

Examples:
    >>> from atom.sysinfo import cpu
    >>> governors = cpu.get_per_core_scaling_governors()
    >>> for i, gov in enumerate(governors):
    ...     print(f"Core {i} governor: {gov}")
)");

    m.def("get_cpu_info", &getCpuInfo,
          R"(Get comprehensive CPU information.

Returns:
    CpuInfo structure containing detailed CPU information.

Examples:
    >>> from atom.sysinfo import cpu
    >>> info = cpu.get_cpu_info()
    >>> print(f"CPU Model: {info.model}")
    >>> print(f"Cores: {info.num_physical_cores} physical, {info.num_logical_cores} logical")
    >>> print(f"Base frequency: {info.base_frequency} GHz")
    >>> print(f"Current temperature: {info.temperature}°C")
    >>> # Check if CPU supports AVX
    >>> has_avx = "avx" in info.flags
    >>> print(f"Supports AVX: {has_avx}")
)");

    m.def("cpu_architecture_to_string", &cpuArchitectureToString,
          py::arg("arch"),
          R"(Convert CPU architecture enum to string.

Args:
    arch: The CPU architecture enum.

Returns:
    String representation of the CPU architecture.

Examples:
    >>> from atom.sysinfo import cpu
    >>> arch = cpu.get_cpu_architecture()
    >>> arch_name = cpu.cpu_architecture_to_string(arch)
    >>> print(f"CPU architecture: {arch_name}")
)");

    m.def("cpu_vendor_to_string", &cpuVendorToString, py::arg("vendor"),
          R"(Convert CPU vendor enum to string.

Args:
    vendor: The CPU vendor enum.

Returns:
    String representation of the CPU vendor.

Examples:
    >>> from atom.sysinfo import cpu
    >>> vendor = cpu.get_cpu_vendor()
    >>> vendor_name = cpu.cpu_vendor_to_string(vendor)
    >>> print(f"CPU vendor: {vendor_name}")
)");

    m.def("refresh_cpu_info", &refreshCpuInfo,
          R"(Refresh all cached CPU information.

Forces a refresh of any cached CPU information.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Force refresh of CPU info
    >>> cpu.refresh_cpu_info()
    >>> # Now get updated information
    >>> info = cpu.get_cpu_info()
)");

    // Additional utility functions
    m.def(
        "is_hyper_threading_enabled",
        []() {
            int physical = getNumberOfPhysicalCores();
            int logical = getNumberOfLogicalCores();
            return logical > physical;
        },
        R"(Check if hyper-threading is enabled.

Returns:
    Boolean indicating whether hyper-threading is enabled.

Examples:
    >>> from atom.sysinfo import cpu
    >>> if cpu.is_hyper_threading_enabled():
    ...     print("Hyper-threading is enabled")
    ... else:
    ...     print("Hyper-threading is disabled or not available")
)");

    m.def(
        "get_cpu_summary",
        []() {
            auto info = getCpuInfo();

            py::dict summary;
            summary["model"] = info.model;
            summary["vendor"] = cpuVendorToString(info.vendor);
            summary["architecture"] =
                cpuArchitectureToString(info.architecture);
            summary["physical_cores"] = info.numPhysicalCores;
            summary["logical_cores"] = info.numLogicalCores;
            summary["frequency"] = info.baseFrequency;
            summary["max_frequency"] = info.maxFrequency;
            summary["temperature"] = info.temperature;
            summary["usage"] = info.usage;
            summary["socket"] = info.socketType;

            // Add cache information in MB
            py::dict cache_info;
            cache_info["l1d"] = info.caches.l1d / 1024.0 / 1024.0;
            cache_info["l1i"] = info.caches.l1i / 1024.0 / 1024.0;
            cache_info["l2"] = info.caches.l2 / 1024.0 / 1024.0;
            cache_info["l3"] = info.caches.l3 / 1024.0 / 1024.0;
            summary["cache"] = cache_info;

            // Key features
            py::list key_features;
            for (const auto& feature :
                 {"avx", "avx2", "avx512", "sse4.1", "sse4.2", "aes",
                  "pclmulqdq", "bmi1", "bmi2"}) {
                bool has_feature =
                    (std::find(info.flags.begin(), info.flags.end(), feature) !=
                     info.flags.end());
                if (has_feature) {
                    key_features.append(feature);
                }
            }
            summary["key_features"] = key_features;

            return summary;
        },
        R"(Get a comprehensive summary of CPU information.

Returns:
    Dictionary containing CPU details in an easy-to-use format.

Examples:
    >>> from atom.sysinfo import cpu
    >>> import pprint
    >>> # Get comprehensive CPU summary
    >>> summary = cpu.get_cpu_summary()
    >>> pprint.pprint(summary)
)");

    // Context manager for CPU monitoring
    py::class_<py::object>(m, "CpuMonitorContext")
        .def(
            py::init([](double interval_sec, py::function callback) {
                return py::object();  // Placeholder, actual implementation in
                                      // __enter__
            }),
            py::arg("interval_sec") = 1.0, py::arg("callback"),
            "Create a context manager for monitoring CPU usage and temperature")
        .def("__enter__",
             [](py::object& self, double interval_sec, py::function callback) {
                 // Import needed Python modules
                 py::module_ threading = py::module_::import("threading");
                 py::module_ time = py::module_::import("time");

                 self.attr("running") = py::bool_(true);
                 self.attr("interval") = py::float_(interval_sec);
                 self.attr("callback") = callback;

                 // Start a monitoring thread
                 auto monitor_func = [](py::object self) {
                     try {
                         py::gil_scoped_release release;  // Release GIL

                         double interval =
                             py::cast<double>(self.attr("interval"));

                         while (py::cast<bool>(self.attr("running"))) {
                             // Get CPU information
                             float usage = getCurrentCpuUsage();
                             float temperature = getCurrentCpuTemperature();
                             double frequency = getProcessorFrequency();

                             std::vector<float> core_usage =
                                 getPerCoreCpuUsage();
                             std::vector<float> core_temps =
                                 getPerCoreCpuTemperature();
                             std::vector<double> core_freqs =
                                 getPerCoreFrequencies();

                             // Call the callback with the collected data
                             {
                                 py::gil_scoped_acquire
                                     acquire;  // Reacquire GIL for Python calls
                                 try {
                                     self.attr("callback")(
                                         usage, temperature, frequency,
                                         core_usage, core_temps, core_freqs);
                                 } catch (py::error_already_set& e) {
                                     // Log but don't propagate the error
                                     PyErr_Print();
                                 }
                             }

                             // Wait for the specified interval
                             std::this_thread::sleep_for(
                                 std::chrono::milliseconds(
                                     static_cast<int>(interval * 1000)));
                         }
                     } catch (const std::exception& e) {
                         py::gil_scoped_acquire acquire;
                         try {
                             py::print("Error in CPU monitor thread:",
                                       e.what());
                         } catch (...) {
                             // Ignore any errors from print
                         }
                     }
                 };

                 // Create and start the thread
                 auto kwargs = py::dict("target"_a = monitor_func,
                                        "args"_a = py::make_tuple(self));
                 self.attr("thread") = threading.attr("Thread")(**kwargs);
                 self.attr("thread").attr("daemon") = py::bool_(true);
                 self.attr("thread").attr("start")();

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 self.attr("running") = py::bool_(false);

                 // Wait for the thread to finish (with timeout)
                 py::module_ time = py::module_::import("time");
                 py::object thread = self.attr("thread");

                 if (py::hasattr(thread, "is_alive") &&
                     py::cast<bool>(thread.attr("is_alive")())) {
                     double timeout = 1.0;  // 1 second timeout
                     thread.attr("join")(timeout);
                 }

                 return py::bool_(false);  // Don't suppress exceptions
             });

    // Factory function for CPU monitor context
    m.def(
        "monitor_cpu",
        [&m](double interval_sec, py::function callback) {
            return m.attr("CpuMonitorContext")(interval_sec, callback);
        },
        py::arg("interval_sec") = 1.0, py::arg("callback"),
        R"(Create a context manager for monitoring CPU parameters.

This function returns a context manager that periodically monitors CPU
usage, temperature, and frequency and calls the provided callback with this data.

Args:
    interval_sec: How often to check CPU status, in seconds (default: 1.0).
    callback: Function to call with CPU data. The callback receives six arguments:
              usage (float), temperature (float), frequency (float),
              core_usage (list), core_temperatures (list), core_frequencies (list).

Returns:
    A context manager for CPU monitoring.

Examples:
    >>> from atom.sysinfo import cpu
    >>> import time
    >>>
    >>> # Define a callback function
    >>> def cpu_callback(usage, temp, freq, core_usage, core_temps, core_freqs):
    ...     print(f"CPU Usage: {usage:.1f}%, Temp: {temp:.1f}°C, Freq: {freq:.2f} GHz")
    ...
    >>> # Use as a context manager
    >>> with cpu.monitor_cpu(0.5, cpu_callback):
    ...     print("Monitoring CPU for 5 seconds...")
    ...     time.sleep(5)
    ...
    >>> print("Monitoring stopped")
)");

    // Function to check if CPU is under high load
    m.def(
        "is_cpu_under_high_load",
        [](float threshold) {
            float usage = getCurrentCpuUsage();
            return usage > threshold;
        },
        py::arg("threshold") = 80.0f,
        R"(Check if CPU is under high load.

Args:
    threshold: Usage percentage threshold to consider as high load (default: 80.0)

Returns:
    Boolean indicating whether CPU usage is above the threshold.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Check if CPU usage is above 90%
    >>> if cpu.is_cpu_under_high_load(90.0):
    ...     print("CPU is under very high load!")
)");

    // Function to check if CPU is overheating
    m.def(
        "is_cpu_overheating",
        [](float threshold) {
            float temp = getCurrentCpuTemperature();
            return temp > threshold;
        },
        py::arg("threshold") = 85.0f,
        R"(Check if CPU is overheating.

Args:
    threshold: Temperature threshold in Celsius to consider as overheating (default: 85.0)

Returns:
    Boolean indicating whether CPU temperature is above the threshold.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Check if CPU temperature is above 90°C
    >>> if cpu.is_cpu_overheating(90.0):
    ...     print("CPU is overheating!")
)");

    // Convenience function to get current CPU status
    m.def(
        "get_cpu_status",
        []() {
            py::dict status;
            status["usage"] = getCurrentCpuUsage();
            status["temperature"] = getCurrentCpuTemperature();
            status["frequency"] = getProcessorFrequency();
            status["load"] = getCpuLoadAverage().oneMinute;

            return status;
        },
        R"(Get current CPU status including usage, temperature, and frequency.

Returns:
    Dictionary containing current CPU status.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Get current CPU status
    >>> status = cpu.get_cpu_status()
    >>> print(f"CPU: {status['usage']:.1f}% at {status['temperature']:.1f}°C, {status['frequency']:.2f} GHz")
)");

    // Function to check if CPU instruction set is supported
    m.def(
        "is_instruction_set_supported",
        [](const std::string& instruction_set) {
            auto features = getCpuFeatureFlags();
            bool supported = false;

            // Check for each instruction set
            if (instruction_set == "sse") {
                supported = std::find(features.begin(), features.end(),
                                      "sse") != features.end();
            } else if (instruction_set == "sse2") {
                supported = std::find(features.begin(), features.end(),
                                      "sse2") != features.end();
            } else if (instruction_set == "sse3") {
                supported = std::find(features.begin(), features.end(),
                                      "sse3") != features.end();
            } else if (instruction_set == "ssse3") {
                supported = std::find(features.begin(), features.end(),
                                      "ssse3") != features.end();
            } else if (instruction_set == "sse4.1") {
                supported = std::find(features.begin(), features.end(),
                                      "sse4_1") != features.end() ||
                            std::find(features.begin(), features.end(),
                                      "sse4.1") != features.end();
            } else if (instruction_set == "sse4.2") {
                supported = std::find(features.begin(), features.end(),
                                      "sse4_2") != features.end() ||
                            std::find(features.begin(), features.end(),
                                      "sse4.2") != features.end();
            } else if (instruction_set == "avx") {
                supported = std::find(features.begin(), features.end(),
                                      "avx") != features.end();
            } else if (instruction_set == "avx2") {
                supported = std::find(features.begin(), features.end(),
                                      "avx2") != features.end();
            } else if (instruction_set == "avx512") {
                supported = std::find_if(features.begin(), features.end(),
                                         [](const std::string& s) {
                                             return s.find("avx512") !=
                                                    std::string::npos;
                                         }) != features.end();
            } else if (instruction_set == "neon") {
                // ARM-specific
                supported = std::find(features.begin(), features.end(),
                                      "neon") != features.end();
            }

            return supported;
        },
        py::arg("instruction_set"),
        R"(Check if a specific CPU instruction set is supported.

Args:
    instruction_set: Name of the instruction set to check (e.g., "avx", "sse4.1")

Returns:
    Boolean indicating whether the instruction set is supported.

Examples:
    >>> from atom.sysinfo import cpu
    >>> # Check support for various instruction sets
    >>> avx_support = cpu.is_instruction_set_supported("avx")
    >>> avx2_support = cpu.is_instruction_set_supported("avx2")
    >>> print(f"AVX support: {avx_support}")
    >>> print(f"AVX2 support: {avx2_support}")
)");
}
