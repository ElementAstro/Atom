# Atom Framework Examples Index

This document provides a comprehensive index of all examples in the Atom framework, organized by category and complexity level.

## 📊 Example Statistics

- **Total Modules**: 22 modules with examples
- **Total Examples**: 150+ individual example files
- **Fully Functional**: 4 comprehensive examples ✅
- **Working Basic Tests**: 6+ basic functionality tests ✅
- **Documentation Coverage**: 5 modules with detailed README files

## 🎯 Example Categories

### **🚀 Beginner Examples** (Getting Started)

Perfect for learning basic concepts and API usage.

#### **Core Functionality**

- **`containers/high_performance_containers_example.cpp`** ✅ - High-performance containers
- **`meta/comprehensive_meta_example.cpp`** ✅ - Metaprogramming and reflection
- **`secret/basic_test.cpp`** ✅ - Basic secure storage test
- **`sysinfo/header_test.cpp`** ✅ - System information headers

#### **Basic Operations**

- **`error/exception.cpp`** - Exception handling
- **`error/stacktrace.cpp`** - Stack trace generation
- **`log/atomlog.cpp`** - Basic logging
- **`memory/memory_pool.cpp`** - Memory pool usage

### **⚡ Intermediate Examples** (Feature Combinations)

Demonstrate real-world usage patterns and feature integration.

#### **Algorithms & Data Structures**

- **`algorithm/hash.cpp`** - Hash computation for various types
- **`algorithm/md5.cpp`** - MD5 hashing (enhanced)
- **`algorithm/sha1.cpp`** - SHA1 hashing
- **`algorithm/matrix.cpp`** - Matrix operations
- **`algorithm/pathfinding.cpp`** - Pathfinding algorithms

#### **Asynchronous Programming**

- **`async/timer.cpp`** - Timer and scheduling (comprehensive)
- **`async/future.cpp`** - Future and promise patterns
- **`async/pool.cpp`** - Thread pool management
- **`async/message_queue.cpp`** - Message passing
- **`async/parallel.cpp`** - Parallel algorithms

#### **Networking & Communication**

- **`connection/tcpclient.cpp`** - TCP client implementation
- **`connection/udpserver.cpp`** - UDP server implementation
- **`connection/async_tcpclient.cpp`** - Asynchronous TCP client
- **`web/curl.cpp`** - HTTP client using CURL
- **`web/httpparser.cpp`** - HTTP parsing

### **🔬 Advanced Examples** (Complex Integrations)

Showcase advanced features, performance optimization, and edge cases.

#### **System Programming**

- **`system/process.cpp`** - Process management
- **`system/network_manager.cpp`** - Network management
- **`system/registry.cpp`** - System registry access
- **`serial/scanner.cpp`** - Serial port scanning

#### **Memory Management**

- **`memory/shared.cpp`** - Shared memory
- **`memory/tracker.cpp`** - Memory tracking
- **`memory/short_alloc.cpp`** - Custom allocators
- **`type/concurrent_*.cpp`** - Concurrent data structures

#### **Metaprogramming & Reflection**

- **`meta/type_info.cpp`** - Type information system
- **`meta/func_traits.cpp`** - Function trait analysis
- **`meta/refl.cpp`** - Reflection capabilities
- **`meta/any.cpp`** - Type erasure patterns

## 🏗️ Module Organization

### **✅ Well-Organized Modules** (With README and clear structure)

#### **Containers Module** (`example/containers/`)

```
containers/
├── README.md                                    # Comprehensive documentation
├── CMakeLists.txt                              # Build configuration
└── high_performance_containers_example.cpp     # Main example ✅
```

#### **Meta Module** (`example/meta/`)

```
meta/
├── README.md                                   # Comprehensive documentation
├── CMakeLists.txt                             # Build configuration
├── comprehensive_meta_example.cpp             # Main example ✅
└── [20+ specific feature examples]            # Individual feature demos
```

#### **Secret Module** (`example/secret/`)

```
secret/
├── README.md                                  # Comprehensive documentation
├── CMakeLists.txt                            # Build configuration
├── basic_test.cpp                            # Working basic test ✅
├── secure_storage_example.cpp               # Comprehensive example (issues)
└── simple_secret_example.cpp                # Simple example (issues)
```

#### **Sysinfo Module** (`example/sysinfo/`)

```
sysinfo/
├── README.md                                 # Comprehensive documentation
├── CMakeLists.txt                           # Build configuration
├── header_test.cpp                          # Working header test ✅
├── basic_sysinfo_example.cpp               # Basic example (issues)
└── system_info_example.cpp                 # Comprehensive example (issues)
```

