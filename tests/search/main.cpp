#include <gtest/gtest.h>

#include "test_cache.hpp"  // Re-enabled - compilation issues resolved
#include "test_lru.hpp"    // Re-enabled - compilation issues resolved
#include "test_search.hpp"
#include "test_sqlite.hpp"  // Re-enabled - linking issues resolved
#include "test_ttl.hpp"    // Re-enabled - compilation issues resolved

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
