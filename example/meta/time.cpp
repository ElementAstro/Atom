/**
 * @file time.cpp
 * @brief Comprehensive example demonstrating compile-time information utilities
 *
 * This example shows how to:
 * - Retrieve compile-time information
 * - Use compile-time constants and macros
 * - Demonstrate build information tracking
 * - Show version and timestamp utilities
 * - Create build metadata systems
 * - Implement compile-time diagnostics
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>


// Atom Meta time utilities
#include "atom/meta/time.hpp"

using namespace atom::meta;

// Additional compile-time information macros
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Version information (would typically come from build system)
#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 3
#define VERSION_BUILD 456

// Build configuration
#ifdef DEBUG
#define BUILD_TYPE "Debug"
#else
#define BUILD_TYPE "Release"
#endif

#ifdef _WIN32
#define TARGET_PLATFORM "Windows"
#elif defined(__linux__)
#define TARGET_PLATFORM "Linux"
#elif defined(__APPLE__)
#define TARGET_PLATFORM "macOS"
#else
#define TARGET_PLATFORM "Unknown"
#endif

// Compiler information
#ifdef _MSC_VER
#define COMPILER_NAME "MSVC"
#define COMPILER_VERSION TOSTRING(_MSC_VER)
#elif defined(__clang__)
#define COMPILER_NAME "Clang"
#define COMPILER_VERSION      \
    TOSTRING(__clang_major__) \
    "." TOSTRING(__clang_minor__) "." TOSTRING(__clang_patchlevel__)
#elif defined(__GNUC__)
#define COMPILER_NAME "GCC"
#define COMPILER_VERSION \
    TOSTRING(__GNUC__)   \
    "." TOSTRING(__GNUC_MINOR__) "." TOSTRING(__GNUC_PATCHLEVEL__)
#else
#define COMPILER_NAME "Unknown"
#define COMPILER_VERSION "Unknown"
#endif

/**
 * @brief Build information structure
 */
struct BuildInfo {
    std::string compile_time;
    std::string version;
    std::string build_type;
    std::string platform;
    std::string compiler;
    std::string compiler_version;
    std::string source_file;
    int line_number;

    BuildInfo(const char* file = __FILE__, int line = __LINE__)
        : compile_time(getCompileTime()),
          version(
              TOSTRING(VERSION_MAJOR) "." TOSTRING(VERSION_MINOR) "." TOSTRING(
                  VERSION_PATCH) "." TOSTRING(VERSION_BUILD)),
          build_type(BUILD_TYPE),
          platform(TARGET_PLATFORM),
          compiler(COMPILER_NAME),
          compiler_version(COMPILER_VERSION),
          source_file(file),
          line_number(line) {}
};

/**
 * @brief Demonstrates basic compile-time information retrieval
 */
