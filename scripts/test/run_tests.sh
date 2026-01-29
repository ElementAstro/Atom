#!/bin/bash

# =============================================================================
# Atom Test Runner Script
# =============================================================================
# This script provides convenient access to the Atom testing infrastructure
# with various options for running tests in different ways.
#
# Usage: ./scripts/run_tests.sh [OPTIONS] [TARGETS]
# =============================================================================

set -e  # Exit on any error

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Default values
VERBOSE=false
PARALLEL=false
THREADS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
OUTPUT_FORMAT=""
OUTPUT_FILE=""
MODULE=""
CATEGORY=""
FILTER=""
HELP=false
CLEAN=false
BUILD_ONLY=false
RUN_TESTS=true
USE_CTEST=false
COVERAGE=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Show help information
show_help() {
    cat << EOF
Atom Test Runner Script

USAGE:
    $0 [OPTIONS] [TARGETS]

OPTIONS:
    -h, --help              Show this help message
    -v, --verbose           Enable verbose output
    -p, --parallel [N]      Run tests in parallel (default: all CPU cores)
    -t, --threads N         Number of threads for parallel execution
    -m, --module NAME       Run tests from specific module only
    -c, --category NAME     Run tests from specific category only
    -f, --filter PATTERN    Run tests matching regex pattern
    --output-format FORMAT  Output format (json, xml, html, text)
    --output FILE           Output file for test results
    --clean                 Clean build directory before building
    --build-only            Build tests without running them
    --ctest                 Use CTest instead of unified test runner
    --coverage              Generate code coverage report
    --build-dir DIR         Specify build directory (default: build/)

TARGETS:
    all                     Run all tests (default)
    algorithm               Run algorithm module tests
    async                   Run async module tests
    components              Run components module tests
    connection              Run connection module tests
    containers              Run containers module tests
    error                   Run error module tests
    extra                   Run extra utilities tests
    image                   Run image module tests
    io                      Run IO module tests
    log                     Run log module tests
    memory                  Run memory module tests
    meta                    Run metaprogramming tests
    search                  Run search module tests
    secret                  Run cryptographic tests
    serial                  Run serial communication tests
    sysinfo                 Run system information tests
    system                  Run system integration tests
    type                    Run type system tests
    utils                   Run utilities tests
    web                     Run web utilities tests
    core                    Run core module tests (error, log, meta, type, utils)
    io_modules              Run IO module tests (io, image, serial)
    system_modules          Run system module tests (system, sysinfo)
    network_modules         Run network module tests (web, connection)

EXAMPLES:
    $0                              # Run all tests
    $0 --verbose --parallel         # Run all tests in parallel with verbose output
    $0 --module error               # Run error module tests only
    $0 --category unit              # Run unit tests only
    $0 --filter ".*socket.*"        # Run tests containing "socket"
    $0 --output-format=json --output=results.json  # Export results to JSON
    $0 --coverage                   # Generate coverage report
    $0 --clean --ctest              # Clean build and run tests via CTest

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                HELP=true
                shift
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -p|--parallel)
                PARALLEL=true
                if [[ "$2" =~ ^[0-9]+$ ]] && [[ "$2" -gt 0 ]]; then
                    THREADS="$2"
                    shift
                fi
                shift
                ;;
            -t|--threads)
                if [[ "$2" =~ ^[0-9]+$ ]] && [[ "$2" -gt 0 ]]; then
                    THREADS="$2"
                    shift 2
                else
                    print_error "Invalid thread count: $2"
                    exit 1
                fi
                ;;
            -m|--module)
                MODULE="$2"
                shift 2
                ;;
            -c|--category)
                CATEGORY="$2"
                shift 2
                ;;
            -f|--filter)
                FILTER="$2"
                shift 2
                ;;
            --output-format)
                OUTPUT_FORMAT="$2"
                shift 2
                ;;
            --output)
                OUTPUT_FILE="$2"
                shift 2
                ;;
            --clean)
                CLEAN=true
                shift
                ;;
            --build-only)
                BUILD_ONLY=true
                RUN_TESTS=false
                shift
                ;;
            --ctest)
                USE_CTEST=true
                shift
                ;;
            --coverage)
                COVERAGE=true
                shift
                ;;
            --build-dir)
                BUILD_DIR="$2"
                shift 2
                ;;
            all|algorithm|async|components|connection|containers|error|extra|image|io|log|memory|meta|search|secret|serial|sysinfo|system|type|utils|web|core|io_modules|system_modules|network_modules)
                # Store target modules
                TARGETS+=("$1")
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# Check if we need to build
check_build_needed() {
    if [[ ! -f "$BUILD_DIR/run_all_tests" ]] && [[ ! -f "$BUILD_DIR/ctest" ]]; then
        return 0
    fi

    # Check if source files are newer than executables
    local newer_sources=$(find "$PROJECT_ROOT" -name "*.cpp" -o -name "*.hpp" -o -name "CMakeLists.txt" \
        -newer "$BUILD_DIR/run_all_tests" 2>/dev/null | wc -l)

    if [[ "$newer_sources" -gt 0 ]]; then
        return 0
    fi

    return 1
}

# Build the project
build_project() {
    print_status "Building project..."

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Clean if requested
    if [[ "$CLEAN" == true ]]; then
        print_status "Cleaning build directory..."
        rm -rf *
    fi

    # Configure with CMake
    local cmake_args=("-DATOM_BUILD_TESTS=ON")

    if [[ "$COVERAGE" == true ]]; then
        cmake_args+=("-DCMAKE_BUILD_TYPE=Debug" "-DCMAKE_CXX_FLAGS_DEBUG=--coverage")
    fi

    if [[ "$VERBOSE" == true ]]; then
        cmake_args+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    fi

    print_status "Configuring with CMake..."
    cmake "${cmake_args[@]}" "$PROJECT_ROOT"

    # Build
    local build_args=("--parallel")
    if [[ "$VERBOSE" == true ]]; then
        build_args+=("--verbose")
    fi

    print_status "Building..."
    cmake --build . "${build_args[@]}"

    print_success "Build completed successfully"
}

