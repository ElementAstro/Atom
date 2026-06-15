# atom/components - Component System Module

> **Module Version:** 0.1.0
> **Documentation Version:** 2.0.0
> **Last Updated:** 2026-06-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **components**

---

## Module Overview

The **atom::components** module provides a component-based architecture with
dynamic command dispatch, change-tracked variables, lifecycle management,
optional scripting, and multi-format serialization. It leans heavily on
`atom::meta` (reflection, function traits, proxies, FFI) and `atom::type`
(trackable values, JSON) instead of reimplementing those facilities.

### Key Features

- **Component base class**: variables + named commands on a single object
- **Registry**: lifecycle, dependency resolution, optional hot reload
- **Command dispatch**: dynamic, introspectable calls with timeouts and
  pre/post conditions (built on `atom::meta`)
- **Variable system**: change-tracked values with ranges, options, aliases,
  groups, and JSON import/export
- **Lifecycle management**: phased hooks, dependency constraints, circular
  detection
- **Component pools**: cache-aligned pooled allocation
- **Serialization**: JSON / Binary / XML / MessagePack
- **Scripting** (optional): Lua and Python engines with a sandbox
- **Event system** (optional): component- and registry-level pub/sub

---

## Directory Structure

```
atom/components/
├── <name>.hpp              # Thin forwarding shims (component.hpp, dispatch.hpp,
│                           # registry.hpp, var.hpp, ...) → subdir headers
├── core/
│   ├── component.{hpp,cpp}        # Component base class
│   ├── component_pool.{hpp,cpp}   # Cache-aligned pooled allocation
│   ├── registry.{hpp,cpp}         # Lifecycle registry, optional hot reload
│   ├── types.hpp                  # ComponentState / ComponentType, events
│   ├── module_macro.hpp           # ATOM_MODULE / ATOM_COMPONENT / initializers
│   └── package.hpp                # Package metadata
├── data/
│   ├── var.{hpp,cpp}              # VariableManager (atom::type::Trackable)
│   └── serialization.{hpp,cpp}    # Multi-format serializers
├── lifecycle/
│   ├── lifecycle.{hpp,cpp}        # LifecycleManager, phases, constraints
│   ├── dispatch.{hpp,cpp}         # CommandDispatcher (atom::meta proxies)
│   └── iteration.{hpp,cpp}        # Cache-friendly / SoA iteration helpers
└── scripting/
    ├── scripting_api.{hpp,cpp}    # IScriptEngine, ScriptValue, ScriptLanguage
    ├── script_engine.{hpp,cpp}    # Loader / executor with caching
    ├── script_sandbox.{hpp,cpp}   # Permissions, resource limits
    ├── bindings.{hpp,cpp}         # Type bindings for engines
    ├── lua_engine.{hpp,cpp}       # Lua engine (ATOM_ENABLE_LUA)
    └── python_engine.{hpp,cpp}    # Python engine (ATOM_ENABLE_PYTHON)
```

The top-level `*.hpp` files are deprecated forwarding shims kept for backwards
compatibility; new code should include the subdirectory headers directly.

---

## Core Components

### Component

A `Component` owns named **variables** and named **commands**. Variables are
change-tracked (`atom::type::Trackable`); commands are registered with `def()`
and invoked with `dispatch()`.

```cpp
#include "atom/components/core/component.hpp"

class Counter : public Component {
public:
    explicit Counter(const std::string& name) : Component(name) {
        addVariable<int>("count", 0, "current value");

        def(
            "increment",
            [this]() -> int {
                auto v = getVariable<int>("count");
                int next = v->get() + 1;
                setValue("count", next);
                return next;
            },
            "ops", "Increment and return the counter");
    }
};

Counter c("counter");
int n = std::any_cast<int>(c.dispatch("increment"));   // 1
auto count = c.getVariable<int>("count");              // shared_ptr<Trackable<int>>
```

Selected API:

```cpp
template <typename T> void addVariable(std::string_view name, T initial,
                                       std::string_view desc = "",
                                       std::string_view alias = "",
                                       std::string_view group = "");
template <typename T> auto getVariable(std::string_view name)
    -> std::shared_ptr<atom::type::Trackable<T>>;
template <typename T> void setValue(std::string_view name, T value);

template <typename Callable>
void def(std::string_view name, Callable&& func,
         std::string_view group = "", std::string_view description = "");

template <typename... Args>
auto dispatch(std::string_view name, Args&&... args) -> std::any;
[[nodiscard]] auto has(std::string_view name) const noexcept -> bool;
```

`def()` has many overloads (free functions, member functions, member/static
variables, getters, properties) that forward to the `CommandDispatcher`.

### Registry

```cpp
#include "atom/components/core/registry.hpp"

auto& registry = Registry::instance();

auto counter = registry.createComponent<Counter>("counter");
auto found   = registry.getComponent("counter");

registry.addDependency("web", "counter");
registry.initializeAll();
// ...
registry.cleanupAll();
```

Module registration uses the macros in `core/module_macro.hpp`:

```cpp
ATOM_MODULE(my_module, [] { return std::make_shared<MyComponent>("my_module"); });
// exports C entry points: my_module_initialize_registry / _cleanup_registry /
// _getInstance / _getVersion — used by Registry::loadComponentFromFile.
```

### CommandDispatcher (lifecycle/dispatch.hpp)

Backs `Component::def`/`dispatch`. Supports command groups, aliases, per-command
timeouts, and pre/post conditions. Argument packing, function-signature
introspection, and type conversion reuse `atom::meta` (`FunctionParams`, `Arg`,
`FunctionTraits`, `ProxyFunction`, `TypeCaster`). Dispatch results are returned
as `std::any`.

