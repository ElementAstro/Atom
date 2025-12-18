#!/usr/bin/env python3
import argparse
import hashlib
import os
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Tuple


def _human_size(num_bytes: int) -> str:
    step = 1024.0
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    size = float(num_bytes)
    for unit in units:
        if size < step:
            return f"{size:.1f} {unit}" if unit != "B" else f"{int(size)} {unit}"
        size /= step
    return f"{size:.1f} PiB"


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def _iter_files(root: Path) -> List[Path]:
    if not root.exists():
        return []
    if not root.is_dir():
        return []

    files: List[Path] = [p for p in root.rglob("*") if p.is_file()]
    files.sort(key=lambda p: str(p.relative_to(root)).lower())
    return files


def _detect_type(filename: str) -> str:
    name = filename.lower()
    if name.endswith(".tar.gz"):
        return "tar.gz"
    if name.endswith(".tar.bz2"):
        return "tar.bz2"
    if name.endswith(".tar.xz"):
        return "tar.xz"
    if name.endswith(".tgz"):
        return "tgz"
    suffix = Path(name).suffix
    return suffix[1:] if suffix.startswith(".") else (suffix or "unknown")


def generate_report(artifacts_dir: Path) -> str:
    now = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")

    repo = os.environ.get("GITHUB_REPOSITORY", "")
    ref_name = os.environ.get("GITHUB_REF_NAME", "")
    sha = os.environ.get("GITHUB_SHA", "")
    run_id = os.environ.get("GITHUB_RUN_ID", "")
    server_url = os.environ.get("GITHUB_SERVER_URL", "")

    files = _iter_files(artifacts_dir)

    total_size = 0
    type_counts: Dict[str, int] = defaultdict(int)
    type_sizes: Dict[str, int] = defaultdict(int)

    rows: List[Tuple[str, int, str]] = []
    for file_path in files:
        rel = str(file_path.relative_to(artifacts_dir))
        size = file_path.stat().st_size
        total_size += size

        t = _detect_type(rel)
        type_counts[t] += 1
        type_sizes[t] += size

        checksum = _sha256(file_path)
        rows.append((rel, size, checksum))

    lines: List[str] = []
    lines.append("# Release Report")
    lines.append("")
    lines.append(f"Generated on: {now}")

    if repo:
        lines.append(f"Repository: {repo}")
    if ref_name:
        lines.append(f"Ref: {ref_name}")
    if sha:
        lines.append(f"Commit: {sha}")
    if run_id and server_url and repo:
        lines.append(f"Workflow run: {server_url}/{repo}/actions/runs/{run_id}")

    lines.append("")

    if not files:
        lines.append("## Artifacts")
        lines.append("")
        lines.append(f"No artifact files found under `{artifacts_dir}`.")
        lines.append("")
        return "\n".join(lines) + "\n"

    lines.append("## Artifact Summary")
    lines.append("")
    lines.append(f"Total files: {len(files)}")
    lines.append(f"Total size: {_human_size(total_size)}")
    lines.append("")

    lines.append("### By type")
    lines.append("")
    lines.append("| Type | Count | Total size |")
    lines.append("| --- | ---: | ---: |")
    for t in sorted(type_counts.keys()):
        lines.append(f"| {t} | {type_counts[t]} | {_human_size(type_sizes[t])} |")
    lines.append("")

    lines.append("## Files")
    lines.append("")
    lines.append("| File | Size | SHA256 |")
    lines.append("| --- | ---: | --- |")
    for rel, size, checksum in rows:
        lines.append(f"| `{rel}` | {_human_size(size)} | `{checksum}` |")

    lines.append("")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts-dir", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    artifacts_dir = Path(args.artifacts_dir).resolve()
    output_path = Path(args.output).resolve()

    report = generate_report(artifacts_dir)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(report, encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
