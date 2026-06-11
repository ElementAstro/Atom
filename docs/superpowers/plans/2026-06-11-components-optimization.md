# atom/components Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make atom/components consistent, deduplicated, and complete per the 2026-06-11 design spec, growing the green test baseline from 332 tests while re-enabling 6 disabled test files.

**Architecture:** Incremental per-submodule passes (core → macros → events → data → scripting → namespace → CMake). Implementation headers stay where they are; backward-compat is preserved via global `using` aliases. Every task ends with a full module rebuild + test run.

**Tech Stack:** C++20 (GCC 15.2, MSYS2 MinGW64), CMake+Ninja, GoogleTest, spdlog, nlohmann-json, atom/meta + atom/type for traits/concepts.

**Build/verify commands (used by every task):**

```powershell
$env:PATH = "D:\msys64\mingw64\bin;" + $env:PATH
cmake --build build/components -j -- -k 0
& "D:\Project\Atom\build\components\bin\DEBUG\atom_components_tests.exe" --gtest_brief=1
```

Expected: exit 0, `[  PASSED  ]` with count ≥ previous task's count.

---

### Task 1: ComponentPerformanceStats cleanup (WP1)

**Files:**
- Modify: `atom/components/core/component.hpp:134-202`

- [ ] **Step 1:** Replace the `#if defined(_MSC_VER)` constexpr fork and mis-indented bodies of `reset()` / `updateExecutionTime()` / the legacy getters with properly indented, single-variant code. `reset()` is plain `void reset() noexcept` (atomics are never constexpr-storable). Keep behavior identical.
- [ ] **Step 2:** Rebuild + run tests. Expected: 332 passed.
- [ ] **Step 3:** Commit `refactor(components): clean up ComponentPerformanceStats formatting and constexpr fork`.

### Task 2: Replace component.template with index_sequence expansion (WP1)

**Files:**
- Modify: `atom/components/core/component.hpp:1133-1147`
- Delete: `atom/components/component.template`

- [ ] **Step 1:** Rewrite the generic `def`:

```cpp
template <typename Callable>
void Component::def(std::string_view name, Callable&& func,
                    std::string_view group, std::string_view description) {
    using Traits = atom::meta::FunctionTraits<std::decay_t<Callable>>;
    if (name.empty()) {
        throw std::invalid_argument("Command name cannot be empty");
    }
    static_assert(Traits::arity <= 8,
                  "Too many arguments in function (maximum is 8)");
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (void)m_CommandDispatcher_->def(
            name, group, description,
            std::function<typename Traits::return_type(
                typename Traits::template argument_t<I>...)>(
                std::forward<Callable>(func)));
    }(std::make_index_sequence<Traits::arity>{});
}
```

- [ ] **Step 2:** `git rm atom/components/component.template`; grep for remaining references (CMake lists do not reference it; double-check).
- [ ] **Step 3:** Rebuild + run tests. Expected: 332 passed.
- [ ] **Step 4:** Commit `refactor(components): replace component.template arity ladder with index_sequence`.

### Task 3: De-leak macros; concept-based registerOperators (WP1)

**Files:**
- Modify: `atom/components/core/component.hpp:779-834` (operators), `:544-555` and `:588-603` and `:1175-1201` (`#undef` after use)

- [ ] **Step 1:** Delete `OP_*`, `CONDITION_*`, `REGISTER_OPERATOR` macros. Rewrite:

```cpp
template <typename T>
void Component::registerOperators(std::string_view typeName) {
    const std::string t{typeName};
    if constexpr (std::equality_comparable<T>) {
        def(t + ".equals",
            [](const T& a, const T& b) -> bool { return a == b; }, "operators",
            "Check if two objects are equal");
        def(t + ".notEquals",
            [](const T& a, const T& b) -> bool { return a != b; }, "operators",
            "Check if two objects are not equal");
    }
    if constexpr (std::totally_ordered<T>) {
        def(t + ".lessThan",
            [](const T& a, const T& b) -> bool { return a < b; }, "operators",
            "Compare objects");
        def(t + ".greaterThan",
            [](const T& a, const T& b) -> bool { return a > b; }, "operators",
            "Compare objects");
        def(t + ".lessThanOrEqual",
            [](const T& a, const T& b) -> bool { return a <= b; }, "operators",
            "Compare objects");
        def(t + ".greaterThanOrEqual",
            [](const T& a, const T& b) -> bool { return a >= b; }, "operators",
            "Compare objects");
    }
}
```

- [ ] **Step 2:** Add `#undef DEF_MEMBER_FUNC`, `#undef DEF_MEMBER_FUNC_WITH_INSTANCE`, `#undef DEF_MEMBER_FUNC_IMPL` after their last expansions.
- [ ] **Step 3:** Rebuild + run tests; commit `refactor(components): replace operator macros with concept-constrained code`.

### Task 4: Unpollute dispatch.hpp global namespace (WP1)

