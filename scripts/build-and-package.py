#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    target = script_dir / "package" / "build-and-package.py"
    if not target.is_file():
        print(f"Error: Script not found: {target}", file=sys.stderr)
        return 1
    return subprocess.call([sys.executable, str(target), *sys.argv[1:]])


if __name__ == "__main__":
    raise SystemExit(main())
