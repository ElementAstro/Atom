#!/usr/bin/env python3

"""
unified_coverage.py - Unified coverage reporting for C++ and Python
This project is licensed under the terms of the GPL3 license.

Author: Max Qian
License: GPL3
"""

import json
import os
import subprocess
import sys
import webbrowser
from pathlib import Path
from typing import Dict, List, Optional
import xml.etree.ElementTree as ET

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'

def print_colored(message: str, color: str = Colors.NC) -> None:
    """Print a colored message."""
    print(f"{color}{message}{Colors.NC}")

class UnifiedCoverageReporter:
    """Generate unified coverage reports for C++ and Python."""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.build_dir = project_root / "build"
        self.coverage_dir = project_root / "coverage"
        self.cpp_coverage_dir = self.coverage_dir / "cpp"
        self.python_coverage_dir = self.coverage_dir / "python"
        self.unified_dir = self.coverage_dir / "unified"

        # Ensure directories exist
        self.coverage_dir.mkdir(exist_ok=True)
        self.unified_dir.mkdir(exist_ok=True)

    def run_cpp_coverage(self) -> bool:
        """Run C++ coverage analysis."""
        print_colored("Running C++ coverage analysis...", Colors.BLUE)

        try:
            # Build with coverage
            result = subprocess.run([
                "cmake", "--build", str(self.build_dir), "--target", "coverage"
            ], capture_output=True, text=True, cwd=self.project_root)

            if result.returncode != 0:
                print_colored(f"C++ coverage failed: {result.stderr}", Colors.RED)
                return False

            print_colored("C++ coverage completed successfully", Colors.GREEN)
            return True

        except Exception as e:
            print_colored(f"Error running C++ coverage: {e}", Colors.RED)
            return False

    def run_python_coverage(self) -> bool:
        """Run Python coverage analysis."""
        print_colored("Running Python coverage analysis...", Colors.BLUE)

        try:
            # Run Python coverage
            result = subprocess.run([
                sys.executable, "scripts/python_coverage.py", "--no-run"
            ], capture_output=True, text=True, cwd=self.project_root)

            if result.returncode != 0:
                print_colored(f"Python coverage setup failed: {result.stderr}", Colors.RED)
                return False

            # Run tests with coverage
            result = subprocess.run([
                sys.executable, "-m", "pytest",
                "python/tests/",
                "--cov=atom",
                "--cov=python",
                f"--cov-report=xml:{self.python_coverage_dir}/coverage.xml",
                f"--cov-report=html:{self.python_coverage_dir}/html",
                "--cov-branch"
            ], capture_output=True, text=True, cwd=self.project_root)

            if result.returncode != 0:
                print_colored("Python tests failed, but continuing with coverage...", Colors.YELLOW)

            print_colored("Python coverage completed", Colors.GREEN)
            return True

        except Exception as e:
            print_colored(f"Error running Python coverage: {e}", Colors.RED)
            return False

    def parse_cpp_coverage(self) -> Dict:
        """Parse C++ coverage data from lcov info files."""
        coverage_data = {
            "language": "C++",
            "total_lines": 0,
            "covered_lines": 0,
            "coverage_percentage": 0.0,
            "modules": {}
        }

        # Look for lcov info files
        cpp_info_file = self.build_dir / "coverage" / "coverage_cleaned.info"
        if not cpp_info_file.exists():
            print_colored("C++ coverage info file not found", Colors.YELLOW)
            return coverage_data

        try:
            with open(cpp_info_file, 'r') as f:
                content = f.read()

            # Parse lcov format
            current_file = None
            for line in content.split('\n'):
                if line.startswith('SF:'):
                    current_file = line[3:]
                elif line.startswith('LH:'):
                    covered = int(line[3:])
                    coverage_data["covered_lines"] += covered
                elif line.startswith('LF:'):
                    total = int(line[3:])
                    coverage_data["total_lines"] += total

            if coverage_data["total_lines"] > 0:
                coverage_data["coverage_percentage"] = (
                    coverage_data["covered_lines"] / coverage_data["total_lines"] * 100
                )

        except Exception as e:
            print_colored(f"Error parsing C++ coverage: {e}", Colors.RED)

        return coverage_data

    def parse_python_coverage(self) -> Dict:
        """Parse Python coverage data from XML report."""
        coverage_data = {
            "language": "Python",
            "total_lines": 0,
            "covered_lines": 0,
            "coverage_percentage": 0.0,
            "modules": {}
        }

        xml_file = self.python_coverage_dir / "coverage.xml"
        if not xml_file.exists():
            print_colored("Python coverage XML file not found", Colors.YELLOW)
            return coverage_data

        try:
            tree = ET.parse(xml_file)
            root = tree.getroot()

            # Parse coverage XML
            for package in root.findall('.//package'):
                for class_elem in package.findall('classes/class'):
                    filename = class_elem.get('filename', '')
                    lines = class_elem.find('lines')
                    if lines is not None:
                        for line in lines.findall('line'):
                            coverage_data["total_lines"] += 1
                            if line.get('hits', '0') != '0':
                                coverage_data["covered_lines"] += 1

            # Get overall coverage from root
            if 'line-rate' in root.attrib:
                coverage_data["coverage_percentage"] = float(root.attrib['line-rate']) * 100

        except Exception as e:
            print_colored(f"Error parsing Python coverage: {e}", Colors.RED)

        return coverage_data

    def generate_unified_report(self, cpp_data: Dict, python_data: Dict) -> str:
        """Generate unified HTML coverage report."""
        total_lines = cpp_data["total_lines"] + python_data["total_lines"]
        total_covered = cpp_data["covered_lines"] + python_data["covered_lines"]
        overall_coverage = (total_covered / total_lines * 100) if total_lines > 0 else 0

        html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Atom Project - Unified Coverage Report</title>
    <style>
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            overflow: hidden;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }}
        .header h1 {{
            margin: 0;
            font-size: 2.5em;
        }}
        .header p {{
            margin: 10px 0 0 0;
            opacity: 0.9;
        }}
        .summary {{
            padding: 30px;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
        }}
        .metric-card {{
            background: #f8f9fa;
            border-radius: 8px;
            padding: 20px;
            text-align: center;
            border-left: 4px solid #667eea;
        }}
        .metric-value {{
            font-size: 2em;
            font-weight: bold;
            color: #333;
        }}
        .metric-label {{
            color: #666;
            margin-top: 5px;
        }}
        .coverage-bar {{
            width: 100%;
            height: 20px;
            background: #e9ecef;
            border-radius: 10px;
            overflow: hidden;
            margin: 10px 0;
        }}
        .coverage-fill {{
            height: 100%;
            background: linear-gradient(90deg, #28a745, #20c997);
            transition: width 0.3s ease;
        }}
        .language-section {{
            margin: 20px 30px;
            padding: 20px;
            border: 1px solid #dee2e6;
            border-radius: 8px;
        }}
        .language-title {{
            font-size: 1.5em;
            margin-bottom: 15px;
            color: #495057;
        }}
        .links {{
            padding: 30px;
            background: #f8f9fa;
            text-align: center;
        }}
        .link-button {{
            display: inline-block;
            padding: 12px 24px;
            margin: 0 10px;
            background: #667eea;
            color: white;
            text-decoration: none;
            border-radius: 6px;
            transition: background 0.3s ease;
        }}
        .link-button:hover {{
            background: #5a6fd8;
        }}
        .timestamp {{
            text-align: center;
            color: #6c757d;
            font-size: 0.9em;
            padding: 20px;
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Atom Project Coverage Report</h1>
            <p>Unified C++ and Python Coverage Analysis</p>
        </div>

        <div class="summary">
            <div class="metric-card">
                <div class="metric-value">{overall_coverage:.1f}%</div>
                <div class="metric-label">Overall Coverage</div>
                <div class="coverage-bar">
                    <div class="coverage-fill" style="width: {overall_coverage}%"></div>
                </div>
            </div>
            <div class="metric-card">
                <div class="metric-value">{total_lines:,}</div>
                <div class="metric-label">Total Lines</div>
            </div>
            <div class="metric-card">
                <div class="metric-value">{total_covered:,}</div>
                <div class="metric-label">Covered Lines</div>
            </div>
        </div>

        <div class="language-section">
            <div class="language-title">🔧 C++ Coverage</div>
            <div class="summary">
                <div class="metric-card">
                    <div class="metric-value">{cpp_data['coverage_percentage']:.1f}%</div>
                    <div class="metric-label">C++ Coverage</div>
                    <div class="coverage-bar">
                        <div class="coverage-fill" style="width: {cpp_data['coverage_percentage']}%"></div>
                    </div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">{cpp_data['total_lines']:,}</div>
                    <div class="metric-label">C++ Lines</div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">{cpp_data['covered_lines']:,}</div>
                    <div class="metric-label">C++ Covered</div>
                </div>
            </div>
        </div>

        <div class="language-section">
            <div class="language-title">🐍 Python Coverage</div>
            <div class="summary">
                <div class="metric-card">
                    <div class="metric-value">{python_data['coverage_percentage']:.1f}%</div>
                    <div class="metric-label">Python Coverage</div>
                    <div class="coverage-bar">
                        <div class="coverage-fill" style="width: {python_data['coverage_percentage']}%"></div>
                    </div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">{python_data['total_lines']:,}</div>
                    <div class="metric-label">Python Lines</div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">{python_data['covered_lines']:,}</div>
                    <div class="metric-label">Python Covered</div>
                </div>
            </div>
        </div>

        <div class="links">
            <a href="../build/coverage/html/index.html" class="link-button">📊 C++ Detailed Report</a>
            <a href="../python/html/index.html" class="link-button">🐍 Python Detailed Report</a>
            <a href="coverage.json" class="link-button">📄 JSON Report</a>
        </div>

        <div class="timestamp">
            Generated on {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
        </div>
    </div>
</body>
</html>"""

        report_file = self.unified_dir / "index.html"
        report_file.write_text(html_content)

        return str(report_file)

    def generate_json_report(self, cpp_data: Dict, python_data: Dict) -> str:
        """Generate JSON coverage report for CI/CD integration."""
        total_lines = cpp_data["total_lines"] + python_data["total_lines"]
        total_covered = cpp_data["covered_lines"] + python_data["covered_lines"]
        overall_coverage = (total_covered / total_lines * 100) if total_lines > 0 else 0

        json_data = {
            "timestamp": __import__('datetime').datetime.now().isoformat(),
            "overall": {
                "coverage_percentage": round(overall_coverage, 2),
                "total_lines": total_lines,
                "covered_lines": total_covered
            },
            "cpp": cpp_data,
            "python": python_data
        }

        json_file = self.unified_dir / "coverage.json"
        with open(json_file, 'w') as f:
            json.dump(json_data, f, indent=2)

        return str(json_file)

    def run_unified_coverage(self, skip_cpp: bool = False, skip_python: bool = False) -> bool:
        """Run unified coverage analysis."""
        print_colored("Starting unified coverage analysis...", Colors.BLUE)

        cpp_success = True
        python_success = True

        if not skip_cpp:
            cpp_success = self.run_cpp_coverage()

        if not skip_python:
            python_success = self.run_python_coverage()

        # Parse coverage data
        cpp_data = self.parse_cpp_coverage()
        python_data = self.parse_python_coverage()

        # Generate reports
        html_report = self.generate_unified_report(cpp_data, python_data)
        json_report = self.generate_json_report(cpp_data, python_data)

        print_colored(f"Unified HTML report: {html_report}", Colors.GREEN)
        print_colored(f"JSON report: {json_report}", Colors.GREEN)

        return cpp_success and python_success

def main():
    """Main function."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Generate unified coverage reports for C++ and Python",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--skip-cpp",
        action="store_true",
        help="Skip C++ coverage analysis"
    )

    parser.add_argument(
        "--skip-python",
        action="store_true",
        help="Skip Python coverage analysis"
    )

    parser.add_argument(
        "--open",
        action="store_true",
        help="Open the unified report in browser"
    )

    args = parser.parse_args()

    project_root = Path.cwd()

    print_colored("Atom Unified Coverage Reporter", Colors.BLUE)
    print_colored("=" * 30, Colors.BLUE)

    reporter = UnifiedCoverageReporter(project_root)
    success = reporter.run_unified_coverage(
        skip_cpp=args.skip_cpp,
        skip_python=args.skip_python
    )

    if args.open:
        report_file = project_root / "coverage" / "unified" / "index.html"
        if report_file.exists():
            webbrowser.open(f"file://{report_file.absolute()}")
        else:
            print_colored("Unified report not found", Colors.RED)

    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
