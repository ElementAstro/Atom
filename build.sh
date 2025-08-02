#!/bin/bash
# Build script for Atom project using xmake or CMake
# Author: Max Qian
# Enhanced build system with better error handling and optimization

set -euo pipefail  # Exit on error, undefined vars, pipe failures

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $*"; }

# Error handler
error_exit() {
    log_error "$1"
    exit 1
}

echo "==============================================="
echo "Atom Project Enhanced Build Script"
echo "==============================================="

# Parse command-line options with enhanced defaults
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
BUILD_DOCS="n"
BUILD_BENCHMARKS="n"
ENABLE_LTO="n"
ENABLE_COVERAGE="n"
ENABLE_SANITIZERS="n"
PARALLEL_JOBS=""
INSTALL_PREFIX=""
CCACHE_ENABLE="auto"
VERBOSE_BUILD="n"

# Parse arguments with enhanced options
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="debug"
            shift
            ;;
        --release)
            BUILD_TYPE="release"
            shift
            ;;
        --relwithdebinfo)
            BUILD_TYPE="relwithdebinfo"
            shift
            ;;
        --minsizerel)
            BUILD_TYPE="minsizerel"
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
        --docs)
            BUILD_DOCS="y"
            shift
            ;;
        --benchmarks)
            BUILD_BENCHMARKS="y"
            shift
            ;;
        --lto)
            ENABLE_LTO="y"
            shift
            ;;
        --coverage)
            ENABLE_COVERAGE="y"
            shift
            ;;
        --sanitizers)
            ENABLE_SANITIZERS="y"
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
        --ccache)
            CCACHE_ENABLE="y"
            shift
            ;;
        --no-ccache)
            CCACHE_ENABLE="n"
            shift
            ;;
        --verbose)
            VERBOSE_BUILD="y"
            shift
            ;;
        -j|--parallel)
            if [[ -n "${2:-}" && "${2:-}" =~ ^[0-9]+$ ]]; then
                PARALLEL_JOBS="$2"
                shift 2
            else
                error_exit "Option $1 requires a numeric argument"
            fi
            ;;
        --prefix)
            if [[ -n "${2:-}" ]]; then
                INSTALL_PREFIX="$2"
                shift 2
            else
                error_exit "Option $1 requires an argument"
            fi
            ;;
        --help|-h)
            SHOW_HELP="y"
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            SHOW_HELP="y"
            shift
            ;;
    esac
done

# Show enhanced help if requested
if [[ "$SHOW_HELP" == "y" ]]; then
    echo "Usage: ./build.sh [options]"
    echo ""
    echo "Build Type Options:"
    echo "  --debug            Build in debug mode"
    echo "  --release          Build in release mode (default)"
    echo "  --relwithdebinfo   Build with release optimizations + debug info"
    echo "  --minsizerel       Build optimized for minimum size"
    echo ""
    echo "Feature Options:"
    echo "  --python           Enable Python bindings"
    echo "  --shared           Build shared libraries"
    echo "  --examples         Build examples"
    echo "  --tests            Build tests"
    echo "  --docs             Build documentation"
    echo "  --benchmarks       Build benchmarks"
    echo "  --cfitsio          Enable CFITSIO support"
    echo "  --ssh              Enable SSH support"
    echo ""
    echo "Optimization Options:"
    echo "  --lto              Enable Link Time Optimization"
    echo "  --coverage         Enable code coverage analysis"
    echo "  --sanitizers       Enable AddressSanitizer and UBSan"
    echo ""
    echo "Build System Options:"
    echo "  --xmake            Use XMake build system"
    echo "  --cmake            Use CMake build system (default)"
    echo "  --ccache           Force enable ccache"
    echo "  --no-ccache        Force disable ccache"
    echo ""
    echo "General Options:"
    echo "  --clean            Clean build directory before building"
    echo "  --verbose          Enable verbose build output"
    echo "  -j, --parallel N   Set number of parallel jobs"
    echo "  --prefix PATH      Set installation prefix"
    echo "  -h, --help         Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  CC                 C compiler to use"
    echo "  CXX                C++ compiler to use"
    echo "  VCPKG_ROOT         Path to vcpkg installation"
    echo ""
    exit 0
