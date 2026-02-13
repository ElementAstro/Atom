# Atom Project Documentation Index

## Standard Operating Procedures (SOP)

[Test Fixes - Systematic Testing Campaign](sop/test-fix-systematic-testing.md): Documents all test fixes applied during systematic testing, including test infrastructure corrections, statistical test improvements, algorithm correctness fixes, and performance threshold calibration across algorithm module tests.

[Python Bindings Build System Improvements](sop/python-bindings-build-system-improvements.md): Documents comprehensive improvements to Python bindings across two phases: Phase 1 created 9 missing extra submodule __init__.py files and enhanced 3 core modules; Phase 2 fixed critical CMake build directory detection, created 10 algorithm subdirectory __init__.py files, created main package __init__.py, and enhanced CMake subdirectory detection for automatic __init__.py installation.

[xmake Python Bindings Support Implementation](sop/xmake-python-bindings-support.md): Documents the addition of comprehensive xmake support for building Python bindings, achieving feature parity with CMake including automatic module discovery, recursive source collection, module-specific dependency configuration, and nested __init__.py installation.

## Features

[Python Bindings for Utils Module](feature/python-bindings-utils.md): Comprehensive documentation of the 31 Python binding files in `python/utils/`, covering 250+ functions, 50+ classes, and the mapping between flat Python module structure and hierarchical C++ organization across 11 subdirectories (core, crypto, text, conversion, container, time, random, process, memory, format, debug).

## Agents

To be populated as agent outputs are generated
