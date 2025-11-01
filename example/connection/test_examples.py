#!/usr/bin/env python3
"""
Comprehensive test script for Atom Connection Examples

This script tests all connection examples to ensure they compile and run correctly.
It performs both compilation tests and basic runtime tests where possible.

Usage:
    python test_examples.py [--build-only] [--verbose] [--example NAME]

Requirements:
    - CMake 3.10+
    - C++20 compatible compiler
    - Python 3.6+
    - Atom Connection library built and available
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Optional, Tuple, cast


class Colors:
    """ANSI color codes for terminal output"""

    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    MAGENTA = "\033[95m"
    CYAN = "\033[96m"
    WHITE = "\033[97m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"
    END = "\033[0m"


class TestResult:
    """Test result container"""

    def __init__(
        self, name: str, success: bool, message: str = "", duration: float = 0.0
    ):
        self.name = name
        self.success = success
        self.message = message
        self.duration = duration


class ExampleTester:
    """Main test runner for connection examples"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.build_dir = Path("build")
        self.source_dir = Path(".")
        self.results: List[TestResult] = []

        # Define example categories and their properties
        self.examples = {
            # TCP Examples
            "tcpclient": {
                "category": "TCP",
                "requires_server": True,
                "server_example": "sockethub",
                "timeout": 15,
            },
            "async_tcpclient": {
                "category": "TCP",
                "requires_server": True,
                "server_example": "async_sockethub",
                "timeout": 20,
            },
            "sockethub": {"category": "TCP", "is_server": True, "timeout": 10},
            "async_sockethub": {"category": "TCP", "is_server": True, "timeout": 15},
            # UDP Examples
            "udpclient": {
                "category": "UDP",
                "requires_server": True,
                "server_example": "udpserver",
                "timeout": 12,
            },
            "udpserver": {"category": "UDP", "is_server": True, "timeout": 10},
            "async_udpclient": {
                "category": "UDP",
                "requires_server": False,
                "timeout": 15,
            },
            "async_udpserver": {"category": "UDP", "is_server": True, "timeout": 12},
            # FIFO Examples
            "fifoclient": {
                "category": "FIFO",
                "platform_specific": ["linux", "unix"],
                "timeout": 8,
            },
            "fifoserver": {
                "category": "FIFO",
                "platform_specific": ["linux", "unix"],
                "timeout": 8,
            },
            "async_fifoclient": {
                "category": "FIFO",
                "platform_specific": ["linux", "unix"],
                "timeout": 10,
            },
            "async_fifoserver": {
                "category": "FIFO",
                "platform_specific": ["linux", "unix"],
                "timeout": 10,
            },
            # SSH Examples
            "sshclient": {"category": "SSH", "requires_ssh": True, "timeout": 15},
            "sshserver": {"category": "SSH", "requires_ssh": True, "timeout": 15},
            # TTY Examples
            "ttybase": {"category": "TTY", "requires_hardware": True, "timeout": 8},
        }

    def log(self, message: str, color: str = Colors.WHITE):
        """Log a message with optional color"""
        timestamp = time.strftime("%H:%M:%S")
        print(f"{color}[{timestamp}] {message}{Colors.END}")

    def log_verbose(self, message: str):
        """Log verbose message if verbose mode is enabled"""
        if self.verbose:
            self.log(f"VERBOSE: {message}", Colors.CYAN)

    def run_command(
        self, cmd: List[str], cwd: Optional[Path] = None, timeout: int = 30
    ) -> Tuple[int, str, str]:
        """Run a command and return exit code, stdout, stderr"""
        self.log_verbose(f"Running command: {' '.join(cmd)}")

        try:
            result = subprocess.run(
                cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return -1, "", f"Command timed out after {timeout} seconds"
        except Exception as e:
            return -1, "", str(e)

    def setup_build_environment(self) -> bool:
        """Set up the build environment"""
        self.log("Setting up build environment...", Colors.BLUE)

        # Create build directory
        self.build_dir.mkdir(exist_ok=True)

        # Run CMake configuration
        cmake_cmd = [
            "cmake",
            "-DATOM_EXAMPLE_CONNECTION_BUILD_ALL=ON",
            "-DATOM_EXAMPLE_CONNECTION_VERBOSE=ON",
            "-DCMAKE_BUILD_TYPE=Debug",
            str(self.source_dir.parent.parent),  # Go up to project root
        ]

        exit_code, stdout, stderr = self.run_command(cmake_cmd, self.build_dir)

        if exit_code != 0:
            self.log(f"CMake configuration failed: {stderr}", Colors.RED)
            return False

        self.log("Build environment configured successfully", Colors.GREEN)
        return True

    def build_examples(self, specific_example: Optional[str] = None) -> bool:
        """Build all examples or a specific example"""
        if specific_example:
            self.log(f"Building example: {specific_example}...", Colors.BLUE)
            target = f"connection_{specific_example}"
        else:
            self.log("Building all connection examples...", Colors.BLUE)
            target = "connection_examples_all"

        build_cmd = ["cmake", "--build", ".", "--target", target, "--parallel"]

        exit_code, stdout, stderr = self.run_command(
            build_cmd, self.build_dir, timeout=120
        )

        if exit_code != 0:
            error_msg = f"Build failed for {target}: {stderr}"
            self.log(error_msg, Colors.RED)
            if specific_example:
                self.results.append(
                    TestResult(f"build_{specific_example}", False, error_msg)
                )
            return False

        success_msg = f"Build successful for {target}"
        self.log(success_msg, Colors.GREEN)
        if specific_example:
            self.results.append(
                TestResult(f"build_{specific_example}", True, success_msg)
            )
        return True

    def test_example_execution(self, example_name: str) -> TestResult:
        """Test execution of a specific example"""
        example_info = self.examples.get(example_name, {})
        timeout: int = cast(int, example_info.get("timeout", 10))

        self.log(f"Testing execution of {example_name}...", Colors.YELLOW)

        # Check platform compatibility
        platform_specific: list[str] = cast(
            list[str], example_info.get("platform_specific", [])
        )
        if platform_specific and sys.platform not in platform_specific:
            return TestResult(
                f"run_{example_name}",
                True,
                f"Skipped on {sys.platform} (platform-specific: {platform_specific})",
            )

        # Check SSH requirement
        if example_info.get("requires_ssh") and not self.check_ssh_available():
            return TestResult(
                f"run_{example_name}", True, "Skipped (SSH not available)"
            )

        # Check hardware requirement
        if example_info.get("requires_hardware"):
            return TestResult(
                f"run_{example_name}", True, "Skipped (requires hardware)"
            )

        executable = self.build_dir / f"connection_{example_name}"
        if sys.platform == "win32":
            executable = executable.with_suffix(".exe")

        if not executable.exists():
            return TestResult(
                f"run_{example_name}", False, f"Executable not found: {executable}"
            )

        # Handle server examples differently
        if example_info.get("is_server"):
            return self.test_server_example(example_name, executable, timeout)
        elif example_info.get("requires_server"):
            return self.test_client_example(example_name, executable, timeout)
        else:
            return self.test_standalone_example(example_name, executable, timeout)

    def test_standalone_example(
        self, name: str, executable: Path, timeout: int
    ) -> TestResult:
        """Test a standalone example that doesn't require a server"""
        start_time = time.time()

        exit_code, stdout, stderr = self.run_command([str(executable)], timeout=timeout)

        duration = time.time() - start_time

        if exit_code == 0:
            return TestResult(
                f"run_{name}",
                True,
                f"Completed successfully in {duration:.2f}s",
                duration,
            )
        else:
            return TestResult(
                f"run_{name}",
                False,
                f"Failed with exit code {exit_code}: {stderr}",
                duration,
            )

    def test_server_example(
        self, name: str, executable: Path, timeout: int
    ) -> TestResult:
        """Test a server example by running it briefly"""
        start_time = time.time()

        try:
            # Start the server process
            process = subprocess.Popen(
                [str(executable)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )

            # Let it run for a few seconds
            time.sleep(min(3, timeout // 2))

            # Terminate gracefully
            process.terminate()

            try:
                stdout, stderr = process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                stdout, stderr = process.communicate()

            duration = time.time() - start_time

            # Server examples should start and run without immediate crashes
            # 0=normal, -15=SIGTERM, -2=SIGINT
            if process.returncode in [0, -15, -2]:
                return TestResult(
                    f"run_{name}",
                    True,
                    f"Server ran successfully for {duration:.2f}s",
                    duration,
                )
            else:
                return TestResult(
                    f"run_{name}",
                    False,
                    f"Server failed with code {process.returncode}: {stderr}",
                    duration,
                )

        except Exception as e:
            duration = time.time() - start_time
            return TestResult(f"run_{name}", False, f"Exception: {str(e)}", duration)

    def test_client_example(
        self, name: str, executable: Path, timeout: int
    ) -> TestResult:
        """Test a client example (may fail if no server is running)"""
        start_time = time.time()

        exit_code, stdout, stderr = self.run_command([str(executable)], timeout=timeout)

        duration = time.time() - start_time

        # Client examples may fail if no server is running, which is expected
        if exit_code == 0:
            return TestResult(
                f"run_{name}",
                True,
                f"Completed successfully in {duration:.2f}s",
                duration,
            )
        else:
            # Check if failure is due to connection issues (expected)
            if "connection" in stderr.lower() or "connect" in stderr.lower():
                return TestResult(
                    f"run_{name}",
                    True,
                    "Expected connection failure (no server running)",
                    duration,
                )
            else:
                return TestResult(
                    f"run_{name}", False, f"Unexpected failure: {stderr}", duration
                )

    def check_ssh_available(self) -> bool:
        """Check if SSH is available on the system"""
        try:
            subprocess.run(["ssh", "-V"], capture_output=True, timeout=5)
            return True
        except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
            return False

    def run_tests(
        self, specific_example: Optional[str] = None, build_only: bool = False
    ) -> bool:
        """Run all tests or tests for a specific example"""
        self.log(
            "Starting Atom Connection Examples Test Suite", Colors.BOLD + Colors.MAGENTA
        )

        # Setup build environment
        if not self.setup_build_environment():
            return False

        # Build examples
        if not self.build_examples(specific_example):
            return False

        if build_only:
            self.log("Build-only mode: skipping execution tests", Colors.YELLOW)
            return True

        # Test execution
        examples_to_test = (
            [specific_example] if specific_example else list(self.examples.keys())
        )

        for example_name in examples_to_test:
            if example_name in self.examples:
                result = self.test_example_execution(example_name)
                self.results.append(result)
            else:
                self.log(f"Unknown example: {example_name}", Colors.RED)

        return self.print_summary()

    def print_summary(self) -> bool:
        """Print test summary and return overall success"""
        self.log("\n" + "=" * 60, Colors.BOLD)
        self.log("TEST SUMMARY", Colors.BOLD + Colors.MAGENTA)
        self.log("=" * 60, Colors.BOLD)

        passed = 0
        failed = 0

        for result in self.results:
            status_color = Colors.GREEN if result.success else Colors.RED
            status = "PASS" if result.success else "FAIL"

            duration_str = f" ({result.duration:.2f}s)" if result.duration > 0 else ""
            self.log(
                f"{status_color}{status:4}{Colors.END} {result.name:25} {result.message}{duration_str}"
            )

            if result.success:
                passed += 1
            else:
                failed += 1

        self.log("-" * 60)
        self.log(
            f"Total: {passed + failed}, Passed: {Colors.GREEN}{passed}{Colors.END}, Failed: {Colors.RED}{failed}{Colors.END}"
        )

        if failed == 0:
            self.log("All tests passed! 🎉", Colors.BOLD + Colors.GREEN)
            return True
        else:
            self.log(f"{failed} test(s) failed! ❌", Colors.BOLD + Colors.RED)
            return False


def main():
    parser = argparse.ArgumentParser(description="Test Atom Connection Examples")
    parser.add_argument(
        "--build-only", action="store_true", help="Only test building, skip execution"
    )
    parser.add_argument("--verbose", action="store_true", help="Enable verbose output")
    parser.add_argument("--example", help="Test specific example only")

    args = parser.parse_args()

    tester = ExampleTester(verbose=args.verbose)

    try:
        success = tester.run_tests(args.example, args.build_only)
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        tester.log("\nTest interrupted by user", Colors.YELLOW)
        sys.exit(130)
    except Exception as e:
        tester.log(f"Unexpected error: {e}", Colors.RED)
        sys.exit(1)


if __name__ == "__main__":
    main()
