// Unit tests for atom::type::FlatMap (atom/type/flatmap.hpp).
//
// The previous revision of this file targeted a long-removed `QuickFlatMap`
// type (a five-template-parameter, multimap-capable, sorted-vector variant with
// camelCase methods). The header now exposes a single `FlatMap<Key, Value,
// Compare, SafetyMode>`, so this test was rewritten against that real API.
//
// Helpers live in a named namespace so the file is safe to compile through the
// test_header_only.cpp aggregator (an anonymous namespace would collide with
// the other aggregated headers).

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <thread>
#include <vector>

#include "atom/type/flatmap.hpp"

namespace flatmap_test {

using atom::type::FlatMap;
using atom::type::ThreadSafetyMode;

using IntMap = FlatMap<int, int>;
using StringMap = FlatMap<std::string, int>;
using SafeIntMap =
    FlatMap<int, int, std::less<int>, ThreadSafetyMode::ReadWrite>;

// Collect a map's contents into a std::map for order-independent comparison.
template <typename Map>
std::map<typename Map::key_type, typename Map::mapped_type> toStdMap(
    const Map& m) {
    std::map<typename Map::key_type, typename Map::mapped_type> out;
    for (const auto& [k, v] : m) {
        out.emplace(k, v);
    }
    return out;
}

}  // namespace flatmap_test

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TEST(FlatMapTest, DefaultConstructIsEmpty) {
    flatmap_test::IntMap m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.begin(), m.end());
}

TEST(FlatMapTest, InitializerListConstruct) {
    flatmap_test::IntMap m{{1, 10}, {2, 20}, {3, 30}};
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(1), 10);
    EXPECT_EQ(m.at(2), 20);
    EXPECT_EQ(m.at(3), 30);
}

TEST(FlatMapTest, RangeConstruct) {
    std::vector<std::pair<int, int>> data{{1, 10}, {2, 20}};
    flatmap_test::IntMap m(data.begin(), data.end());
    EXPECT_EQ(m.size(), 2u);
    EXPECT_TRUE(m.contains(1));
    EXPECT_TRUE(m.contains(2));
}

TEST(FlatMapTest, CopyAndMove) {
    flatmap_test::IntMap original{{1, 10}, {2, 20}};

    flatmap_test::IntMap copy(original);
    EXPECT_EQ(copy.size(), 2u);
    EXPECT_EQ(copy.at(1), 10);
    EXPECT_EQ(original.size(), 2u);  // copy did not steal

    flatmap_test::IntMap moved(std::move(original));
    EXPECT_EQ(moved.size(), 2u);
    EXPECT_EQ(moved.at(2), 20);

    flatmap_test::IntMap assigned;
    assigned = copy;
    EXPECT_EQ(assigned.size(), 2u);

    flatmap_test::IntMap moveAssigned;
    moveAssigned = std::move(copy);
    EXPECT_EQ(moveAssigned.size(), 2u);
}

// ---------------------------------------------------------------------------
// Insertion semantics — insert() updates existing keys (no duplicates)
// ---------------------------------------------------------------------------
TEST(FlatMapTest, InsertNewAndDuplicateKey) {
    flatmap_test::IntMap m;

    auto [it1, inserted1] = m.insert({1, 100});
    EXPECT_TRUE(inserted1);
    EXPECT_EQ(it1->second, 100);
    EXPECT_EQ(m.size(), 1u);

    // Same key: FlatMap assigns and reports "not inserted" (no duplicate row).
    auto [it2, inserted2] = m.insert({1, 200});
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at(1), 200);
}

TEST(FlatMapTest, InsertOrAssign) {
    flatmap_test::IntMap m;
    auto [itNew, insertedNew] = m.insert_or_assign(1, 100);
    EXPECT_TRUE(insertedNew);
    EXPECT_EQ(itNew->second, 100);
    EXPECT_EQ(m.at(1), 100);

    auto [itUpd, insertedUpd] = m.insert_or_assign(1, 150);
    EXPECT_FALSE(insertedUpd);
    EXPECT_EQ(itUpd->second, 150);
    EXPECT_EQ(m.at(1), 150);
    EXPECT_EQ(m.size(), 1u);
}

TEST(FlatMapTest, Emplace) {
    flatmap_test::IntMap m;
    auto [it, inserted] = m.emplace(7, 70);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 7);
    EXPECT_EQ(it->second, 70);
}

TEST(FlatMapTest, SubscriptInsertsAndAccesses) {
    flatmap_test::IntMap m;
    m[1] = 100;  // insert
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at(1), 100);

    m[1] = 250;  // overwrite
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at(1), 250);

    int defaulted = m[2];  // default-construct
    EXPECT_EQ(defaulted, 0);
    EXPECT_EQ(m.size(), 2u);
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------
TEST(FlatMapTest, FindContainsCount) {
    flatmap_test::IntMap m{{1, 10}, {2, 20}};

    EXPECT_NE(m.find(1), m.end());
    EXPECT_EQ(m.find(99), m.end());
    EXPECT_TRUE(m.contains(2));
    EXPECT_FALSE(m.contains(99));
    EXPECT_EQ(m.count(1), 1u);
    EXPECT_EQ(m.count(99), 0u);
}

