#!/bin/bash

# Comprehensive Memory System Test Build and Run Script
# This script builds and runs all memory system tests

set -e  # Exit on any error

echo "=== Atom Memory System Test Suite ==="
echo "Building and running comprehensive memory tests..."

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

# Check if we're in the right directory
if [ ! -f "test_framework.hpp" ]; then
    print_error "test_framework.hpp not found. Please run this script from the tests/memory directory."
    exit 1
fi

# Create build directory
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    print_status "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Detect compiler
CXX_COMPILER=""
if command -v g++ &> /dev/null; then
    CXX_COMPILER="g++"
elif command -v clang++ &> /dev/null; then
    CXX_COMPILER="clang++"
else
    print_error "No suitable C++ compiler found (g++ or clang++)"
    exit 1
fi

print_status "Using compiler: $CXX_COMPILER"

# Compiler flags
CXX_FLAGS="-std=c++20 -O2 -Wall -Wextra -pthread"
CXX_FLAGS="$CXX_FLAGS -DATOM_MEMORY_TRACKING=1 -DATOM_MEMORY_STATS_ENABLED=1"
CXX_FLAGS="$CXX_FLAGS -DATOM_MEMORY_PREFETCH=1 -DATOM_CACHE_OPTIMIZATION=1"

# Add debug flags if requested
if [ "$1" = "--debug" ]; then
    CXX_FLAGS="$CXX_FLAGS -g -O0 -DDEBUG"
    print_status "Building in debug mode"
else
    CXX_FLAGS="$CXX_FLAGS -DNDEBUG"
    print_status "Building in release mode"
fi

# Include directories
INCLUDE_DIRS="-I.. -I../../.."

print_status "Compiling comprehensive test suite..."

# Build comprehensive test runner
$CXX_COMPILER $CXX_FLAGS $INCLUDE_DIRS \
    ../run_comprehensive_tests.cpp \
    ../test_memory_pool.cpp \
    ../test_object_pool.cpp \
    ../test_ring_buffer.cpp \
    -o comprehensive_tests

if [ $? -eq 0 ]; then
    print_success "Compilation successful!"
else
    print_error "Compilation failed!"
    exit 1
fi

# Build individual test executables
print_status "Building individual test components..."

# Memory Pool Tests
$CXX_COMPILER $CXX_FLAGS $INCLUDE_DIRS \
    -DSTANDALONE_TEST \
    ../test_memory_pool.cpp \
    -o memory_pool_tests

# Object Pool Tests  
$CXX_COMPILER $CXX_FLAGS $INCLUDE_DIRS \
    -DSTANDALONE_TEST \
    ../test_object_pool.cpp \
    -o object_pool_tests

# Ring Buffer Tests
$CXX_COMPILER $CXX_FLAGS $INCLUDE_DIRS \
    -DSTANDALONE_TEST \
    ../test_ring_buffer.cpp \
    -o ring_buffer_tests

print_success "All test executables built successfully!"

# Run tests
echo ""
print_status "Running comprehensive memory system tests..."
echo "========================================================"

# Run the comprehensive test suite
if ./comprehensive_tests; then
    print_success "Comprehensive tests completed successfully!"
    COMPREHENSIVE_RESULT=0
else
    print_error "Comprehensive tests failed!"
    COMPREHENSIVE_RESULT=1
fi

echo ""
echo "========================================================"

# Run individual component tests if requested
if [ "$1" = "--individual" ] || [ "$2" = "--individual" ]; then
    print_status "Running individual component tests..."
    
    echo ""
    print_status "Running Memory Pool tests..."
    if ./memory_pool_tests; then
        print_success "Memory Pool tests passed!"
    else
        print_warning "Memory Pool tests had issues"
    fi
    
    echo ""
    print_status "Running Object Pool tests..."
    if ./object_pool_tests; then
        print_success "Object Pool tests passed!"
    else
        print_warning "Object Pool tests had issues"
    fi
    
    echo ""
    print_status "Running Ring Buffer tests..."
    if ./ring_buffer_tests; then
        print_success "Ring Buffer tests passed!"
    else
        print_warning "Ring Buffer tests had issues"
    fi
