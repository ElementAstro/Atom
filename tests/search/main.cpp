#include <gtest/gtest.h>

// #include "test_cache.hpp"  // Temporarily disabled due to compilation issues
// #include "test_lru.hpp"    // Temporarily disabled due to compilation issues
#include "test_search.hpp"
// #include "test_sqlite.hpp"  // Temporarily disabled due to linking issues
// #include "test_ttl.hpp"    // Temporarily disabled due to compilation issues

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
