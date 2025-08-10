#include "test_cache.hpp"
#include "test_lru.hpp"
#include "test_search.hpp"
#include "test_ttl.hpp"
#include "test_search_enhanced.hpp"
#include "test_similarity_search.hpp"
#include "test_boolean_search.hpp"
#include "test_performance.hpp"
#include "test_enhanced_search_methods.hpp"
#include "test_advanced_features.hpp"
#include "test_concurrency_stress.hpp"
#include "test_error_handling.hpp"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