fi

# Enhanced system capability detection
detect_system_capabilities() {
    log_info "Detecting system capabilities..."

    # Detect operating system
    OS_TYPE=$(uname -s)
    case "$OS_TYPE" in
        Linux*)     OS_NAME="Linux";;
        Darwin*)    OS_NAME="macOS";;
        CYGWIN*)    OS_NAME="Windows";;
        MINGW*)     OS_NAME="Windows";;
        MSYS*)      OS_NAME="Windows";;
        *)          OS_NAME="Unknown";;
    esac

    # Detect architecture
    ARCH=$(uname -m)
    case "$ARCH" in
        x86_64|amd64)   ARCH_NAME="x64";;
        i386|i686)      ARCH_NAME="x86";;
        aarch64|arm64)  ARCH_NAME="arm64";;
        armv7l)         ARCH_NAME="arm";;
        *)              ARCH_NAME="unknown";;
    esac

    log_info "Detected platform: $OS_NAME ($ARCH_NAME)"

    # Detect number of CPU cores if not specified
    if [[ -z "$PARALLEL_JOBS" ]]; then
        if command -v nproc &> /dev/null; then
            PARALLEL_JOBS=$(nproc)
        elif command -v sysctl &> /dev/null && [[ "$OS_NAME" == "macOS" ]]; then
            PARALLEL_JOBS=$(sysctl -n hw.ncpu)
        elif [[ "$OS_NAME" == "Windows" ]] && command -v wmic &> /dev/null; then
            PARALLEL_JOBS=$(wmic cpu get NumberOfCores /value | grep -o '[0-9]*' | head -1)
        else
            PARALLEL_JOBS=4  # Default to 4 cores
        fi
    fi

    # Auto-detect compiler cache tools
    if [[ "$CCACHE_ENABLE" == "auto" ]]; then
        if command -v ccache &> /dev/null; then
            CCACHE_ENABLE="y"
            CACHE_TOOL="ccache"
            log_info "ccache detected and will be used"
        elif command -v sccache &> /dev/null; then
            CCACHE_ENABLE="y"
            CACHE_TOOL="sccache"
            log_info "sccache detected and will be used"
        else
            CCACHE_ENABLE="n"
            log_warn "No compiler cache found (ccache/sccache), compilation caching disabled"
        fi
    fi

    # Enhanced memory detection
    local available_memory_gb=0
    local total_memory_gb=0

    if [[ "$OS_NAME" == "Linux" ]] && [[ -f /proc/meminfo ]]; then
        available_memory_gb=$(awk '/MemAvailable/{printf "%.0f", $2/1024/1024}' /proc/meminfo)
        total_memory_gb=$(awk '/MemTotal/{printf "%.0f", $2/1024/1024}' /proc/meminfo)
    elif [[ "$OS_NAME" == "macOS" ]] && command -v vm_stat &> /dev/null; then
        # macOS memory detection
        local page_size=$(vm_stat | head -1 | awk '{print $8}')
        local free_pages=$(vm_stat | grep "Pages free" | awk '{print $3}' | sed 's/\.//')
        local inactive_pages=$(vm_stat | grep "Pages inactive" | awk '{print $3}' | sed 's/\.//')
        available_memory_gb=$(((free_pages + inactive_pages) * page_size / 1024 / 1024 / 1024))

        # Get total memory
        total_memory_gb=$(sysctl -n hw.memsize | awk '{printf "%.0f", $1/1024/1024/1024}')
    elif [[ "$OS_NAME" == "Windows" ]] && command -v wmic &> /dev/null; then
        # Windows memory detection
        total_memory_gb=$(wmic computersystem get TotalPhysicalMemory /value | grep -o '[0-9]*' | awk '{printf "%.0f", $1/1024/1024/1024}')
        available_memory_gb=$((total_memory_gb * 80 / 100))  # Estimate 80% available
    else
        # Fallback defaults
        total_memory_gb=8
        available_memory_gb=6
    fi

    log_info "Memory: ${available_memory_gb}GB available / ${total_memory_gb}GB total"

    # Intelligent parallel job calculation
    local memory_per_job=2  # Base memory per job in GB

    # Adjust based on build type and architecture
    if [[ "$BUILD_TYPE" == "debug" ]]; then
        memory_per_job=1.5  # Debug builds use less memory
    elif [[ "$BUILD_TYPE" == "release" ]]; then
        memory_per_job=2.5  # Release builds with optimizations use more
    fi

    # Adjust for architecture
    if [[ "$ARCH_NAME" == "arm64" ]] || [[ "$ARCH_NAME" == "arm" ]]; then
        memory_per_job=$(echo "$memory_per_job * 0.8" | bc -l 2>/dev/null || echo "1.6")
    fi

    local memory_limited_jobs=$((available_memory_gb * 10 / (memory_per_job * 10)))

    # Platform-specific CPU job limits
    local cpu_limited_jobs=$PARALLEL_JOBS
    case "$OS_NAME" in
        "Linux")
            # Linux handles parallel builds well
            if [[ $PARALLEL_JOBS -gt 16 ]]; then
                cpu_limited_jobs=$((PARALLEL_JOBS - 2))
            fi
            ;;
        "macOS")
            # macOS can be more memory constrained
            cpu_limited_jobs=$((PARALLEL_JOBS > 1 ? PARALLEL_JOBS - 1 : 1))
            ;;
        "Windows")
            # Windows - be conservative
            cpu_limited_jobs=$((PARALLEL_JOBS > 12 ? 12 : PARALLEL_JOBS))
            ;;
    esac

    # Use the more conservative limit
    if [[ $memory_limited_jobs -lt $cpu_limited_jobs ]]; then
        PARALLEL_JOBS=$memory_limited_jobs
        log_warn "Limiting parallel jobs to $PARALLEL_JOBS due to memory constraints"
    else
        PARALLEL_JOBS=$cpu_limited_jobs
    fi

    # Ensure at least 1 job
    if [[ $PARALLEL_JOBS -lt 1 ]]; then
        PARALLEL_JOBS=1
    fi

    log_info "Optimized for $PARALLEL_JOBS parallel jobs"
}

