#!/usr/bin/env python3

"""
reorganize_tests.py - Script to reorganize test directory structure
This project is licensed under the terms of the GPL3 license.

Author: Max Qian
License: GPL3
"""

import os
import shutil
import sys
from pathlib import Path
from typing import Dict, List, Set

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'

def print_colored(message: str, color: str = Colors.NC) -> None:
    """Print a colored message."""
    print(f"{color}{message}{Colors.NC}")

class TestReorganizer:
    """Reorganize test directory structure for better organization."""
    
    def __init__(self, project_root: Path, dry_run: bool = True):
        self.project_root = project_root
        self.dry_run = dry_run
        self.cpp_test_dir = project_root / "tests"
        self.python_test_dir = project_root / "python" / "tests"
        
        # Define the ideal structure
        self.cpp_modules = {
            "algorithm", "async", "components", "connection", "error",
            "image", "io", "log", "memory", "meta", "search", "secret",
            "serial", "sysinfo", "system", "type", "utils", "web"
        }
        
        self.python_modules = {
            "algorithm", "connection", "error", "io", "search", 
            "sysinfo", "type", "web"
        }
    
    def analyze_current_structure(self) -> Dict[str, any]:
        """Analyze the current test directory structure."""
        analysis = {
            "cpp_modules_found": set(),
            "cpp_orphaned_files": [],
            "python_modules_found": set(),
            "python_orphaned_files": [],
            "missing_cpp_modules": set(),
            "missing_python_modules": set(),
        }
        
        # Analyze C++ tests
        if self.cpp_test_dir.exists():
            for item in self.cpp_test_dir.iterdir():
                if item.is_dir() and item.name in self.cpp_modules:
                    analysis["cpp_modules_found"].add(item.name)
                elif item.is_file() and item.suffix in [".cpp", ".hpp"]:
                    analysis["cpp_orphaned_files"].append(item)
        
        analysis["missing_cpp_modules"] = self.cpp_modules - analysis["cpp_modules_found"]
        
        # Analyze Python tests
        if self.python_test_dir.exists():
            for item in self.python_test_dir.iterdir():
                if item.is_file() and item.name.startswith("test_") and item.suffix == ".py":
                    module_name = item.name[5:-3]  # Remove "test_" and ".py"
                    if module_name in self.python_modules:
                        analysis["python_modules_found"].add(module_name)
                elif item.is_file() and item.suffix == ".py" and item.name not in ["__init__.py", "conftest.py"]:
                    analysis["python_orphaned_files"].append(item)
        
        analysis["missing_python_modules"] = self.python_modules - analysis["python_modules_found"]
        
        return analysis
    
    def create_missing_directories(self) -> List[str]:
        """Create missing test directories."""
        changes = []
        
        # Create missing C++ module directories
        analysis = self.analyze_current_structure()
        
        for module in analysis["missing_cpp_modules"]:
            module_dir = self.cpp_test_dir / module
            if not self.dry_run:
                module_dir.mkdir(parents=True, exist_ok=True)
                
                # Create a basic CMakeLists.txt for the module
                cmake_content = f"""cmake_minimum_required(VERSION 3.20)

project(atom_{module}.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${{PROJECT_SOURCE_DIR}}/*.cpp)

add_executable(${{PROJECT_NAME}} ${{TEST_SOURCES}})

target_link_libraries(${{PROJECT_NAME}} gtest gtest_main gmock gmock_main atom-{module} atom-error loguru)

# Register tests with CTest
add_test(NAME ${{PROJECT_NAME}} COMMAND ${{PROJECT_NAME}})
"""
                cmake_file = module_dir / "CMakeLists.txt"
                cmake_file.write_text(cmake_content)
                
                changes.append(f"Created C++ test directory: {module_dir.relative_to(self.project_root)}")
                changes.append(f"Created CMakeLists.txt for {module}")
            else:
                changes.append(f"Would create C++ test directory: tests/{module}")
        
        return changes
    
    def organize_orphaned_files(self) -> List[str]:
        """Organize orphaned test files into appropriate directories."""
        changes = []
        analysis = self.analyze_current_structure()
        
        # Handle C++ orphaned files
        for orphaned_file in analysis["cpp_orphaned_files"]:
            # Try to determine which module this file belongs to
            filename = orphaned_file.name
            
            # Simple heuristic: look for module names in filename
            target_module = None
            for module in self.cpp_modules:
                if module in filename.lower():
                    target_module = module
                    break
            
            if target_module:
                target_dir = self.cpp_test_dir / target_module
                target_path = target_dir / filename
                
                if not self.dry_run:
                    target_dir.mkdir(parents=True, exist_ok=True)
                    shutil.move(str(orphaned_file), str(target_path))
                
                changes.append(f"Moved {orphaned_file.relative_to(self.project_root)} to {target_path.relative_to(self.project_root)}")
            else:
                changes.append(f"Could not determine module for orphaned file: {orphaned_file.relative_to(self.project_root)}")
        
        return changes
    
    def create_test_templates(self) -> List[str]:
        """Create test file templates for modules that don't have tests."""
        changes = []
        analysis = self.analyze_current_structure()
        
        # Create C++ test templates
        for module in analysis["missing_cpp_modules"]:
            module_dir = self.cpp_test_dir / module
            test_file = module_dir / f"test_{module}.cpp"
            
            if not test_file.exists():
                cpp_template = f"""#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "atom/{module}/{module}.hpp"

using namespace atom::{module};

class {module.title()}Test : public ::testing::Test {{
protected:
    void SetUp() override {{
        // Setup code for {module} tests
    }}

    void TearDown() override {{
        // Cleanup code for {module} tests
    }}
}};

TEST_F({module.title()}Test, BasicFunctionality) {{
    // TODO: Implement basic functionality test for {module}
    SUCCEED() << "Test template created - implement actual tests";
}}

TEST_F({module.title()}Test, ErrorHandling) {{
    // TODO: Implement error handling tests for {module}
    SUCCEED() << "Test template created - implement actual tests";
}}
"""
                
                if not self.dry_run:
                    module_dir.mkdir(parents=True, exist_ok=True)
                    test_file.write_text(cpp_template)
                
                changes.append(f"Created C++ test template: {test_file.relative_to(self.project_root)}")
        
        # Create Python test templates
        for module in analysis["missing_python_modules"]:
            test_file = self.python_test_dir / f"test_{module}.py"
            
            if not test_file.exists():
                python_template = f"""# test_{module}.py - Tests for atom_{module} module
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

import pytest

# Try to import the {module} module
try:
    import atom_{module}
    {module.upper()}_AVAILABLE = True
except ImportError:
    {module.upper()}_AVAILABLE = False

pytestmark = pytest.mark.skipif(
    not {module.upper()}_AVAILABLE, 
    reason="atom_{module} module not available"
)

class Test{module.title()}Module:
    \"\"\"Test cases for the {module} module.\"\"\"
    
    def test_module_import(self):
        \"\"\"Test that the {module} module can be imported.\"\"\"
        assert atom_{module} is not None
    
    def test_module_attributes(self):
        \"\"\"Test that the module has expected attributes.\"\"\"
        assert hasattr(atom_{module}, '__doc__')

class Test{module.title()}Functionality:
    \"\"\"Test cases for {module} functionality.\"\"\"
    
    def test_basic_functionality(self):
        \"\"\"Test basic {module} functionality.\"\"\"
        # TODO: Implement basic functionality tests
        pass
    
    @pytest.mark.integration
    def test_integration(self):
        \"\"\"Test {module} integration.\"\"\"
        # TODO: Implement integration tests
        pass

@pytest.mark.slow
class Test{module.title()}Performance:
    \"\"\"Performance tests for {module} module.\"\"\"
    
    def test_performance(self):
        \"\"\"Test {module} performance.\"\"\"
        # TODO: Implement performance tests
        pass
"""
                
                if not self.dry_run:
                    self.python_test_dir.mkdir(parents=True, exist_ok=True)
                    test_file.write_text(python_template)
                
                changes.append(f"Created Python test template: {test_file.relative_to(self.project_root)}")
        
        return changes
    
    def create_documentation(self) -> List[str]:
        """Create documentation for the test structure."""
        changes = []
        
        readme_content = """# Test Directory Structure

This directory contains the test suite for the Atom project, organized by module for better maintainability and navigation.

## Directory Organization

### C++ Tests (`tests/`)
Each module has its own subdirectory containing:
- `CMakeLists.txt` - Build configuration for the module tests
- `test_<module>.cpp` - Main test file for the module
- Additional test files as needed

### Python Tests (`python/tests/`)
- `test_<module>.py` - Test files for Python bindings
- `conftest.py` - Pytest configuration and fixtures
- `__init__.py` - Package initialization

## Running Tests

### C++ Tests
```bash
# Build and run all tests
make test

# Run tests with coverage
make test-coverage

# Run specific module tests
cd build && ctest -R "algorithm"
```

### Python Tests
```bash
# Run all Python tests
python -m pytest python/tests/

# Run with coverage
python scripts/python_coverage.py

# Run specific module tests
python -m pytest python/tests/test_algorithm.py
```

## Adding New Tests

### For C++ Modules
1. Create test files in the appropriate module directory
2. Follow the naming convention: `test_<feature>.cpp`
3. Use the existing CMakeLists.txt or create a new one if needed

### For Python Modules
1. Create test files following the pattern: `test_<module>.py`
2. Use appropriate pytest markers for categorization
3. Follow the established class and method naming conventions

## Test Categories

- **Unit Tests**: Test individual functions/classes in isolation
- **Integration Tests**: Test module interactions
- **Performance Tests**: Benchmark critical functionality
- **Functional Tests**: Test end-to-end scenarios

Refer to `docs/TESTING_CONVENTIONS.md` for detailed guidelines.
"""
        
        readme_file = self.cpp_test_dir / "README.md"
        if not self.dry_run:
            self.cpp_test_dir.mkdir(parents=True, exist_ok=True)
            readme_file.write_text(readme_content)
        
        changes.append(f"Created test documentation: {readme_file.relative_to(self.project_root)}")
        
        return changes
    
    def run_reorganization(self) -> Dict[str, List[str]]:
        """Run the complete reorganization process."""
        results = {}
        
        print_colored("Analyzing current structure...", Colors.BLUE)
        analysis = self.analyze_current_structure()
        
        print_colored(f"Found {len(analysis['cpp_modules_found'])} C++ modules", Colors.GREEN)
        print_colored(f"Found {len(analysis['python_modules_found'])} Python modules", Colors.GREEN)
        print_colored(f"Missing {len(analysis['missing_cpp_modules'])} C++ modules", Colors.YELLOW)
        print_colored(f"Missing {len(analysis['missing_python_modules'])} Python modules", Colors.YELLOW)
        
        results["directory_creation"] = self.create_missing_directories()
        results["file_organization"] = self.organize_orphaned_files()
        results["template_creation"] = self.create_test_templates()
        results["documentation"] = self.create_documentation()
        
        return results

