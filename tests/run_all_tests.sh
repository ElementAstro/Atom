#!/bin/bash

# Comprehensive Test Runner for Atom Project
# This script builds and runs all test modules with proper error handling and reporting
# Author: Max Qian
# License: GPL3

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
TEST_BUILD_DIR="${BUILD_DIR}/tests"
PARALLEL_JOBS=$(nproc)

# Test modules
TEST_MODULES=(
    "algorithm"
    "async" 
    "components"
    "connection"
    "error"
    "extra"
    "image"
    "io"
    "log"
    "memory"
    "meta"
    "search"
    "secret"
    "serial"
    "sysinfo"
    "system"
    "type"
    "utils"
    "web"
)

# Function to print colored output
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

print_header() {
    echo -e "\n${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}\n"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check dependencies
check_dependencies() {
    print_header "Checking Dependencies"
    
    local missing_deps=()
    
    if ! command_exists cmake; then
        missing_deps+=("cmake")
    fi
    
    if ! command_exists make; then
        missing_deps+=("make")
    fi
    
    if ! command_exists g++; then
        missing_deps+=("g++")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_error "Please install the missing dependencies and try again."
        exit 1
    fi
    
    print_success "All dependencies are available"
}

# Function to configure build
configure_build() {
    print_header "Configuring Build"
    
    cd "$PROJECT_ROOT"
    
    # Create build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"
    
    # Configure with CMake
    print_status "Running CMake configuration..."
    cmake -B "$BUILD_DIR" \
          -DCMAKE_BUILD_TYPE=Release \
          -DATOM_ENABLE_TESTING=ON \
          -DATOM_TEST_BUILD_ALL=ON
    
    print_success "Build configured successfully"
}

# Function to build tests
build_tests() {
    print_header "Building Tests"
    
    cd "$BUILD_DIR"
    
    print_status "Building all test modules with $PARALLEL_JOBS parallel jobs..."
    make -j"$PARALLEL_JOBS" || {
        print_error "Build failed"
        return 1
    }
    
    print_success "All tests built successfully"
}

# Function to run individual test module
run_test_module() {
    local module="$1"
    local test_executable="${TEST_BUILD_DIR}/atom_${module}.test"
    
    print_status "Running $module tests..."
    
    if [ -f "$test_executable" ]; then
        if "$test_executable"; then
            print_success "$module tests passed"
            return 0
        else
            print_error "$module tests failed"
            return 1
        fi
    else
        print_warning "$module test executable not found, skipping"
        return 0
    fi
}

# Function to run all tests
run_all_tests() {
    print_header "Running All Tests"
    
    local failed_modules=()
    local passed_modules=()
    local skipped_modules=()
    
    for module in "${TEST_MODULES[@]}"; do
        if run_test_module "$module"; then
            if [ -f "${TEST_BUILD_DIR}/atom_${module}.test" ]; then
                passed_modules+=("$module")
            else
                skipped_modules+=("$module")
            fi
        else
            failed_modules+=("$module")
        fi
    done
    
    # Print summary
    print_header "Test Summary"
    
    echo -e "${GREEN}Passed modules (${#passed_modules[@]}):${NC} ${passed_modules[*]}"
    
    if [ ${#skipped_modules[@]} -ne 0 ]; then
        echo -e "${YELLOW}Skipped modules (${#skipped_modules[@]}):${NC} ${skipped_modules[*]}"
    fi
    
    if [ ${#failed_modules[@]} -ne 0 ]; then
        echo -e "${RED}Failed modules (${#failed_modules[@]}):${NC} ${failed_modules[*]}"
        return 1
    fi
    
    print_success "All available tests passed!"
    return 0
}

# Function to run with CTest
run_ctest() {
    print_header "Running Tests with CTest"
    
    cd "$BUILD_DIR"
    
    print_status "Running CTest..."
    if ctest --output-on-failure --parallel "$PARALLEL_JOBS"; then
        print_success "All CTest tests passed"
        return 0
    else
        print_error "Some CTest tests failed"
        return 1
    fi
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -c, --configure     Only configure the build"
    echo "  -b, --build         Only build the tests"
    echo "  -r, --run           Only run the tests (assumes already built)"
    echo "  -t, --ctest         Use CTest to run tests"
    echo "  -m, --module MODULE Run tests for specific module only"
    echo ""
    echo "Examples:"
    echo "  $0                  Configure, build, and run all tests"
    echo "  $0 --build          Only build tests"
    echo "  $0 --run            Only run tests"
    echo "  $0 --module memory  Run only memory module tests"
    echo "  $0 --ctest          Use CTest runner"
}

# Main function
main() {
    local configure_only=false
    local build_only=false
    local run_only=false
    local use_ctest=false
    local specific_module=""
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -c|--configure)
                configure_only=true
                shift
                ;;
            -b|--build)
                build_only=true
                shift
                ;;
            -r|--run)
                run_only=true
                shift
                ;;
            -t|--ctest)
                use_ctest=true
                shift
                ;;
            -m|--module)
                specific_module="$2"
                shift 2
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    print_header "Atom Test Runner"
    
    # Check dependencies first
    check_dependencies
    
    # Execute based on options
    if [ "$configure_only" = true ]; then
        configure_build
    elif [ "$build_only" = true ]; then
        configure_build
        build_tests
    elif [ "$run_only" = true ]; then
        if [ -n "$specific_module" ]; then
            run_test_module "$specific_module"
        elif [ "$use_ctest" = true ]; then
            run_ctest
        else
            run_all_tests
        fi
    else
        # Default: configure, build, and run
        configure_build
        build_tests
        
        if [ -n "$specific_module" ]; then
            run_test_module "$specific_module"
        elif [ "$use_ctest" = true ]; then
            run_ctest
        else
            run_all_tests
        fi
    fi
}

# Run main function with all arguments
main "$@"
