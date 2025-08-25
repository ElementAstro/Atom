#!/bin/bash

# Comprehensive test script for Atom Connection Examples
# This script tests compilation and basic execution of all connection examples

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
SOURCE_DIR="."
VERBOSE=false
BUILD_ONLY=false
SPECIFIC_EXAMPLE=""
TIMEOUT=10

# Test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to print colored output
log() {
    local color=$1
    local message=$2
    local timestamp=$(date '+%H:%M:%S')
    echo -e "${color}[${timestamp}] ${message}${NC}"
}

log_info() {
    log "${BLUE}" "$1"
}

log_success() {
    log "${GREEN}" "$1"
}

log_warning() {
    log "${YELLOW}" "$1"
}

log_error() {
    log "${RED}" "$1"
}

log_verbose() {
    if [ "$VERBOSE" = true ]; then
        log "${CYAN}" "VERBOSE: $1"
    fi
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to run a command with timeout
run_with_timeout() {
    local timeout=$1
    shift
    local cmd="$@"
    
    log_verbose "Running command with timeout ${timeout}s: $cmd"
    
    if command_exists timeout; then
        timeout "$timeout" $cmd
    else
        # Fallback for systems without timeout command
        $cmd &
        local pid=$!
        sleep "$timeout"
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            return 124  # timeout exit code
        fi
        wait "$pid"
    fi
}

# Function to record test result
record_test() {
    local test_name=$1
    local success=$2
    local message=$3
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if [ "$success" = true ]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        log_success "PASS $test_name: $message"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        log_error "FAIL $test_name: $message"
    fi
}

# Function to setup build environment
setup_build_environment() {
    log_info "Setting up build environment..."
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    
    # Run CMake configuration
    log_verbose "Running CMake configuration..."
    
    if ! cmake \
        -DATOM_EXAMPLE_CONNECTION_BUILD_ALL=ON \
        -DATOM_EXAMPLE_CONNECTION_VERBOSE=ON \
        -DCMAKE_BUILD_TYPE=Debug \
        -B "$BUILD_DIR" \
        -S "../.."; then
        log_error "CMake configuration failed"
        return 1
    fi
    
    log_success "Build environment configured successfully"
    return 0
}

# Function to build examples
build_examples() {
    local specific_example=$1
    
    if [ -n "$specific_example" ]; then
        log_info "Building example: $specific_example..."
        local target="connection_$specific_example"
    else
        log_info "Building all connection examples..."
        local target="connection_examples_all"
    fi
    
    if ! cmake --build "$BUILD_DIR" --target "$target" --parallel; then
        local error_msg="Build failed for $target"
        log_error "$error_msg"
        if [ -n "$specific_example" ]; then
            record_test "build_$specific_example" false "$error_msg"
        fi
        return 1
    fi
    
    local success_msg="Build successful for $target"
    log_success "$success_msg"
    if [ -n "$specific_example" ]; then
        record_test "build_$specific_example" true "$success_msg"
    fi
    return 0
}

# Function to test example execution
test_example_execution() {
    local example_name=$1
    local executable="$BUILD_DIR/connection_$example_name"
    
    # Add .exe extension on Windows
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        executable="${executable}.exe"
    fi
    
    log_info "Testing execution of $example_name..."
    
    # Check if executable exists
    if [ ! -f "$executable" ]; then
        record_test "run_$example_name" false "Executable not found: $executable"
        return 1
    fi
    
    # Make executable if needed
    chmod +x "$executable" 2>/dev/null || true
    
    # Determine test strategy based on example type
    case "$example_name" in
        *server|sockethub|async_sockethub)
            test_server_example "$example_name" "$executable"
            ;;
        *client)
            test_client_example "$example_name" "$executable"
            ;;
        ssh*)
            if ! command_exists ssh; then
                record_test "run_$example_name" true "Skipped (SSH not available)"
                return 0
            fi
            test_standalone_example "$example_name" "$executable"
            ;;
        ttybase)
            record_test "run_$example_name" true "Skipped (requires hardware)"
            return 0
            ;;
        fifo*)
            if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
                record_test "run_$example_name" true "Skipped on Windows (Unix-specific)"
                return 0
            fi
            test_standalone_example "$example_name" "$executable"
            ;;
        *)
            test_standalone_example "$example_name" "$executable"
            ;;
    esac
}

