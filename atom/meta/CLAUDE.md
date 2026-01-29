# atom/meta - Reflection and Metaprogramming Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **meta**

---

## Module Overview

The **atom::meta** module provides comprehensive reflection, metaprogramming, and type introspection utilities for the Atom framework. It enables runtime type information, property systems, FFI (Foreign Function Interface) capabilities, and advanced template metaprogramming.

### Key Features

- **Property System**: Observable, validated, and computed properties
- **Type Traits**: Extensive compile-time type introspection
- **Reflection**: Runtime type information and member access
- **FFI Support**: C-compatible function bindings
- **Variant Types**: Type-safe discriminated unions
- **Function Traits**: Compile-time function signature analysis
- **Template Utilities**: Advanced template metaprogramming tools
- **Member Inspection**: Struct member count and iteration

---

## Directory Structure

```
atom/meta/
├── property.hpp       # Property system (with history, validation)
├── any.hpp           # Type-safe variant container
├── vany.hpp          # Variant type with type erasure
├── anymeta.hpp       # Meta-programming utilities
├── awaitable.hpp     # Awaitable/coroutine support
├── bind_first.hpp    # Partial function application
├── concept.hpp       # Concept definitions
├── constructor.hpp   # Constructor reflection
├── container_traits.hpp # Container type traits
├── conversion.hpp    # Type conversion utilities
├── decorate.hpp      # Decorator pattern
├── enum.hpp          # Enum reflection utilities
├── facade.hpp        # Facade pattern implementation
├── facade_any.hpp    # Facade with variant support
├── facade_proxy.hpp  # Proxy-based facade
├── ffi.hpp           # Foreign Function Interface
├── field_count.hpp   # Compile-time field counting
├── func_traits.hpp   # Function type traits
├── global_ptr.hpp    # Global pointer management
├── global_ptr.cpp
├── god.hpp           # Advanced meta-programming
├── invoke.hpp        # Universal function invocation
├── member.hpp        # Member reflection
├── overload.hpp      # Overload resolution
├── proxy.hpp         # Proxy pattern
├── proxy_params.hpp  # Proxy parameter utilities
├── raw_name.hpp      # Raw type name extraction
├── refl.hpp          # Reflection framework
├── refl_json.hpp     # JSON serialization
├── refl_yaml.hpp     # YAML serialization
├── signature.hpp     # Function signatures
├── stepper.hpp       # Stepper utilities
├── template_traits.hpp # Template metaprogramming
├── time.hpp          # Time-related meta utilities
├── type_caster.hpp   # Type casting utilities
├── type_info.hpp     # Runtime type information
└── abi.hpp           # ABI utilities
```

---

## Core Components

### Property System

The property system provides observable, validated properties with history tracking:

```cpp
#include "atom/meta/property.hpp"

using namespace atom::meta;

// Basic property
Property<int> age(25);
age.set(26);
int value = age.get();  // 26

// Property with getter/setter
Property<std::string> name(
    []() { return "John"; },
    [](const std::string& value) { /* setter logic */ }
);

// Observable property with change callback
Property<float> temperature(20.0f);
temperature.setOnChange([](const float& newValue) {
    std::cout << "Temperature changed to " << newValue << "\n";
});

temperature.set(25.0f);  // Prints: Temperature changed to 25
```

### Advanced Properties

```cpp
#include "atom/meta/property.hpp"

using namespace atom::meta;

// Validated property
ValidatedProperty<int> validatedAge;
validatedAge.withValidator(
    [](const int& value) { return value >= 0 && value <= 150; },
    "Age must be between 0 and 150"
);

if (!validatedAge.trySet(200)) {
    std::cout << validatedAge.getValidationError() << "\n";
}

// Historical property with undo support
HistoricalProperty<std::string, 10> history;
history.set("State1");
history.set("State2");
history.set("State3");

auto previous = history.getPrevious(1);  // "State2"
history.undo();  // Reverts to "State2"

// Computed property (read-only, derived)
int base = 10;
ComputedProperty<int> derived([&base]() { return base * 2; });
std::cout << derived.get() << "\n";  // 20

base = 20;
derived.invalidate();
std::cout << derived.get() << "\n";  // 40
```