TEST(FlatMapTest, AtThrowsForMissingKey) {
    flatmap_test::IntMap m{{1, 10}};
    EXPECT_EQ(m.at(1), 10);
    EXPECT_THROW(m.at(2), atom::type::exceptions::key_not_found_error);

    const flatmap_test::IntMap& cm = m;
    EXPECT_EQ(cm.at(1), 10);
    EXPECT_THROW(cm.at(2), atom::type::exceptions::key_not_found_error);
}

TEST(FlatMapTest, TryGet) {
    flatmap_test::IntMap m{{1, 10}};
    auto present = m.try_get(1);
    ASSERT_TRUE(present.has_value());
    EXPECT_EQ(*present, 10);
    EXPECT_FALSE(m.try_get(2).has_value());
}

// ---------------------------------------------------------------------------
// Erasure
// ---------------------------------------------------------------------------
TEST(FlatMapTest, EraseByKey) {
    flatmap_test::IntMap m{{1, 10}, {2, 20}, {3, 30}};
    EXPECT_EQ(m.erase(2), 1u);
    EXPECT_FALSE(m.contains(2));
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.erase(2), 0u);  // already gone
}

TEST(FlatMapTest, EraseByIteratorAndRange) {
    flatmap_test::IntMap m{{1, 10}, {2, 20}, {3, 30}, {4, 40}};

    auto it = m.find(1);
    ASSERT_NE(it, m.end());
    m.erase(it);
    EXPECT_FALSE(m.contains(1));
    EXPECT_EQ(m.size(), 3u);

    // Range-erase everything that remains.
    m.erase(m.begin(), m.end());
    EXPECT_TRUE(m.empty());
}

// ---------------------------------------------------------------------------
// Capacity / clear
// ---------------------------------------------------------------------------
TEST(FlatMapTest, ReserveClear) {
    flatmap_test::IntMap m;
    m.reserve(64);
    EXPECT_GE(m.capacity(), 64u);
    EXPECT_TRUE(m.empty());

    m.insert({1, 10});
    m.insert({2, 20});
    EXPECT_EQ(m.size(), 2u);

    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
}

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------
TEST(FlatMapTest, IterationVisitsAllElements) {
    flatmap_test::IntMap m{{1, 10}, {2, 20}, {3, 30}};
    auto expected = std::map<int, int>{{1, 10}, {2, 20}, {3, 30}};
    EXPECT_EQ(flatmap_test::toStdMap(m), expected);
}

// ---------------------------------------------------------------------------
// String keys
// ---------------------------------------------------------------------------
TEST(FlatMapTest, StringKeys) {
    flatmap_test::StringMap m;
    m.insert({"apple", 1});
    m.insert({"banana", 2});
    m["cherry"] = 3;

    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at("banana"), 2);
    EXPECT_TRUE(m.contains("cherry"));
    EXPECT_EQ(m.erase("apple"), 1u);
    EXPECT_FALSE(m.contains("apple"));
}

// ---------------------------------------------------------------------------
// Free operator== and swap
// ---------------------------------------------------------------------------
TEST(FlatMapTest, EqualityAndSwap) {
    flatmap_test::IntMap a{{1, 10}, {2, 20}};
    flatmap_test::IntMap b{{1, 10}, {2, 20}};
    flatmap_test::IntMap c{{1, 10}, {2, 99}};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);

    using std::swap;
    swap(a, c);
    EXPECT_EQ(a.at(2), 99);
    EXPECT_EQ(c.at(2), 20);
}

// ---------------------------------------------------------------------------
// Thread-safe mode
// ---------------------------------------------------------------------------
TEST(FlatMapTest, ThreadSafeModeSingleThreaded) {
    flatmap_test::SafeIntMap m;
    m.insert({1, 10});
    m.insert_or_assign(1, 15);
    m[2] = 20;
    EXPECT_EQ(m.at(1), 15);
    EXPECT_EQ(m.at(2), 20);
    EXPECT_EQ(m.size(), 2u);
}

TEST(FlatMapTest, ThreadSafeConcurrentInsertDistinctKeys) {
    flatmap_test::SafeIntMap m;
    constexpr int kThreads = 4;
    constexpr int kPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&m, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                int key = t * kPerThread + i;
                m.insert_or_assign(key, key * 2);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(m.size(), static_cast<size_t>(kThreads * kPerThread));
    for (int k = 0; k < kThreads * kPerThread; ++k) {
        auto v = m.try_get(k);
        ASSERT_TRUE(v.has_value()) << "missing key " << k;
        EXPECT_EQ(*v, k * 2);
    }
}
