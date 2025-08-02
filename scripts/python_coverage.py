#!/usr/bin/env python3

"""
python_coverage.py - Python coverage analysis script for Atom project
This project is licensed under the terms of the GPL3 license.

Author: Max Qian
License: GPL3
"""

import argparse
import subprocess
import sys
import os
import webbrowser
from pathlib import Path
from typing import List, Optional

# Colors for terminal output
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

def print_colored(message: str, color: str = Colors.NC) -> None:
    """Print a colored message to the terminal."""
    print(f"{color}{message}{Colors.NC}")

def check_dependencies() -> bool:
    """Check if required dependencies are installed."""
    required_packages = ['pytest', 'pytest-cov', 'coverage']
    missing_packages = []
    
    for package in required_packages:
        try:
            __import__(package.replace('-', '_'))
        except ImportError:
            missing_packages.append(package)
    
    if missing_packages:
        print_colored(f"Error: Missing required packages: {', '.join(missing_packages)}", Colors.RED)
        print_colored("Install them with: pip install " + " ".join(missing_packages), Colors.YELLOW)
        return False
    
    return True

def run_command(cmd: List[str], cwd: Optional[Path] = None) -> int:
    """Run a command and return the exit code."""
    print_colored(f"Running: {' '.join(cmd)}", Colors.BLUE)
    try:
        result = subprocess.run(cmd, cwd=cwd, check=False)
        return result.returncode
    except FileNotFoundError:
        print_colored(f"Error: Command not found: {cmd[0]}", Colors.RED)
        return 1

def setup_coverage_directories() -> None:
    """Create necessary coverage directories."""
    coverage_dirs = [
        Path("coverage/python"),
        Path("coverage/python/html"),
    ]
    
    for dir_path in coverage_dirs:
        dir_path.mkdir(parents=True, exist_ok=True)
        print_colored(f"Created directory: {dir_path}", Colors.GREEN)

def run_tests(test_path: Optional[str] = None, markers: Optional[str] = None) -> int:
    """Run pytest with coverage."""
    cmd = ["python", "-m", "pytest"]
    
    if test_path:
        cmd.append(test_path)
    else:
        cmd.extend(["python/tests", "tests"])
    
    if markers:
        cmd.extend(["-m", markers])
    
    # Add coverage options
    cmd.extend([
        "--cov=atom",
        "--cov=python",
        "--cov-report=html:coverage/python/html",
        "--cov-report=xml:coverage/python/coverage.xml",
        "--cov-report=term-missing",
        "--cov-branch",
    ])
    
    return run_command(cmd)

def generate_coverage_report() -> int:
    """Generate coverage report from existing data."""
    cmd = ["python", "-m", "coverage", "html", "-d", "coverage/python/html"]
    return run_command(cmd)

def combine_coverage() -> int:
    """Combine coverage data from multiple runs."""
    cmd = ["python", "-m", "coverage", "combine"]
    return run_command(cmd)

def erase_coverage() -> int:
    """Erase existing coverage data."""
    cmd = ["python", "-m", "coverage", "erase"]
    return run_command(cmd)

def open_report() -> None:
    """Open the HTML coverage report in a browser."""
    report_path = Path("coverage/python/html/index.html")
    if report_path.exists():
        print_colored(f"Opening coverage report: {report_path.absolute()}", Colors.GREEN)
        webbrowser.open(f"file://{report_path.absolute()}")
    else:
        print_colored("Coverage report not found. Run coverage analysis first.", Colors.RED)

def main() -> int:
    """Main function."""
    parser = argparse.ArgumentParser(
        description="Python coverage analysis for Atom project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Run all tests with coverage
  %(prog)s --test-path python/tests # Run only Python binding tests
  %(prog)s --markers "not slow"     # Skip slow tests
  %(prog)s --erase --run --open     # Full coverage workflow
  %(prog)s --report-only --open     # Generate report and open
        """
    )
    
    parser.add_argument(
        "--test-path",
        help="Specific test path to run (default: all tests)"
    )
    
    parser.add_argument(
        "--markers",
        help="Pytest markers to filter tests (e.g., 'not slow')"
    )
    
    parser.add_argument(
        "--erase",
        action="store_true",
        help="Erase existing coverage data before running"
    )
    
    parser.add_argument(
        "--run",
        action="store_true",
        default=True,
        help="Run tests with coverage (default: True)"
    )
    
    parser.add_argument(
        "--no-run",
        dest="run",
        action="store_false",
        help="Don't run tests, only generate reports"
    )
    
    parser.add_argument(
        "--report-only",
        action="store_true",
        help="Only generate HTML report from existing data"
    )
    
    parser.add_argument(
        "--combine",
        action="store_true",
        help="Combine coverage data from multiple runs"
    )
    
    parser.add_argument(
        "--open",
        action="store_true",
        help="Open HTML report in browser after generation"
    )
    
    args = parser.parse_args()
    
    print_colored("Atom Python Coverage Analysis", Colors.BLUE)
    print_colored("=" * 30, Colors.BLUE)
    
    # Check dependencies
    if not check_dependencies():
        return 1
    
    # Setup directories
    setup_coverage_directories()
    
    exit_code = 0
    
    # Erase existing data if requested
    if args.erase:
        print_colored("Erasing existing coverage data...", Colors.YELLOW)
        erase_coverage()
    
    # Combine coverage data if requested
    if args.combine:
        print_colored("Combining coverage data...", Colors.YELLOW)
        combine_coverage()
    
    # Run tests with coverage
    if args.run and not args.report_only:
        print_colored("Running tests with coverage analysis...", Colors.GREEN)
        exit_code = run_tests(args.test_path, args.markers)
        
        if exit_code != 0:
            print_colored("Tests failed!", Colors.RED)
        else:
            print_colored("Tests completed successfully!", Colors.GREEN)
    
    # Generate report only
    if args.report_only:
        print_colored("Generating coverage report...", Colors.GREEN)
        exit_code = generate_coverage_report()
    
    # Open report if requested
    if args.open:
        open_report()
    
    # Print summary
    report_path = Path("coverage/python/html/index.html")
    if report_path.exists():
        print_colored(f"Coverage report available at: {report_path.absolute()}", Colors.GREEN)
    
    return exit_code

if __name__ == "__main__":
    sys.exit(main())