**Files:**
- Modify: `atom/components/lifecycle/dispatch.hpp:26`, plus every bare `json` use in `dispatch.hpp` / `lifecycle/dispatch.cpp`

- [ ] **Step 1:** Remove `using json = nlohmann::json;`; qualify uses as `nlohmann::json`.
- [ ] **Step 2:** Rebuild + tests; fix any consumer that relied on the leaked alias (qualify there too).
- [ ] **Step 3:** Commit `refactor(components): stop leaking global json alias from dispatch.hpp`.

### Task 5: Hot-path log levels (WP1)

**Files:**
- Modify: `atom/components/data/var.cpp`, `atom/components/core/registry.cpp` (per-variable / per-command `spdlog::info` calls only)

- [ ] **Step 1:** Downgrade per-call `spdlog::info` (e.g. "Adding variable: …") to `spdlog::trace`. Keep lifecycle-level messages (init/cleanup) at info.
- [ ] **Step 2:** Rebuild + tests; commit `perf(components): demote per-call logging to trace`.

### Task 6: Fix module macros, re-enable types_and_macros test (WP2)

**Files:**
- Modify: `atom/components/core/module_macro.hpp`, `tests/components/types_and_macros.cpp`, `tests/components/CMakeLists.txt:101-108`

- [ ] **Step 1:** Read `core/registry.cpp` (`registerModule`, `addInitializer`, `initializeAll`, `getComponent`) to pin real semantics.
- [ ] **Step 2:** Fix `ATOM_MODULE_INIT` lambdas so they match `Component::InitFunc = std::function<void(Component&)>` and the Registry API (Registry is the source of truth). Compile-check with a TU that expands `ATOM_MODULE` and `ATOM_EMBED_MODULE`.
- [ ] **Step 3:** Rewrite `tests/components/types_and_macros.cpp` against the real macros (`ComponentType` enum + `EnumTraits`, `REGISTER_INITIALIZER`, `ATOM_EMBED_MODULE` expansion smoke test) and re-add it to `TEST_SOURCES`.
- [ ] **Step 4:** Rebuild + run; test count grows. Commit `fix(components): make module macros compile and re-enable types_and_macros tests`.

### Task 7: Event system — define types, enable, test (WP3)

**Files:**
- Modify: `atom/components/core/types.hpp`, `atom/components/core/component.hpp/.cpp`, `atom/components/core/registry.hpp/.cpp`, `atom/components/CMakeLists.txt`
- Test: `tests/components/component.cpp` (new event test cases)

- [ ] **Step 1:** In `core/types.hpp` (namespace `atom::components`):

```cpp
namespace atom::components {
using EventCallbackId = std::uint64_t;

struct Event {
    std::string name;
    std::any data;
    std::string source;
    std::chrono::system_clock::time_point timestamp{
        std::chrono::system_clock::now()};
};

using EventCallback = std::function<void(const Event&)>;
}  // namespace atom::components
```

- [ ] **Step 2:** In `atom/components/CMakeLists.txt` add `option(ATOM_COMPONENTS_ENABLE_EVENTS "Enable component event system" ON)` → `target_compile_definitions(atom-component PUBLIC ENABLE_EVENT_SYSTEM=1)` when ON.
- [ ] **Step 3:** Build; fix every long-dead `#if ENABLE_EVENT_SYSTEM` body in component.cpp/registry.cpp until green (types above are the contract; adjust call sites, not the contract, unless the code reveals a needed field).
- [ ] **Step 4:** Add tests: `emitEvent`/`on` delivers payload; `once` fires exactly once; `off` unsubscribes; `Registry::subscribeToEvent`/`triggerEvent` round-trip.
- [ ] **Step 5:** Rebuild + run; commit `feat(components): complete and enable the component event system`.

### Task 8: data/type_conversion.hpp — reuse atom/meta, re-enable test (WP4)

**Files:**
- Modify: `atom/components/data/type_conversion.hpp`, `tests/components/type_conversion.cpp`, `tests/components/CMakeLists.txt`

- [ ] **Step 1:** Delete local `type_traits` namespace duplicates (`is_container`, `is_associative`, `is_optional`, `is_smart_pointer`, `is_tuple`); replace uses with `atom::meta::ContainerTraits`/`is_associative_container_v` (atom/meta/container_traits.hpp), `atom::meta::TupleLike` (template_traits.hpp), `SmartPointer` concept (concept.hpp). Keep public converter API.
- [ ] **Step 2:** Fix the converter methods the disabled test expects, or align the test to the real converter API (implementation is source of truth; add only small missing pieces).
- [ ] **Step 3:** Re-enable `type_conversion.cpp` in `TEST_SOURCES`; rebuild + run; commit `refactor(components): base type_conversion on atom/meta traits and re-enable tests`.

### Task 9: package.hpp — drop absl, scope constants (WP4)