void basicCompileTimeExample() {
    std::cout << "\n=== Basic Compile-Time Information Example ===\n";

    try {
        // Get compile time using the meta utility
        std::string compileTime = getCompileTime();
        std::cout << "Compile Time: " << compileTime << "\n";

        // Show raw compile-time macros
        std::cout << "Raw Compile Date: " << __DATE__ << "\n";
        std::cout << "Raw Compile Time: " << __TIME__ << "\n";
        std::cout << "Source File: " << __FILE__ << "\n";
        std::cout << "Current Line: " << __LINE__ << "\n";

        // Show C++ standard version
        std::cout << "C++ Standard: ";
#if __cplusplus >= 202002L
        std::cout << "C++20 or later (" << __cplusplus << ")\n";
#elif __cplusplus >= 201703L
        std::cout << "C++17 (" << __cplusplus << ")\n";
#elif __cplusplus >= 201402L
        std::cout << "C++14 (" << __cplusplus << ")\n";
#elif __cplusplus >= 201103L
        std::cout << "C++11 (" << __cplusplus << ")\n";
#else
        std::cout << "Pre-C++11 (" << __cplusplus << ")\n";
#endif

    } catch (const std::exception& e) {
        std::cerr << "Error in basic compile-time example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates build information tracking
 */
void buildInfoExample() {
    std::cout << "\n=== Build Information Example ===\n";

    try {
        BuildInfo buildInfo;

        std::cout << "Build Information:\n";
        std::cout << "  Version: " << buildInfo.version << "\n";
        std::cout << "  Compile Time: " << buildInfo.compile_time << "\n";
        std::cout << "  Build Type: " << buildInfo.build_type << "\n";
        std::cout << "  Target Platform: " << buildInfo.platform << "\n";
        std::cout << "  Compiler: " << buildInfo.compiler << " "
                  << buildInfo.compiler_version << "\n";
        std::cout << "  Source File: " << buildInfo.source_file << "\n";
        std::cout << "  Line Number: " << buildInfo.line_number << "\n";

        // Show architecture information
        std::cout << "  Architecture: ";
#if defined(_M_X64) || defined(__x86_64__)
        std::cout << "x86_64 (64-bit)\n";
#elif defined(_M_IX86) || defined(__i386__)
        std::cout << "x86 (32-bit)\n";
#elif defined(_M_ARM64) || defined(__aarch64__)
        std::cout << "ARM64 (64-bit)\n";
#elif defined(_M_ARM) || defined(__arm__)
        std::cout << "ARM (32-bit)\n";
#else
        std::cout << "Unknown\n";
#endif

        // Show endianness
        std::cout << "  Endianness: ";
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        std::cout << "Little Endian\n";
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        std::cout << "Big Endian\n";
#else
        std::cout << "Unknown\n";
#endif

    } catch (const std::exception& e) {
        std::cerr << "Error in build info example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates compile-time diagnostics and feature detection
 */
void featureDetectionExample() {
    std::cout << "\n=== Feature Detection Example ===\n";

    try {
        std::cout << "Compiler Feature Support:\n";

// C++ feature detection
#ifdef __cpp_concepts
        std::cout << "  ✓ Concepts (C++20): " << __cpp_concepts << "\n";
#else
        std::cout << "  ✗ Concepts: Not supported\n";
#endif

#ifdef __cpp_modules
        std::cout << "  ✓ Modules (C++20): " << __cpp_modules << "\n";
#else
        std::cout << "  ✗ Modules: Not supported\n";
#endif

#ifdef __cpp_coroutines
        std::cout << "  ✓ Coroutines (C++20): " << __cpp_coroutines << "\n";
#else
        std::cout << "  ✗ Coroutines: Not supported\n";
#endif

#ifdef __cpp_constexpr
        std::cout << "  ✓ Constexpr: " << __cpp_constexpr << "\n";
#else
        std::cout << "  ✗ Constexpr: Not supported\n";
#endif

#ifdef __cpp_if_constexpr
        std::cout << "  ✓ If Constexpr (C++17): " << __cpp_if_constexpr << "\n";
#else
        std::cout << "  ✗ If Constexpr: Not supported\n";
#endif

#ifdef __cpp_structured_bindings
        std::cout << "  ✓ Structured Bindings (C++17): "
                  << __cpp_structured_bindings << "\n";
#else
        std::cout << "  ✗ Structured Bindings: Not supported\n";
#endif

        // Library feature detection
        std::cout << "\nStandard Library Features:\n";

#ifdef __cpp_lib_filesystem
        std::cout << "  ✓ Filesystem (C++17): " << __cpp_lib_filesystem << "\n";
#else
        std::cout << "  ✗ Filesystem: Not supported\n";
#endif

#ifdef __cpp_lib_optional
        std::cout << "  ✓ Optional (C++17): " << __cpp_lib_optional << "\n";
#else
        std::cout << "  ✗ Optional: Not supported\n";
#endif

#ifdef __cpp_lib_variant
        std::cout << "  ✓ Variant (C++17): " << __cpp_lib_variant << "\n";
#else
        std::cout << "  ✗ Variant: Not supported\n";
#endif

#ifdef __cpp_lib_string_view
        std::cout << "  ✓ String View (C++17): " << __cpp_lib_string_view
                  << "\n";
#else
        std::cout << "  ✗ String View: Not supported\n";
#endif

    } catch (const std::exception& e) {
        std::cerr << "Error in feature detection example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates runtime vs compile-time information
 */
void runtimeVsCompileTimeExample() {
    std::cout << "\n=== Runtime vs Compile-Time Information Example ===\n";

    try {
        // Compile-time information (fixed at build time)
        std::cout << "Compile-Time Information (Fixed at Build):\n";
        std::cout << "  Build Timestamp: " << getCompileTime() << "\n";
        std::cout << "  Source File: " << __FILE__ << "\n";
        std::cout << "  Function: " << __FUNCTION__ << "\n";

        // Runtime information (changes during execution)
        std::cout << "\nRuntime Information (Dynamic):\n";

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();

        std::cout << "  Current Time: " << ss.str() << "\n";
        std::cout << "  Execution Line: " << __LINE__ << "\n";

        // Show the difference
        std::cout << "\nTime Difference Analysis:\n";

        // Parse compile time for comparison (simplified)
        std::string compileTimeStr = getCompileTime();
        std::cout << "  Compile Time: " << compileTimeStr << "\n";
        std::cout << "  Runtime: " << ss.str() << "\n";
        std::cout << "  Note: Runtime changes with each execution, compile "
                     "time is fixed\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in runtime vs compile-time example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates practical applications of compile-time information
 */
void practicalApplicationsExample() {
    std::cout << "\n=== Practical Applications Example ===\n";

    try {
        std::cout << "Practical Uses of Compile-Time Information:\n";

        // 1. Version tracking
        std::cout << "\n1. Version Tracking:\n";
        BuildInfo buildInfo;
        std::cout << "   Application Version: " << buildInfo.version << "\n";
        std::cout << "   Built: " << buildInfo.compile_time << "\n";
        std::cout << "   Platform: " << buildInfo.platform << "\n";

        // 2. Debug information
        std::cout << "\n2. Debug Information:\n";
        std::cout << "   Build Type: " << buildInfo.build_type << "\n";
        std::cout << "   Compiler: " << buildInfo.compiler << " "
                  << buildInfo.compiler_version << "\n";

        // 3. Conditional compilation
        std::cout << "\n3. Conditional Compilation:\n";
#ifdef DEBUG
        std::cout << "   Debug mode: Enabled (extra logging, assertions)\n";
#else
        std::cout << "   Release mode: Optimized for performance\n";
#endif

        // 4. Feature flags
        std::cout << "\n4. Feature Availability:\n";
#if __cplusplus >= 202002L
        std::cout << "   C++20 features: Available\n";
#elif __cplusplus >= 201703L
        std::cout << "   C++17 features: Available\n";
#else
        std::cout << "   Legacy C++ mode\n";
#endif

        // 5. Build metadata for logging
        std::cout << "\n5. Build Metadata for Logging:\n";
        std::cout << "   [" << buildInfo.compile_time << "] "
                  << "Application started (v" << buildInfo.version << ", "
                  << buildInfo.build_type << " build)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in practical applications example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating all compile-time information utilities
 */
int main() {
    std::cout << "================================================\n";
    std::cout << "  Atom Meta Compile-Time Information Examples\n";
    std::cout << "================================================\n";

    try {
        basicCompileTimeExample();
        buildInfoExample();
        featureDetectionExample();
        runtimeVsCompileTimeExample();
        practicalApplicationsExample();

        std::cout << "\n=== All Compile-Time Information Examples Completed "
                     "Successfully ===\n";
        std::cout << "The compile-time information system provides:\n";
        std::cout << "  ✓ Build timestamp and version tracking\n";
        std::cout << "  ✓ Compiler and platform detection\n";
        std::cout << "  ✓ Feature availability checking\n";
        std::cout << "  ✓ Debug vs release mode detection\n";
        std::cout << "  ✓ Architecture and endianness information\n";
        std::cout << "  ✓ C++ standard version detection\n";
        std::cout << "  ✓ Source file and line number tracking\n";
        std::cout << "  ✓ Practical build metadata for applications\n";

        // Show final build summary
        std::cout << "\n=== Build Summary ===\n";
        BuildInfo finalInfo;
        std::cout << "This example was compiled:\n";
        std::cout << "  Time: " << finalInfo.compile_time << "\n";
        std::cout << "  Version: " << finalInfo.version << "\n";
        std::cout << "  Platform: " << finalInfo.platform << "\n";
        std::cout << "  Compiler: " << finalInfo.compiler << " "
                  << finalInfo.compiler_version << "\n";
        std::cout << "  Build Type: " << finalInfo.build_type << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