def main():
    """Main function."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Reorganize test directory structure",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --dry-run          # Show what would be changed
  %(prog)s --apply            # Apply the changes
        """
    )
    
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=True,
        help="Show what would be changed without making changes (default)"
    )
    
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Apply the changes to files"
    )
    
    args = parser.parse_args()
    
    # If --apply is specified, turn off dry-run
    if args.apply:
        args.dry_run = False
    
    project_root = Path.cwd()
    
    print_colored("Test Directory Reorganization Tool", Colors.BLUE)
    print_colored("=" * 35, Colors.BLUE)
    
    if args.dry_run:
        print_colored("🔍 DRY RUN MODE - No changes will be made", Colors.YELLOW)
    else:
        print_colored("⚠️  APPLYING CHANGES - Files will be modified", Colors.RED)
    
    reorganizer = TestReorganizer(project_root, dry_run=args.dry_run)
    results = reorganizer.run_reorganization()
    
    total_changes = 0
    
    for category, changes in results.items():
        if changes:
            print_colored(f"\n{category.replace('_', ' ').title()}:", Colors.YELLOW)
            for change in changes:
                print_colored(f"  📝 {change}", Colors.GREEN)
                total_changes += 1
        else:
            print_colored(f"\n{category.replace('_', ' ').title()}: ✅ No changes needed", Colors.GREEN)
    
    print_colored(f"\nSummary:", Colors.BLUE)
    if total_changes == 0:
        print_colored("🎉 Test structure is already well organized!", Colors.GREEN)
    else:
        if args.dry_run:
            print_colored(f"📋 Found {total_changes} potential improvements. Use --apply to make them.", Colors.YELLOW)
        else:
            print_colored(f"✅ Applied {total_changes} improvements successfully!", Colors.GREEN)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
