/**
 * @file interface_test_runner.cpp
 * @brief Interface validation functions (no main - used by other tests)
 * @date 2025-08-04
 */

#include <iostream>

// Mock spdlog to avoid linking issues
namespace spdlog {
    template<typename... Args>
    void info(const char* fmt, Args&&... args) {
        // No-op for interface testing
    }
}

#include "interface_validation_test.hpp"

// Interface validation functions are defined in the header
// This file is included for compilation but doesn't have main()
