#!/usr/bin/env python3
"""
Test validation script for Atom Image Processing Module tests.
This script validates the test suite for completeness, consistency, and quality.
"""

import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple


class TestValidator:
    def __init__(self, test_dir: str):
        self.test_dir = Path(test_dir)
        self.test_files = []
        self.issues = []
        self.stats = {
            'total_files': 0,
            'total_tests': 0,
            'total_test_classes': 0,
            'files_with_issues': 0,
            'coverage_areas': set()
        }

    def find_test_files(self) -> List[Path]:
        """Find all test header files."""
        test_files = []
        for file in self.test_dir.glob("test_*.hpp"):
            if file.name != "test_utils.hpp":  # Utility file, not a test file
                test_files.append(file)
        return sorted(test_files)

    def validate_file_structure(self, file_path: Path) -> List[str]:
        """Validate the structure of a test file."""
        issues = []

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except Exception as e:
            issues.append(f"Cannot read file: {e}")
            return issues

        # Check for required includes
        required_includes = ['<gtest/gtest.h>', '<gmock/gmock.h>']
        for include in required_includes:
            if include not in content:
                issues.append(f"Missing required include: {include}")

        # Check for header guards
        if '#ifndef' not in content or '#define' not in content or '#endif' not in content:
            issues.append("Missing or incomplete header guards")

        # Check for namespace
        if 'namespace atom::image::test' not in content:
            issues.append("Missing proper namespace declaration")

        # Check for test class
        test_class_pattern = r'class\s+\w+Test\s*:\s*public\s+::testing::Test'
        if not re.search(test_class_pattern, content):
            issues.append("Missing test class inheriting from ::testing::Test")

        return issues

    def count_tests(self, file_path: Path) -> Tuple[int, int]:
        """Count test cases and test classes in a file."""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except:
            return 0, 0

        # Count TEST_F macros
        test_cases = len(re.findall(r'TEST_F\s*\(', content))

        # Count test classes
        test_classes = len(re.findall(r'class\s+\w+Test\s*:\s*public\s+::testing::Test', content))

        return test_cases, test_classes

    def check_test_coverage(self, file_path: Path) -> List[str]:
        """Check if the test file covers important areas."""
        issues = []

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except:
            return ["Cannot read file for coverage analysis"]

        # Define coverage areas to check for
        coverage_checks = {
            'error_handling': [r'EXPECT_THROW', r'ASSERT_THROW', r'exception', r'error'],
            'edge_cases': [r'empty', r'null', r'zero', r'boundary', r'edge'],
            'performance': [r'performance', r'benchmark', r'time', r'speed'],
            'memory': [r'memory', r'leak', r'allocation', r'cleanup'],
            'concurrency': [r'thread', r'concurrent', r'parallel', r'atomic'],
        }

        file_coverage = set()
        for area, patterns in coverage_checks.items():
            for pattern in patterns:
                if re.search(pattern, content, re.IGNORECASE):
                    file_coverage.add(area)
                    self.stats['coverage_areas'].add(area)
                    break

        # Minimum coverage expectations
        if 'error_handling' not in file_coverage:
            issues.append("Missing error handling tests")
        if 'edge_cases' not in file_coverage:
            issues.append("Missing edge case tests")

        return issues

    def validate_test_naming(self, file_path: Path) -> List[str]:
        """Validate test naming conventions."""
        issues = []

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except:
            return ["Cannot read file for naming validation"]

        # Find all TEST_F declarations
        test_pattern = r'TEST_F\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)'
        tests = re.findall(test_pattern, content)

        for class_name, test_name in tests:
            # Check class naming convention
            if not class_name.endswith('Test'):
                issues.append(f"Test class '{class_name}' should end with 'Test'")

            # Check test name convention (should be descriptive)
            if len(test_name) < 5:
                issues.append(f"Test name '{test_name}' is too short, should be more descriptive")

            # Check for common anti-patterns
            if test_name.lower() in ['test', 'test1', 'test2', 'basic', 'simple']:
                issues.append(f"Test name '{test_name}' is too generic")

        return issues

    def validate_all_files(self) -> None:
        """Validate all test files."""
        self.test_files = self.find_test_files()
        self.stats['total_files'] = len(self.test_files)

        print(f"Found {len(self.test_files)} test files to validate...")
        print()

        for file_path in self.test_files:
            print(f"Validating {file_path.name}...")
            file_issues = []

            # Validate file structure
            file_issues.extend(self.validate_file_structure(file_path))

            # Count tests
            test_cases, test_classes = self.count_tests(file_path)
            self.stats['total_tests'] += test_cases
            self.stats['total_test_classes'] += test_classes

            # Check coverage
            file_issues.extend(self.check_test_coverage(file_path))

            # Validate naming
            file_issues.extend(self.validate_test_naming(file_path))

            if file_issues:
                self.stats['files_with_issues'] += 1
                self.issues.extend([(file_path.name, issue) for issue in file_issues])
                print(f"  ❌ {len(file_issues)} issues found")
                for issue in file_issues:
                    print(f"    - {issue}")
            else:
                print(f"  ✅ No issues found")

            print(f"  📊 {test_cases} test cases, {test_classes} test classes")
            print()

    def check_cmake_integration(self) -> List[str]:
        """Check if all test files are properly integrated in CMakeLists.txt."""
        issues = []
        cmake_file = self.test_dir / "CMakeLists.txt"

        if not cmake_file.exists():
            return ["CMakeLists.txt not found"]

        try:
            with open(cmake_file, 'r', encoding='utf-8') as f:
                cmake_content = f.read()
        except:
            return ["Cannot read CMakeLists.txt"]

        # Check if all test files are listed
        for test_file in self.test_files:
            if test_file.name not in cmake_content:
                issues.append(f"Test file {test_file.name} not listed in CMakeLists.txt")

        return issues

    def generate_report(self) -> None:
        """Generate a comprehensive validation report."""
        print("=" * 80)
        print("TEST SUITE VALIDATION REPORT")
        print("=" * 80)
        print()

        # Statistics
        print("📊 STATISTICS:")
        print(f"  Total test files: {self.stats['total_files']}")
        print(f"  Total test cases: {self.stats['total_tests']}")
        print(f"  Total test classes: {self.stats['total_test_classes']}")
        print(f"  Files with issues: {self.stats['files_with_issues']}")
        print(f"  Coverage areas: {', '.join(sorted(self.stats['coverage_areas']))}")
        print()

        # CMake integration
        print("🔧 CMAKE INTEGRATION:")
        cmake_issues = self.check_cmake_integration()
        if cmake_issues:
            for issue in cmake_issues:
                print(f"  ❌ {issue}")
        else:
            print("  ✅ All test files properly integrated")
        print()

        # Issues summary
        if self.issues:
            print("❌ ISSUES FOUND:")
            current_file = None
            for file_name, issue in self.issues:
                if file_name != current_file:
                    print(f"\n  {file_name}:")
                    current_file = file_name
                print(f"    - {issue}")
        else:
            print("✅ NO ISSUES FOUND!")
        print()

        # Recommendations
        print("💡 RECOMMENDATIONS:")
        if self.stats['total_tests'] < 50:
            print("  - Consider adding more test cases for better coverage")
        if 'performance' not in self.stats['coverage_areas']:
            print("  - Add performance/benchmark tests")
        if 'concurrency' not in self.stats['coverage_areas']:
            print("  - Add concurrency/thread safety tests")
        if self.stats['files_with_issues'] == 0:
            print("  - Test suite looks good! Consider running actual tests to verify functionality")
        print()

        # Overall assessment
        success_rate = ((self.stats['total_files'] - self.stats['files_with_issues']) /
                       max(self.stats['total_files'], 1)) * 100

        print("🎯 OVERALL ASSESSMENT:")
        print(f"  Success rate: {success_rate:.1f}%")

        if success_rate >= 90:
            print("  Status: ✅ EXCELLENT - Test suite is well-structured")
        elif success_rate >= 75:
            print("  Status: ✅ GOOD - Minor issues to address")
        elif success_rate >= 50:
            print("  Status: ⚠️  FAIR - Several issues need attention")
        else:
            print("  Status: ❌ POOR - Significant improvements needed")

        print("=" * 80)

def main():
    """Main function."""
    if len(sys.argv) > 1:
        test_dir = sys.argv[1]
    else:
        test_dir = "."

    validator = TestValidator(test_dir)
    validator.validate_all_files()
    validator.generate_report()

    # Exit with error code if issues found
    if validator.issues or validator.stats['files_with_issues'] > 0:
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    main()