# Detect system capabilities
detect_system_capabilities

echo "Build configuration:"
echo "  Build type: $BUILD_TYPE"
echo "  Python bindings: $BUILD_PYTHON"
echo "  Shared libraries: $BUILD_SHARED"
echo "  Build examples: $BUILD_EXAMPLES"
echo "  Build tests: $BUILD_TESTS"
echo "  Build documentation: $BUILD_DOCS"
echo "  Build benchmarks: $BUILD_BENCHMARKS"
echo "  CFITSIO support: $BUILD_CFITSIO"
echo "  SSH support: $BUILD_SSH"
echo "  Link Time Optimization: $ENABLE_LTO"
echo "  Code coverage: $ENABLE_COVERAGE"
echo "  Sanitizers: $ENABLE_SANITIZERS"
echo "  Build system: $BUILD_SYSTEM"
echo "  Clean build: $CLEAN_BUILD"
echo "  Parallel jobs: $PARALLEL_JOBS"
echo "  ccache enabled: $CCACHE_ENABLE"
echo "  Verbose build: $VERBOSE_BUILD"
if [[ -n "$INSTALL_PREFIX" ]]; then
    echo "  Install prefix: $INSTALL_PREFIX"
fi
echo ""

# Enhanced build system availability check
check_build_system_availability() {
    if [[ "$BUILD_SYSTEM" == "xmake" ]]; then
        if ! command -v xmake &> /dev/null; then
            error_exit "xmake not found in PATH. Please install xmake from https://xmake.io/"
        fi
        log_info "XMake found: $(xmake --version | head -1)"
    else
        if ! command -v cmake &> /dev/null; then
            error_exit "cmake not found in PATH. Please install CMake from https://cmake.org/download/"
        fi
        local cmake_version=$(cmake --version | head -1 | awk '{print $3}')
        log_info "CMake found: $cmake_version"

        # Check minimum CMake version
        local min_version="3.21"
        if ! printf '%s\n' "$min_version" "$cmake_version" | sort -V | head -1 | grep -q "^$min_version$"; then
            log_warn "CMake version $cmake_version is older than recommended minimum $min_version"
        fi

        # Check for Ninja if available
        if command -v ninja &> /dev/null; then
            log_info "Ninja found: $(ninja --version)"
        fi
    fi
}

