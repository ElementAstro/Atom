#!/usr/bin/env python3

"""
test_system_status.py - Show status of testing and coverage system
This project is licensed under the terms of the GPL3 license.

Author: Max Qian
License: GPL3
"""

import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Dict, Optional


class Colors:
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[1;33m"
    BLUE = "\033[0;34m"
    CYAN = "\033[0;36m"
    BOLD = "\033[1m"
    NC = "\033[0m"


def print_colored(message: str, color: str = Colors.NC) -> None:
    """Print a colored message."""
    print(f"{color}{message}{Colors.NC}")


def print_section(title: str) -> None:
    """Print a section header."""
    print_colored(f"\n{Colors.BOLD}{'='*60}{Colors.NC}")
    print_colored(f"{Colors.BOLD}{title}{Colors.NC}")
    print_colored(f"{Colors.BOLD}{'='*60}{Colors.NC}")


def check_command(command: str) -> bool:
    """Check if a command is available."""
    try:
        subprocess.run([command, "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def check_python_package(package: str) -> bool:
    """Check if a Python package is installed."""
    try:
        __import__(package.replace("-", "_"))
        return True
    except ImportError:
        return False


def get_file_count(directory: Path, pattern: str) -> int:
    """Get count of files matching pattern in directory."""
    if not directory.exists():
        return 0
    return len(list(directory.rglob(pattern)))


def get_coverage_data() -> Optional[Dict]:
    """Get coverage data if available."""
    coverage_file = Path("coverage/unified/coverage.json")
    if coverage_file.exists():
        try:
            with open(coverage_file) as f:
                return json.load(f)
        except Exception:
            pass
    return None


def main():
    """Main function."""
    project_root = Path.cwd()

    print_colored(
        f"{Colors.CYAN}{Colors.BOLD}Atom Testing & Coverage System Status{Colors.NC}"
    )
    print_colored(f"{Colors.CYAN}Project: {project_root.name}{Colors.NC}")
    print_colored(f"{Colors.CYAN}Path: {project_root}{Colors.NC}")

    # System Dependencies
    print_section("System Dependencies")

    dependencies = {
        "C++ Tools": {
            "cmake": check_command("cmake"),
            "make": check_command("make"),
            "gcc/g++": check_command("g++"),
            "gcov": check_command("gcov"),
            "lcov": check_command("lcov"),
            "genhtml": check_command("genhtml"),
        },
        "Python Tools": {
            "python": check_command("python"),
            "pip": check_command("pip"),
            "pytest": check_python_package("pytest"),
            "pytest-cov": check_python_package("pytest_cov"),
            "coverage": check_python_package("coverage"),
        },
    }

    for category, tools in dependencies.items():
        print_colored(f"\n{Colors.BOLD}{category}:{Colors.NC}")
        for tool, available in tools.items():
            status = (
                f"{Colors.GREEN}✓{Colors.NC}"
                if available
                else f"{Colors.RED}✗{Colors.NC}"
            )
            print(f"  {status} {tool}")

    # Test Directory Structure
    print_section("Test Directory Structure")

    cpp_test_dir = project_root / "tests"
    python_test_dir = project_root / "python" / "tests"

    print_colored(f"\n{Colors.BOLD}C++ Tests ({cpp_test_dir}):{Colors.NC}")
    if cpp_test_dir.exists():
        cpp_modules = [d.name for d in cpp_test_dir.iterdir() if d.is_dir()]
        cpp_test_files = get_file_count(cpp_test_dir, "*.cpp")
        cpp_header_files = get_file_count(cpp_test_dir, "*.hpp")

        print(f"  📁 Modules: {len(cpp_modules)}")
        print(f"  📄 Test files (.cpp): {cpp_test_files}")
        print(f"  📄 Header files (.hpp): {cpp_header_files}")
        print(f"  📂 Modules: {', '.join(sorted(cpp_modules))}")
    else:
        print_colored("  ❌ Directory not found", Colors.RED)

    print_colored(f"\n{Colors.BOLD}Python Tests ({python_test_dir}):{Colors.NC}")
    if python_test_dir.exists():
        python_test_files = get_file_count(python_test_dir, "test_*.py")
        python_all_files = get_file_count(python_test_dir, "*.py")

        print(f"  📄 Test files: {python_test_files}")
        print(f"  📄 Total Python files: {python_all_files}")

        # List test files
        test_files = [f.name for f in python_test_dir.glob("test_*.py")]
        if test_files:
            print(f"  📂 Test files: {', '.join(sorted(test_files))}")
    else:
        print_colored("  ❌ Directory not found", Colors.RED)

    # Configuration Files
    print_section("Configuration Files")

    config_files = {
        "CMakeLists.txt": project_root / "CMakeLists.txt",
        "pyproject.toml": project_root / "pyproject.toml",
        "CMakePresets.json": project_root / "CMakePresets.json",
        "Makefile": project_root / "Makefile",
        "Coverage Config": project_root / "cmake" / "CoverageConfig.cmake",
        "Test Build Options": project_root / "cmake" / "TestsBuildOptions.cmake",
        "GitHub Actions": project_root / ".github" / "workflows" / "coverage.yml",
    }

    for name, path in config_files.items():
        status = (
            f"{Colors.GREEN}✓{Colors.NC}"
            if path.exists()
            else f"{Colors.RED}✗{Colors.NC}"
        )
        print(f"  {status} {name}")

    # Scripts
    print_section("Coverage Scripts")

    scripts_dir = project_root / "scripts"
    scripts = {
        "Unified Coverage": "unified_coverage.py",
        "Python Coverage": "python_coverage.py",
        "C++ Coverage": "coverage.sh",
        "Coverage Badges": "coverage_badge.py",
        "Convention Checker": "check_test_conventions.py",
        "Test Migrator": "migrate_test_conventions.py",
        "Test Reorganizer": "reorganize_tests.py",
    }

    for name, script in scripts.items():
        script_path = scripts_dir / script
        status = (
            f"{Colors.GREEN}✓{Colors.NC}"
            if script_path.exists()
            else f"{Colors.RED}✗{Colors.NC}"
        )
        executable = (
            "🔧" if script_path.exists() and os.access(script_path, os.X_OK) else ""
        )
        print(f"  {status} {name} {executable}")

    # Documentation
    print_section("Documentation")

    docs_dir = project_root / "docs"
    docs = {
        "Testing Conventions": "TESTING_CONVENTIONS.md",
        "Coverage Guide": "COVERAGE_GUIDE.md",
        "Test README": project_root / "tests" / "README.md",
    }

    for name, doc in docs.items():
        if isinstance(doc, str):
            doc_path = docs_dir / doc
        else:
            doc_path = doc
        status = (
            f"{Colors.GREEN}✓{Colors.NC}"
            if doc_path.exists()
            else f"{Colors.RED}✗{Colors.NC}"
        )
        print(f"  {status} {name}")

    # Coverage Status
    print_section("Coverage Status")

    coverage_data = get_coverage_data()
    if coverage_data:
        overall = coverage_data.get("overall", {})
        cpp = coverage_data.get("cpp", {})
        python = coverage_data.get("python", {})

        print_colored(f"\n{Colors.BOLD}Latest Coverage Results:{Colors.NC}")
        print(f"  🎯 Overall: {overall.get('coverage_percentage', 0):.1f}%")
        print(f"  🔧 C++: {cpp.get('coverage_percentage', 0):.1f}%")
        print(f"  🐍 Python: {python.get('coverage_percentage', 0):.1f}%")
        print(f"  📊 Total Lines: {overall.get('total_lines', 0):,}")
        print(f"  ✅ Covered Lines: {overall.get('covered_lines', 0):,}")

        timestamp = coverage_data.get("timestamp", "Unknown")
        print(f"  🕒 Generated: {timestamp}")
    else:
        print_colored("  ❌ No coverage data found", Colors.YELLOW)
        print("     Run 'make coverage-unified' to generate coverage reports")

    # Build Status
    print_section("Build System Status")

    build_dir = project_root / "build"
    if build_dir.exists():
        print_colored(f"  ✅ Build directory exists: {build_dir}", Colors.GREEN)

        # Check for CMake cache
        cmake_cache = build_dir / "CMakeCache.txt"
        if cmake_cache.exists():
            print("  ✅ CMake configured")

            # Check for coverage configuration
            try:
                with open(cmake_cache) as f:
                    cache_content = f.read()
                    if "ATOM_ENABLE_COVERAGE:BOOL=ON" in cache_content:
                        print("  ✅ Coverage enabled in build")
                    else:
                        print("  ⚠️  Coverage not enabled in build")
            except Exception:
                print("  ❓ Could not read CMake cache")
        else:
            print("  ❌ CMake not configured")
    else:
        print_colored("  ❌ Build directory not found", Colors.YELLOW)
        print("     Run 'cmake -B build' to configure build")

    # Quick Start Guide
    print_section("Quick Start Commands")

    commands = [
        ("Configure build with coverage", "cmake -B build -DATOM_ENABLE_COVERAGE=ON"),
        ("Build project", "make build"),
        ("Run tests with coverage", "make test-coverage"),
        ("Generate unified coverage", "make coverage-unified"),
        ("Check test conventions", "python scripts/check_test_conventions.py"),
        ("View coverage guide", "cat docs/COVERAGE_GUIDE.md"),
    ]

    for description, command in commands:
        print(f"  📝 {description}:")
        print_colored(f"     {command}", Colors.CYAN)

    print_colored(
        f"\n{Colors.GREEN}✨ Testing system status check complete!{Colors.NC}"
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
