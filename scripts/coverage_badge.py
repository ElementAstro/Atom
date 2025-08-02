#!/usr/bin/env python3

"""
coverage_badge.py - Generate coverage badges for README
This project is licensed under the terms of the GPL3 license.

Author: Max Qian
License: GPL3
"""

import json
import sys
from pathlib import Path
from typing import Dict, Tuple

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
    
    if not coverage_file.exists():
        print(f"Coverage file not found: {coverage_file}")
        return badges
    
    try:
        with open(coverage_file, 'r') as f:
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
        
    except Exception as e:
        print(f"Error reading coverage data: {e}")
    
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
    if not readme_file.exists():
        print(f"README file not found: {readme_file}")
        return False
    
    try:
        content = readme_file.read_text()
        
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
        
        readme_file.write_text(new_content)
        return True
        
    except Exception as e:
        print(f"Error updating README: {e}")
        return False

def main():
    """Main function."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Generate coverage badges for README",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument(
        "--coverage-file",
        type=Path,
        default=Path("coverage/unified/coverage.json"),
        help="Path to coverage JSON file"
    )
    
    parser.add_argument(
        "--readme-file",
        type=Path,
        default=Path("README.md"),
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
    
    # Generate badges
    badges = generate_coverage_badges(args.coverage_file)
    
    if not badges:
        print("No coverage data found")
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
        success = update_readme_badges(args.readme_file, markdown)
        
        if success:
            print(f"✅ Updated {args.readme_file} with coverage badges")
        else:
            print(f"❌ Failed to update {args.readme_file}")
            return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