# Check build system availability
check_build_system_availability

# Enhanced build directory management
manage_build_directory() {
    if [[ "$CLEAN_BUILD" == "y" ]]; then
        log_info "Cleaning build directory..."
        rm -rf build
        mkdir -p build
    elif [[ ! -d "build" ]]; then
        log_info "Creating build directory..."
        mkdir -p build
    fi

    # Setup ccache if enabled
    if [[ "$CCACHE_ENABLE" == "y" ]]; then
        export CC="ccache ${CC:-gcc}"
        export CXX="ccache ${CXX:-g++}"
        log_info "ccache enabled for compilation"
    fi
}

# Manage build directory
manage_build_directory

# Enhanced build process
if [[ "$BUILD_SYSTEM" == "xmake" ]]; then
    log_info "Building with XMake..."

    # Configure XMake options
    XMAKE_ARGS=""
    if [[ "$BUILD_TYPE" == "debug" ]]; then XMAKE_ARGS="$XMAKE_ARGS -m debug"; fi
    if [[ "$BUILD_TYPE" == "release" ]]; then XMAKE_ARGS="$XMAKE_ARGS -m release"; fi
    if [[ "$BUILD_PYTHON" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --python=y"; fi
    if [[ "$BUILD_SHARED" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --shared=y"; fi
    if [[ "$BUILD_EXAMPLES" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --examples=y"; fi
    if [[ "$BUILD_TESTS" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --tests=y"; fi
    if [[ "$BUILD_CFITSIO" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --cfitsio=y"; fi
    if [[ "$BUILD_SSH" == "y" ]]; then XMAKE_ARGS="$XMAKE_ARGS --ssh=y"; fi

    # Run XMake
    log_info "Configuring XMake project..."
    if ! xmake f $XMAKE_ARGS; then
        error_exit "XMake configuration failed"
    fi

    log_info "Building project with $PARALLEL_JOBS parallel jobs..."
    XMAKE_BUILD_ARGS="-j $PARALLEL_JOBS"
    if [[ "$VERBOSE_BUILD" == "y" ]]; then
        XMAKE_BUILD_ARGS="$XMAKE_BUILD_ARGS -v"
    fi

    if ! xmake $XMAKE_BUILD_ARGS; then
        error_exit "XMake build failed"
    fi
else
    log_info "Building with CMake..."

    # Configure CMake options
    CMAKE_ARGS="-B build"

    # Build type configuration
    case "$BUILD_TYPE" in
        "debug") CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Debug" ;;
        "release") CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release" ;;
        "relwithdebinfo") CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=RelWithDebInfo" ;;
        "minsizerel") CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=MinSizeRel" ;;
    esac

    # Feature configuration
    if [[ "$BUILD_PYTHON" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_BUILD_PYTHON_BINDINGS=ON"; fi
    if [[ "$BUILD_SHARED" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=ON"; fi
    if [[ "$BUILD_EXAMPLES" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_BUILD_EXAMPLES=ON"; fi
    if [[ "$BUILD_TESTS" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_BUILD_TESTS=ON"; fi
    if [[ "$BUILD_DOCS" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_BUILD_DOCS=ON"; fi
    if [[ "$BUILD_CFITSIO" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_USE_CFITSIO=ON"; fi
    if [[ "$BUILD_SSH" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DATOM_USE_SSH=ON"; fi

    # Optimization configuration
    if [[ "$ENABLE_LTO" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON"; fi
    if [[ "$ENABLE_COVERAGE" == "y" ]]; then CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_FLAGS=--coverage -DCMAKE_C_FLAGS=--coverage"; fi
    if [[ "$ENABLE_SANITIZERS" == "y" ]]; then
        CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -DCMAKE_C_FLAGS=-fsanitize=address,undefined"
    fi

    # Installation prefix
    if [[ -n "$INSTALL_PREFIX" ]]; then
        CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX"
    fi

    # Use Ninja if available
    if command -v ninja &> /dev/null; then
        CMAKE_ARGS="$CMAKE_ARGS -G Ninja"
    fi

    # Export compile commands for IDE support
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"

    # Run CMake configuration
    log_info "Configuring CMake project..."
    if ! cmake $CMAKE_ARGS .; then
        error_exit "CMake configuration failed"
    fi

    # Build configuration
    CMAKE_BUILD_ARGS="--build build --config $BUILD_TYPE --parallel $PARALLEL_JOBS"
    if [[ "$VERBOSE_BUILD" == "y" ]]; then
        CMAKE_BUILD_ARGS="$CMAKE_BUILD_ARGS --verbose"
    fi

    log_info "Building project with $PARALLEL_JOBS parallel jobs..."
    if ! cmake $CMAKE_BUILD_ARGS; then
        error_exit "CMake build failed"
    fi
fi

# Post-build actions
post_build_actions() {
    log_success "Build completed successfully!"

    # Run tests if requested and built
    if [[ "$BUILD_TESTS" == "y" ]]; then
        log_info "Running tests..."
        if [[ "$BUILD_SYSTEM" == "cmake" ]]; then
            cd build && ctest --output-on-failure --parallel $PARALLEL_JOBS && cd ..
        elif [[ "$BUILD_SYSTEM" == "xmake" ]]; then
            xmake test
        fi
    fi

    # Generate documentation if requested
    if [[ "$BUILD_DOCS" == "y" ]]; then
        log_info "Generating documentation..."
        if command -v doxygen &> /dev/null; then
            doxygen Doxyfile 2>/dev/null || log_warn "Documentation generation failed"
        else
            log_warn "Doxygen not found, skipping documentation generation"
        fi
    fi

    # Show build summary
    echo ""
    echo "==============================================="
    echo "Build Summary"
    echo "==============================================="
    echo "Build system: $BUILD_SYSTEM"
    echo "Build type: $BUILD_TYPE"
    echo "Build time: $((SECONDS/60))m $((SECONDS%60))s"
    echo "Parallel jobs used: $PARALLEL_JOBS"

    if [[ -d "build" ]]; then
        local build_size=$(du -sh build 2>/dev/null | cut -f1)
        echo "Build directory size: $build_size"
    fi

    # Show important artifacts
    echo ""
    echo "Built artifacts:"
    if [[ "$BUILD_SYSTEM" == "cmake" ]]; then
        find build -name "*.so" -o -name "*.a" -o -name "*.dll" -o -name "*.exe" | head -10
    fi

    # Installation instructions
    if [[ "$BUILD_SYSTEM" == "cmake" ]]; then
        echo ""
        echo "To install, run:"
        echo "  cmake --build build --target install"
    fi
}

# Record start time
SECONDS=0

# Run post-build actions
post_build_actions

echo "==============================================="
