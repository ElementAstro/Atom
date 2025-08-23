#!/bin/bash
# Enhanced Build script for Atom project using xmake or CMake
# Author: Max Qian
# Enhanced with comprehensive build management, dependency handling, and distribution

set -euo pipefail  # Exit on error, undefined vars, pipe failures

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build"
DIST_DIR="$PROJECT_ROOT/dist"
LOG_DIR="$PROJECT_ROOT/logs"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$LOG_DIR/build_${TIMESTAMP}.log"

# Ensure log directory exists
mkdir -p "$LOG_DIR"

# Logging functions
log_info() { echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') $*" | tee -a "$LOG_FILE"; }
log_warn() { echo "[WARN] $(date '+%Y-%m-%d %H:%M:%S') $*" | tee -a "$LOG_FILE"; }
log_error() { echo "[ERROR] $(date '+%Y-%m-%d %H:%M:%S') $*" | tee -a "$LOG_FILE" >&2; }

echo "==============================================="
echo "Atom Project Enhanced Build Script"
echo "==============================================="
log_info "Build script started"
log_info "Project root: $PROJECT_ROOT"
log_info "Build log: $LOG_FILE"

# Parse command-line options with enhanced functionality
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
INSTALL_DEPS="n"
CREATE_PACKAGE="n"
RUN_TESTS="n"
GENERATE_DOCS="n"
PARALLEL_JOBS=""
INSTALL_PREFIX=""
VERBOSE="n"
DRY_RUN="n"

# Enhanced argument parsing with comprehensive options
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
        --install-deps)
            INSTALL_DEPS="y"
            shift
            ;;
        --package)
            CREATE_PACKAGE="y"
            shift
            ;;
        --run-tests)
            RUN_TESTS="y"
            BUILD_TESTS="y"  # Automatically enable test building
            shift
            ;;
        --docs)
            GENERATE_DOCS="y"
            shift
            ;;
        --jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE="y"
            shift
            ;;
        --dry-run)
            DRY_RUN="y"
            shift
            ;;
        --help)
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

# Enhanced help system
if [[ "$SHOW_HELP" == "y" ]]; then
    cat << 'EOF'
Usage: ./build.sh [options]

Enhanced Build Options:
  Build Types:
    --debug              Build in debug mode with debug symbols
    --release            Build in release mode (default)
    --relwithdebinfo     Build in release mode with debug info

  Features:
    --python             Enable Python bindings
    --shared             Build shared libraries instead of static
    --examples           Build example applications
    --tests              Build test suite
    --cfitsio            Enable CFITSIO support for astronomical data
    --ssh                Enable SSH support for remote connections

  Build Systems:
    --cmake              Use CMake build system (default)
    --xmake              Use XMake build system

  Build Management:
    --clean              Clean build directory before building
    --install-deps       Install system dependencies automatically
    --package            Create distribution packages after build
    --run-tests          Build and run test suite
    --docs               Generate documentation with Doxygen

  Performance & Control:
    --jobs N             Use N parallel jobs for building
    --prefix PATH        Set installation prefix (default: /usr/local)
    --verbose            Enable verbose output
    --dry-run            Show what would be done without executing

  Information:
    --help               Show this help message

Examples:
  ./build.sh --debug --tests --run-tests
  ./build.sh --release --python --package --jobs 8
  ./build.sh --clean --install-deps --examples --docs
  ./build.sh --xmake --shared --prefix /opt/atom

EOF
    exit 0
fi

# System detection and utility functions
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

detect_package_manager() {
    if command -v apt-get &> /dev/null; then
        echo "apt"
    elif command -v yum &> /dev/null; then
        echo "yum"
    elif command -v dnf &> /dev/null; then
        echo "dnf"
    elif command -v pacman &> /dev/null; then
        echo "pacman"
    elif command -v brew &> /dev/null; then
        echo "brew"
    else
        echo "none"
    fi
}

get_cpu_count() {
    if command -v nproc &> /dev/null; then
        nproc
    elif command -v sysctl &> /dev/null && [[ "$(detect_os)" == "macos" ]]; then
        sysctl -n hw.ncpu
    else
        echo "4"  # Default fallback
    fi
}

