#!/bin/bash

# Enhanced Test Runner for Atom System and Sysinfo Modules
# This script builds and runs the enhanced tests for the latest implementation updates

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

print_status "Enhanced Test Runner for Atom System and Sysinfo Modules"
print_status "Project root: $PROJECT_ROOT"
print_status "Build directory: $BUILD_DIR"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    print_error "Build directory not found: $BUILD_DIR"
    print_status "Please run cmake to configure the project first"
    exit 1
fi

cd "$BUILD_DIR"

# Function to build tests
build_tests() {
    print_status "Building tests..."
    
    # Build system tests
    if make -j$(nproc) atom_system.test 2>/dev/null; then
        print_success "System tests built successfully"
    else
        print_warning "Failed to build system tests (may not be configured)"
    fi
    
    # Build sysinfo tests
    if make -j$(nproc) atom_sysinfo.test 2>/dev/null; then
        print_success "Sysinfo tests built successfully"
    else
        print_warning "Failed to build sysinfo tests (may not be configured)"
    fi
    
    # Build individual test targets
    print_status "Building individual test targets..."
    
    # Try to build advanced executor tests
    if make -j$(nproc) test_advanced_executor_individual 2>/dev/null; then
        print_success "Advanced executor tests built successfully"
    else
        print_warning "Advanced executor tests not built (may need configuration)"
    fi
    
    # Try to build enhanced features tests
    if make -j$(nproc) test_enhanced_features_test 2>/dev/null; then
        print_success "Enhanced features tests built successfully"
    else
        print_warning "Enhanced features tests not built (may need configuration)"
    fi
}

# Function to run tests
run_tests() {
    print_status "Running tests..."
    
    local test_results=()
    local total_tests=0
    local passed_tests=0
    
    # Run system tests
    if [ -f "tests/system/atom_system.test" ]; then
        print_status "Running system tests..."
        if ./tests/system/atom_system.test --gtest_output=xml:system_test_results.xml; then
            print_success "System tests passed"
            ((passed_tests++))
        else
            print_error "System tests failed"
            test_results+=("System tests: FAILED")
        fi
        ((total_tests++))
    fi
    
    # Run sysinfo tests
    if [ -f "tests/sysinfo/atom_sysinfo.test" ]; then
        print_status "Running sysinfo tests..."
        if ./tests/sysinfo/atom_sysinfo.test --gtest_output=xml:sysinfo_test_results.xml; then
            print_success "Sysinfo tests passed"
            ((passed_tests++))
        else
            print_error "Sysinfo tests failed"
            test_results+=("Sysinfo tests: FAILED")
        fi
        ((total_tests++))
    fi
    
    # Run individual tests if available
    if [ -f "tests/system/test_advanced_executor_individual" ]; then
        print_status "Running advanced executor tests..."
        if ./tests/system/test_advanced_executor_individual; then
            print_success "Advanced executor tests passed"
            ((passed_tests++))
        else
            print_error "Advanced executor tests failed"
            test_results+=("Advanced executor tests: FAILED")
        fi
        ((total_tests++))
    fi
    
    # Run battery tests if available
    if [ -f "tests/battery_tests" ]; then
        print_status "Running battery tests..."
        if ./tests/battery_tests; then
            print_success "Battery tests passed"
            ((passed_tests++))
        else
            print_error "Battery tests failed"
            test_results+=("Battery tests: FAILED")
        fi
        ((total_tests++))
    fi
    
    # Print summary
    echo
    print_status "Test Summary:"
    echo "=============="
    echo "Total test suites: $total_tests"
    echo "Passed: $passed_tests"
    echo "Failed: $((total_tests - passed_tests))"
    
    if [ ${#test_results[@]} -gt 0 ]; then
        echo
        print_error "Failed tests:"
        for result in "${test_results[@]}"; do
            echo "  - $result"
        done
    fi
    
    if [ $passed_tests -eq $total_tests ]; then
        print_success "All tests passed!"
        return 0
    else
        print_error "Some tests failed"
        return 1
    fi
}

# Function to run specific test categories
run_category_tests() {
    local category="$1"
    
    case "$category" in
        "system")
            print_status "Running system-specific tests..."
            if [ -f "tests/system/atom_system.test" ]; then
                ./tests/system/atom_system.test
            else
                print_warning "System tests not found"
            fi
            ;;
        "sysinfo")
            print_status "Running sysinfo-specific tests..."
            if [ -f "tests/sysinfo/atom_sysinfo.test" ]; then
                ./tests/sysinfo/atom_sysinfo.test
            else
                print_warning "Sysinfo tests not found"
            fi
            ;;
        "advanced")
            print_status "Running advanced feature tests..."
            if [ -f "tests/system/test_advanced_executor_individual" ]; then
                ./tests/system/test_advanced_executor_individual
            fi
            if [ -f "tests/sysinfo/test_enhanced_features_test" ]; then
                ./tests/sysinfo/test_enhanced_features_test
            fi
            ;;
        *)
            print_error "Unknown test category: $category"
            print_status "Available categories: system, sysinfo, advanced"
            exit 1
            ;;
    esac
}

# Main execution
case "${1:-all}" in
    "build")
        build_tests
        ;;
    "run")
        run_tests
        ;;
    "system"|"sysinfo"|"advanced")
        run_category_tests "$1"
        ;;
    "all"|"")
        build_tests
        echo
        run_tests
        ;;
    "help"|"-h"|"--help")
        echo "Usage: $0 [command]"
        echo
        echo "Commands:"
        echo "  build     - Build all tests"
        echo "  run       - Run all tests"
        echo "  system    - Run system tests only"
        echo "  sysinfo   - Run sysinfo tests only"
        echo "  advanced  - Run advanced feature tests only"
        echo "  all       - Build and run all tests (default)"
        echo "  help      - Show this help message"
        ;;
    *)
        print_error "Unknown command: $1"
        print_status "Use '$0 help' for usage information"
        exit 1
        ;;
esac
