#!/bin/bash
# Build script for Atom project using xmake or CMake
# Author: Max Qian

echo "==============================================="
echo "Atom Project Build Script"
echo "==============================================="

# Parse command-line options
BUILD_TYPE="release"
BUILD_PYTHON="n"
BUILD_SHARED="n"
BUILD_EXAMPLES="n"
BUILD_TESTS="n"
BUILD_CFITSIO="n"
BUILD_SSH="n"
BUILD_SYSTEM="cmake"
CLEAN_BUILD="n"
SHOW_HELP="n"

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
        --xmake)
            BUILD_SYSTEM="xmake"
            shift
            ;;
        --cmake)
            BUILD_SYSTEM="cmake"
            shift
            ;;
        --clean)
            CLEAN_BUILD="y"
            shift
            ;;
        --help)
            SHOW_HELP="y"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            SHOW_HELP="y"
            shift
            ;;
    esac
done

# Show help if requested
if [[ "$SHOW_HELP" == "y" ]]; then
    echo "Usage: ./build.sh [options]"
    echo ""
    echo "Options:"
    echo "  --debug        Build in debug mode"
    echo "  --python       Enable Python bindings"
    echo "  --shared       Build shared libraries"
    echo "  --examples     Build examples"
    echo "  --tests        Build tests"
    echo "  --cfitsio      Enable CFITSIO support"
    echo "  --ssh          Enable SSH support"
    echo "  --xmake        Use XMake build system"
    echo "  --cmake        Use CMake build system (default)"
    echo "  --clean        Clean build directory before building"
    echo "  --help         Show this help message"
    echo ""
    exit 0
fi

echo "Build configuration:"
echo "  Build type: $BUILD_TYPE"
echo "  Python bindings: $BUILD_PYTHON"
echo "  Shared libraries: $BUILD_SHARED"
echo "  Build examples: $BUILD_EXAMPLES"
echo "  Build tests: $BUILD_TESTS"
echo "  CFITSIO support: $BUILD_CFITSIO"
echo "  SSH support: $BUILD_SSH"
echo "  Build system: $BUILD_SYSTEM"
echo "  Clean build: $CLEAN_BUILD"
echo ""

# Check if the selected build system is available
if [[ "$BUILD_SYSTEM" == "xmake" ]]; then
    if ! command -v xmake &> /dev/null; then
        echo "Error: xmake not found in PATH"
        echo "Please install xmake from https://xmake.io/"
        exit 1
    fi
else
    if ! command -v cmake &> /dev/null; then
        echo "Error: cmake not found in PATH"
        echo "Please install CMake from https://cmake.org/download/"
        exit 1
    fi
fi

# Clean build directory if requested
if [[ "$CLEAN_BUILD" == "y" ]]; then
    echo "Cleaning build directory..."
    rm -rf build
    mkdir -p build
fi

# Build using the selected system
if [[ "$BUILD_SYSTEM" == "xmake" ]]; then
    echo "Building with XMake..."
    
    # Configure XMake options
    XMAKE_ARGS=""
    if [[ "$BUILD_TYPE" == "debug" ]]; then XMAKE_ARGS="$XMAKE_ARGS -m debug"; fi
    if [[ "$BUILD_PYTHON" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --python=y"; fi
    if [[ "$BUILD_SHARED" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --shared=y"; fi
    if [[ "$BUILD_EXAMPLES" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --examples=y"; fi
    if [[ "$BUILD_TESTS" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --tests=y"; fi
    if [[ "$BUILD_CFITSIO" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --cfitsio=y"; fi
    if [[ "$BUILD_SSH" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --ssh=y"; fi
    
    # Run XMake
    echo "Configuring XMake project..."
    xmake f $XMAKE_ARGS
    if [ $? -ne 0 ]; then
        echo "Error: XMake configuration failed"
        exit 1
    fi
    
    echo "Building project..."
    xmake
    if [ $? -ne 0 ]; then
        echo "Error: XMake build failed"
        exit 1
    fi
else
    echo "Building with CMake..."
    
    # Configure CMake options
    CMAKE_ARGS="-B build"
    if [[ "$BUILD_TYPE" == "debug" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Debug"; fi
    if [[ "$BUILD_TYPE" == "release" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release"; fi
    if [[ "$BUILD_PYTHON" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_BUILD_PYTHON_BINDINGS=ON"; fi
    if [[ "$BUILD_SHARED" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=ON"; fi
    if [[ "$BUILD_EXAMPLES" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_BUILD_EXAMPLES=ON"; fi
    if [[ "$BUILD_TESTS" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_BUILD_TESTS=ON"; fi
    if [[ "$BUILD_CFITSIO" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_USE_CFITSIO=ON"; fi
    if [[ "$BUILD_SSH" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_USE_SSH=ON"; fi
    
    # Run CMake
    echo "Configuring CMake project..."
    cmake $CMAKE_ARGS .
    if [ $? -ne 0 ]; then
        echo "Error: CMake configuration failed"
        exit 1
    fi
    
    # Determine number of CPU cores for parallel build
    if command -v nproc &> /dev/null; then
        CORES=$(nproc)
    elif command -v sysctl &> /dev/null && [[ "$(uname)" == "Darwin" ]]; then
        CORES=$(sysctl -n hw.ncpu)
    else
        CORES=4  # Default to 4 cores if we can't determine
    fi
    
    echo "Building project using $CORES cores..."
    cmake --build build --config $BUILD_TYPE --parallel $CORES
    if [ $? -ne 0 ]; then
        echo "Error: CMake build failed"
        exit 1
    fi
fi

echo ""
echo "Build completed successfully!"
echo "==============================================="
