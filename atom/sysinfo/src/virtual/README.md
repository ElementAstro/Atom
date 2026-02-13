# Virtual Environment Detection Module

This module provides comprehensive detection and analysis of virtualization and containerization environments. It has been completely redesigned from the original monolithic `virtual.cpp` into a modular, extensible system while maintaining backward compatibility.

## Features

### Core Capabilities

- **Comprehensive Virtualization Detection**: Detects VMware, VirtualBox, Hyper-V, KVM/QEMU, Xen, Parallels, and cloud hypervisors
- **Container Detection**: Supports Docker, LXC/LXD, Podman, Kubernetes, systemd-nspawn, and advanced runtimes
- **Platform Support**: Windows, Linux, and macOS with platform-specific optimizations
- **Confidence Scoring**: Weighted detection methods provide reliability scores
- **Backward Compatibility**: Original API remains functional

### Advanced Features

- **Cloud Platform Detection**: AWS Nitro, Google Cloud, Microsoft Azure
- **Security Analysis**: Container isolation, capabilities, security profiles
- **Performance Profiling**: Hardware fingerprinting and timing analysis
- **Nested Virtualization**: Detection of virtualization within virtualization
- **Runtime Detection**: containerd, CRI-O, Kata Containers, gVisor, Firecracker

## Architecture

The module is organized into several specialized components:

```
virtual/
├── virtual.hpp          # Main API and VirtualizationDetector class
├── common.hpp/cpp       # Shared utilities and constants
├── detection.hpp/cpp    # Core detection methods (CPUID, BIOS, hardware, etc.)
├── hypervisor.hpp/cpp   # Hypervisor-specific detection and identification
├── container.hpp/cpp    # Container detection and analysis
├── linux.hpp/cpp        # Linux-specific implementations
├── windows.hpp/cpp      # Windows-specific implementations
├── macos.hpp/cpp        # macOS-specific implementations
└── CMakeLists.txt       # Build configuration
```

## Usage

### Basic Detection

```cpp
#include "atom/sysinfo/virtual/virtual.hpp"

using namespace atom::system::virtual_env;

// Quick checks
VirtualizationDetector detector;
bool isVirtual = detector.isVirtual();
bool isContainer = detector.isContainer();
double confidence = detector.getConfidenceScore();

// Comprehensive analysis
auto info = detector.detect();
std::cout << "Virtual: " << info.is_virtual << std::endl;
std::cout << "Type: " << info.virtualization_type << std::endl;
std::cout << "Confidence: " << info.confidence_score << std::endl;
```

### Hypervisor Detection

```cpp
#include "atom/sysinfo/virtual/hypervisor.hpp"

using namespace atom::system::virtual_env::hypervisor;

// Detect specific hypervisors
if (vmware::detect()) {
    std::cout << "VMware version: " << vmware::getVersion() << std::endl;
    std::cout << "Has VMware Tools: " << vmware::hasVMwareTools() << std::endl;
}

// Get comprehensive hypervisor information
auto hypervisorInfo = getHypervisorInfo();
std::cout << "Hypervisor: " << hypervisorInfo.name << std::endl;
std::cout << "Vendor: " << hypervisorInfo.vendor << std::endl;
```

### Container Detection

```cpp
#include "atom/sysinfo/virtual/container.hpp"

using namespace atom::system::virtual_env::container;

// Detect containers
if (docker::detect()) {
    std::cout << "Container ID: " << docker::getContainerID() << std::endl;
    std::cout << "Image: " << docker::getImageInfo() << std::endl;
}

// Kubernetes detection
if (kubernetes::detect()) {
    std::cout << "Pod: " << kubernetes::getPodName() << std::endl;
    std::cout << "Namespace: " << kubernetes::getNamespace() << std::endl;
}
```

### Backward Compatibility

The original API is still available for existing code:

```cpp
#include "atom/sysinfo/virtual.hpp"

using namespace atom::system;

// Legacy functions still work
bool isVM = isVirtualMachine();
bool isDocker = isDockerContainer();
std::string vendor = getHypervisorVendor();
std::string type = getVirtualizationType();
```

## Detection Methods

### Virtualization Detection

- **CPUID Analysis**: Hypervisor bit and vendor strings
- **BIOS/UEFI Information**: Manufacturer and product strings
- **Hardware Fingerprinting**: Network adapters, disk drives, graphics cards
- **Process Detection**: VM tools and services
- **Registry Analysis**: Windows-specific VM indicators
- **Timing Analysis**: Performance characteristics and time drift

### Container Detection

- **Filesystem Markers**: `.dockerenv`, `.containerenv`, cgroup information
- **Process Analysis**: Container runtimes and orchestrators
- **Environment Variables**: Container-specific variables
- **Mount Points**: Container-specific filesystems
- **Security Context**: Capabilities, namespaces, security profiles

## Platform-Specific Features

### Linux

- DMI information parsing
- cgroup analysis
- Kernel module detection
- systemd container detection

### Windows

- WMI system information
- Registry analysis
- Service detection
- Hardware enumeration

### macOS

- IOKit hardware information
- System profiler analysis
- sysctl virtualization flags
- Hypervisor framework detection

## Building

The module integrates with the main CMake build system:

```bash
# Build with virtual module
cmake -DBUILD_VIRTUAL_TESTS=ON -DBUILD_VIRTUAL_EXAMPLES=ON ..
make

# Run tests
make run_virtual_tests

# Run examples
make run_virtual_demo
make run_virtual_benchmark
```

## Testing

Comprehensive test suite includes:

- Unit tests for all detection methods
- Platform-specific tests
- Performance benchmarks
- Memory usage analysis
- Error handling verification

```bash
# Run all tests
./virtual_tests

# Run with memory checking (if valgrind available)
make run_virtual_memory_tests

# Generate coverage report (debug builds)
make virtual_coverage
```

## Examples

### Demo Application

```bash
./virtual_demo
```

Shows comprehensive detection results and demonstrates all features.

### Benchmark Application

```bash
./virtual_benchmark
```

Measures performance of detection methods and provides timing analysis.

## Configuration

Detection methods can be enabled/disabled individually:

```cpp
VirtualizationDetector detector;

// Disable time-consuming methods for faster detection
detector.setDetectionMethod("Time Drift", false);
detector.setDetectionMethod("PCI Bus", false);

// Get list of available methods
auto methods = detector.getAvailableDetectionMethods();
```

## Dependencies

- **spdlog**: Logging framework
- **Platform Libraries**:
  - Windows: wbemuuid, ole32, oleaut32, advapi32
  - Linux: pthread, optional systemd
  - macOS: IOKit, CoreFoundation, Security, optional Hypervisor framework

## Migration from Legacy API

The original `virtual.cpp` functions are now implemented as inline wrappers around the new modular system. No code changes are required for existing users, but new features are available through the enhanced API.

### Recommended Migration Path

1. Continue using existing API for compatibility
2. Gradually adopt new `VirtualizationDetector` class for enhanced features
3. Use specialized namespaces (`hypervisor::`, `container::`) for detailed analysis

## Performance

The modular design provides:

- **Faster Quick Checks**: Optimized paths for simple yes/no questions
- **Configurable Detection**: Enable only needed methods
- **Caching**: Results cached within detector instances
- **Platform Optimization**: Native APIs for each platform

Typical performance (on modern hardware):

- Quick virtual check: < 1ms
- Full detection: 10-50ms depending on enabled methods
- Container detection: < 5ms

## Contributing

When adding new detection methods:

1. Add to appropriate namespace in detection/, hypervisor/, or container/
2. Update the main VirtualizationDetector class
3. Add platform-specific implementations as needed
4. Include comprehensive tests
5. Update documentation and examples

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>