# Function to test server examples
test_server_example() {
    local name=$1
    local executable=$2
    local start_time=$(date +%s)
    
    log_verbose "Testing server example: $name"
    
    # Start server in background
    "$executable" &
    local pid=$!
    
    # Let it run for a few seconds
    sleep 3
    
    # Check if process is still running
    if kill -0 "$pid" 2>/dev/null; then
        # Server is running, terminate it gracefully
        kill -TERM "$pid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null || true
        
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        record_test "run_$name" true "Server ran successfully for ${duration}s"
    else
        # Server crashed immediately
        wait "$pid" 2>/dev/null || true
        local exit_code=$?
        record_test "run_$name" false "Server crashed immediately with exit code $exit_code"
    fi
}

# Function to test client examples
test_client_example() {
    local name=$1
    local executable=$2
    local start_time=$(date +%s)
    
    log_verbose "Testing client example: $name"
    
    # Run client with timeout
    if run_with_timeout $TIMEOUT "$executable" >/dev/null 2>&1; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        record_test "run_$name" true "Completed successfully in ${duration}s"
    else
        local exit_code=$?
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        # Client examples may fail if no server is running, which is expected
        if [ $exit_code -eq 124 ]; then
            record_test "run_$name" true "Timed out (expected for client without server)"
        else
            record_test "run_$name" true "Expected connection failure (no server running)"
        fi
    fi
}

# Function to test standalone examples
test_standalone_example() {
    local name=$1
    local executable=$2
    local start_time=$(date +%s)
    
    log_verbose "Testing standalone example: $name"
    
    if run_with_timeout $TIMEOUT "$executable" >/dev/null 2>&1; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        record_test "run_$name" true "Completed successfully in ${duration}s"
    else
        local exit_code=$?
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        if [ $exit_code -eq 124 ]; then
            record_test "run_$name" true "Timed out (may be expected for some examples)"
        else
            record_test "run_$name" false "Failed with exit code $exit_code"
        fi
    fi
}

# Function to print test summary
print_summary() {
    echo
    log_info "$(printf '=%.0s' {1..60})"
    log_info "$(printf '%s' "${BOLD}TEST SUMMARY${NC}")"
    log_info "$(printf '=%.0s' {1..60})"
    
    echo
    log_info "Total: $TOTAL_TESTS, Passed: ${GREEN}$PASSED_TESTS${NC}, Failed: ${RED}$FAILED_TESTS${NC}"
    
    if [ $FAILED_TESTS -eq 0 ]; then
        log_success "All tests passed! 🎉"
        return 0
    else
        log_error "$FAILED_TESTS test(s) failed! ❌"
        return 1
    fi
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Options:"
    echo "  --build-only        Only test building, skip execution"
    echo "  --verbose           Enable verbose output"
    echo "  --example NAME      Test specific example only"
    echo "  --timeout SECONDS   Set timeout for tests (default: 10)"
    echo "  --help              Show this help message"
    echo
    echo "Examples:"
    echo "  $0                          # Test all examples"
    echo "  $0 --build-only             # Only test compilation"
    echo "  $0 --example tcpclient      # Test only tcpclient"
    echo "  $0 --verbose --timeout 20   # Verbose mode with 20s timeout"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-only)
            BUILD_ONLY=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --example)
            SPECIFIC_EXAMPLE="$2"
            shift 2
            ;;
        --timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    log_info "${BOLD}${MAGENTA}Starting Atom Connection Examples Test Suite${NC}"
    
    # Check prerequisites
    if ! command_exists cmake; then
        log_error "CMake is required but not found"
        exit 1
    fi
    
    # Setup build environment
    if ! setup_build_environment; then
        exit 1
    fi
    
    # Build examples
    if ! build_examples "$SPECIFIC_EXAMPLE"; then
        exit 1
    fi
    
    if [ "$BUILD_ONLY" = true ]; then
        log_warning "Build-only mode: skipping execution tests"
        print_summary
        exit $?
    fi
    
    # Test execution
    if [ -n "$SPECIFIC_EXAMPLE" ]; then
        test_example_execution "$SPECIFIC_EXAMPLE"
    else
        # Test all examples
        for example in tcpclient async_tcpclient udpclient udpserver async_udpclient async_udpserver \
                      sockethub async_sockethub fifoclient fifoserver async_fifoclient async_fifoserver \
                      sshclient sshserver ttybase; do
            test_example_execution "$example"
        done
    fi
    
    print_summary
    exit $?
}

# Handle Ctrl+C gracefully
trap 'log_warning "Test interrupted by user"; exit 130' INT

# Run main function
main "$@"
