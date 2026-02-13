#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="$SCRIPT_DIR/package/create-distribution.sh"

if [[ ! -f "$TARGET" ]]; then
    echo "Error: Script not found: $TARGET" >&2
    exit 1
fi

exec bash "$TARGET" "$@"
