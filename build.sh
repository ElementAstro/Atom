#!/bin/bash
# Wrapper script for backward compatibility
# This script has been moved to scripts/build.sh
# This wrapper maintains backward compatibility for existing workflows

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check if the actual build script exists
ACTUAL_SCRIPT="$SCRIPT_DIR/scripts/build.sh"

if [[ ! -f "$ACTUAL_SCRIPT" ]]; then
    echo "Error: Build script not found at $ACTUAL_SCRIPT"
    echo "Please ensure the scripts directory contains build.sh"
    exit 1
fi

# Make sure the actual script is executable
chmod +x "$ACTUAL_SCRIPT"

# Forward all arguments to the actual build script
exec "$ACTUAL_SCRIPT" "$@"