# Dependency installation functions
install_system_dependencies() {
    local os=$(detect_os)
    local pkg_mgr=$(detect_package_manager)

    log_info "Installing system dependencies for $os using $pkg_mgr"

    case "$pkg_mgr" in
        apt)
            sudo apt-get update
            sudo apt-get install -y \
                build-essential cmake ninja-build \
                libssl-dev zlib1g-dev libsqlite3-dev \
                libfmt-dev libreadline-dev \
                python3-dev python3-pip \
                doxygen graphviz \
                pkg-config git curl
            ;;
        yum|dnf)
            sudo $pkg_mgr install -y \
                gcc-c++ cmake ninja-build \
                openssl-devel zlib-devel sqlite-devel \
                fmt-devel readline-devel \
                python3-devel python3-pip \
                doxygen graphviz \
                pkgconfig git curl
            ;;
        pacman)
            sudo pacman -S --noconfirm \
                base-devel cmake ninja \
                openssl zlib sqlite \
                fmt readline \
                python python-pip \
                doxygen graphviz \
                pkgconf git curl
            ;;
        brew)
            brew install \
                cmake ninja \
                openssl zlib sqlite3 \
                fmt readline \
                python3 \
                doxygen graphviz \
                pkg-config git curl
            ;;
        *)
            log_warn "Unknown package manager. Please install dependencies manually."
            log_info "Required packages: cmake, ninja, openssl, zlib, sqlite3, fmt, readline, python3, doxygen"
            ;;
    esac
}

# Configuration display
log_info "Build configuration:"
log_info "  Build type: $BUILD_TYPE"
log_info "  Python bindings: $BUILD_PYTHON"
log_info "  Shared libraries: $BUILD_SHARED"
log_info "  Build examples: $BUILD_EXAMPLES"
log_info "  Build tests: $BUILD_TESTS"
log_info "  CFITSIO support: $BUILD_CFITSIO"
log_info "  SSH support: $BUILD_SSH"
log_info "  Build system: $BUILD_SYSTEM"
log_info "  Clean build: $CLEAN_BUILD"
log_info "  Install dependencies: $INSTALL_DEPS"
log_info "  Create package: $CREATE_PACKAGE"
log_info "  Run tests: $RUN_TESTS"
log_info "  Generate docs: $GENERATE_DOCS"
log_info "  Parallel jobs: ${PARALLEL_JOBS:-auto}"
log_info "  Install prefix: ${INSTALL_PREFIX:-default}"
log_info "  Verbose: $VERBOSE"
log_info "  Dry run: $DRY_RUN"
echo ""

# Install system dependencies if requested
if [[ "$INSTALL_DEPS" == "y" ]]; then
    if [[ "$DRY_RUN" == "y" ]]; then
        log_info "[DRY RUN] Would install system dependencies"
    else
        install_system_dependencies
    fi
fi

# Enhanced build system availability check
check_build_system() {
    local system=$1
    case "$system" in
        xmake)
            if ! command -v xmake &> /dev/null; then
                log_error "xmake not found in PATH"
                log_info "Install xmake from https://xmake.io/"
                if [[ "$INSTALL_DEPS" == "y" ]]; then
                    log_info "Attempting to install xmake..."
                    if [[ "$(detect_os)" == "linux" ]]; then
                        curl -fsSL https://xmake.io/shget.text | bash
                    elif [[ "$(detect_os)" == "macos" ]]; then
                        brew install xmake
                    else
                        log_error "Automatic xmake installation not supported on this platform"
                        return 1
                    fi
                else
                    return 1
                fi
            fi
            ;;
        cmake)
            if ! command -v cmake &> /dev/null; then
                log_error "cmake not found in PATH"
                log_info "Install CMake from https://cmake.org/download/"
                return 1
            fi
            # Check CMake version
            local cmake_version=$(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
            local required_version="3.21"
            if ! printf '%s\n%s\n' "$required_version" "$cmake_version" | sort -V -C; then
                log_error "CMake version $cmake_version is too old. Required: $required_version or newer"
                return 1
            fi
            log_info "Using CMake version: $cmake_version"
            ;;
    esac
    return 0
}

if ! check_build_system "$BUILD_SYSTEM"; then
    exit 1
fi

# Version management functions
get_git_version() {
    if command -v git &> /dev/null && git rev-parse --git-dir > /dev/null 2>&1; then
        local version=$(git describe --tags --always --dirty 2>/dev/null || echo "unknown")
        local commit=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
        local branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
        echo "${version}-${commit}-${branch}"
    else
        echo "0.1.0-unknown-unknown"
    fi
}

