# Repository Guidelines

## Project Structure & Module Organization

- `atom/` — C++ core library, organized by domain (algorithm, async, io, etc.).
- `python/` — Pybind11 bindings and the `atom` Python package.
- `tests/` — C++ test suite (GoogleTest via CMake/CTest). Python tests, if any, also live here.
- `docs/` — Sphinx docs; `doc/` — Doxygen configuration (`Doxyfile`).
- `cmake/`, `scripts/`, `example/`, `build/` (generated).

## Build, Test, and Development Commands

- C++ build (Ninja default): `cmake --preset release && cmake --build --preset release -j`
- C++ tests: `cmake --preset debug && cmake --build --preset debug -j && ctest --preset default --output-on-failure`
- Cross‑platform scripts: `./build.sh` (Unix) or `build.bat` (Windows) - wrapper scripts for backward compatibility
- Direct script access: `./scripts/build.sh` (Unix) or `scripts\build.bat` (Windows) - actual build scripts
- Python dev setup: `pip install -e .[dev]`
- Python tests: `pytest -q` (coverage configured via `pyproject.toml`)
- Docs: Sphinx `sphinx-build -b html docs docs/_build`; Doxygen `doxygen Doxyfile`

## Coding Style & Naming Conventions

- C++: 4‑space indent, 80‑column guide; format with `clang-format` (see `.clang-format`).
- Naming (C++): camelCase for variables/functions, PascalCase for classes/namespaces, UPPER_SNAKE_CASE for constants, files `lower_snake_case.[cpp|hpp]` (see `STYLE_OF_CODE.md`). Prefer Doxygen comments.
- Python: Black (88 cols), isort, Ruff, MyPy (configured in `pyproject.toml`). Run: `pre-commit run -a`.

## Testing Guidelines

- C++: Use GoogleTest; place tests under `tests/<module>/` and register targets in the local `CMakeLists.txt`. Run via CTest; include edge cases and failure paths.
- Python: pytest patterns `test_*.py`, marks available (`unit`, `integration`, `slow`). Aim to keep coverage healthy; prefer small, focused tests.

## Commit & Pull Request Guidelines

- Commits: short imperative subject (≤72 chars), descriptive body when needed. Reference issues (`#123`). Conventional commit prefixes are optional.
- PRs: clear description, rationale, linked issues, tests added/updated, and doc changes if behavior/user‑facing APIs change. Ensure `pre-commit` passes and CI is green.

## Security & Configuration Tips

- Don’t commit secrets; prefer env vars. Build requires CMake ≥3.21 and a modern compiler (MSVC 2022/GCC/Clang). C/C++ deps via vcpkg/Conan; Python ≥3.8.
