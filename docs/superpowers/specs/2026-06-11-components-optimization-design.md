# atom/components Optimization — Design

> Date: 2026-06-11
> Status: approved for implementation (autonomous goal session)
> Baseline: build green on MSYS2 MinGW64 (GCC 15.2), `atom_components_tests` 332/332 passing

## 1. Problem statement

`atom/components` is feature-rich (command dispatch, variables, lifecycle, pooling,
scripting, serialization) but suffers from:

1. **Namespace chaos** — `Component`, `Registry`, `CommandDispatcher`,
   `VariableManager` and all their exceptions live in the **global namespace**;
   pool/lifecycle/serialization are in `atom::components`; scripting (and,
   inconsistently, `data/type_conversion.hpp`) in `atom::components::scripting`.
   `lifecycle/dispatch.hpp` puts `using json = nlohmann::json;` at global scope in a
   public header. `core/component.hpp` leaks `OP_EQ`/`CONDITION_*`/`REGISTER_OPERATOR`
   macros; `core/package.hpp` leaks `ALIGNMENT`/`MAX_ELEMENTS` globals.
2. **Dead/broken subsystems**
   - The event system (`Component::emitEvent/on/once/off`, `Registry::subscribeToEvent`)
     is guarded by `ENABLE_EVENT_SYSTEM`, which is never 1, and references types
     (`atom::components::Event`, `EventCallback`, `EventCallbackId`) that are
     **defined nowhere** — it cannot compile if enabled.
   - `core/module_macro.hpp`: `ATOM_MODULE_INIT` passes 0-arg lambdas where
     `Component::InitFunc` (= `std::function<void(Component&)>`) is expected; the
     `ATOM_MODULE`/`ATOM_EMBED_MODULE` macros do not compile when instantiated.
     The corresponding test (`types_and_macros.cpp`) is disabled.
3. **Disabled tests** — 6 test files disabled in `tests/components/CMakeLists.txt`
   (`type_conversion`, `scripting_api`, `script_sandbox`, `script_engine`,
   `advanced_bindings`, `types_and_macros`) due to API drift.
4. **Duplication instead of reuse**
   - `data/type_conversion.hpp` hand-rolls `is_container`/`is_associative`/
     `is_optional`/`is_smart_pointer`/`is_tuple` traits that already exist in
     `atom/meta/concept.hpp`, `atom/meta/container_traits.hpp`,
     `atom/meta/template_traits.hpp`.
   - `core/package.hpp` hand-rolls a constexpr JSON-line parser and pulls in
     **abseil** (`absl/strings/match.h`) — the only absl use in the repo; the project
     standard is `atom/type/json.hpp` (nlohmann).
   - `component.template` is a 98-line manual arity-0..8 expansion included
     *inside a member function body* — replaceable by one `std::index_sequence` lambda.
5. **Build-system debt** — `atom/components/CMakeLists.txt` hardcodes
   `link_directories(../../build/...)`, installs root compat headers twice, and
   references a nonexistent `tests/` subdirectory.
6. **Cosmetics that hurt review** — mis-indented blocks in
   `ComponentPerformanceStats` (`reset`, `updateExecutionTime`), an MSVC-vs-GCC
   `constexpr` `#if` hack, spdlog `info`-level spam in hot paths (`addVariable`).

External consumers: none inside atom (application-level module); only tests and
examples include it, so interface rationalization is low-risk **if compat aliases
are kept**.

## 2. Approaches considered

- **A. Big-bang rewrite** into `atom::components` with breaking changes — rejected:
  high risk, destroys the green baseline, no incremental verification.
- **B. Incremental hygiene + completion passes per submodule, with backward-compat
  aliases, keeping tests green after every pass** — **chosen**.
- **C. Bug-fix only** — rejected: does not meet the goal (reuse, interface
  rationalization, added functionality).

## 3. Design (approach B) — work packages

Each WP ends with: full rebuild + `atom_components_tests` run; no regressions.

### WP1 — core hygiene (`core/component.hpp`, `component.template`, `lifecycle/dispatch.hpp`)
- Fix `ComponentPerformanceStats` indentation; drop the `#if defined(_MSC_VER)`
  constexpr fork (plain `void reset() noexcept` — atomics are not constexpr anyway).
