#!/bin/bash
# Build script for Atom project using xmake
# Author: Max Qian

echo "==============================================="
echo "Atom Project Build Script"
echo "==============================================="

# Check if xmake is installed
if ! command -v xmake &> /dev/null; then
    echo "Error: xmake not found in PATH"
    echo "Please install xmake from https://xmake.io/"
    exit 1
fi

echo "Configuring Atom project..."

# Parse command-line options
BUILD_TYPE="release"
BUILD_PYTHON="n"
BUILD_SHARED="n"
BUILD_EXAMPLES="n"
BUILD_TESTS="n"
BUILD_CFITSIO="n"
BUILD_SSH="n"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="debug"
            shift
            ;;
        --python)
            BUILD_PYTHON="y"
            shift
            ;;
        --shared)
            BUILD_SHARED="y"
            shift
            ;;
        --examples)
            BUILD_EXAMPLES="y"
            shift
            ;;
        --tests)
            BUILD_TESTS="y"
            shift
            ;;
        --cfitsio)
            BUILD_CFITSIO="y"
            shift
            ;;
        --ssh)
            BUILD_SSH="y"
            shift
            ;;
        --help)
            echo "Usage: ./build.sh [options]"
            echo "Options:"
            echo "  --debug       Build in debug mode"
            echo "  --python      Build with Python bindings"
            echo "  --shared      Build shared libraries instead of static"
            echo "  --examples    Build examples"
            echo "  --tests       Build tests"
            echo "  --cfitsio     Build with CFITSIO support"
            echo "  --ssh         Build with SSH support"
            echo "  --help        Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "Configuration:"
echo "  Build type: $BUILD_TYPE"
echo "  Python bindings: $BUILD_PYTHON"
echo "  Shared libraries: $BUILD_SHARED"
echo "  Examples: $BUILD_EXAMPLES"
echo "  Tests: $BUILD_TESTS"
echo "  CFITSIO support: $BUILD_CFITSIO"
echo "  SSH support: $BUILD_SSH"

# Configure build
xmake config -m $BUILD_TYPE --build_python=$BUILD_PYTHON --shared_libs=$BUILD_SHARED \
    --build_examples=$BUILD_EXAMPLES --build_tests=$BUILD_TESTS \
    --enable_ssh=$BUILD_SSH

if [ $? -ne 0 ]; then
    echo "Configuration failed!"
    exit $?
fi

echo "==============================================="
echo "Building Atom..."
echo "==============================================="

xmake build -v

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit $?
fi

echo "==============================================="
echo "Build completed successfully!"
echo "==============================================="

echo "To install, run: xmake install"
echo "To run tests (if built), run: xmake run -g test"