# Run tests using unified test runner
run_unified_tests() {
    cd "$BUILD_DIR"

    local test_args=()

    if [[ "$VERBOSE" == true ]]; then
        test_args+=("--verbose")
    fi

    if [[ "$PARALLEL" == true ]]; then
        test_args+=("--parallel" "--threads=$THREADS")
    fi

    if [[ -n "$MODULE" ]]; then
        test_args+=("--module=$MODULE")
    fi

    if [[ -n "$CATEGORY" ]]; then
        test_args+=("--category=$CATEGORY")
    fi

    if [[ -n "$FILTER" ]]; then
        test_args+=("--filter=$FILTER")
    fi

    if [[ -n "$OUTPUT_FORMAT" ]]; then
        test_args+=("--output-format=$OUTPUT_FORMAT")
    fi

    if [[ -n "$OUTPUT_FILE" ]]; then
        test_args+=("--output=$OUTPUT_FILE")
    fi

    if [[ ! -f "./run_all_tests" ]]; then
        print_error "Unified test runner not found. Did you build the project?"
        return 1
    fi

    print_status "Running tests with unified test runner..."
    print_status "Command: ./run_all_tests ${test_args[*]}"

    if ./run_all_tests "${test_args[@]}"; then
        print_success "All tests passed!"
        return 0
    else
        print_error "Some tests failed!"
        return 1
    fi
}

# Run tests using CTest
run_ctest_tests() {
    cd "$BUILD_DIR"

    local ctest_args=("--output-on-failure")

    if [[ "$PARALLEL" == true ]]; then
        ctest_args+=("--parallel" "$THREADS")
    fi

    if [[ -n "$MODULE" ]]; then
        ctest_args+=("-L" "$MODULE")
    fi

    if [[ -n "$CATEGORY" ]]; then
        ctest_args+=("-L" "$CATEGORY")
    fi

    if [[ -n "$FILTER" ]]; then
        ctest_args+=("-R" "$FILTER")
    fi

    # Handle specific targets
    if [[ ${#TARGETS[@]} -gt 0 ]]; then
        case "${TARGETS[0]}" in
            core)
                ctest_args=("-L" "error|log|meta|type|utils" "${ctest_args[@]}")
                ;;
            io_modules)
                ctest_args=("-L" "io|image|serial" "${ctest_args[@]}")
                ;;
            system_modules)
                ctest_args=("-L" "system|sysinfo" "${ctest_args[@]}")
                ;;
            network_modules)
                ctest_args=("-L" "web|connection" "${ctest_args[@]}")
                ;;
            *)
                ctest_args=("-L" "${TARGETS[0]}" "${ctest_args[@]}")
                ;;
        esac
    fi

    print_status "Running tests with CTest..."
    print_status "Command: ctest ${ctest_args[*]}"

    if ctest "${ctest_args[@]}"; then
        print_success "All tests passed!"
        return 0
    else
        print_error "Some tests failed!"
        return 1
    fi
}

# Generate coverage report
generate_coverage() {
    if [[ "$COVERAGE" != true ]]; then
        return 0
    fi

    cd "$BUILD_DIR"

    print_status "Generating code coverage report..."

    # Check if lcov is available
    if ! command -v lcov &> /dev/null; then
        print_warning "lcov not found. Skipping coverage report generation."
        return 0
    fi

    # Generate coverage data
    lcov --directory . --capture --output-file coverage.info
    lcov --remove coverage.info '/usr/*' --output-file coverage.info
    lcov --remove coverage.info '*/tests/*' --output-file coverage.info

    # Generate HTML report if genhtml is available
    if command -v genhtml &> /dev/null; then
        genhtml -o coverage_html coverage.info
        print_success "Coverage report generated in $BUILD_DIR/coverage_html/"
    else
        print_warning "genhtml not found. HTML report not generated."
        print_status "Coverage data available in $BUILD_DIR/coverage.info"
    fi

    # Show coverage summary
    if command -v lcov-summary &> /dev/null; then
        lcov-summary coverage.info
    else
        print_status "Coverage summary: $(lcov --summary coverage.info 2>&1 | grep lines...)"
    fi
}

# Main execution
main() {
    local TARGETS=()

    # Set default target if none specified
    TARGETS=("all")

    parse_args "$@"

    if [[ "$HELP" == true ]]; then
        show_help
        exit 0
    fi

    # Validate arguments
    if [[ "$PARALLEL" == true ]] && [[ "$THREADS" -le 0 ]]; then
        print_error "Thread count must be positive"
        exit 1
    fi

    # Change to project root
    cd "$PROJECT_ROOT"

    # Build if needed
    if check_build_needed || [[ "$CLEAN" == true ]]; then
        build_project
    elif [[ "$VERBOSE" == true ]]; then
        print_status "Build is up to date"
    fi

    # Run tests
    if [[ "$RUN_TESTS" == true ]]; then
        local test_result=0

        if [[ "$USE_CTEST" == true ]]; then
            run_ctest_tests || test_result=1
        else
            run_unified_tests || test_result=1
        fi

        # Generate coverage report if requested
        generate_coverage

        exit $test_result
    else
        print_success "Build completed. Use --run-tests to execute tests."
    fi
}

# Execute main function
main "$@"