**Files:**
- Modify: `atom/components/core/package.hpp`, `tests/components/package.cpp` (only if names move)

- [ ] **Step 1:** Remove `#include <absl/strings/match.h>`; replace any `absl::` calls with `std::string_view` equivalents (e.g. `sv.starts_with(...)`).
- [ ] **Step 2:** Move `ALIGNMENT`/`MAX_ELEMENTS` and the parser into `namespace atom::components::package` with global compat aliases if tests use unqualified names.
- [ ] **Step 3:** Rebuild + run; commit `refactor(components): remove abseil dependency from package.hpp`.

### Task 10: Scripting tests — align and re-enable (WP5)

**Files:**
- Modify: `tests/components/scripting_api.cpp`, `script_sandbox.cpp`, `script_engine.cpp`, `advanced_bindings.cpp`, `tests/components/CMakeLists.txt`; scripting headers/sources only for small genuine API gaps

One sub-cycle per file (4×):
- [ ] **Step A:** Diff the test's expected API vs the real header; rewrite test to the real API.
- [ ] **Step B:** Re-add to `TEST_SOURCES`; rebuild; fix compile errors (test side first; implementation only for clear gaps).
- [ ] **Step C:** Run; commit `test(components): re-enable <name> tests against current API`.

### Task 11: Fluent def + typed dispatch + command introspection (added functionality)

**Files:**
- Modify: `atom/components/core/component.hpp` (+ `.cpp`)
- Test: `tests/components/component.cpp`

- [ ] **Step 1:** Add:

```cpp
template <typename T, typename... Args>
auto dispatchAs(std::string_view name, Args&&... args) -> T {
    return std::any_cast<T>(dispatch(name, std::forward<Args>(args)...));
}
```

- [ ] **Step 2:** Add `getCommandInfo(std::string_view) -> nlohmann::json` returning `{name, description, aliases[], argTypes[]}` built from existing CommandDispatcher getters.
- [ ] **Step 3:** Tests: `dispatchAs<int>` returns value and throws `std::bad_any_cast` on mismatch; `getCommandInfo` contains description/aliases.
- [ ] **Step 4:** Rebuild + run; commit `feat(components): add dispatchAs and command introspection`.

### Task 12: Namespace unification with compat aliases (WP6)

**Files:**
- Modify: `atom/components/core/component.hpp/.cpp`, `core/registry.hpp/.cpp`, `lifecycle/dispatch.hpp/.cpp`, `data/var.hpp/.cpp`, `core/types.hpp`, `core/package.hpp`

- [ ] **Step 1:** Wrap declarations in `namespace atom::components { ... }`; at the end of each header add compat aliases, e.g.:

```cpp
using atom::components::Component;
using atom::components::ComponentState;
using atom::components::Registry;
using atom::components::CommandDispatcher;
using atom::components::VariableManager;
// + exception types
```

- [ ] **Step 2:** Build; chase qualification fallout inside the module (`Component::InitFunc` refs in macros, forward declarations `class Component;` in registry.hpp must move into the namespace).
- [ ] **Step 3:** Run tests (they keep using unqualified names via aliases). Commit `refactor(components)!: move core classes into atom::components with compat aliases`.

### Task 13: CMake cleanup (WP7)

**Files:**
- Modify: `atom/components/CMakeLists.txt`

- [ ] **Step 1:** Remove `link_directories(...)` block (lines 119-125), phantom `add_subdirectory(tests)` block (lines 220-225), and the duplicated compat-header install (lines 176-196 duplicate 173-174's `${HEADERS}` install).
- [ ] **Step 2:** Reconfigure from scratch (`cmake -B build/components ...` same flags) + rebuild + run tests.
- [ ] **Step 3:** Commit `build(components): remove stale link_directories and duplicate installs`.

### Task 14: Docs + final verification

**Files:**
- Modify: `atom/components/CLAUDE.md` (changelog + corrected API examples), `docs/superpowers/plans/2026-06-11-components-optimization.md` (checkboxes)

- [ ] **Step 1:** Update module CLAUDE.md: real namespace story, event system option, removed absl, new APIs.
- [ ] **Step 2:** Full clean verify: reconfigure + rebuild + full test run; record final test count vs 332.
- [ ] **Step 3:** Commit `docs(components): update module documentation after optimization pass`.

---

## Self-review notes

- Spec coverage: WP1→Tasks 1-5, WP2→6, WP3→7, WP4→8-9, WP5→10, WP6→12, WP7→13, added functionality→11, verification→14. Out-of-scope items (Lua/Python enablement, iteration.hpp SIMD redesign, example corruption) intentionally have no tasks.
- Tasks 6, 7, 8, 10 are compiler-feedback-driven by nature (dead/drifted code); their contracts (type definitions, "implementation is source of truth") are pinned so the executor cannot wander.
- Type names used across tasks are consistent (`Event`, `EventCallback`, `EventCallbackId`, `dispatchAs`, `getCommandInfo`).
