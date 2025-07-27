#!/bin/bash

# Build test script for BIOS module
# This script tests the compilation of the BIOS module

set -e  # Exit on any error

echo "BIOS Module Build Test"
echo "====================="

# Create build directory
BUILD_DIR="build_test"
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring CMake..."
cmake .. -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON

echo "Building BIOS module..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "Build completed successfully!"

# Check if executables were created
if [ -f "examples/bios_example" ]; then
    echo "✓ Example executable created"
else
    echo "⚠ Example executable not found"
fi

if [ -f "tests/test_bios_basic" ]; then
    echo "✓ Test executable created"
else
    echo "⚠ Test executable not found"
fi

echo ""
echo "To run the example:"
echo "  cd $BUILD_DIR && ./examples/bios_example"
echo ""
echo "To run the tests:"
echo "  cd $BUILD_DIR && ./tests/test_bios_basic"
echo "  or: cd $BUILD_DIR && ctest"

cd ..
echo "Build test completed!"
