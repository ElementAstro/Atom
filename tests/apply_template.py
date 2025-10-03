#!/usr/bin/env python3
"""
Apply standardized CMakeLists.txt templates to test modules.

This script helps standardize test module CMakeLists.txt files by applying
the appropriate template based on the module's test structure.
"""

import os
import sys
import argparse
import shutil
from pathlib import Path

def detect_test_type(module_path):
    """Detect whether a module uses .cpp files or header-only tests."""
    cpp_files = list(module_path.glob("**/*.cpp"))
    hpp_files = list(module_path.glob("**/*.hpp"))
    
    # If there are .cpp files, it's a regular test module
    if cpp_files:
        return "cpp", len(cpp_files), len(hpp_files)
    # If only .hpp files, it's header-only
    elif hpp_files:
        return "header_only", 0, len(hpp_files)
    else:
        return "empty", 0, 0

def apply_template(module_name, template_type="auto", dry_run=False):
    """Apply the appropriate template to a test module."""
    
    # Paths
    tests_dir = Path(__file__).parent
    module_dir = tests_dir / module_name
    
    if not module_dir.exists():
        print(f"Error: Module directory {module_dir} does not exist")
        return False
    
    # Detect test type if auto
    if template_type == "auto":
        test_type, cpp_count, hpp_count = detect_test_type(module_dir)
        print(f"Detected test type for {module_name}: {test_type} ({cpp_count} .cpp, {hpp_count} .hpp)")
        
        if test_type == "empty":
            print(f"Warning: No test files found in {module_name}")
            return False
        elif test_type == "header_only":
            template_type = "header_only"
        else:
            template_type = "cpp"
    
    # Select template
    if template_type == "header_only":
        template_file = tests_dir / "CMakeLists_header_only_template.txt"
    else:
        template_file = tests_dir / "CMakeLists_template.txt"
    
    if not template_file.exists():
        print(f"Error: Template file {template_file} does not exist")
        return False
    
    # Read template
    with open(template_file, 'r') as f:
        template_content = f.read()
    
    # Replace placeholders
    module_upper = module_name.upper()
    module_title = module_name.title()
    
    content = template_content.replace("{module}", module_name)
    content = content.replace("{MODULE}", module_title)
    content = content.replace("{MODULE_UPPER}", module_upper)
    
    # Output file
    output_file = module_dir / "CMakeLists.txt"
    backup_file = module_dir / "CMakeLists.txt.backup"
    
    # Create backup if file exists
    if output_file.exists() and not dry_run:
        shutil.copy2(output_file, backup_file)
        print(f"Created backup: {backup_file}")
    
    if dry_run:
        print(f"Would apply {template_type} template to {module_name}")
        print(f"Output would be written to: {output_file}")
        return True
    
    # Write new CMakeLists.txt
    with open(output_file, 'w') as f:
        f.write(content)
    
    print(f"Applied {template_type} template to {module_name}")
    print(f"New CMakeLists.txt written to: {output_file}")
    
    return True

def list_modules():
    """List all available test modules."""
    tests_dir = Path(__file__).parent
    modules = []
    
    for item in tests_dir.iterdir():
        if item.is_dir() and item.name != "__pycache__" and not item.name.startswith('.'):
            # Skip special directories
            if item.name in ['tests', 'build']:
                continue
            
            cmake_file = item / "CMakeLists.txt"
            test_type, cpp_count, hpp_count = detect_test_type(item)
            
            modules.append({
                'name': item.name,
                'has_cmake': cmake_file.exists(),
                'test_type': test_type,
                'cpp_count': cpp_count,
                'hpp_count': hpp_count
            })
    
    return modules

def main():
    parser = argparse.ArgumentParser(description="Apply standardized CMakeLists.txt templates to test modules")
    parser.add_argument("module", nargs="?", help="Module name to process (or 'all' for all modules)")
    parser.add_argument("--type", choices=["auto", "cpp", "header_only"], default="auto",
                       help="Template type to apply (default: auto-detect)")
    parser.add_argument("--dry-run", action="store_true", help="Show what would be done without making changes")
    parser.add_argument("--list", action="store_true", help="List all available modules")
    
    args = parser.parse_args()
    
    if args.list:
        modules = list_modules()
        print("Available test modules:")
        print("-" * 80)
        print(f"{'Module':<15} {'CMake':<8} {'Type':<12} {'Files':<15} {'Status'}")
        print("-" * 80)
        
        for module in modules:
            cmake_status = "✓" if module['has_cmake'] else "✗"
            files_info = f"{module['cpp_count']} cpp, {module['hpp_count']} hpp"
            status = "Ready" if module['test_type'] != "empty" else "Empty"
            
            print(f"{module['name']:<15} {cmake_status:<8} {module['test_type']:<12} {files_info:<15} {status}")
        
        return 0
    
    if not args.module:
        parser.print_help()
        return 1
    
    if args.module == "all":
        modules = list_modules()
        success_count = 0
        
        for module in modules:
            if module['test_type'] != "empty":
                if apply_template(module['name'], args.type, args.dry_run):
                    success_count += 1
        
        print(f"\nProcessed {success_count} modules successfully")
        return 0
    else:
        success = apply_template(args.module, args.type, args.dry_run)
        return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