### Type Traits

```cpp
#include "atom/meta/template_traits.hpp"

using namespace atom::meta;

// Check function properties
static_assert(is_function_v<void(int, float)>);
static_assert(!is_function_v<int>);

// Get function traits
using Func = void(int, float);
using traits = function_traits<Func>;

static_assert(std::is_same_v<traits::return_type, void>);
static_assert(traits::arity == 2);
static_assert(std::is_same_v<traits::arg<0>, int>);
```

### Member Reflection

```cpp
#include "atom/meta/member.hpp"

using namespace atom::meta;

struct Person {
    std::string name;
    int age;
    float height;
};

// Get field count at compile time
constexpr size_t fieldCount = field_count_v<Person>;  // 3

// Get field names
constexpr auto fieldNames = field_names_v<Person>;
// {"name", "age", "height"}

// Iterate fields
Person person{"Alice", 30, 1.7f};
for_each_field(person, [](auto&& field, auto&& name) {
    std::cout << name << ": " << field << "\n";
});
```

### Function Reflection

```cpp
#include "atom/meta/func_traits.hpp"

using namespace atom::meta;

void myFunction(int a, float b);

// Extract function information
using Traits = function_traits<decltype(myFunction)>;

// Get return type
using ReturnType = Traits::return_type;  // void

// Get argument types
using Arg1 = Traits::arg<0>;  // int
using Arg2 = Traits::arg<1>;  // float

// Get arity
constexpr size_t arity = Traits::arity;  // 2
```

---

## Public Interfaces

### Property Class

```cpp
template <typename T>
class Property {
public:
    // Constructors
    Property() = default;
    explicit Property(const T& defaultValue);
    Property(std::function<T()> getter);
    Property(std::function<T()> getter, std::function<void(const T&)> setter);

    // Access
    [[nodiscard]] auto get() const -> T;
    void set(const T& newValue);
    explicit operator T() const;
    Property& operator=(const T& newValue);

    // Configuration
    void makeReadonly();
    void makeWriteonly();
    void clear();

    // Callbacks
    void setOnChange(std::function<void(const T&)> callback);

    // Query
    [[nodiscard]] auto hasValue() const -> bool;
    [[nodiscard]] auto isReadonly() const -> bool;
    [[nodiscard]] auto isWriteonly() const -> bool;
};
```

### Advanced Property Types

```cpp
// Validated property
template <PropertyType T>
class ValidatedProperty : public Property<T> {
public:
    ValidatedProperty& withValidator(
        std::function<bool(const T&)> validator,
        std::string_view error_msg = "Validation failed");

    bool trySet(const T& value);
    [[nodiscard]] std::string_view getValidationError() const;
};

// Historical property
template <PropertyType T, std::size_t HistorySize = 10>
class HistoricalProperty : public Property<T> {
public:
    void set(const T& value);
    [[nodiscard]] std::optional<T> getPrevious(std::size_t steps = 1) const;
    bool undo();
};

// Computed property
template <PropertyType T>
class ComputedProperty {
public:
    explicit ComputedProperty(std::function<T()> compute);
    [[nodiscard]] T get() const;
    void invalidate();
    operator T() const { return get(); }
};
```

### Function Traits

```cpp
template <typename T>
struct function_traits {
    using return_type = /* ... */;
    static constexpr std::size_t arity = /* ... */;

    template <std::size_t N>
    using arg = /* ... */;
};

// Helper variable templates
template <typename T>
inline constexpr bool is_function_v = /* ... */;
```

### Member Reflection

```cpp
// Field count
template <typename T>
inline constexpr size_t field_count_v = /* ... */;

// Field names
template <typename T>
inline constexpr auto field_names_v = /* ... */;

// Field iteration
template <typename T, typename F>
void for_each_field(T&& obj, F&& func);
```

---

## Dependencies

### Required Dependencies

- **spdlog**: Enhanced logging

### Optional Dependencies

- **json-cpp**: JSON serialization support
- **yaml-cpp**: YAML serialization support

---

## Build Configuration

### CMake Options

```cmake
# Build meta module
-DBUILD_META=ON

# All components are header-only except global_ptr.cpp
```

---

## Usage Examples

