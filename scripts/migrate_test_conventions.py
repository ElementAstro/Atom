#!/usr/bin/env python3

"""
migrate_test_conventions.py - Script to migrate tests to follow naming conventions
This project is licensed under the terms of the GPL3 license.

Author: Max Qian
License: GPL3
"""

import os
import re
import sys
import shutil
from pathlib import Path
from typing import List, Dict, Tuple

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'

def print_colored(message: str, color: str = Colors.NC) -> None:
    """Print a colored message."""
    print(f"{color}{message}{Colors.NC}")

class TestMigrator:
    """Migrate test files to follow naming conventions."""

    def __init__(self, project_root: Path, dry_run: bool = True):
        self.project_root = project_root
        self.dry_run = dry_run
        self.cpp_test_dir = project_root / "tests"
        self.python_test_dir = project_root / "python" / "tests"
        self.changes = []

    def suggest_cpp_file_renames(self) -> List[Tuple[Path, Path]]:
        """Suggest C++ file renames to follow conventions."""
        renames = []

        if not self.cpp_test_dir.exists():
            return renames

        for test_file in self.cpp_test_dir.rglob("*.cpp"):
            filename = test_file.name

            # Skip files that already follow convention
            if filename.startswith("test_") or filename == "main.cpp":
                continue

            # Suggest rename based on current name
            if filename.endswith(".cpp"):
                base_name = filename[:-4]  # Remove .cpp
                new_name = f"test_{base_name}.cpp"
                new_path = test_file.parent / new_name

                if not new_path.exists():
                    renames.append((test_file, new_path))

        return renames

    def suggest_python_file_renames(self) -> List[Tuple[Path, Path]]:
        """Suggest Python file renames to follow conventions."""
        renames = []

        if not self.python_test_dir.exists():
            return renames

        for test_file in self.python_test_dir.rglob("*.py"):
            filename = test_file.name

            # Skip special files and files that already follow convention
            if (filename in ["__init__.py", "conftest.py", "pytest.ini"] or
                filename.startswith("test_")):
                continue

            # Suggest rename based on current name
            if filename.endswith(".py"):
                base_name = filename[:-3]  # Remove .py
                new_name = f"test_{base_name}.py"
                new_path = test_file.parent / new_name

                if not new_path.exists():
                    renames.append((test_file, new_path))

        return renames

    def fix_cpp_class_names(self) -> List[str]:
        """Fix C++ test class names to follow conventions."""
        changes = []

        for test_file in self.cpp_test_dir.rglob("*.cpp"):
            try:
                content = test_file.read_text(encoding='utf-8')
                original_content = content

                # Find and fix class definitions
                class_pattern = r'class\s+(\w+)\s*:\s*public\s*::testing::Test'

                def fix_class_name(match):
                    class_name = match.group(1)
                    if not class_name.endswith("Test"):
                        new_name = f"{class_name}Test"
                        changes.append(f"Renamed class {class_name} to {new_name} in {test_file.relative_to(self.project_root)}")
                        return match.group(0).replace(class_name, new_name)
                    return match.group(0)

                content = re.sub(class_pattern, fix_class_name, content)

                # Also fix TEST_F references
                test_f_pattern = r'TEST_F\((\w+),'

                def fix_test_f(match):
                    class_name = match.group(1)
                    if not class_name.endswith("Test"):
                        new_name = f"{class_name}Test"
                        return match.group(0).replace(class_name, new_name)
                    return match.group(0)

                content = re.sub(test_f_pattern, fix_test_f, content)

                if content != original_content and not self.dry_run:
                    test_file.write_text(content, encoding='utf-8')

            except Exception as e:
                changes.append(f"Error processing {test_file}: {e}")

        return changes

    def fix_python_class_names(self) -> List[str]:
        """Fix Python test class names to follow conventions."""
        changes = []

        for test_file in self.python_test_dir.rglob("test_*.py"):
            try:
                content = test_file.read_text(encoding='utf-8')
                original_content = content

                # Find and fix class definitions
                class_pattern = r'class\s+(\w+)(\([^)]*\))?:'

                def fix_class_name(match):
                    class_name = match.group(1)
                    inheritance = match.group(2) or ""

                    if not class_name.startswith("Test") and class_name != "Colors":
                        new_name = f"Test{class_name}"
                        changes.append(f"Renamed class {class_name} to {new_name} in {test_file.relative_to(self.project_root)}")
                        return f"class {new_name}{inheritance}:"
                    return match.group(0)

                content = re.sub(class_pattern, fix_class_name, content)

                if content != original_content and not self.dry_run:
                    test_file.write_text(content, encoding='utf-8')

            except Exception as e:
                changes.append(f"Error processing {test_file}: {e}")

        return changes

    def perform_file_renames(self, renames: List[Tuple[Path, Path]], file_type: str) -> List[str]:
        """Perform file renames."""
        changes = []

        for old_path, new_path in renames:
            try:
                if not self.dry_run:
                    shutil.move(str(old_path), str(new_path))
                changes.append(f"Renamed {file_type} file: {old_path.relative_to(self.project_root)} -> {new_path.relative_to(self.project_root)}")
            except Exception as e:
                changes.append(f"Error renaming {old_path}: {e}")

        return changes

    def run_migration(self) -> Dict[str, List[str]]:
        """Run the complete migration process."""
        results = {}

        # File renames
        cpp_renames = self.suggest_cpp_file_renames()
        python_renames = self.suggest_python_file_renames()

        results["cpp_file_renames"] = self.perform_file_renames(cpp_renames, "C++")
        results["python_file_renames"] = self.perform_file_renames(python_renames, "Python")

        # Class name fixes
        results["cpp_class_fixes"] = self.fix_cpp_class_names()
        results["python_class_fixes"] = self.fix_python_class_names()

        return results

def main():
    """Main function."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Migrate test files to follow naming conventions",
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

    print_colored("Test Convention Migration Tool", Colors.BLUE)
    print_colored("=" * 30, Colors.BLUE)

    if args.dry_run:
        print_colored("🔍 DRY RUN MODE - No changes will be made", Colors.YELLOW)
    else:
        print_colored("⚠️  APPLYING CHANGES - Files will be modified", Colors.RED)

    migrator = TestMigrator(project_root, dry_run=args.dry_run)
    results = migrator.run_migration()

    total_changes = 0

    for category, changes in results.items():
        if changes:
            print_colored(f"\n{category.replace('_', ' ').title()}:", Colors.YELLOW)
            for change in changes:
                print_colored(f"  📝 {change}", Colors.GREEN if not change.startswith("Error") else Colors.RED)
                total_changes += 1
        else:
            print_colored(f"\n{category.replace('_', ' ').title()}: ✅ No changes needed", Colors.GREEN)

    print_colored(f"\nSummary:", Colors.BLUE)
    if total_changes == 0:
        print_colored("🎉 All files already follow conventions!", Colors.GREEN)
    else:
        if args.dry_run:
            print_colored(f"📋 Found {total_changes} potential changes. Use --apply to make them.", Colors.YELLOW)
        else:
            print_colored(f"✅ Applied {total_changes} changes successfully!", Colors.GREEN)

    return 0

if __name__ == "__main__":
    sys.exit(main())
