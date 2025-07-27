#!/bin/bash

# CPU System Information Test Runner
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
BUILD_DIR="${BUILD_DIR:-build}"
TEST_TIMEOUT="${TEST_TIMEOUT:-300}"
PERFORMANCE_TESTS="${PERFORMANCE_TESTS:-true}"
VERBOSE="${VERBOSE:-false}"

echo -e "${BLUE}=== CPU System Information Test Suite ===${NC}"
echo "Build directory: $BUILD_DIR"
echo "Test timeout: $TEST_TIMEOUT seconds"
echo "Performance tests: $PERFORMANCE_TESTS"
echo "Verbose output: $VERBOSE"
echo

# Function to run a test with timeout and error handling
run_test() {
    local test_name="$1"
    local test_command="$2"
    local description="$3"

    echo -e "${YELLOW}Running $description...${NC}"

    if [ "$VERBOSE" = "true" ]; then
        echo "Command: $test_command"
    fi

    if timeout $TEST_TIMEOUT $test_command; then
        echo -e "${GREEN}✓ $test_name passed${NC}"
        return 0
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "${RED}✗ $test_name timed out after $TEST_TIMEOUT seconds${NC}"
        else
            echo -e "${RED}✗ $test_name failed with exit code $exit_code${NC}"
        fi
        return $exit_code
    fi
}

# Function to check if test executable exists
check_executable() {
    local exe_path="$1"
    local exe_name="$2"

    if [ ! -f "$exe_path" ]; then
        echo -e "${RED}Error: $exe_name not found at $exe_path${NC}"
        echo "Please build the tests first with: cmake --build $BUILD_DIR"
        return 1
    fi

    if [ ! -x "$exe_path" ]; then
        echo -e "${RED}Error: $exe_name is not executable${NC}"
        return 1
    fi

    return 0
}

# Function to display system information
show_system_info() {
    echo -e "${BLUE}=== System Information ===${NC}"

    if command -v uname >/dev/null 2>&1; then
        echo "OS: $(uname -s)"
        echo "Kernel: $(uname -r)"
        echo "Architecture: $(uname -m)"
    fi

    if command -v nproc >/dev/null 2>&1; then
        echo "CPU Cores: $(nproc)"
    elif command -v sysctl >/dev/null 2>&1; then
        echo "CPU Cores: $(sysctl -n hw.ncpu 2>/dev/null || echo 'Unknown')"
    fi

    if command -v free >/dev/null 2>&1; then
        echo "Memory: $(free -h | awk '/^Mem:/ {print $2}')"
    elif command -v vm_stat >/dev/null 2>&1; then
        echo "Memory: $(vm_stat | head -1 | awk '{print $3}' | sed 's/\.//')"
    fi

    echo
}

# Function to run unit tests
run_unit_tests() {
    local test_exe="$BUILD_DIR/tests/sysinfo/cpu_tests"

    echo -e "${BLUE}=== Unit Tests ===${NC}"

    if ! check_executable "$test_exe" "CPU unit tests"; then
        return 1
    fi

    local test_args=""
    if [ "$VERBOSE" = "true" ]; then
        test_args="--gtest_output=xml:cpu_test_results.xml --gtest_print_time=1"
    fi

    run_test "CPU_Unit_Tests" "$test_exe $test_args" "CPU unit tests"
}

# Function to run performance tests
run_performance_tests() {
    local test_exe="$BUILD_DIR/tests/sysinfo/cpu_performance_tests"

    echo -e "${BLUE}=== Performance Tests ===${NC}"

    if ! check_executable "$test_exe" "CPU performance tests"; then
        return 1
    fi

    local test_args="--benchmark_format=console"
    if [ "$VERBOSE" = "true" ]; then
        test_args="$test_args --benchmark_out=cpu_benchmark_results.json --benchmark_out_format=json"
    fi

    run_test "CPU_Performance_Tests" "$test_exe $test_args" "CPU performance tests and benchmarks"
}

