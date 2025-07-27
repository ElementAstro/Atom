#!/bin/bash

# Memory Module Test Runner Script
# This script provides an easy way to run various memory module tests

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

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

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to build tests
build_tests() {
    print_status "Building memory module tests..."
    
    # Create build directory
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # Configure with CMake
    if command_exists cmake; then
        cmake -DCMAKE_BUILD_TYPE=Release "${SCRIPT_DIR}"
        make -j$(nproc)
        print_success "Tests built successfully"
    else
        print_error "CMake not found. Please install CMake to build tests."
        exit 1
    fi
}

# Function to run all tests
run_all_tests() {
    print_status "Running all memory module tests..."
    cd "${BUILD_DIR}"
    
    if [ -f "./memory_tests" ]; then
        ./memory_tests
        print_success "All tests completed"
    else
        print_error "Test executable not found. Please build tests first."
        exit 1
    fi
}

# Function to run specific test category
run_test_category() {
    local category="$1"
    print_status "Running ${category} tests..."
    cd "${BUILD_DIR}"
    
    if [ -f "./memory_tests" ]; then
        ./memory_tests --gtest_filter="*${category}*"
        print_success "${category} tests completed"
    else
        print_error "Test executable not found. Please build tests first."
        exit 1
    fi
}

# Function to run performance benchmarks
run_benchmarks() {
    print_status "Running memory performance benchmarks..."
    cd "${BUILD_DIR}"
    
    if [ -f "./memory_tests" ]; then
        ./memory_tests --gtest_filter="*Performance*:*Benchmark*"
        print_success "Benchmarks completed"
    else
        print_error "Test executable not found. Please build tests first."
        exit 1
    fi
}

# Function to run tests with Valgrind
run_valgrind_tests() {
    if command_exists valgrind; then
        print_status "Running tests with Valgrind memory checking..."
        cd "${BUILD_DIR}"
        
        if [ -f "./memory_tests" ]; then
            valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
                     --track-origins=yes --verbose ./memory_tests
            print_success "Valgrind tests completed"
        else
            print_error "Test executable not found. Please build tests first."
            exit 1
        fi
    else
        print_warning "Valgrind not found. Skipping memory checking tests."
    fi
}

# Function to generate coverage report
generate_coverage() {
    if command_exists gcov; then
        print_status "Generating test coverage report..."
        cd "${BUILD_DIR}"
        
        # Rebuild with coverage flags
        cmake -DCMAKE_BUILD_TYPE=Debug "${SCRIPT_DIR}"
        make -j$(nproc)
        
        # Run tests
        ./memory_tests
        
        # Generate coverage
        gcov *.cpp
        
        if command_exists lcov; then
            lcov --capture --directory . --output-file coverage.info
            lcov --remove coverage.info '/usr/*' --output-file coverage.info
            lcov --list coverage.info
            
            if command_exists genhtml; then
                genhtml coverage.info --output-directory coverage_html
                print_success "Coverage report generated in coverage_html/"
            fi
        fi
        
        print_success "Coverage analysis completed"
    else
        print_warning "gcov not found. Cannot generate coverage report."
    fi
}

# Function to run stress tests
run_stress_tests() {
    print_status "Running memory stress tests..."
    cd "${BUILD_DIR}"
    
    if [ -f "./memory_tests" ]; then
        # Run tests multiple times to stress the system
        for i in {1..5}; do
            print_status "Stress test iteration $i/5"
            ./memory_tests --gtest_filter="*Stress*:*Performance*"
        done
        print_success "Stress tests completed"
    else
        print_error "Test executable not found. Please build tests first."
        exit 1
    fi
}

# Function to show help
show_help() {
    echo "Memory Module Test Runner"
    echo ""
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Options:"
    echo "  build              Build the test suite"
    echo "  test               Run all tests"
    echo "  performance        Run performance tests only"
    echo "  leak               Run leak detection tests only"
    echo "  numa               Run NUMA topology tests only"
    echo "  utility            Run utility function tests only"
    echo "  benchmark          Run performance benchmarks"
    echo "  valgrind           Run tests with Valgrind memory checking"
    echo "  coverage           Generate test coverage report"
    echo "  stress             Run stress tests"
    echo "  clean              Clean build directory"
    echo "  help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 build && $0 test     # Build and run all tests"
    echo "  $0 performance          # Run only performance tests"
    echo "  $0 valgrind             # Run tests with memory checking"
}

# Function to clean build directory
clean_build() {
    print_status "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
    print_success "Build directory cleaned"
}

# Main script logic
case "${1:-help}" in
    build)
        build_tests
        ;;
    test)
        run_all_tests
        ;;
    performance)
        run_test_category "Performance"
        ;;
    leak)
        run_test_category "Leak"
        ;;
    numa)
        run_test_category "Numa"
        ;;
    utility)
        run_test_category "Utility"
        ;;
    benchmark)
        run_benchmarks
        ;;
    valgrind)
        run_valgrind_tests
        ;;
    coverage)
        generate_coverage
        ;;
    stress)
        run_stress_tests
        ;;
    clean)
        clean_build
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        print_error "Unknown option: $1"
        show_help
        exit 1
        ;;
esac
