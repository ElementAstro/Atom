#include "test_cache.hpp"
#include "test_lru.hpp"
#include "test_search.hpp"
#include "test_ttl.hpp"
#include "test_search_enhanced.hpp"
#include "test_similarity_search.hpp"
#include "test_boolean_search.hpp"
#include "test_performance.hpp"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
