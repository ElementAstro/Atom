// Test runner for header-only test files in the system module
// This file includes all header-only test files to ensure they are compiled and
// run
//
// This project is licensed under the terms of the GPL3 license.
// Author: Max Qian
// License: GPL3

#include <gtest/gtest.h>

// Include all header-only test files for the system module
// These files contain complete test implementations in headers

// Scheduling tests
#include "test_crontab.hpp"

// System information tests
#include "test_env.hpp"
#include "test_stat.hpp"
#include "test_user.hpp"

// Hardware tests
#include "test_gpio.hpp"

// Registry tests
#include "test_lregistry.hpp"

// Network tests
#include "test_network_manager.hpp"

// Process management tests
#include "test_pidwatcher.hpp"

// Signal handling tests
#include "test_signal.hpp"

// Main function is provided by gtest_main library (linked via atom-test-common)
// This file just ensures all header-only tests are included in the build