- Inline `component.template` into `Component::def(Callable&&)` using
  `[&]<std::size_t... I>(std::index_sequence<I...>)`; delete the file; update CMake.
- Remove `OP_*`/`CONDITION_*`/`REGISTER_OPERATOR` macros — implement
  `registerOperators` with `if constexpr` + standard concepts directly.
- `#undef` the `DEF_MEMBER_FUNC*` helper macros after use.
- Remove global `using json = nlohmann::json;` from `dispatch.hpp` (qualify uses).
- Reduce hot-path logging to `spdlog::trace`/`debug` where it is per-call spam.

### WP2 — module macros + registry coherence (`core/module_macro.hpp`, `core/registry.*`)
- Make `ATOM_MODULE_INIT`/`ATOM_MODULE`/`ATOM_EMBED_MODULE` actually compile against
  the real `Registry`/`Component::InitFunc` signatures (adapt the lambdas; the
  Registry API is the source of truth).
- Re-enable `types_and_macros.cpp`, fixing the test to target the real macros/API.

### WP3 — event system completion (`core/types.hpp`, `core/component.*`, `core/registry.*`)
- Define `atom::components::Event` (name + `std::any` payload + timestamp + source),
  `EventCallback`, `EventCallbackId` in `core/types.hpp`.
- Replace `ENABLE_EVENT_SYSTEM` with CMake option `ATOM_COMPONENTS_ENABLE_EVENTS`
  (default ON) defining `ENABLE_EVENT_SYSTEM=1` for compatibility; compile and fix
  the long-dead `#if` bodies; add event tests (emit/on/once/off, registry-level
  subscribe/trigger).

### WP4 — data/: reuse meta, drop absl (`data/type_conversion.hpp`, `data/var.*`, `core/package.hpp`)
- Rebase `type_conversion.hpp` traits on `atom/meta` (delete local trait
  duplicates); fix/align converter methods; re-enable `type_conversion.cpp` test.
- `package.hpp`: remove the absl include (string_view replacements), scope constants
  into a namespace; keep the constexpr parser (it serves compile-time package
  manifests; nlohmann stays the runtime JSON).
- `var.hpp`: keep `Trackable<T>` reuse; logging to trace level.

### WP5 — scripting/: align API with tests, re-enable 4 test files
- For each of `scripting_api`, `script_sandbox`, `script_engine`,
  `advanced_bindings`: implementation is the source of truth; update tests to the
  real API, but add genuinely missing small APIs where tests reveal sensible gaps.

### WP6 — namespace unification with compat aliases
- Move `Component`, `Registry`, `CommandDispatcher`, `VariableManager`,
  `ObjectExpiredError`, `VariableTypeError`, `DispatchException`, `DispatchTimeout`,
  `ComponentState`, `ComponentPerformanceStats`, `ComponentType` into
  `namespace atom::components`.
- Keep global-scope `using atom::components::Component;` (etc.) in the same headers
  so existing tests/examples compile unchanged.
- `data/type_conversion.hpp` namespace stays `atom::components::scripting` only if
  it remains scripting-coupled; otherwise `atom::components`.

### WP7 — CMake cleanup (`atom/components/CMakeLists.txt`)
- Drop `link_directories` hacks (targets come from the top-level build), the
  phantom `add_subdirectory(tests)`, and the duplicated header install list.
- Remove `component.template` from installs; add the events option.

### Added functionality (beyond restoration)
- `Component::def(...)` returns `Component&` for fluent chaining (pybind11 style).
- `Component::dispatchAs<T>(name, args...)` — typed `any_cast` wrapper.
- Command introspection: `Component::getCommandInfo(name)` → JSON (name, group,
  description, aliases) for tooling/scripting UIs.

### Out of scope
- Lua/Python engine enablement (optional deps, not installed in CI baseline).
- `lifecycle/iteration.hpp` SoA/SIMD redesign (works; performance work is separate).
- `example/components/*` glued-comment corruption (documented; examples are not the
  verification target — same policy as example/meta).

## 4. Verification

- After every WP: `cmake --build build/components -j -- -k 0` +
  `atom_components_tests.exe` — zero failures, test count strictly grows as files
  are re-enabled (baseline 332).
- New event-system and introspection APIs get new GoogleTest coverage.
