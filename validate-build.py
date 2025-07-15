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
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Any
import time
import platform

# Import rich components
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
# from rich.text import Text # Unused
from rich import box
from rich.padding import Padding
from rich.status import Status
# from rich.live import Live # Unused

# Try importing tomllib for pyproject.toml parsing
try:
    import tomllib
except ImportError:
    tomllib = None
    try:
        import tomli as tomllib  # type: ignore
    except ImportError:
        pass  # No TOML parser available


class BuildValidator:
    """Validates the build system and configurations"""

    def __init__(self, project_root: Path, console: Console):
        self.project_root = project_root
        self.test_results: List[Dict[str, Any]] = []
        self.console = console  # Add console instance

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
        self.console.print(
            Panel("Testing CMake configurations...", expand=False))

        # Test basic configuration
        with Status("Running basic CMake configuration...", console=self.console) as status:
            success, _, stderr = self.run_command([  # stdout unused
                'cmake', '-B', 'build-test',
                '-DCMAKE_BUILD_TYPE=Debug',
                '-DATOM_BUILD_TESTS=OFF',
                '-DATOM_BUILD_EXAMPLES=OFF',
                '.'
            ])

            if not success:
                status.update(
                    "Basic CMake configuration [bold red]FAILED[/bold red] ❌")
                self.console.print(stderr, style="red")
                return False

            status.update(
                "Basic CMake configuration [bold green]PASSED[/bold green] ✅")

        # Test with CMake presets if available
        if (self.project_root / "CMakePresets.json").exists():
            presets_to_test = ['debug', 'release', 'minimal']
            for preset in presets_to_test:
                with Status(f"Running CMake preset '{preset}' configuration...", console=self.console) as status:
                    success, _, stderr = self.run_command([  # stdout unused
                        'cmake', '--preset', preset
                    ])

                    if success:
                        status.update(
                            f"CMake preset '{preset}' configuration [bold green]PASSED[/bold green] ✅")
                    else:
                        status.update(
                            f"CMake preset '{preset}' configuration [bold yellow]SKIPPED/FAILED[/bold yellow] ⚠️")
                        # Use yellow for warnings/skips
                        self.console.print(stderr, style="yellow")

        # Cleanup
        shutil.rmtree(self.project_root / "build-test", ignore_errors=True)
        return True

    def test_xmake_configuration(self) -> bool:
        """Test XMake configuration if available"""
        if not shutil.which('xmake'):
            self.console.print(
                "XMake not available, skipping tests ⚠️", style="yellow")
            return True

        self.console.print(
            Panel("Testing XMake configurations...", expand=False))

        # Test basic configuration
        with Status("Running basic XMake configuration...", console=self.console) as status:
            success, _, stderr = self.run_command(
                ['xmake', 'f', '-c'])  # stdout unused

            if not success:
                status.update(
                    "XMake configuration [bold red]FAILED[/bold red] ❌")
                self.console.print(stderr, style="red")
                return False

            status.update(
                "XMake configuration [bold green]PASSED[/bold green] ✅")
        return True

    def test_build_scripts(self) -> bool:
        """Test build scripts"""
        self.console.print(Panel("Testing build scripts...", expand=False))

        scripts_to_test = [
            ('build.sh', ['--help']),
            ('build.py', ['--help']),
            ('build.py', ['--list-presets'])
        ]

        all_passed = True
        for script, args in scripts_to_test:
            script_path = self.project_root / script
            if not script_path.exists():
                self.console.print(
                    f"Script {script} not found ⚠️", style="yellow")
                all_passed = False  # Consider missing scripts a failure for validation
                continue

            if script.endswith('.py'):
                cmd = [sys.executable, str(script_path)] + args
            else:
                cmd = ['bash', str(script_path)] + args

            with Status(f"Running script '{script}' with args {args}...", console=self.console) as status:
                success, _, stderr = self.run_command(
                    cmd, timeout=30)  # stdout unused

                if success:
                    status.update(
                        f"Script '{script}' with args {args} [bold green]PASSED[/bold green] ✅")
                else:
                    status.update(
                        f"Script '{script}' with args {args} [bold red]FAILED[/bold red] ❌")
                    self.console.print(stderr, style="red")
                    all_passed = False

        return all_passed

    def test_dependencies(self) -> bool:
        """Test dependency availability"""
        self.console.print(Panel("Testing dependencies...", expand=False))

        required_tools = ['cmake', 'git']
        optional_tools = ['ninja', 'xmake', 'ccache', 'doxygen']

        all_required_found = True
        for tool in required_tools:
            if shutil.which(tool):
                self.console.print(
                    f"Required tool '{tool}' found ✅", style="green")
            else:
                self.console.print(
                    f"Required tool '{tool}' not found ❌", style="red")
                all_required_found = False

        for tool in optional_tools:
            if shutil.which(tool):
                self.console.print(
                    f"Optional tool '{tool}' found ✅", style="green")
            else:
                self.console.print(
                    f"Optional tool '{tool}' not found ⚠️", style="yellow")

        return all_required_found

    def test_vcpkg_integration(self) -> bool:
        """Test vcpkg integration if available"""
        vcpkg_json = self.project_root / "vcpkg.json"
        if not vcpkg_json.exists():
            self.console.print(
                "vcpkg.json not found, skipping vcpkg tests ⚠️", style="yellow")
            return True  # Not a failure if vcpkg isn't used

        self.console.print(Panel("Testing vcpkg integration...", expand=False))

        try:
            with open(vcpkg_json) as f:
                vcpkg_config = json.load(f)

            # Check required fields
            required_fields = ['name', 'version', 'dependencies']
            missing_fields = [
                field for field in required_fields if field not in vcpkg_config]
            if missing_fields:
                self.console.print(
                    f"vcpkg.json missing required fields: {', '.join(missing_fields)} ❌", style="red")
                return False

            self.console.print("vcpkg.json format is valid ✅", style="green")

            # Test vcpkg installation if VCPKG_ROOT is set
            vcpkg_root = os.environ.get('VCPKG_ROOT')
            if vcpkg_root and Path(vcpkg_root).exists():
                vcpkg_exe = Path(vcpkg_root) / \
                    ('vcpkg.exe' if os.name == 'nt' else 'vcpkg')
                if vcpkg_exe.exists():
                    with Status("Running 'vcpkg list'...", console=self.console) as status:
                        success, _, stderr = self.run_command([  # stdout unused
                            str(vcpkg_exe), 'list'
                        ], timeout=60)

                        if success:
                            # Removed style parameter
                            status.update(
                                "[green]vcpkg is functional ✅[/green]")
                        else:
                            # Removed style parameter
                            status.update(
                                "[yellow]vcpkg list failed ⚠️[/yellow]")
                            self.console.print(stderr, style="yellow")
                else:
                    self.console.print(
                        "vcpkg executable not found ⚠️", style="yellow")
            else:
                self.console.print(
                    "VCPKG_ROOT not set or invalid ⚠️", style="yellow")

        except json.JSONDecodeError as e:
            self.console.print(
                f"vcpkg.json is invalid JSON: {e} ❌", style="red")
            return False
        except Exception as e:
            self.console.print(f"vcpkg test failed: {e} ❌", style="red")
            return False

        return True

    def test_python_setup(self) -> bool:
        """Test Python package setup"""
        pyproject_toml = self.project_root / "pyproject.toml"
        if not pyproject_toml.exists():
            self.console.print(
                "pyproject.toml not found, skipping Python tests ⚠️", style="yellow")
            return True  # Not a failure if Python isn't used

        self.console.print(
            Panel("Testing Python package setup...", expand=False))

        # Test pyproject.toml syntax
        if tomllib:
            try:
                with open(pyproject_toml, 'rb') as f:
                    _ = tomllib.load(f)  # config variable unused
                self.console.print(
                    "pyproject.toml syntax is valid ✅", style="green")
            except Exception as e:
                self.console.print(
                    f"pyproject.toml syntax error: {e} ❌", style="red")
                return False
        else:
            self.console.print(
                "No TOML parser available, skipping pyproject.toml validation ⚠️", style="yellow")

        # Test pip install in dry-run mode
        with Status("Running 'pip install --dry-run .'...", console=self.console) as status:
            success, _, stderr = self.run_command([  # stdout unused
                sys.executable, '-m', 'pip', 'install', '--dry-run', '.'
            ], timeout=60)

            if success:
                # Removed style parameter
                status.update(
                    "[green]Python package can be installed ✅[/green]")
            else:
                # Removed style parameter
                status.update(
                    "[yellow]Python package install check failed ⚠️[/yellow]")
                self.console.print(stderr, style="yellow")
                # Consider this a warning, not a hard failure for validation script

        return True  # Return True as syntax check passed and install check is warning

    def run_smoke_test(self) -> bool:
        """Run a quick smoke test build"""
        self.console.print(Panel("Running smoke test build...", expand=False))

        build_dir = self.project_root / "build-smoke-test"

        try:
            # Configure with minimal options
            with Status("Running smoke test CMake configuration...", console=self.console) as status:
                success, _, stderr = self.run_command([  # stdout unused
                    'cmake', '-B', str(build_dir),
                    '-DCMAKE_BUILD_TYPE=Debug',
                    '-DATOM_BUILD_TESTS=OFF',
                    '-DATOM_BUILD_EXAMPLES=OFF',
                    '-DATOM_BUILD_PYTHON_BINDINGS=OFF',
                    '.'
                ], timeout=120)

                if not success:
                    status.update(
                        "Smoke test configuration [bold red]FAILED[/bold red] ❌")
                    self.console.print(stderr, style="red")
                    return False
                status.update(
                    "Smoke test configuration [bold green]PASSED[/bold green] ✅")

            # Try to build just one target quickly
            with Status("Running smoke test build...", console=self.console) as status:
                success, _, stderr = self.run_command([  # stdout unused
                    'cmake', '--build', str(build_dir), '--parallel', '2'
                ], timeout=300)

                if success:
                    status.update(
                        "Smoke test build [bold green]PASSED[/bold green] ✅")
                    return True
                else:
                    status.update(
                        "Smoke test build [bold red]FAILED[/bold red] ❌")
                    self.console.print(stderr, style="red")
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
                'architecture': platform.machine(),
            },
            'tests': self.test_results
        }

        report_file = self.project_root / "build-validation-report.json"
        try:
            with open(report_file, 'w') as f:
                json.dump(report, f, indent=2)
            self.console.print(
                f"\n📋 Validation report saved to: [link=file://{report_file}]{report_file}[/link]", style="blue")
        except Exception as e:
            self.console.print(
                f"\n❌ Failed to save validation report: {e}", style="red")

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

        self.console.print(
            Panel("🔍 Running build system validation...", expand=False, style="bold blue"))

        passed_count = 0
        total_count = len(tests)

        for test_name, test_func in tests:
            # Individual test functions now handle their own rich output
            # We just need to capture the result and store it
            try:
                result = test_func()
                self.test_results.append({
                    'name': test_name,
                    'passed': result,
                    'error': None
                })
                if result:
                    passed_count += 1
            except Exception as e:
                self.console.print(
                    f"❌ {test_name} failed with unexpected exception: {e}", style="red")
                self.test_results.append({
                    'name': test_name,
                    'passed': False,
                    'error': str(e)
                })

        # Final Summary Table
        summary_table = Table(title="Validation Summary", box=box.ROUNDED)
        summary_table.add_column("Test", style="cyan", justify="left")
        summary_table.add_column("Status", style="magenta", justify="center")

        all_passed = True
        for result in self.test_results:
            status_icon = "[bold green]PASSED ✅[/bold green]" if result['passed'] else "[bold red]FAILED ❌[/bold red]"
            summary_table.add_row(result['name'], status_icon)
            if not result['passed']:
                all_passed = False

        self.console.print(Padding(summary_table, (1, 0)))

        if all_passed:
            self.console.print(
                "🎉 All validation tests passed!", style="bold green")
        elif passed_count >= total_count * 0.8:
            self.console.print(
                "⚠️  Most tests passed, minor issues detected.", style="bold yellow")
        else:
            self.console.print(
                "❌ Significant issues detected in build system.", style="bold red")

        self.generate_report()
        return all_passed


def main():
    """Main entry point"""
    console = Console()  # Create rich console instance
    project_root = Path(__file__).parent
    validator = BuildValidator(project_root, console)  # Pass console

    try:
        success = validator.run_all_tests()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        console.print("\nValidation interrupted by user ⚠️", style="yellow")
        sys.exit(130)
    except Exception as e:
        console.print(
            f"\nUnexpected error during validation: {e} ❌", style="red")
        sys.exit(1)


if __name__ == "__main__":
    main()