### **📚 Feature-Rich Modules** (Many examples, need organization)

#### **Algorithm Module** (`example/algorithm/`)

**Categories**: Cryptography, Mathematics, Graphics, Data Structures

- **Cryptography**: `md5.cpp`, `sha1.cpp`, `tea.cpp`, `hash.cpp`
- **Mathematics**: `matrix.cpp`, `fraction.cpp`, `bignumber.cpp`
- **Graphics**: `perlin.cpp`, `pathfinding.cpp`, `flood.cpp`
- **Data Structures**: `huffman.cpp`, `weight.cpp`

#### **Async Module** (`example/async/`)

**Categories**: Concurrency, Messaging, Synchronization, Utilities

- **Concurrency**: `pool.cpp`, `parallel.cpp`, `executor.cpp`
- **Messaging**: `message_queue.cpp`, `message_bus.cpp`, `slot.cpp`
- **Synchronization**: `lock.cpp`, `safetype.cpp`, `trigger.cpp`
- **Utilities**: `timer.cpp`, `future.cpp`, `promise.cpp`

#### **System Module** (`example/system/`)

**Categories**: Process Management, Hardware, Network, Storage

- **Process**: `process.cpp`, `process_manager.cpp`, `pidwatcher.cpp`
- **Hardware**: `gpio.cpp`, `power/`, `hardware/`
- **Network**: `network_manager.cpp`, `network/`
- **Storage**: `storage.cpp`, `storage/`

## 🎨 Naming Conventions

### **Current Naming Patterns**

- **Simple names**: `timer.cpp`, `hash.cpp`, `pool.cpp`
- **Descriptive names**: `memory_pool.cpp`, `message_queue.cpp`
- **Feature-specific**: `async_tcpclient.cpp`, `concurrent_map.cpp`

### **Recommended Naming Convention**

```
[module]_[feature]_[type].cpp

Where:
- module: Optional module prefix for clarity
- feature: Main feature being demonstrated
- type: example, test, demo, benchmark

Examples:
- containers_high_performance_example.cpp ✅
- meta_comprehensive_example.cpp ✅
- secret_basic_test.cpp ✅
- algorithm_md5_example.cpp (enhanced)
```

## 📁 Directory Structure Standards

### **Recommended Module Structure**

```
module_name/
├── README.md                    # Module documentation
├── CMakeLists.txt              # Build configuration
├── basic_example.cpp           # Simple getting-started example
├── comprehensive_example.cpp   # Full-featured demonstration
├── [feature]_example.cpp       # Specific feature examples
├── [feature]_test.cpp          # Basic functionality tests
└── subdirectories/             # For complex modules only
    ├── category1/
    └── category2/
```

### **Documentation Structure**

```
README.md should contain:
├── Overview and key features
├── Example descriptions and status
├── Build and run instructions
├── Key features demonstrated
├── Usage patterns and best practices
├── Current status and known issues
├── Troubleshooting guide
└── Further reading
```

## 🔧 Build System Organization

### **CMakeLists.txt Standards**

Each module should have:

- Clear target naming: `${module}_${example_name}`
- Proper dependency management
- Conditional compilation for optional features
- Clear error messages for missing dependencies

### **Build Options**

- `ATOM_EXAMPLE_BUILD_ALL=ON` - Build all examples
- `ATOM_EXAMPLE_BUILD_${MODULE}=ON` - Build specific module
- Individual target building: `cmake --build build --target specific_target`

## 🎯 Quality Standards

### **Code Quality Requirements**

- **Documentation**: Comprehensive header comments
- **Error Handling**: Proper exception handling
- **Output**: Clear, formatted output with explanations
- **Structure**: Logical function organization
- **Comments**: Inline explanations for complex operations

### **Example Completeness Levels**

#### **Level 1: Basic Test** ✅

- Verifies module can be loaded/compiled
- Minimal functionality demonstration
- Quick pass/fail indication

#### **Level 2: Feature Example**

- Demonstrates specific feature usage
- Clear input/output examples
- Basic error handling

#### **Level 3: Comprehensive Example** ✅

- Multiple feature integration
- Real-world usage patterns
- Advanced error handling
- Performance considerations
- Best practices demonstration

## 📈 Future Organization Goals

### **Short-term Improvements**

1. Add README files to all major modules
2. Standardize naming conventions
3. Improve build system integration
4. Fix runtime issues in existing examples

### **Long-term Goals**

1. Create tutorial sequences for each module
2. Add performance benchmarks
3. Implement automated testing
4. Create interactive examples

---

This index serves as a roadmap for navigating the extensive Atom framework examples and understanding the current state of documentation and organization.
