#!/usr/bin/env python3
"""
Validation script for Atom I/O examples
This script validates the structure and content of the I/O examples
"""

import os
import re
import sys
from pathlib import Path


def validate_file_structure():
    """Validate the directory structure and file presence"""
    print("🔍 Validating file structure...")

    base_dir = Path(__file__).parent
    expected_structure = {
        'core': [
            'basic_file_operations.cpp',
            'file_splitting.cpp',
            'directory_traversal.cpp',
            'glob_patterns.cpp',
            'advanced_glob_patterns.cpp'
        ],
        'async': [
            'basic_async_io.cpp',
            'advanced_async_operations.cpp',
            'coroutine_operations.cpp',
            'async_glob_operations.cpp'
        ],
        'compression': [
            'basic_compression.cpp',
            'advanced_compression.cpp',
            'async_compression.cpp'
        ],
        'filesystem': [
            'directory_stack.cpp',
            'file_permissions.cpp',
            'file_information.cpp'
        ]
    }

    issues = []

    # Check directories exist
    for category in expected_structure.keys():
        category_dir = base_dir / category
        if not category_dir.exists():
            issues.append(f"Missing directory: {category}")
            continue

        # Check files in each category
        for filename in expected_structure[category]:
            file_path = category_dir / filename
            if not file_path.exists():
                issues.append(f"Missing file: {category}/{filename}")

    # Check integration examples
    integration_file = base_dir / 'integration_examples.cpp'
    if not integration_file.exists():
        issues.append("Missing file: integration_examples.cpp")

    # Check build files
    cmake_file = base_dir / 'CMakeLists.txt'
    if not cmake_file.exists():
        issues.append("Missing file: CMakeLists.txt")

    readme_file = base_dir / 'README.md'
    if not readme_file.exists():
        issues.append("Missing file: README.md")

    if issues:
        print("❌ Structure validation failed:")
        for issue in issues:
            print(f"  - {issue}")
        return False
    else:
        print("✅ File structure validation passed")
        return True


def validate_cpp_files():
    """Validate C++ files for basic syntax and structure"""
    print("\n🔍 Validating C++ files...")

    base_dir = Path(__file__).parent
    cpp_files = list(base_dir.rglob("*.cpp"))

    issues = []

    for cpp_file in cpp_files:
        try:
            with open(cpp_file, 'r', encoding='utf-8') as f:
                content = f.read()

            # Check for basic C++ structure
            if not re.search(r'#include\s*<iostream>', content):
                issues.append(f"{cpp_file.name}: Missing iostream include")

            if not re.search(r'int\s+main\s*\(', content):
                issues.append(f"{cpp_file.name}: Missing main function")

            # Check for atom/io includes
            if not re.search(r'#include\s*"atom/io/', content):
                issues.append(f"{cpp_file.name}: Missing atom/io includes")

            # Check for documentation
            if not re.search(r'/\*\*.*@file.*\*/', content, re.DOTALL):
                issues.append(f"{cpp_file.name}: Missing file documentation")

            # Check for cleanup function (most examples should have this)
            if 'cleanup' not in content.lower() and 'clean' not in content.lower():
                issues.append(
                    f"{cpp_file.name}: Missing cleanup functionality")

        except Exception as e:
            issues.append(f"{cpp_file.name}: Error reading file - {e}")

    if issues:
        print("❌ C++ validation failed:")
        for issue in issues:
            print(f"  - {issue}")
        return False
    else:
        print("✅ C++ files validation passed")
        return True


def validate_cmake():
    """Validate CMakeLists.txt structure"""
    print("\n🔍 Validating CMakeLists.txt...")

    base_dir = Path(__file__).parent
    cmake_file = base_dir / 'CMakeLists.txt'

    if not cmake_file.exists():
        print("❌ CMakeLists.txt not found")
        return False

    try:
        with open(cmake_file, 'r', encoding='utf-8') as f:
            content = f.read()

        issues = []

        # Check for required CMake elements
        required_elements = [
            r'cmake_minimum_required',
            r'set\(CMAKE_CXX_STANDARD\s+20\)',
            r'create_subdir_examples',
            r'target_link_libraries.*atom',
            r'add_custom_target.*io_examples'
        ]

        for element in required_elements:
            if not re.search(element, content):
                issues.append(f"Missing or incorrect: {element}")

        # Check for category handling
        categories = ['core', 'async', 'compression', 'filesystem']
        for category in categories:
            if category not in content:
                issues.append(f"Missing category handling: {category}")

        if issues:
            print("❌ CMakeLists.txt validation failed:")
            for issue in issues:
                print(f"  - {issue}")
            return False
        else:
            print("✅ CMakeLists.txt validation passed")
            return True

    except Exception as e:
        print(f"❌ Error reading CMakeLists.txt: {e}")
        return False


def validate_documentation():
    """Validate README.md and documentation"""
    print("\n🔍 Validating documentation...")

    base_dir = Path(__file__).parent
    readme_file = base_dir / 'README.md'

    if not readme_file.exists():
        print("❌ README.md not found")
        return False

    try:
        with open(readme_file, 'r', encoding='utf-8') as f:
            content = f.read()

        issues = []

        # Check for required sections
        required_sections = [
            r'# Atom I/O Examples',
            r'## Directory Structure',
            r'## Example Categories',
            r'## Building the Examples',
            r'## Running the Examples'
        ]

        for section in required_sections:
            if not re.search(section, content):
                issues.append(f"Missing section: {section}")

        # Check that all example files are documented
        cpp_files = list(base_dir.rglob("*.cpp"))
        for cpp_file in cpp_files:
            filename = cpp_file.name
            if filename not in content:
                issues.append(f"Example not documented: {filename}")

        if issues:
            print("❌ Documentation validation failed:")
            for issue in issues:
                print(f"  - {issue}")
            return False
        else:
            print("✅ Documentation validation passed")
            return True

    except Exception as e:
        print(f"❌ Error reading README.md: {e}")
        return False


def main():
    """Main validation function"""
    print("🧪 Atom I/O Examples Validation")
    print("=" * 40)

    all_passed = True

    # Run all validations
    validations = [
        validate_file_structure,
        validate_cpp_files,
        validate_cmake,
        validate_documentation
    ]

    for validation in validations:
        if not validation():
            all_passed = False

    print("\n" + "=" * 40)
    if all_passed:
        print("🎉 All validations passed! Examples are ready to use.")
        return 0
    else:
        print("❌ Some validations failed. Please review the issues above.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