### Property Registry

```cpp
#include "atom/meta/property.hpp"

using namespace atom::meta;

class Settings {
public:
    Property<int> width{800};
    Property<int> height{600};
    Property<bool> fullscreen{false};

    void registerProperties() {
        PropertyRegistry::getInstance().registerProperty("width", &width);
        PropertyRegistry::getInstance().registerProperty("height", &height);
        PropertyRegistry::getInstance().registerProperty("fullscreen", &fullscreen);
    }
};

// Access by name
Settings settings;
settings.registerProperties();

auto* widthProp = PropertyRegistry::getInstance().getProperty<int>("width");
if (widthProp) {
    std::cout << "Width: " << widthProp->get() << "\n";
}
```

### Universal Function Invocation

```cpp
#include "atom/meta/invoke.hpp"

using namespace atom::meta;

// Free function
int add(int a, int b) { return a + b; }

// Lambda
auto multiply = [](int a, int b) { return a * b; };

// Member function
struct Calculator {
    int subtract(int a, int b) { return a - b; }
};

// Invoke any callable
std::tuple args1{3, 4};
int result1 = invoke(add, args1);  // 7

std::tuple args2{5, 6};
int result2 = invoke(multiply, args2);  // 30

Calculator calc;
std::tuple args3{&calc, 10, 3};
int result3 = invoke(&Calculator::subtract, args3);  // 7
```

### Type Information

```cpp
#include "atom/meta/type_info.hpp"

using namespace atom::meta;

// Get type name
std::string typeName = getTypeName<int>();  // "int"
typeName = getTypeName<std::vector<int>>();  // "std::vector<int>"

// Check type properties
static_assert(is_integral_v<int>);
static_assert(is_floating_point_v<float>);
static_assert(is_pointer_v<int*>);
```

---

## Compile-Time Reflection

### Field Count

```cpp
#include "atom/meta/member.hpp"

struct MyStruct {
    int a;
    float b;
    std::string c;
};

constexpr size_t count = field_count_v<MyStruct>;  // 3
```

### Type Categories

```cpp
#include "atom/meta/concept.hpp"

using namespace atom::meta;

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric T>
void processNumber(T value) {
    std::cout << value << "\n";
}

processNumber(42);    // OK
processNumber(3.14);  // OK
// processNumber("text");  // Error: not Numeric
```

---

## Property Macros

### Property Definition Macros

```cpp
#include "atom/meta/property.hpp"

class MyClass {
public:
    // Read-write property
    DEFINE_RW_PROPERTY(int, Value);

    // Read-only property
    DEFINE_RO_PROPERTY(std::string, Name);

    // Write-only property
    DEFINE_WO_PROPERTY(float, InternalState);
};

// Usage
MyClass obj;
obj.Value = 42;      // Setter
int v = obj.Value;   // Getter

std::string n = obj.Name;  // OK
// obj.Name = "test";  // Error: read-only

obj.InternalState = 1.0f;  // OK
// float s = obj.InternalState;  // Error: write-only
```

---

## Testing

### Test Organization

Tests are located in `tests/meta/`:

- `test_property.cpp`: Property system tests
- `test_traits.cpp`: Type traits tests
- `test_reflection.cpp`: Reflection tests
- `test_member.cpp`: Member reflection tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run meta tests
ctest -R meta_ --output-on-failure
```

---

## Best Practices

### Property Usage

**DO:**

- Use properties for public interfaces
- Set up change callbacks for reactive behavior
- Use validated properties for constraints
- Document property behavior

**DON'T:**

- Create properties with no getter or value
- Throw exceptions in property callbacks
- Use properties for temporary storage
- Forget about thread safety (add external locking if needed)

### Reflection

**DO:**

- Use constexpr reflection where possible
- Document field order requirements
- Use type-safe wrappers

**DON'T:**

- Assume field order across compilers
- Modify reflected types without updating code
- Use reflection in hot paths (compile-time only)

---

## Related Modules

- **atom::type**: Type utilities and wrappers
- **atom::error**: Error handling for property validation
- **atom::utils**: Utility functions

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented property system, traits, reflection
- Added usage examples and best practices

---

**Maintained By:** Atom Framework Team