### VariableManager (data/var.hpp)

Backs component variables. Each value is wrapped in `atom::type::Trackable<T>`
for change notification, with optional numeric ranges (`setRange`), string
option sets (`setStringOptions`), aliases, groups, and JSON `export`/`import`.
Container storage is selectable: `std::unordered_map` (default),
`emhash8::HashMap` (`ENABLE_FASTHASH`), or `atom::containers::flat_map`
(`ATOM_USE_BOOST_CONTAINERS`).

### LifecycleManager (lifecycle/lifecycle.hpp)

Phased lifecycle (`PreConstruction` … `PostDestruction`) with per-component and
global hooks, `DependencyConstraint`s (required/optional/weak + version +
validator), topological resolution, circular-dependency detection, and an
event history.

### ComponentPool (core/component_pool.hpp)

Cache-aligned, chunked pooled allocation for `Component` subclasses with hit/miss
statistics and defragmentation.

### Serialization (data/serialization.hpp)

`SerializationManager` dispatches to JSON / Binary / XML / MessagePack
serializers with optional metadata, compression, and encryption.

---

## Scripting (optional)

```cpp
#include "atom/components/scripting/scripting_api.hpp"
using namespace atom::components::scripting;

auto& api = ComponentScriptingAPI::instance();
auto engine = api.createEngine(ScriptLanguage::Auto);   // Lua / Python / Auto
```

- `ScriptLanguage` ∈ `{ Lua, Python, Auto }`. `Auto` detects by extension
  (`.lua` / `.py`) or content.
- Engines are gated behind `ATOM_ENABLE_LUA` / `ATOM_ENABLE_PYTHON`; without
  either, `createEngine` returns `nullptr`.
- `ScriptSandbox` enforces permissions and resource limits for untrusted code.

---

## Build Configuration

```cmake
-DATOM_BUILD_COMPONENTS=ON              # build this module

-DATOM_COMPONENTS_ENABLE_EVENTS=ON      # component/registry pub-sub (default ON)
-DATOM_COMPONENTS_ENABLE_HOT_RELOAD=OFF # Registry::loadComponentFromFile (default OFF)
-DATOM_ENABLE_LUA=ON                    # Lua engine
-DATOM_ENABLE_PYTHON=ON                 # Python engine
```

Internal toggles consumed via preprocessor macros: `ENABLE_EVENT_SYSTEM`,
`ENABLE_HOT_RELOAD`, `ENABLE_FASTHASH`, `ATOM_USE_BOOST_CONTAINERS`.

The module is built as a shared library (`atom-component`). Hot reload links
`${CMAKE_DL_LIBS}` (`dl` on Linux; `LoadLibrary` is used on Windows).

---

## Dependencies

### Required

- **atom-error** — exceptions
- **atom-utils** — string / to_string helpers
- **atom-type** — `Trackable`, JSON
- **atom-meta** — `type_info`, `type_caster`, `proxy`, `func_traits`,
  `conversion`, `constructor`, `enum`, `ffi` (`DynamicLibrary` for hot reload)
- **spdlog** / **fmt** — logging / formatting

### Optional

| Feature | Dependency | Flag |
|---------|------------|------|
| Lua scripting | Lua 5.3+ | `ATOM_ENABLE_LUA` |
| Python scripting | Python 3 | `ATOM_ENABLE_PYTHON` |
| Flat containers | atom-containers (+ Boost) | `ATOM_USE_BOOST_CONTAINERS` |

---

## Testing

GoogleTest suite under `tests/components/` (one file per area:
`component`, `registry`, `component_pool`, `lifecycle`, `dispatch`,
`iteration`, `var`, `serialization`, `scripting_api`, `script_engine`,
`script_sandbox`, `bindings`, `types_and_macros`, plus `lua_engine` /
`python_engine` gated on the corresponding flags).

```bash
# Configure components + deps, build, and run
cmake -B build/components -G Ninja -DATOM_BUILD_ALL=OFF \
  -DATOM_BUILD_COMPONENTS=ON -DATOM_AUTO_RESOLVE_DEPS=ON \
  -DATOM_BUILD_TESTS=ON -DATOM_BUILD_TESTS_SELECTIVE=ON \
  -DATOM_TEST_BUILD_COMPONENTS=ON
cmake --build build/components --target atom_components_tests -j
ctest --test-dir build/components -R components --output-on-failure
```

Scripting tests that require a live engine are skipped (or fail) unless
`ATOM_ENABLE_LUA` / `ATOM_ENABLE_PYTHON` is set.

---

## Related Modules

- **atom::meta** — reflection, function traits, proxies, FFI (heavily reused)
- **atom::type** — `Trackable`, JSON, containers
- **atom::containers** — opt-in high-performance containers
- **atom::error** — exception types

---

## Change Log

### 2026-06-15

- Rewrote documentation to match the actual code: removed references to the
  deleted `advanced_bindings` / `type_conversion` files and to non-existent
  APIs (`getInstance`, `setProperty`, a `Var` value type, `Dispatcher::subscribe`)
- Documented the real `Component` / `Registry` / `CommandDispatcher` /
  `VariableManager` interfaces and the `meta` / `type` reuse
- `ScriptLanguage` documented as `{ Lua, Python, Auto }`
- Added the `ATOM_COMPONENTS_ENABLE_HOT_RELOAD` option and dynamic loading

### 2025-01-15

- Initial module documentation

---

**Maintained By:** Atom Framework Team
