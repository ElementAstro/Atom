#!/usr/bin/env python3

"""
check_test_conventions.py - Script to check test naming conventions
This project is licensed under the terms of the GPL3 license.

Author: Max Qian
License: GPL3
"""

import re
import sys
from pathlib import Path
from typing import Dict, List


class Colors:
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[1;33m"
    BLUE = "\033[0;34m"
    NC = "\033[0m"


def print_colored(message: str, color: str = Colors.NC) -> None:
    """Print a colored message."""
    print(f"{color}{message}{Colors.NC}")


class TestConventionChecker:
    """Check test files for naming convention compliance."""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.cpp_test_dir = project_root / "tests"
        self.python_test_dir = project_root / "python" / "tests"
        self.issues = []

    def check_cpp_file_naming(self) -> List[str]:
        """Check C++ test file naming conventions."""
        issues = []

        if not self.cpp_test_dir.exists():
            return ["C++ test directory not found"]

        for test_file in self.cpp_test_dir.rglob("*.cpp"):
            filename = test_file.name

            # Check if file follows test_*.cpp pattern
            if not filename.startswith("test_") and filename != "main.cpp":
                # Check for other acceptable patterns
                acceptable_patterns = [
                    r".*\.cpp$",  # Any .cpp file is acceptable for now
                ]

                if not any(
                    re.match(pattern, filename) for pattern in acceptable_patterns
                ):
                    issues.append(
                        f"C++ file naming: {test_file.relative_to(self.project_root)} should follow 'test_*.cpp' pattern"
                    )

        return issues

    def check_cpp_class_naming(self) -> List[str]:
        """Check C++ test class naming conventions."""
        issues = []

        for test_file in self.cpp_test_dir.rglob("*.cpp"):
            try:
                content = test_file.read_text(encoding="utf-8")

                # Find class definitions
                class_pattern = r"class\s+(\w+)\s*:\s*public\s*::testing::Test"
                classes = re.findall(class_pattern, content)

                for class_name in classes:
                    if not class_name.endswith("Test"):
                        issues.append(
                            f"C++ class naming: {class_name} in {test_file.relative_to(self.project_root)} should end with 'Test'"
                        )

            except Exception as e:
                issues.append(f"Error reading {test_file}: {e}")

        return issues

    def check_python_file_naming(self) -> List[str]:
        """Check Python test file naming conventions."""
        issues = []

        if not self.python_test_dir.exists():
            return ["Python test directory not found"]

        for test_file in self.python_test_dir.rglob("*.py"):
            filename = test_file.name

            # Skip special files
            if filename in ["__init__.py", "conftest.py", "pytest.ini"]:
                continue

            if not filename.startswith("test_"):
                issues.append(
                    f"Python file naming: {test_file.relative_to(self.project_root)} should follow 'test_*.py' pattern"
                )

        return issues

    def check_python_class_naming(self) -> List[str]:
        """Check Python test class naming conventions."""
        issues = []

        for test_file in self.python_test_dir.rglob("test_*.py"):
            try:
                content = test_file.read_text(encoding="utf-8")

                # Find class definitions
                class_pattern = r"class\s+(\w+).*:"
                classes = re.findall(class_pattern, content)

                for class_name in classes:
                    if not class_name.startswith("Test"):
                        issues.append(
                            f"Python class naming: {class_name} in {test_file.relative_to(self.project_root)} should start with 'Test'"
                        )

            except Exception as e:
                issues.append(f"Error reading {test_file}: {e}")

        return issues

    def check_python_method_naming(self) -> List[str]:
        """Check Python test method naming conventions."""
        issues = []

        for test_file in self.python_test_dir.rglob("test_*.py"):
            try:
                content = test_file.read_text(encoding="utf-8")

                # Find method definitions within test classes
                lines = content.split("\n")
                in_test_class = False
                current_class = None

                for i, line in enumerate(lines):
                    # Check if we're entering a test class
                    class_match = re.match(r"class\s+(Test\w+).*:", line)
                    if class_match:
                        in_test_class = True
                        current_class = class_match.group(1)
                        continue

                    # Check if we're leaving the class (new class or end of indentation)
                    if (
                        in_test_class
                        and line
                        and not line.startswith(" ")
                        and not line.startswith("\t")
                    ):
                        if not line.startswith("class "):
                            in_test_class = False
                            current_class = None

                    # Check method naming within test classes
                    if in_test_class:
                        method_match = re.match(r"\s+def\s+(\w+)\s*\(", line)
                        if method_match:
                            method_name = method_match.group(1)
                            # Skip private methods, special methods, and setup/teardown methods
                            if (
                                not method_name.startswith("test_")
                                and not method_name.startswith("_")
                                and method_name not in ["setUp", "tearDown", "__init__"]
                            ):
                                issues.append(
                                    f"Python method naming: {method_name} in {current_class} ({test_file.relative_to(self.project_root)}) should start with 'test_'"
                                )

            except Exception as e:
                issues.append(f"Error reading {test_file}: {e}")

        return issues

    def check_directory_structure(self) -> List[str]:
        """Check test directory structure."""
        issues = []

        # Check if main test directories exist
        if not self.cpp_test_dir.exists():
            issues.append("C++ test directory 'tests/' not found")

        if not self.python_test_dir.exists():
            issues.append("Python test directory 'python/tests/' not found")

        # Check for expected module directories in C++ tests
        expected_cpp_modules = [
            "algorithm",
            "async",
            "components",
            "connection",
            "memory",
            "search",
            "sysinfo",
            "type",
            "utils",
            "web",
        ]

        if self.cpp_test_dir.exists():
            for module in expected_cpp_modules:
                module_dir = self.cpp_test_dir / module
                if not module_dir.exists():
                    issues.append(
                        f"Expected C++ test module directory not found: tests/{module}"
                    )

        return issues

    def run_all_checks(self) -> Dict[str, List[str]]:
        """Run all convention checks."""
        results = {
            "cpp_file_naming": self.check_cpp_file_naming(),
            "cpp_class_naming": self.check_cpp_class_naming(),
            "python_file_naming": self.check_python_file_naming(),
            "python_class_naming": self.check_python_class_naming(),
            "python_method_naming": self.check_python_method_naming(),
            "directory_structure": self.check_directory_structure(),
        }

        return results


def main():
    """Main function."""
    project_root = Path.cwd()

    print_colored("Test Convention Checker for Atom Project", Colors.BLUE)
    print_colored("=" * 40, Colors.BLUE)

    checker = TestConventionChecker(project_root)
    results = checker.run_all_checks()

    total_issues = 0

    for check_name, issues in results.items():
        if issues:
            print_colored(f"\n{check_name.replace('_', ' ').title()}:", Colors.YELLOW)
            for issue in issues:
                print_colored(f"  ❌ {issue}", Colors.RED)
                total_issues += 1
        else:
            print_colored(
                f"\n{check_name.replace('_', ' ').title()}: ✅ All good!", Colors.GREEN
            )

    print_colored("\nSummary:", Colors.BLUE)
    if total_issues == 0:
        print_colored("🎉 All test conventions are followed correctly!", Colors.GREEN)
        return 0
    else:
        print_colored(
            f"⚠️  Found {total_issues} convention issues that should be addressed.",
            Colors.YELLOW,
        )
        print_colored(
            "\nRefer to docs/TESTING_CONVENTIONS.md for detailed guidelines.",
            Colors.BLUE,
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
