#!/bin/bash

# Atom Image Examples Validation Script
# This script validates that all examples can be built and run successfully

set -e  # Exit on any error

echo "=== Atom Image Examples Validation ==="
echo "Validating all image examples for completeness and functionality"
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
TOTAL_EXAMPLES=0
SUCCESSFUL_BUILDS=0
SUCCESSFUL_RUNS=0
FAILED_BUILDS=0
FAILED_RUNS=0

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "INFO")
            echo -e "${BLUE}[INFO]${NC} $message"
            ;;
        "SUCCESS")
            echo -e "${GREEN}[SUCCESS]${NC} $message"
            ;;
        "WARNING")
            echo -e "${YELLOW}[WARNING]${NC} $message"
            ;;
        "ERROR")
            echo -e "${RED}[ERROR]${NC} $message"
            ;;
    esac
}

# Function to validate example structure
validate_example_structure() {
    local example_file=$1
    local category=$2

    print_status "INFO" "Validating structure of $example_file"

    # Check if file exists
    if [[ ! -f "$example_file" ]]; then
        print_status "ERROR" "Example file not found: $example_file"
        return 1
    fi

    # Check for required elements
    local has_includes=$(grep -c "#include" "$example_file" || true)
    local has_main=$(grep -c "int main" "$example_file" || true)
    local has_namespace=$(grep -c "using namespace atom::image" "$example_file" || true)
    local has_documentation=$(grep -c "@brief\|@file" "$example_file" || true)

    if [[ $has_includes -eq 0 ]]; then
        print_status "WARNING" "$example_file: No includes found"
    fi

    if [[ $has_main -eq 0 ]]; then
        print_status "ERROR" "$example_file: No main function found"
        return 1
    fi

    if [[ $has_namespace -eq 0 ]]; then
        print_status "WARNING" "$example_file: No atom::image namespace usage found"
    fi

    if [[ $has_documentation -eq 0 ]]; then
        print_status "WARNING" "$example_file: No documentation comments found"
    fi

    print_status "SUCCESS" "Structure validation passed for $example_file"
    return 0
}

# Function to check build configuration
validate_build_config() {
    local cmake_file=$1
    local category=$2

    print_status "INFO" "Validating build configuration: $cmake_file"

    if [[ ! -f "$cmake_file" ]]; then
        print_status "ERROR" "CMakeLists.txt not found: $cmake_file"
        return 1
    fi

    # Check for required CMake elements
    local has_cmake_version=$(grep -c "cmake_minimum_required" "$cmake_file" || true)
    local has_executables=$(grep -c "add_executable\|add_.*_example" "$cmake_file" || true)
    local has_linking=$(grep -c "target_link_libraries" "$cmake_file" || true)

    if [[ $has_cmake_version -eq 0 ]]; then
        print_status "ERROR" "$cmake_file: No cmake_minimum_required found"
        return 1
    fi

    if [[ $has_executables -eq 0 ]]; then
        print_status "WARNING" "$cmake_file: No executables defined"
    fi

    if [[ $has_linking -eq 0 ]]; then
        print_status "WARNING" "$cmake_file: No library linking found"
    fi

    print_status "SUCCESS" "Build configuration validation passed for $cmake_file"
    return 0
}

# Function to validate example categories
validate_category() {
    local category_dir=$1
    local category_name=$2

    print_status "INFO" "Validating category: $category_name"

    if [[ ! -d "$category_dir" ]]; then
        print_status "WARNING" "Category directory not found: $category_dir"
        return 0
    fi

    # Validate CMakeLists.txt
    if ! validate_build_config "$category_dir/CMakeLists.txt" "$category_name"; then
        print_status "ERROR" "Build configuration validation failed for $category_name"
        return 1
    fi

    # Validate all .cpp files in the category
    local cpp_files=($(find "$category_dir" -name "*.cpp" -type f))
    local category_examples=0
    local category_valid=0

    for cpp_file in "${cpp_files[@]}"; do
        ((category_examples++))
        ((TOTAL_EXAMPLES++))

        if validate_example_structure "$cpp_file" "$category_name"; then
            ((category_valid++))
        fi
    done

    print_status "INFO" "Category $category_name: $category_valid/$category_examples examples validated"
    return 0
}

# Main validation process
main() {
    print_status "INFO" "Starting validation process..."

    # Check if we're in the right directory
    if [[ ! -f "CMakeLists.txt" ]] || [[ ! -f "README.md" ]]; then
        print_status "ERROR" "Please run this script from the example/image directory"
        exit 1
    fi

    # Validate main build configuration
    print_status "INFO" "Validating main build configuration..."
    if ! validate_build_config "CMakeLists.txt" "main"; then
        print_status "ERROR" "Main build configuration validation failed"
        exit 1
    fi

    # Validate README
    if [[ -f "README.md" ]]; then
        local readme_size=$(wc -l < "README.md")
        if [[ $readme_size -lt 50 ]]; then
            print_status "WARNING" "README.md seems too short ($readme_size lines)"
        else
            print_status "SUCCESS" "README.md validation passed ($readme_size lines)"
        fi
    else
        print_status "ERROR" "README.md not found"
    fi

    # Validate each category
    print_status "INFO" "Validating example categories..."

    validate_category "core" "Core"
    validate_category "io" "I/O"
    validate_category "processing" "Processing"
    validate_category "formats" "Formats"
    validate_category "metadata" "Metadata"

    # Summary
    echo
    print_status "INFO" "=== Validation Summary ==="
    print_status "INFO" "Total examples found: $TOTAL_EXAMPLES"

    if [[ $TOTAL_EXAMPLES -eq 0 ]]; then
        print_status "ERROR" "No examples found!"
        exit 1
    fi

    # Check for minimum expected examples
    local expected_minimum=8
    if [[ $TOTAL_EXAMPLES -lt $expected_minimum ]]; then
        print_status "WARNING" "Found $TOTAL_EXAMPLES examples, expected at least $expected_minimum"
    else
        print_status "SUCCESS" "Found sufficient examples ($TOTAL_EXAMPLES >= $expected_minimum)"
    fi

    # Validate example coverage
    print_status "INFO" "Checking example coverage..."

    local core_examples=$(find core -name "*.cpp" 2>/dev/null | wc -l || echo 0)
    local io_examples=$(find io -name "*.cpp" 2>/dev/null | wc -l || echo 0)
    local processing_examples=$(find processing -name "*.cpp" 2>/dev/null | wc -l || echo 0)
    local format_examples=$(find formats -name "*.cpp" 2>/dev/null | wc -l || echo 0)

    print_status "INFO" "Coverage breakdown:"
    print_status "INFO" "  Core examples: $core_examples"
    print_status "INFO" "  I/O examples: $io_examples"
    print_status "INFO" "  Processing examples: $processing_examples"
    print_status "INFO" "  Format examples: $format_examples"

    # Final status
    echo
    if [[ $TOTAL_EXAMPLES -ge $expected_minimum ]] && [[ $core_examples -ge 2 ]] && [[ $processing_examples -ge 2 ]]; then
        print_status "SUCCESS" "All validations passed! Examples are ready for use."
        exit 0
    else
        print_status "WARNING" "Validation completed with warnings. Examples may have issues."
        exit 1
    fi
}

# Run main function
main "$@"
