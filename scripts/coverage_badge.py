#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
coverage_badge.py - Generate coverage badges for README
This project is licensed under the terms of the GPL3 license.

Cross-platform support for Windows, macOS, and Linux.

Usage:
  Windows: python scripts\\coverage_badge.py
  Unix:    python scripts/coverage_badge.py

Author: Max Qian
License: GPL3
"""

import json
import os
import platform
import sys
from pathlib import Path
from typing import Dict

def is_windows() -> bool:
    """Check if running on Windows."""
    return platform.system().lower() == 'windows'

def normalize_path(path: Path) -> Path:
    """Normalize path for the current platform."""
    if is_windows():
        # Convert forward slashes to backslashes on Windows
        return Path(str(path).replace('/', os.sep))
    return path

def get_coverage_color(percentage: float) -> str:
    """Get color for coverage badge based on percentage."""
    if percentage >= 90:
        return "brightgreen"
    elif percentage >= 80:
        return "green"
    elif percentage >= 70:
        return "yellowgreen"
    elif percentage >= 60:
        return "yellow"
    elif percentage >= 50:
        return "orange"
    else:
        return "red"

def generate_badge_url(label: str, message: str, color: str) -> str:
    """Generate shields.io badge URL."""
    return f"https://img.shields.io/badge/{label}-{message}-{color}"

def generate_coverage_badges(coverage_file: Path) -> Dict[str, str]:
    """Generate coverage badge URLs from coverage data."""
    badges = {}

    # Normalize path for current platform
    coverage_file = normalize_path(coverage_file)

    if not coverage_file.exists():
        print(f"Coverage file not found: {coverage_file}")
        return badges


    try:
        # Use UTF-8 encoding explicitly for cross-platform compatibility
        with open(coverage_file, 'r', encoding='utf-8') as f:
            data = json.load(f)

        # Overall coverage badge
        overall_pct = data.get("overall", {}).get("coverage_percentage", 0)
        overall_color = get_coverage_color(overall_pct)
        badges["overall"] = generate_badge_url(
            "coverage", f"{overall_pct:.1f}%25", overall_color
        )

        # C++ coverage badge
        cpp_pct = data.get("cpp", {}).get("coverage_percentage", 0)
        cpp_color = get_coverage_color(cpp_pct)
        badges["cpp"] = generate_badge_url(
            "C%2B%2B%20coverage", f"{cpp_pct:.1f}%25", cpp_color
        )

        # Python coverage badge
        python_pct = data.get("python", {}).get("coverage_percentage", 0)
        python_color = get_coverage_color(python_pct)
        badges["python"] = generate_badge_url(
            "Python%20coverage", f"{python_pct:.1f}%25", python_color
        )

    except FileNotFoundError:
        print(f"❌ Coverage file not found: {coverage_file}")
        if is_windows():
            print("💡 Make sure to use backslashes in Windows paths or forward slashes with quotes")
    except PermissionError:
        print(f"❌ Permission denied reading: {coverage_file}")
        if is_windows():
            print("💡 Try running as administrator or check file permissions")
    except json.JSONDecodeError as e:
        print(f"❌ Invalid JSON in coverage file: {e}")
    except Exception as e:
        print(f"❌ Error reading coverage data: {e}")
        if is_windows():
            print("💡 Check file encoding and ensure it's UTF-8")

    return badges

def generate_badge_markdown(badges: Dict[str, str]) -> str:
    """Generate markdown for coverage badges."""
    markdown_lines = []

    if "overall" in badges:
        markdown_lines.append(f"![Coverage]({badges['overall']})")

    if "cpp" in badges:
        markdown_lines.append(f"![C++ Coverage]({badges['cpp']})")

    if "python" in badges:
        markdown_lines.append(f"![Python Coverage]({badges['python']})")

    return " ".join(markdown_lines)

def update_readme_badges(readme_file: Path, badges_markdown: str) -> bool:
    """Update README.md with coverage badges."""
    # Normalize path for current platform
    readme_file = normalize_path(readme_file)

    if not readme_file.exists():
        print(f"README file not found: {readme_file}")
        return False


    try:
        # Use UTF-8 encoding and handle different line endings
        content = readme_file.read_text(encoding='utf-8')

        # Normalize line endings for cross-platform compatibility
        content = content.replace('\r\n', '\n').replace('\r', '\n')

        # Look for existing coverage badges section
        start_marker = "<!-- COVERAGE-BADGES-START -->"
        end_marker = "<!-- COVERAGE-BADGES-END -->"


        start_idx = content.find(start_marker)
        end_idx = content.find(end_marker)


        if start_idx != -1 and end_idx != -1:
            # Replace existing badges
            new_content = (
                content[:start_idx + len(start_marker)] +
                f"\n{badges_markdown}\n" +
                content[end_idx:]
            )
        else:
            # Add badges section at the top after title
            lines = content.split('\n')
            insert_idx = 0


            # Find the first heading
            for i, line in enumerate(lines):
                if line.startswith('# '):
                    insert_idx = i + 1
                    break


            # Insert badges section
            badge_section = [
                "",
                start_marker,
                badges_markdown,
                end_marker,
                ""
            ]


            lines[insert_idx:insert_idx] = badge_section
            new_content = '\n'.join(lines)

        # Use platform-appropriate line endings when writing
        if is_windows():
            new_content = new_content.replace('\n', '\r\n')

        # Write with UTF-8 encoding
        readme_file.write_text(new_content, encoding='utf-8')
        return True

    except FileNotFoundError:
        print(f"❌ README file not found: {readme_file}")
        return False
    except PermissionError:
        print(f"❌ Permission denied writing to: {readme_file}")
        if is_windows():
            print("💡 Close the file in any editors and try running as administrator")
        return False
    except UnicodeEncodeError as e:
        print(f"❌ Unicode encoding error: {e}")
        if is_windows():
            print("💡 Ensure your terminal supports UTF-8 encoding")
        return False
    except Exception as e:
        print(f"❌ Error updating README: {e}")
        return False

def get_default_coverage_file() -> Path:
    """Get default coverage file path for the current platform."""
    if is_windows():
        # On Windows, use backslashes for default path display
        return Path("coverage\\unified\\coverage.json")
    return Path("coverage/unified/coverage.json")

def get_default_readme_file() -> Path:
    """Get default README file path for the current platform."""
    return Path("README.md")

def main():
    """Main function."""
    import argparse

    # Detect platform for help text
    platform_info = f" (detected: {platform.system()})"

    parser = argparse.ArgumentParser(
        description=f"Generate coverage badges for README{platform_info}",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Windows:
    python scripts\\coverage_badge.py
    python scripts\\coverage_badge.py --coverage-file coverage\\unified\\coverage.json

  Unix/Linux/macOS:
    python scripts/coverage_badge.py
    python scripts/coverage_badge.py --coverage-file coverage/unified/coverage.json
        """
    )


    parser.add_argument(
        "--coverage-file",
        type=Path,
        default=get_default_coverage_file(),
        help="Path to coverage JSON file"
    )


    parser.add_argument(
        "--readme-file",
        type=Path,
        default=get_default_readme_file(),
        help="Path to README.md file"
    )

    parser.add_argument(
        "--output",
        choices=["markdown", "urls", "update-readme"],
        default="update-readme",
        help="Output format"
    )

    args = parser.parse_args()


    print("Coverage Badge Generator")
    print("=" * 25)
    print(f"Platform: {platform.system()} {platform.release()}")
    print(f"Python: {sys.version.split()[0]}")

    # Normalize input paths
    coverage_file = normalize_path(args.coverage_file)
    readme_file = normalize_path(args.readme_file)

    print(f"Coverage file: {coverage_file}")
    print(f"README file: {readme_file}")
    print()

    # Generate badges
    badges = generate_coverage_badges(coverage_file)

    if not badges:
        print("❌ No coverage data found")
        if is_windows():
            print("💡 Try running: cmake --build build --target coverage")
        else:
            print("💡 Try running: make coverage-unified")
        return 1

    if args.output == "urls":
        print("Coverage Badge URLs:")
        for name, url in badges.items():
            print(f"{name}: {url}")

    elif args.output == "markdown":
        markdown = generate_badge_markdown(badges)
        print("Coverage Badges Markdown:")
        print(markdown)

    elif args.output == "update-readme":
        markdown = generate_badge_markdown(badges)
        success = update_readme_badges(readme_file, markdown)

        if success:
            print(f"✅ Updated {readme_file} with coverage badges")
            if is_windows():
                print("💡 Tip: Use Git Bash or WSL for better Unicode support")
        else:
            print(f"❌ Failed to update {readme_file}")
            if is_windows():
                print("💡 Check file permissions and ensure the file is not open in another program")
            return 1

    print()
    print("🎉 Coverage badge generation completed successfully!")

    return 0

if __name__ == "__main__":
    sys.exit(main())