# Function to run stress tests
run_stress_tests() {
    echo -e "${BLUE}=== Stress Tests ===${NC}"

    local test_exe="$BUILD_DIR/tests/sysinfo/cpu_tests"

    if ! check_executable "$test_exe" "CPU tests"; then
        return 1
    fi

    # Run specific stress tests
    local stress_tests=(
        "CpuTest.ConcurrentAccess"
        "CpuPerformanceTest.ConcurrentPerformance"
        "CpuPerformanceTest.ScalabilityTest"
    )

    for test in "${stress_tests[@]}"; do
        run_test "Stress_$test" "$test_exe --gtest_filter=$test" "stress test: $test"
    done
}

# Function to run integration tests
run_integration_tests() {
    echo -e "${BLUE}=== Integration Tests ===${NC}"

    local test_exe="$BUILD_DIR/tests/sysinfo/cpu_tests"

    if ! check_executable "$test_exe" "CPU tests"; then
        return 1
    fi

    # Run integration test
    run_test "CPU_Integration_Test" "$test_exe --gtest_filter=CpuTest.FullSystemIntegration" "CPU system integration test"
}

# Function to generate test report
generate_report() {
    echo -e "${BLUE}=== Test Summary ===${NC}"

    local total_tests=0
    local passed_tests=0
    local failed_tests=0

    # Count test results (simplified)
    if [ -f "cpu_test_results.xml" ]; then
        total_tests=$(grep -c "testcase" cpu_test_results.xml 2>/dev/null || echo "0")
        failed_tests=$(grep -c "failure\|error" cpu_test_results.xml 2>/dev/null || echo "0")
        passed_tests=$((total_tests - failed_tests))
    fi

    echo "Total tests run: $total_tests"
    echo -e "Passed: ${GREEN}$passed_tests${NC}"
    echo -e "Failed: ${RED}$failed_tests${NC}"

    if [ $failed_tests -eq 0 ] && [ $total_tests -gt 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        return 0
    elif [ $total_tests -eq 0 ]; then
        echo -e "${YELLOW}No test results found${NC}"
        return 1
    else
        echo -e "${RED}Some tests failed${NC}"
        return 1
    fi
}

# Function to cleanup test artifacts
cleanup() {
    echo -e "${BLUE}=== Cleanup ===${NC}"

    # Remove temporary files
    rm -f cpu_test_results.xml
    rm -f cpu_benchmark_results.json
    rm -f core.*

    echo "Cleanup completed"
}

# Main execution
main() {
    local exit_code=0

    # Show system information
    show_system_info

    # Change to script directory
    cd "$(dirname "$0")"

    # Run tests based on configuration
    echo -e "${BLUE}Starting CPU system information tests...${NC}"
    echo

    # Unit tests (always run)
    if ! run_unit_tests; then
        exit_code=1
    fi
    echo

    # Integration tests
    if ! run_integration_tests; then
        exit_code=1
    fi
    echo

    # Performance tests (optional)
    if [ "$PERFORMANCE_TESTS" = "true" ]; then
        if ! run_performance_tests; then
            exit_code=1
        fi
        echo

        # Stress tests
        if ! run_stress_tests; then
            exit_code=1
        fi
        echo
    fi

    # Generate report
    generate_report
    local report_exit=$?
    if [ $report_exit -ne 0 ]; then
        exit_code=$report_exit
    fi

    echo

    # Cleanup
    cleanup

    # Final result
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}=== All CPU tests completed successfully! ===${NC}"
    else
        echo -e "${RED}=== Some CPU tests failed ===${NC}"
    fi

    return $exit_code
}

# Handle script arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --timeout)
            TEST_TIMEOUT="$2"
            shift 2
            ;;
        --no-performance)
            PERFORMANCE_TESTS="false"
            shift
            ;;
        --verbose)
            VERBOSE="true"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --build-dir DIR     Build directory (default: build)"
            echo "  --timeout SECONDS   Test timeout (default: 300)"
            echo "  --no-performance    Skip performance tests"
            echo "  --verbose           Enable verbose output"
            echo "  --help              Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Run main function
main
exit $?
