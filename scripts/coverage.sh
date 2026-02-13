#!/bin/bash

# coverage.sh - Comprehensive coverage analysis script for Atom project
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_DIR="build"
COVERAGE_TYPE="full"
MODULE=""
OPEN_REPORT=false
CLEAN_BUILD=false

# Function to print usage
print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -b, --build-dir DIR     Build directory (default: build)"
    echo "  -t, --type TYPE         Coverage type: full, module, reset, report (default: full)"
    echo "  -m, --module MODULE     Module name for module-specific coverage"
    echo "  -o, --open              Open HTML report in browser after generation"
    echo "  -c, --clean             Clean build before running coverage"
    echo ""
    echo "Coverage types:"
    echo "  full     - Run all tests and generate complete coverage report"
    echo "  module   - Generate coverage for specific module (requires -m)"
    echo "  reset    - Reset coverage counters only"
    echo "  report   - Generate report from existing coverage data"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Full coverage analysis"
    echo "  $0 -t module -m algorithm             # Coverage for algorithm module"
    echo "  $0 -t reset                           # Reset coverage counters"
    echo "  $0 -o                                 # Full coverage and open report"
    echo "  $0 -c -t full                         # Clean build and full coverage"
}

# Function to check if required tools are available
check_tools() {
    local missing_tools=()

    if ! command -v gcov &> /dev/null; then
        missing_tools+=("gcov")
    fi

    if ! command -v lcov &> /dev/null; then
        missing_tools+=("lcov")
    fi

    if ! command -v genhtml &> /dev/null; then
        missing_tools+=("genhtml")
    fi

    if [ ${#missing_tools[@]} -ne 0 ]; then
        echo -e "${RED}Error: Missing required tools: ${missing_tools[*]}${NC}"
        echo "Please install lcov package (includes gcov, lcov, and genhtml)"
        echo "Ubuntu/Debian: sudo apt-get install lcov"
        echo "CentOS/RHEL: sudo yum install lcov"
        echo "macOS: brew install lcov"
        exit 1
    fi
}

# Function to configure and build with coverage
build_with_coverage() {
    echo -e "${BLUE}Configuring build with coverage enabled...${NC}"

    if [ "$CLEAN_BUILD" = true ]; then
        echo -e "${YELLOW}Cleaning build directory...${NC}"
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake .. -DCMAKE_BUILD_TYPE=Debug \
             -DATOM_ENABLE_COVERAGE=ON \
             -DATOM_COVERAGE_HTML=ON \
             -DATOM_BUILD_TESTS=ON

    echo -e "${BLUE}Building project...${NC}"
    make -j$(nproc)

    cd ..
}

# Function to run full coverage analysis
run_full_coverage() {
    echo -e "${GREEN}Running full coverage analysis...${NC}"
    cd "$BUILD_DIR"
    make coverage
    cd ..

    local report_path="$BUILD_DIR/coverage/html/index.html"
    if [ -f "$report_path" ]; then
        echo -e "${GREEN}Coverage report generated: $report_path${NC}"
        if [ "$OPEN_REPORT" = true ]; then
            open_report "$report_path"
        fi
    else
        echo -e "${RED}Error: Coverage report not found${NC}"
        exit 1
    fi
}

# Function to run module-specific coverage
run_module_coverage() {
    if [ -z "$MODULE" ]; then
        echo -e "${RED}Error: Module name required for module coverage${NC}"
        exit 1
    fi

    echo -e "${GREEN}Running coverage analysis for module: $MODULE${NC}"
    cd "$BUILD_DIR"
    make "coverage-$MODULE"
    cd ..

    local report_path="$BUILD_DIR/coverage/${MODULE}_html/index.html"
    if [ -f "$report_path" ]; then
        echo -e "${GREEN}Module coverage report generated: $report_path${NC}"
        if [ "$OPEN_REPORT" = true ]; then
            open_report "$report_path"
        fi
    else
        echo -e "${RED}Error: Module coverage report not found${NC}"
        exit 1
    fi
}

# Function to reset coverage counters
reset_coverage() {
    echo -e "${YELLOW}Resetting coverage counters...${NC}"
    cd "$BUILD_DIR"
    make coverage-reset
    cd ..
    echo -e "${GREEN}Coverage counters reset${NC}"
}

# Function to generate report from existing data
generate_report() {
    echo -e "${BLUE}Generating coverage report from existing data...${NC}"
    cd "$BUILD_DIR"
    make coverage-capture coverage-html
    cd ..

    local report_path="$BUILD_DIR/coverage/html/index.html"
    if [ -f "$report_path" ]; then
        echo -e "${GREEN}Coverage report generated: $report_path${NC}"
        if [ "$OPEN_REPORT" = true ]; then
            open_report "$report_path"
        fi
    else
        echo -e "${RED}Error: Coverage report not found${NC}"
        exit 1
    fi
}

# Function to open report in browser
open_report() {
    local report_path="$1"
    echo -e "${BLUE}Opening coverage report in browser...${NC}"

    if command -v xdg-open &> /dev/null; then
        xdg-open "$report_path"
    elif command -v open &> /dev/null; then
        open "$report_path"
    elif command -v start &> /dev/null; then
        start "$report_path"
    else
        echo -e "${YELLOW}Cannot open browser automatically. Please open: $report_path${NC}"
    fi
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_usage
            exit 0
            ;;
        -b|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -t|--type)
            COVERAGE_TYPE="$2"
            shift 2
            ;;
        -m|--module)
            MODULE="$2"
            shift 2
            ;;
        -o|--open)
            OPEN_REPORT=true
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

# Validate coverage type
case $COVERAGE_TYPE in
    full|module|reset|report)
        ;;
    *)
        echo -e "${RED}Invalid coverage type: $COVERAGE_TYPE${NC}"
        print_usage
        exit 1
        ;;
esac

# Main execution
echo -e "${BLUE}Atom Coverage Analysis Tool${NC}"
echo -e "${BLUE}===========================${NC}"

check_tools

# Build if necessary
if [ "$COVERAGE_TYPE" != "reset" ] && [ "$COVERAGE_TYPE" != "report" ]; then
    build_with_coverage
elif [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found. Please build first.${NC}"
    exit 1
fi

# Execute coverage analysis based on type
case $COVERAGE_TYPE in
    full)
        run_full_coverage
        ;;
    module)
        run_module_coverage
        ;;
    reset)
        reset_coverage
        ;;
    report)
        generate_report
        ;;
esac

echo -e "${GREEN}Coverage analysis completed successfully!${NC}"
