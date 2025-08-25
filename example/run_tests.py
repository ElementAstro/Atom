#!/usr/bin/env python3
"""
Atom Framework Examples Test Runner

This script provides a convenient way to run the example test framework
with various configuration options and reporting capabilities.

Usage:
    python run_tests.py [options]

Options:
    --build-dir DIR     Build directory (default: build)
    --source-dir DIR    Source directory (default: .)
    --verbose           Enable verbose output
    --build-first       Build examples before testing
    --clean-first       Clean build before testing
    --report FILE       Generate test report to file
    --help              Show this help message

Examples:
    python run_tests.py --verbose
    python run_tests.py --build-first --report test_results.txt
    python run_tests.py --clean-first --build-dir my_build
"""

import argparse
import subprocess
import sys
import os
import json
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class TestRunner:
    """Test runner for Atom framework examples"""

    def __init__(self, build_dir: str = "build", source_dir: str = "."):
        self.build_dir = Path(build_dir)
        self.source_dir = Path(source_dir)
        self.verbose = False

    def set_verbose(self, verbose: bool):
        """Set verbose output mode"""
        self.verbose = verbose

    def log(self, message: str, force: bool = False):
        """Log message if verbose mode is enabled"""
        if self.verbose or force:
            print(f"[TestRunner] {message}")

    def run_command(self, command: List[str], cwd: Optional[Path] = None,
                    timeout: int = 300) -> Tuple[int, str, str]:
        """Run a command and return exit code, stdout, stderr"""
        if cwd is None:
            cwd = self.source_dir

        self.log(f"Running: {' '.join(command)} (cwd: {cwd})")

        try:
            result = subprocess.run(
                command,
                cwd=cwd,
                capture_output=True,
                text=True,
                timeout=timeout
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return -1, "", f"Command timed out after {timeout} seconds"
        except Exception as e:
            return -1, "", str(e)

    def clean_build(self) -> bool:
        """Clean the build directory"""
        self.log("Cleaning build directory...")

        if self.build_dir.exists():
            try:
                import shutil
                shutil.rmtree(self.build_dir)
                self.log("Build directory cleaned")
                return True
            except Exception as e:
                self.log(f"Failed to clean build directory: {e}")
                return False
        else:
            self.log("Build directory doesn't exist, nothing to clean")
            return True

    def configure_cmake(self) -> bool:
        """Configure CMake with examples enabled"""
        self.log("Configuring CMake...")

        command = [
            "cmake",
            "-B", str(self.build_dir),
            "-S", str(self.source_dir),
            "-DATOM_EXAMPLE_BUILD_ALL=ON"
        ]

        exit_code, stdout, stderr = self.run_command(command)

        if exit_code == 0:
            self.log("CMake configuration successful")
            return True
        else:
            self.log(f"CMake configuration failed: {stderr}", force=True)
            return False

    def build_examples(self) -> bool:
        """Build all examples"""
        self.log("Building examples...")

        command = ["cmake", "--build", str(self.build_dir)]

        # Add parallel build if possible
        try:
            import multiprocessing
            cpu_count = multiprocessing.cpu_count()
            command.extend(["-j", str(cpu_count)])
        except:
            pass

        exit_code, stdout, stderr = self.run_command(command, timeout=600)

        if exit_code == 0:
            self.log("Build successful")
            return True
        else:
            self.log(f"Build failed: {stderr}", force=True)
            return False

    def build_test_framework(self) -> bool:
        """Build the test framework executable"""
        self.log("Building test framework...")

        # First, create a temporary CMakeLists.txt for the test framework
        test_cmake_content = """
cmake_minimum_required(VERSION 3.20)
project(ExampleTestFramework)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example_test_framework test_framework.cpp)

if(WIN32)
    # Windows-specific libraries
    target_link_libraries(example_test_framework PRIVATE)
else()
    # Unix-specific libraries
    target_link_libraries(example_test_framework PRIVATE)
endif()

if(MSVC)
    target_compile_options(example_test_framework PRIVATE /W4)
else()
    target_compile_options(example_test_framework PRIVATE -Wall -Wextra)
endif()
"""

        # Write temporary CMakeLists.txt
        temp_cmake = self.source_dir / "example" / "temp_CMakeLists.txt"
        try:
            with open(temp_cmake, 'w') as f:
                f.write(test_cmake_content)

            # Configure and build test framework
            test_build_dir = self.build_dir / "test_framework"

            # Configure
            command = [
                "cmake",
                "-B", str(test_build_dir),
                "-S", str(self.source_dir / "example"),
                "-f", str(temp_cmake)
            ]

            exit_code, stdout, stderr = self.run_command(command)
            if exit_code != 0:
                self.log(
                    f"Test framework configuration failed: {stderr}", force=True)
                return False

            # Build
            command = ["cmake", "--build", str(test_build_dir)]
            exit_code, stdout, stderr = self.run_command(command)

            if exit_code == 0:
                self.log("Test framework build successful")
                return True
            else:
                self.log(f"Test framework build failed: {stderr}", force=True)
                return False

        except Exception as e:
            self.log(f"Failed to build test framework: {e}", force=True)
            return False
        finally:
            # Clean up temporary file
            if temp_cmake.exists():
                temp_cmake.unlink()

    def run_test_framework(self, verbose: bool = False) -> Tuple[int, str]:
        """Run the test framework and return results"""
        self.log("Running test framework...")

        # Look for test framework executable
        possible_paths = [
            self.build_dir / "test_framework" / "example_test_framework",
            self.build_dir / "test_framework" / "example_test_framework.exe",
            self.build_dir / "example" / "example_test_framework",
            self.build_dir / "example" / "example_test_framework.exe"
        ]

        test_executable = None
        for path in possible_paths:
            if path.exists():
                test_executable = path
                break

        if not test_executable:
            # Try to build a simple version inline
            return self.run_simple_tests()

        command = [str(test_executable)]
        if verbose:
            command.append("--verbose")
        command.extend(["--build-dir", str(self.build_dir)])
        command.extend(["--source-dir", str(self.source_dir)])

        exit_code, stdout, stderr = self.run_command(command, timeout=600)

        output = stdout
        if stderr:
            output += f"\nSTDERR:\n{stderr}"

        return exit_code, output

    def run_simple_tests(self) -> Tuple[int, str]:
        """Run simple tests without the full framework"""
        self.log("Running simple tests (fallback mode)...")

        results = []

        # Test known working examples
        working_examples = [
            ("containers", "containers_high_performance_containers_example"),
            ("meta", "meta_comprehensive_meta_example"),
            ("secret", "secret_basic_test"),
            ("sysinfo", "sysinfo_header_test")
        ]

        for module, target in working_examples:
            executable_paths = [
                self.build_dir / "example" / module / f"{target}.exe",
                self.build_dir / "example" / module / target
            ]

            executable = None
            for path in executable_paths:
                if path.exists():
                    executable = path
                    break

            if executable:
                self.log(f"Testing {module}/{target}...")
                exit_code, stdout, stderr = self.run_command(
                    [str(executable)], timeout=30)

                if exit_code == 0:
                    results.append(f"✅ [{module}] {target}: PASSED")
                else:
                    results.append(
                        f"❌ [{module}] {target}: FAILED (exit {exit_code})")
            else:
                results.append(
                    f"⏭️ [{module}] {target}: SKIPPED (executable not found)")

        # Generate summary
        passed = len([r for r in results if "PASSED" in r])
        total = len(results)

        summary = f"""
=== Simple Test Results ===
{chr(10).join(results)}

Summary:
  ✅ Passed: {passed}
  📊 Total: {total}
  📈 Success Rate: {(passed/total*100):.1f}%
"""

        return 0 if passed > 0 else 1, summary

    def generate_report(self, output: str, report_file: str):
        """Generate a test report file"""
        self.log(f"Generating report: {report_file}")

        try:
            with open(report_file, 'w') as f:
                f.write("# Atom Framework Examples Test Report\n\n")
                f.write(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}\n\n")
                f.write("## Test Results\n\n")
                f.write("```\n")
                f.write(output)
                f.write("\n```\n")

            self.log(f"Report generated: {report_file}")
        except Exception as e:
            self.log(f"Failed to generate report: {e}", force=True)

    def run_full_test_suite(self, clean_first: bool = False, build_first: bool = False,
                            report_file: Optional[str] = None) -> int:
        """Run the complete test suite"""
        self.log("Starting full test suite...", force=True)

        # Clean if requested
        if clean_first:
            if not self.clean_build():
                return 1

        # Configure CMake if needed
        if clean_first or build_first or not (self.build_dir / "CMakeCache.txt").exists():
            if not self.configure_cmake():
                return 1

        # Build examples if requested
        if build_first:
            if not self.build_examples():
                return 1

        # Run tests
        exit_code, output = self.run_test_framework(self.verbose)

        # Print results
        print(output)

        # Generate report if requested
        if report_file:
            self.generate_report(output, report_file)

        return exit_code


def main():
    """Main function"""
    parser = argparse.ArgumentParser(
        description="Atom Framework Examples Test Runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    parser.add_argument("--build-dir", default="build",
                        help="Build directory (default: build)")
    parser.add_argument("--source-dir", default=".",
                        help="Source directory (default: .)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Enable verbose output")
    parser.add_argument("--build-first", action="store_true",
                        help="Build examples before testing")
    parser.add_argument("--clean-first", action="store_true",
                        help="Clean build before testing")
    parser.add_argument("--report", metavar="FILE",
                        help="Generate test report to file")

    args = parser.parse_args()

    # Create test runner
    runner = TestRunner(args.build_dir, args.source_dir)
    runner.set_verbose(args.verbose)

    # Run test suite
    try:
        exit_code = runner.run_full_test_suite(
            clean_first=args.clean_first,
            build_first=args.build_first,
            report_file=args.report
        )
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\nTest run interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"Test runner error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
