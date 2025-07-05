#!/usr/bin/env python3
"""
Build system validation and testing script
Validates build configurations and runs smoke tests
Author: Max Qian
"""

import subprocess
import sys
import os
import json
import tempfile
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import time


class BuildValidator:
    """Validates the build system and configurations"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.test_results = []

    def run_command(self, cmd: List[str], cwd: Optional[Path] = None,
                    timeout: int = 300) -> Tuple[bool, str, str]:
        """Run a command and return success, stdout, stderr"""
        try:
            result = subprocess.run(
                cmd,
                cwd=cwd or self.project_root,
                capture_output=True,
                text=True,
                timeout=timeout
            )
            return result.returncode == 0, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return False, "", f"Command timed out after {timeout}s"
        except Exception as e:
            return False, "", str(e)

    def test_cmake_configuration(self) -> bool:
        """Test CMake configuration with different presets"""
        print("Testing CMake configurations...")

        # Test basic configuration
        success, stdout, stderr = self.run_command([
            'cmake', '-B', 'build-test',
            '-DCMAKE_BUILD_TYPE=Debug',
            '-DATOM_BUILD_TESTS=OFF',
            '-DATOM_BUILD_EXAMPLES=OFF',
            '.'
        ])

        if not success:
            print(f"❌ Basic CMake configuration failed: {stderr}")
            return False

        print("✅ Basic CMake configuration passed")

        # Test with CMake presets if available
        if (self.project_root / "CMakePresets.json").exists():
            presets_to_test = ['debug', 'release', 'minimal']
            for preset in presets_to_test:
                success, stdout, stderr = self.run_command([
                    'cmake', '--preset', preset
                ])

                if success:
                    print(f"✅ CMake preset '{preset}' configuration passed")
                else:
                    print(f"⚠️  CMake preset '{preset}' failed: {stderr}")

        # Cleanup
        shutil.rmtree(self.project_root / "build-test", ignore_errors=True)
        return True

    def test_xmake_configuration(self) -> bool:
        """Test XMake configuration if available"""
        if not shutil.which('xmake'):
            print("⚠️  XMake not available, skipping tests")
            return True

        print("Testing XMake configurations...")

        # Test basic configuration
        success, stdout, stderr = self.run_command(['xmake', 'f', '-c'])

        if not success:
            print(f"❌ XMake configuration failed: {stderr}")
            return False

        print("✅ XMake configuration passed")
        return True

    def test_build_scripts(self) -> bool:
        """Test build scripts"""
        print("Testing build scripts...")

        scripts_to_test = [
            ('build.sh', ['--help']),
            ('build.py', ['--help']),
            ('build.py', ['--list-presets'])
        ]

        for script, args in scripts_to_test:
            script_path = self.project_root / script
            if not script_path.exists():
                print(f"⚠️  Script {script} not found")
                continue

            if script.endswith('.py'):
                cmd = [sys.executable, str(script_path)] + args
            else:
                cmd = ['bash', str(script_path)] + args

            success, stdout, stderr = self.run_command(cmd, timeout=30)

            if success:
                print(f"✅ Script {script} with args {args} passed")
            else:
                print(f"❌ Script {script} with args {args} failed: {stderr}")
                return False

        return True

    def test_dependencies(self) -> bool:
        """Test dependency availability"""
        print("Testing dependencies...")

        required_tools = ['cmake', 'git']
        optional_tools = ['ninja', 'xmake', 'ccache', 'doxygen']

        for tool in required_tools:
            if shutil.which(tool):
                print(f"✅ Required tool '{tool}' found")
            else:
                print(f"❌ Required tool '{tool}' not found")
                return False

        for tool in optional_tools:
            if shutil.which(tool):
                print(f"✅ Optional tool '{tool}' found")
            else:
                print(f"⚠️  Optional tool '{tool}' not found")

        return True

    def test_vcpkg_integration(self) -> bool:
        """Test vcpkg integration if available"""
        vcpkg_json = self.project_root / "vcpkg.json"
        if not vcpkg_json.exists():
            print("⚠️  vcpkg.json not found, skipping vcpkg tests")
            return True

        print("Testing vcpkg integration...")

        try:
            with open(vcpkg_json) as f:
                vcpkg_config = json.load(f)

            # Check required fields
            required_fields = ['name', 'version', 'dependencies']
            for field in required_fields:
                if field not in vcpkg_config:
                    print(f"❌ vcpkg.json missing required field: {field}")
                    return False

            print("✅ vcpkg.json format is valid")

            # Test vcpkg installation if VCPKG_ROOT is set
            vcpkg_root = os.environ.get('VCPKG_ROOT')
            if vcpkg_root and Path(vcpkg_root).exists():
                vcpkg_exe = Path(vcpkg_root) / \
                    ('vcpkg.exe' if os.name == 'nt' else 'vcpkg')
                if vcpkg_exe.exists():
                    success, stdout, stderr = self.run_command([
                        str(vcpkg_exe), 'list'
                    ], timeout=60)

                    if success:
                        print("✅ vcpkg is functional")
                    else:
                        print(f"⚠️  vcpkg list failed: {stderr}")
                else:
                    print("⚠️  vcpkg executable not found")
            else:
                print("⚠️  VCPKG_ROOT not set or invalid")

        except json.JSONDecodeError as e:
            print(f"❌ vcpkg.json is invalid JSON: {e}")
            return False
        except Exception as e:
            print(f"❌ vcpkg test failed: {e}")
            return False

        return True

    def test_python_setup(self) -> bool:
        """Test Python package setup"""
        pyproject_toml = self.project_root / "pyproject.toml"
        if not pyproject_toml.exists():
            print("⚠️  pyproject.toml not found, skipping Python tests")
            return True

        print("Testing Python package setup...")

        # Test pyproject.toml syntax
        tomllib = None
        try:
            # Try Python 3.11+ built-in tomllib
            import tomllib
        except ImportError:
            try:
                # Fall back to tomli package
                import tomli as tomllib  # type: ignore
            except ImportError:
                print("⚠️  No TOML parser available, skipping pyproject.toml validation")
                return True

        try:
            with open(pyproject_toml, 'rb') as f:
                config = tomllib.load(f)
            print("✅ pyproject.toml syntax is valid")
        except Exception as e:
            print(f"❌ pyproject.toml syntax error: {e}")
            return False

        # Test pip install in dry-run mode
        success, stdout, stderr = self.run_command([
            sys.executable, '-m', 'pip', 'install', '--dry-run', '.'
        ], timeout=60)

        if success:
            print("✅ Python package can be installed")
        else:
            print(f"⚠️  Python package install check failed: {stderr}")

        return True

    def run_smoke_test(self) -> bool:
        """Run a quick smoke test build"""
        print("Running smoke test build...")

        build_dir = self.project_root / "build-smoke-test"

        try:
            # Configure with minimal options
            success, stdout, stderr = self.run_command([
                'cmake', '-B', str(build_dir),
                '-DCMAKE_BUILD_TYPE=Debug',
                '-DATOM_BUILD_TESTS=OFF',
                '-DATOM_BUILD_EXAMPLES=OFF',
                '-DATOM_BUILD_PYTHON_BINDINGS=OFF',
                '.'
            ], timeout=120)

            if not success:
                print(f"❌ Smoke test configuration failed: {stderr}")
                return False

            # Try to build just one target quickly
            success, stdout, stderr = self.run_command([
                'cmake', '--build', str(build_dir), '--parallel', '2'
            ], timeout=300)

            if success:
                print("✅ Smoke test build passed")
                return True
            else:
                print(f"⚠️  Smoke test build failed: {stderr}")
                return False

        finally:
            # Cleanup
            shutil.rmtree(build_dir, ignore_errors=True)

    def generate_report(self) -> None:
        """Generate a validation report"""
        report = {
            'timestamp': time.strftime('%Y-%m-%d %H:%M:%S'),
            'system': {
                'platform': sys.platform,
                'python_version': sys.version,
            },
            'tests': self.test_results
        }

        report_file = self.project_root / "build-validation-report.json"
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)

        print(f"\n📋 Validation report saved to: {report_file}")

    def run_all_tests(self) -> bool:
        """Run all validation tests"""
        tests = [
            ("Dependencies", self.test_dependencies),
            ("CMake Configuration", self.test_cmake_configuration),
            ("XMake Configuration", self.test_xmake_configuration),
            ("Build Scripts", self.test_build_scripts),
            ("vcpkg Integration", self.test_vcpkg_integration),
            ("Python Setup", self.test_python_setup),
            ("Smoke Test", self.run_smoke_test),
        ]

        print("🔍 Running build system validation...\n")

        passed = 0
        total = len(tests)

        for test_name, test_func in tests:
            print(f"\n--- {test_name} ---")
            try:
                result = test_func()
                self.test_results.append({
                    'name': test_name,
                    'passed': result,
                    'error': None
                })
                if result:
                    passed += 1
            except Exception as e:
                print(f"❌ {test_name} failed with exception: {e}")
                self.test_results.append({
                    'name': test_name,
                    'passed': False,
                    'error': str(e)
                })

        print(f"\n{'='*50}")
        print(f"VALIDATION SUMMARY: {passed}/{total} tests passed")
        print(f"{'='*50}")

        if passed == total:
            print("🎉 All validation tests passed!")
        elif passed >= total * 0.8:
            print("⚠️  Most tests passed, minor issues detected")
        else:
            print("❌ Significant issues detected in build system")

        self.generate_report()
        return passed == total


def main():
    """Main entry point"""
    project_root = Path(__file__).parent
    validator = BuildValidator(project_root)

    success = validator.run_all_tests()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