# Enhanced build directory management
setup_build_environment() {
    local build_variant="${BUILD_SYSTEM}-${BUILD_TYPE}"
    BUILD_DIR="$PROJECT_ROOT/build/$build_variant"

    if [[ "$CLEAN_BUILD" == "y" ]]; then
        log_info "Cleaning build directory: $BUILD_DIR"
        if [[ "$DRY_RUN" == "y" ]]; then
            log_info "[DRY RUN] Would remove $BUILD_DIR"
        else
            rm -rf "$BUILD_DIR"
        fi
    fi

    log_info "Setting up build directory: $BUILD_DIR"
    if [[ "$DRY_RUN" != "y" ]]; then
        mkdir -p "$BUILD_DIR"
        mkdir -p "$DIST_DIR"
    fi

    # Set parallel jobs
    if [[ -z "$PARALLEL_JOBS" ]]; then
        PARALLEL_JOBS=$(get_cpu_count)
        log_info "Auto-detected $PARALLEL_JOBS CPU cores for parallel building"
    fi

    # Set install prefix
    if [[ -z "$INSTALL_PREFIX" ]]; then
        if [[ "$(detect_os)" == "macos" ]]; then
            INSTALL_PREFIX="/usr/local"
        else
            INSTALL_PREFIX="/usr/local"
        fi
    fi

    log_info "Build environment configured:"
    log_info "  Build directory: $BUILD_DIR"
    log_info "  Distribution directory: $DIST_DIR"
    log_info "  Parallel jobs: $PARALLEL_JOBS"
    log_info "  Install prefix: $INSTALL_PREFIX"
    log_info "  Git version: $(get_git_version)"
}

setup_build_environment

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
    log_info "Building with CMake..."

    # Enhanced CMake configuration
    build_with_cmake() {
        local cmake_args=()

        # Basic configuration
        cmake_args+=("-B" "$BUILD_DIR")
        cmake_args+=("-S" "$PROJECT_ROOT")

        # Build type configuration
        case "$BUILD_TYPE" in
            debug)
                cmake_args+=("-DCMAKE_BUILD_TYPE=Debug")
                ;;
            release)
                cmake_args+=("-DCMAKE_BUILD_TYPE=Release")
                ;;
            relwithdebinfo)
                cmake_args+=("-DCMAKE_BUILD_TYPE=RelWithDebInfo")
                ;;
        esac

        # Feature configuration
        [[ "$BUILD_PYTHON" == "y" ]] && cmake_args+=("-DATOM_BUILD_PYTHON_BINDINGS=ON")
        [[ "$BUILD_SHARED" == "y" ]] && cmake_args+=("-DBUILD_SHARED_LIBS=ON")
        [[ "$BUILD_EXAMPLES" == "y" ]] && cmake_args+=("-DATOM_BUILD_EXAMPLES=ON")
        [[ "$BUILD_TESTS" == "y" ]] && cmake_args+=("-DATOM_BUILD_TESTS=ON")
        [[ "$BUILD_CFITSIO" == "y" ]] && cmake_args+=("-DATOM_USE_CFITSIO=ON")
        [[ "$BUILD_SSH" == "y" ]] && cmake_args+=("-DATOM_USE_SSH=ON")
        [[ "$GENERATE_DOCS" == "y" ]] && cmake_args+=("-DATOM_BUILD_DOCS=ON")

        # Installation and packaging
        cmake_args+=("-DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX")
        cmake_args+=("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

        # Optimization and debugging
        if [[ "$VERBOSE" == "y" ]]; then
            cmake_args+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
        fi

        # Generator selection (prefer Ninja if available)
        if command -v ninja &> /dev/null; then
            cmake_args+=("-G" "Ninja")
            log_info "Using Ninja generator for faster builds"
        fi

        # Version information
        local git_version=$(get_git_version)
        cmake_args+=("-DATOM_VERSION_OVERRIDE=$git_version")

        log_info "CMake configuration command:"
        log_info "cmake ${cmake_args[*]}"

        if [[ "$DRY_RUN" == "y" ]]; then
            log_info "[DRY RUN] Would configure CMake project"
            return 0
        fi

        # Configure project
        log_info "Configuring CMake project..."
        if ! cmake "${cmake_args[@]}"; then
            log_error "CMake configuration failed"
            return 1
        fi

        # Build project
        local build_args=("--build" "$BUILD_DIR")
        build_args+=("--config" "$BUILD_TYPE")
        build_args+=("--parallel" "$PARALLEL_JOBS")

        if [[ "$VERBOSE" == "y" ]]; then
            build_args+=("--verbose")
        fi

        log_info "Building project with $PARALLEL_JOBS parallel jobs..."
        log_info "cmake ${build_args[*]}"

        if ! cmake "${build_args[@]}"; then
            log_error "CMake build failed"
            return 1
        fi

        return 0
    }

    if ! build_with_cmake; then
        exit 1
    fi
