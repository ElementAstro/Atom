#include <gtest/gtest.h>

// Comprehensive test runner for utils module
// Compilation issues have been resolved

// Include all working test headers
#include "test_anyutils.hpp"
#include "test_switch.hpp"
#include "test_to_any.hpp"
#include "test_to_byte.hpp"
#include "../string/test_string.hpp"
#include "../string/test_to_string.hpp"
#include "../string/test_valid_string.hpp"
#include "../time/test_time.hpp"
#include "../time/test_stopwatcher.hpp"
#include "../crypto/test_aes.hpp"
#include "../container/test_container.hpp"
#include "../math/test_random.hpp"

// Simple test to verify the test framework works
TEST(UtilsModuleTest, BasicTest) {
    EXPECT_TRUE(true);
    EXPECT_EQ(1 + 1, 2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
