#!/usr/bin/env python3
"""
Build System Validation Script for Atom Project
This script validates the CMake build system improvements and ensures
all modules are properly configured and can be built selectively.

Author: Max Qian
License: GPL3
"""

import os
import sys
import subprocess
import argparse
import json
from pathlib import Path
from typing import List, Dict, Set, Tuple

class BuildSystemValidator:
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.atom_dir = project_root / "atom"
        self.tests_dir = project_root / "tests"
        self.cmake_dir = project_root / "cmake"
        self.build_dir = project_root / "build"
        
        # List of all expected modules
        self.modules = [
            "algorithm", "async", "components", "connection", "containers",
            "error", "image", "io", "log", "memory", "meta", "search",
            "secret", "serial", "sysinfo", "system", "type", "utils", "web"
        ]
        
        self.validation_results = {
            "module_coverage": {},
            "test_coverage": {},
            "dependency_validation": {},
            "build_tests": {},
            "errors": [],
            "warnings": []
        }

    def validate_module_coverage(self) -> bool:
        """Validate that all modules have proper CMakeLists.txt files."""
        print("🔍 Validating module coverage...")
        
        all_valid = True
        for module in self.modules:
            module_dir = self.atom_dir / module
            cmake_file = module_dir / "CMakeLists.txt"
            
            if not module_dir.exists():
                self.validation_results["errors"].append(f"Module directory missing: {module}")
                all_valid = False
                continue
                
            if not cmake_file.exists():
                self.validation_results["errors"].append(f"CMakeLists.txt missing for module: {module}")
                all_valid = False
                continue
                
            # Check if CMakeLists.txt uses standardized patterns
            with open(cmake_file, 'r', encoding='utf-8') as f:
                content = f.read()
                
            uses_standard_config = "atom_configure_module" in content
            has_project_declaration = f"project(atom-{module}" in content
            
            self.validation_results["module_coverage"][module] = {
                "cmake_exists": True,
                "uses_standard_config": uses_standard_config,
                "has_project_declaration": has_project_declaration
            }
            
            if not uses_standard_config:
                self.validation_results["warnings"].append(
                    f"Module {module} doesn't use standardized atom_configure_module()"
                )
        
        print(f"✅ Module coverage validation: {'PASSED' if all_valid else 'FAILED'}")
        return all_valid

    def validate_test_coverage(self) -> bool:
        """Validate that test directories have proper CMakeLists.txt files."""
        print("🔍 Validating test coverage...")
        
        all_valid = True
        for module in self.modules:
            test_dir = self.tests_dir / module
            cmake_file = test_dir / "CMakeLists.txt"
            
            if test_dir.exists():
                if cmake_file.exists():
                    self.validation_results["test_coverage"][module] = {
                        "test_dir_exists": True,
                        "cmake_exists": True
                    }
                else:
                    self.validation_results["test_coverage"][module] = {
                        "test_dir_exists": True,
                        "cmake_exists": False
                    }
                    self.validation_results["warnings"].append(
                        f"Test directory exists but CMakeLists.txt missing for: {module}"
                    )
            else:
                self.validation_results["test_coverage"][module] = {
                    "test_dir_exists": False,
                    "cmake_exists": False
                }
        
        print(f"✅ Test coverage validation: COMPLETED")
        return True

    def validate_dependency_configuration(self) -> bool:
        """Validate module dependency configuration."""
        print("🔍 Validating dependency configuration...")
        
        module_deps_file = self.cmake_dir / "module_dependencies.cmake"
        if not module_deps_file.exists():
            self.validation_results["errors"].append("module_dependencies.cmake not found")
            return False
            
        with open(module_deps_file, 'r', encoding='utf-8') as f:
            content = f.read()
            
        # Check for required functions
        required_functions = [
            "atom_configure_module",
            "atom_auto_resolve_dependencies",
            "atom_validate_module_dependencies"
        ]
        
        for func in required_functions:
            if func not in content:
                self.validation_results["errors"].append(f"Required function missing: {func}")
                return False
                
        print("✅ Dependency configuration validation: PASSED")
        return True

    def test_selective_build(self, module: str) -> bool:
        """Test selective build for a specific module."""
        print(f"🔨 Testing selective build for module: {module}")
        
        # Clean build directory
        if self.build_dir.exists():
            import shutil
            shutil.rmtree(self.build_dir, ignore_errors=True)
            
        self.build_dir.mkdir(exist_ok=True)
        
        try:
            # Configure with only this module enabled
            configure_cmd = [
                "cmake",
                f"-DATOM_BUILD_ALL=OFF",
                f"-DATOM_BUILD_{module.upper()}=ON",
                f"-DATOM_AUTO_RESOLVE_DEPS=ON",
                str(self.project_root)
            ]
            
            result = subprocess.run(
                configure_cmd,
                cwd=self.build_dir,
                capture_output=True,
                text=True,
                timeout=120
            )
            
            if result.returncode != 0:
                self.validation_results["build_tests"][module] = {
                    "configure_success": False,
                    "error": result.stderr
                }
                return False
                
            # Try to build
            build_cmd = ["cmake", "--build", ".", "--target", f"atom-{module}"]
            result = subprocess.run(
                build_cmd,
                cwd=self.build_dir,
                capture_output=True,
                text=True,
                timeout=300
            )
            
            success = result.returncode == 0
            self.validation_results["build_tests"][module] = {
                "configure_success": True,
                "build_success": success,
                "error": result.stderr if not success else None
            }
            
            return success
            
        except subprocess.TimeoutExpired:
            self.validation_results["build_tests"][module] = {
                "configure_success": False,
                "build_success": False,
                "error": "Build timeout"
            }
            return False
        except Exception as e:
            self.validation_results["build_tests"][module] = {
                "configure_success": False,
                "build_success": False,
                "error": str(e)
            }
            return False

    def run_validation(self, test_builds: bool = False, modules_to_test: List[str] = None) -> bool:
        """Run complete validation suite."""
        print("🚀 Starting Atom build system validation...")
        
        # Basic validations
        module_valid = self.validate_module_coverage()
        test_valid = self.validate_test_coverage()
        deps_valid = self.validate_dependency_configuration()
        
        # Optional build tests
        build_valid = True
        if test_builds:
            modules_to_test = modules_to_test or ["error", "containers", "memory"]
            print(f"🔨 Testing selective builds for modules: {modules_to_test}")
            
            for module in modules_to_test:
                if not self.test_selective_build(module):
                    build_valid = False
                    
        # Generate report
        self.generate_report()
        
        overall_success = module_valid and test_valid and deps_valid and build_valid
        print(f"\n{'✅ VALIDATION PASSED' if overall_success else '❌ VALIDATION FAILED'}")
        
        return overall_success

    def generate_report(self):
        """Generate validation report."""
        report_file = self.project_root / "build_validation_report.json"
        
        with open(report_file, 'w', encoding='utf-8') as f:
            json.dump(self.validation_results, f, indent=2)
            
        print(f"📊 Validation report saved to: {report_file}")
        
        # Print summary
        print("\n📋 VALIDATION SUMMARY:")
        print(f"   Modules validated: {len(self.validation_results['module_coverage'])}")
        print(f"   Tests validated: {len(self.validation_results['test_coverage'])}")
        print(f"   Build tests: {len(self.validation_results['build_tests'])}")
        print(f"   Errors: {len(self.validation_results['errors'])}")
        print(f"   Warnings: {len(self.validation_results['warnings'])}")
        
        if self.validation_results['errors']:
            print("\n❌ ERRORS:")
            for error in self.validation_results['errors']:
                print(f"   - {error}")
                
        if self.validation_results['warnings']:
            print("\n⚠️  WARNINGS:")
            for warning in self.validation_results['warnings']:
                print(f"   - {warning}")

def main():
    parser = argparse.ArgumentParser(description="Validate Atom build system")
    parser.add_argument("--test-builds", action="store_true", 
                       help="Test selective builds (slower)")
    parser.add_argument("--modules", nargs="+", 
                       help="Specific modules to test builds for")
    parser.add_argument("--project-root", type=Path, default=Path.cwd(),
                       help="Project root directory")
    
    args = parser.parse_args()
    
    validator = BuildSystemValidator(args.project_root)
    success = validator.run_validation(args.test_builds, args.modules)
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