fi

# Post-build operations
post_build_operations() {
    log_info "Performing post-build operations..."

    # Run tests if requested
    if [[ "$RUN_TESTS" == "y" ]]; then
        log_info "Running test suite..."
        if [[ "$DRY_RUN" == "y" ]]; then
            log_info "[DRY RUN] Would run tests"
        else
            if [[ "$BUILD_SYSTEM" == "cmake" ]]; then
                if ! cmake --build "$BUILD_DIR" --target test; then
                    log_warn "Some tests failed, but continuing..."
                fi
            elif [[ "$BUILD_SYSTEM" == "xmake" ]]; then
                if ! xmake test; then
                    log_warn "Some tests failed, but continuing..."
                fi
            fi
        fi
    fi

    # Generate documentation if requested
    if [[ "$GENERATE_DOCS" == "y" ]]; then
        log_info "Generating documentation..."
        if [[ "$DRY_RUN" == "y" ]]; then
            log_info "[DRY RUN] Would generate documentation"
        else
            if [[ "$BUILD_SYSTEM" == "cmake" ]]; then
                cmake --build "$BUILD_DIR" --target doc 2>/dev/null || log_warn "Documentation generation failed or not configured"
            fi
        fi
    fi

    # Create packages if requested
    if [[ "$CREATE_PACKAGE" == "y" ]]; then
        log_info "Creating distribution packages..."
        if [[ "$DRY_RUN" == "y" ]]; then
            log_info "[DRY RUN] Would create packages"
        else
            create_distribution_packages
        fi
    fi
}

# Package creation function
create_distribution_packages() {
    local package_dir="$DIST_DIR/atom-$(get_git_version)"
    local os_name=$(detect_os)
    local arch=$(uname -m)

    log_info "Creating distribution package in $package_dir"
    mkdir -p "$package_dir"

    # Install to temporary directory
    local temp_install="$package_dir/install"
    if [[ "$BUILD_SYSTEM" == "cmake" ]]; then
        cmake --install "$BUILD_DIR" --prefix "$temp_install"
    elif [[ "$BUILD_SYSTEM" == "xmake" ]]; then
        xmake install -o "$temp_install"
    fi

    # Create archive
    local archive_name="atom-$(get_git_version)-${os_name}-${arch}"
    cd "$DIST_DIR"

    if command -v tar &> /dev/null; then
        tar -czf "${archive_name}.tar.gz" -C "$package_dir" install
        log_info "Created package: $DIST_DIR/${archive_name}.tar.gz"
    fi

    if command -v zip &> /dev/null; then
        zip -r "${archive_name}.zip" "$package_dir/install"
        log_info "Created package: $DIST_DIR/${archive_name}.zip"
    fi

    cd "$PROJECT_ROOT"
}

# Execute post-build operations
post_build_operations

# Final summary
log_info ""
log_info "==============================================="
log_info "Build completed successfully!"
log_info "==============================================="
log_info "Build summary:"
log_info "  Build system: $BUILD_SYSTEM"
log_info "  Build type: $BUILD_TYPE"
log_info "  Build directory: $BUILD_DIR"
log_info "  Version: $(get_git_version)"
log_info "  Log file: $LOG_FILE"

if [[ "$CREATE_PACKAGE" == "y" ]]; then
    log_info "  Packages created in: $DIST_DIR"
fi

if [[ "$GENERATE_DOCS" == "y" ]]; then
    log_info "  Documentation available in build directory"
fi

log_info "Build script completed successfully at $(date)"
echo ""