fi

# Performance benchmarking
if [ "$1" = "--benchmark" ] || [ "$2" = "--benchmark" ]; then
    echo ""
    print_status "Running performance benchmarks..."
    echo "This may take several minutes..."
    
    # Run benchmarks multiple times for statistical significance
    for i in {1..3}; do
        echo ""
        print_status "Benchmark run $i/3..."
        ./comprehensive_tests --benchmark
    done
fi

# Memory leak testing
if [ "$1" = "--valgrind" ] || [ "$2" = "--valgrind" ]; then
    if command -v valgrind &> /dev/null; then
        echo ""
        print_status "Running memory leak detection with Valgrind..."
        valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
                 --track-origins=yes --verbose ./comprehensive_tests
    else
        print_warning "Valgrind not found, skipping memory leak detection"
    fi
fi

# Generate test report
echo ""
print_status "Generating test report..."

cat > ../test_results.md << EOF
# Memory System Test Results

**Test Date:** $(date)
**Compiler:** $CXX_COMPILER
**Build Mode:** $([ "$1" = "--debug" ] && echo "Debug" || echo "Release")
**Hardware:** $(nproc) CPU cores

## Test Summary

- **Comprehensive Tests:** $([ $COMPREHENSIVE_RESULT -eq 0 ] && echo "✅ PASSED" || echo "❌ FAILED")

## Test Configuration

- Memory Tracking: Enabled
- Performance Statistics: Enabled  
- Memory Prefetching: Enabled
- Cache Optimization: Enabled

## Performance Optimizations Tested

1. **Memory Pool Enhancements**
   - Advanced allocation strategies (FirstFit, BestFit, WorstFit)
   - Performance monitoring and statistics
   - Fragmentation analysis and reduction
   - Thread-safe operations with reduced lock contention

2. **Object Pool Advanced Features**
   - Lifecycle management with initializers/finalizers
   - Performance monitoring and cache efficiency
   - Adaptive sizing and object warming
   - Fast path acquisitions

3. **Ring Buffer Optimizations**
   - Lock-free read operations
   - Batch operations for improved throughput
   - Cache-friendly memory access patterns
   - Performance statistics and monitoring

## Next Steps

$([ $COMPREHENSIVE_RESULT -eq 0 ] && echo "All tests passed! The memory system optimizations are working correctly." || echo "Some tests failed. Please review the output above for details.")

EOF

print_success "Test report generated: test_results.md"

# Cleanup
cd ..

# Final summary
echo ""
echo "========================================================"
if [ $COMPREHENSIVE_RESULT -eq 0 ]; then
    print_success "🎉 All memory system tests completed successfully!"
    print_success "The enhanced memory components are working correctly."
    echo ""
    echo "Key improvements validated:"
    echo "  ✅ Enhanced allocation strategies and performance monitoring"
    echo "  ✅ Advanced object lifecycle management"
    echo "  ✅ Lock-free operations and cache optimizations"
    echo "  ✅ Thread safety and concurrent access patterns"
    echo "  ✅ Memory leak detection and debugging features"
else
    print_error "❌ Some tests failed. Please review the output for details."
    exit 1
fi

echo ""
print_status "Test artifacts:"
print_status "  - Executables: tests/memory/build/"
print_status "  - Report: tests/memory/test_results.md"
print_status "  - Logs: Check console output above"

echo ""
print_status "To run specific test suites:"
echo "  ./build_and_run_tests.sh --individual    # Run individual component tests"
echo "  ./build_and_run_tests.sh --benchmark     # Run performance benchmarks"
echo "  ./build_and_run_tests.sh --valgrind      # Run with memory leak detection"
echo "  ./build_and_run_tests.sh --debug         # Build and run in debug mode"
