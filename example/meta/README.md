# Atom Meta Module Examples

This directory contains examples demonstrating the metaprogramming and reflection capabilities of the Atom framework.

## 🚀 Overview

The Atom meta module provides powerful metaprogramming and reflection capabilities including:
- **Type Introspection**: Runtime and compile-time type information
- **Function Analysis**: Function signature and trait analysis
- **Type-Safe Any**: Runtime type-safe value storage and retrieval
- **Template Metaprogramming**: Advanced template techniques and SFINAE
- **Reflection**: Runtime type information and manipulation

## 📁 Examples

### ✅ **Comprehensive Meta Example**
**File**: `comprehensive_meta_example.cpp`
**Status**: Fully functional ✅

This comprehensive example demonstrates:

#### **Type Information System**
- `TypeInfo` class for detailed type introspection
- Runtime type name resolution
- Type property queries (arithmetic, class, pointer, etc.)
- JSON serialization of type information
- Cross-platform type name demangling

#### **Function Traits Analysis**
- `FunctionTraits` template for function signature analysis
- Return type and argument type extraction
- Function arity (argument count) determination
- Member function analysis and properties
- Lambda function trait extraction

#### **BoxedValue (Any) System**
- Type-safe runtime value storage
- `tryCast<T>()` for safe type conversion
- `canCast<T>()` for type compatibility checking
- Runtime type verification and validation
- Debug string representation

#### **Template Metaprogramming**
- Standard library type traits integration
- SFINAE (Substitution Failure Is Not An Error) demonstrations
- Conditional type selection
- Compile-time type computations
- Template specialization patterns

## 🛠️ Building and Running

### Build the Example
```bash
# Configure CMake with examples enabled
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# Build the meta example
cmake --build build --target meta_comprehensive_meta_example
```

### Run the Example
```bash
# Execute the example
./build/example/meta/meta_comprehensive_meta_example.exe
```

### Expected Output
The example will demonstrate:
1. **Type Information**: Detailed type analysis and JSON representation
2. **Function Traits**: Function signature analysis and properties
3. **BoxedValue Usage**: Type-safe value storage and retrieval
4. **Template Metaprogramming**: SFINAE and conditional compilation

## 🎯 Key Features Demonstrated

### **1. Type Information**
```cpp
// Get detailed type information
auto typeInfo = TypeInfo::fromType<MyClass>();
std::cout << "Type name: " << typeInfo.name() << std::endl;
std::cout << "Is class: " << typeInfo.isClass() << std::endl;
std::cout << "JSON: " << typeInfo.toJson() << std::endl;
```

### **2. Function Traits**
```cpp
// Analyze function signatures
using Traits = FunctionTraits<decltype(myFunction)>;
std::cout << "Return type: " << TypeInfo::fromType<Traits::return_type>().name() << std::endl;
std::cout << "Argument count: " << Traits::arity << std::endl;
std::cout << "First arg type: " << TypeInfo::fromType<Traits::argument_t<0>>().name() << std::endl;
```

### **3. BoxedValue (Any)**
```cpp
// Type-safe value storage
BoxedValue value(42);
if (auto intValue = value.tryCast<int>()) {
    std::cout << "Value: " << *intValue << std::endl;
} else {
    std::cout << "Not an int!" << std::endl;
}
```

### **4. Template Metaprogramming**
```cpp
// SFINAE example
template<typename T>
constexpr bool canProcess() {
    if constexpr (std::is_arithmetic_v<T>) {
        return true;
    } else {
        return false;
    }
}
```

## 📊 Core Components

### **TypeInfo Class**
The `TypeInfo` class provides comprehensive type information:

#### **Properties**
- `name()`: Human-readable type name
- `bareName()`: Unqualified type name
- `isArithmetic()`: Check if type is arithmetic
- `isClass()`: Check if type is a class
- `isPointer()`: Check if type is a pointer
- `isReference()`: Check if type is a reference
- `isConst()`: Check if type is const-qualified

#### **Serialization**
- `toJson()`: JSON representation of type information
- Includes all type traits and properties
- Useful for debugging and introspection

### **FunctionTraits Template**
The `FunctionTraits` template analyzes function signatures:

#### **Type Aliases**
- `return_type`: Function return type
- `argument_types`: Tuple of argument types
- `argument_t<N>`: N-th argument type

#### **Constants**
- `arity`: Number of function arguments
- `is_member_function`: True for member functions
- `is_const_member_function`: True for const member functions
- `is_noexcept`: True for noexcept functions

#### **Supported Function Types**
- Free functions
- Member functions (const and non-const)
- Lambda expressions
- Function pointers
- std::function objects

### **BoxedValue Class**
The `BoxedValue` class provides type-safe runtime value storage:

#### **Core Methods**
- `tryCast<T>()`: Safe casting with optional return
- `canCast<T>()`: Check if cast is possible
- `getTypeInfo()`: Get type information
- `debugString()`: Debug representation

#### **Features**
- Type safety at runtime
- Automatic type deduction
- Exception-safe operations
- Memory efficient storage

## 🔧 Advanced Usage Patterns

### **Type-Based Dispatch**
```cpp
template<typename T>
void processValue(const BoxedValue& value) {
    if (auto typed = value.tryCast<T>()) {
        // Process typed value
        handleTypedValue(*typed);
    }
}
```

### **Function Signature Matching**
```cpp
template<typename Func>
constexpr bool hasCorrectSignature() {
    using Traits = FunctionTraits<Func>;
    return std::is_same_v<typename Traits::return_type, int> &&
           Traits::arity == 2;
}
```

### **Conditional Compilation**
```cpp
template<typename T>
auto getValue() {
    if constexpr (std::is_arithmetic_v<T>) {
        return getNumericValue<T>();
    } else {
        return getObjectValue<T>();
    }
}
```

## 🎨 Design Patterns

### **Visitor Pattern with BoxedValue**
```cpp
struct ValueVisitor {
    void operator()(int value) { /* handle int */ }
    void operator()(double value) { /* handle double */ }
    void operator()(const std::string& value) { /* handle string */ }
};

boxedValue.visit(ValueVisitor{});
```

### **Type Registry Pattern**
```cpp
class TypeRegistry {
    std::unordered_map<std::string, TypeInfo> types_;
public:
    template<typename T>
    void registerType() {
        types_[TypeInfo::fromType<T>().name()] = TypeInfo::fromType<T>();
    }
};
```

### **Generic Factory Pattern**
```cpp
template<typename Base>
class Factory {
    std::unordered_map<std::string, std::function<std::unique_ptr<Base>()>> creators_;
public:
    template<typename Derived>
    void registerType() {
        creators_[TypeInfo::fromType<Derived>().name()] =
            []() { return std::make_unique<Derived>(); };
    }
};
```

## 🔍 Performance Considerations

### **Compile-Time vs Runtime**
- **TypeInfo**: Runtime type information with caching
- **FunctionTraits**: Compile-time analysis, zero runtime cost
- **BoxedValue**: Runtime type checking with optimization
- **Template metaprogramming**: Compile-time computation

### **Memory Usage**
- **TypeInfo**: Lightweight, shared type information
- **BoxedValue**: Efficient storage with small object optimization
- **Function traits**: Zero memory overhead (compile-time only)

### **Optimization Tips**
- Use compile-time techniques when possible
- Cache TypeInfo objects for repeated use
- Prefer `tryCast` over exception-based casting
- Use template metaprogramming for performance-critical code

## 🚨 Important Notes

### **Thread Safety**
- **TypeInfo**: Thread-safe for read operations
- **BoxedValue**: Thread-safe for read operations, not for modifications
- **Function traits**: Compile-time only, inherently thread-safe

### **Exception Safety**
- All operations provide strong exception safety guarantees
- `tryCast` never throws, returns optional
- Type information queries are noexcept

### **Platform Compatibility**
- Cross-platform type name demangling
- Compiler-specific optimizations
- Standard library integration

## 📚 Further Reading

- **C++ Template Metaprogramming**: Advanced template techniques
- **Type Erasure**: Design patterns for type-safe generic programming
- **Reflection in C++**: Current and future reflection capabilities
- **SFINAE and Concepts**: Modern C++ constraint programming

---

This example demonstrates the sophisticated metaprogramming capabilities of the Atom framework, enabling powerful runtime introspection and compile-time optimization techniques.
