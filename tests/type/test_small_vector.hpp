#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "atom/type/small_vector.hpp"

// Custom type to test complex object behavior
class TestObject {
public:
    TestObject() : value_(0), copied_(false), moved_(false) {}
    explicit TestObject(int v) : value_(v), copied_(false), moved_(false) {}
    TestObject(const TestObject& other)
        : value_(other.value_), copied_(true), moved_(false) {
        ++copy_count_;
    }
    TestObject(TestObject&& other) noexcept
        : value_(other.value_), copied_(false), moved_(true) {
        other.value_ = 0;
        ++move_count_;
    }

    ~TestObject() { ++destructor_count_; }

    TestObject& operator=(const TestObject& other) {
        if (this != &other) {
            value_ = other.value_;
            copied_ = true;
            moved_ = false;
            ++copy_assign_count_;
        }
        return *this;
    }

    TestObject& operator=(TestObject&& other) noexcept {
        if (this != &other) {
            value_ = other.value_;
            other.value_ = 0;
            copied_ = false;
            moved_ = true;
            ++move_assign_count_;
        }
        return *this;
    }

    bool operator==(const TestObject& other) const {
        return value_ == other.value_;
    }

    bool operator<(const TestObject& other) const {
        return value_ < other.value_;
    }

    int value() const { return value_; }
    bool was_copied() const { return copied_; }
    bool was_moved() const { return moved_; }

    static void reset_counters() {
        copy_count_ = 0;
        move_count_ = 0;
        destructor_count_ = 0;
        copy_assign_count_ = 0;
        move_assign_count_ = 0;
    }

    static int copy_count() { return copy_count_; }
    static int move_count() { return move_count_; }
    static int destructor_count() { return destructor_count_; }
    static int copy_assign_count() { return copy_assign_count_; }
    static int move_assign_count() { return move_assign_count_; }

private:
    int value_;
    bool copied_;
    bool moved_;

    static inline int copy_count_ = 0;
    static inline int move_count_ = 0;
    static inline int destructor_count_ = 0;
    static inline int copy_assign_count_ = 0;
    static inline int move_assign_count_ = 0;
};

// Custom allocator for testing allocator awareness
template <typename T>
class TrackingAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind {
        using other = TrackingAllocator<U>;
    };

    TrackingAllocator() noexcept : id_(next_id_++) {}

    template <typename U>
    TrackingAllocator(const TrackingAllocator<U>& other) noexcept
        : id_(other.id()) {}

    TrackingAllocator(const TrackingAllocator& other) noexcept = default;
    TrackingAllocator& operator=(const TrackingAllocator& other) noexcept =
        default;

    pointer allocate(size_type n) {
        ++allocation_count_;
        total_allocated_ += n;
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    void deallocate(pointer p, size_type n) noexcept {
        ++deallocation_count_;
        total_deallocated_ += n;
        ::operator delete(p);
    }

    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ++construct_count_;
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* p) noexcept {
        ++destroy_count_;
        p->~U();
    }

    bool operator==(const TrackingAllocator& other) const noexcept {
        return id_ == other.id_;
    }

    bool operator!=(const TrackingAllocator& other) const noexcept {
        return !(*this == other);
    }

    int id() const noexcept { return id_; }

    static void reset_counters() {
        allocation_count_ = 0;
        deallocation_count_ = 0;
        construct_count_ = 0;
        destroy_count_ = 0;
        total_allocated_ = 0;
        total_deallocated_ = 0;
    }

    static int allocation_count() { return allocation_count_; }
    static int deallocation_count() { return deallocation_count_; }
    static int construct_count() { return construct_count_; }
    static int destroy_count() { return destroy_count_; }
    static size_t total_allocated() { return total_allocated_; }
    static size_t total_deallocated() { return total_deallocated_; }

private:
    int id_;
    static inline int next_id_ = 0;
    static inline int allocation_count_ = 0;
    static inline int deallocation_count_ = 0;
    static inline int construct_count_ = 0;
    static inline int destroy_count_ = 0;
    static inline size_t total_allocated_ = 0;
    static inline size_t total_deallocated_ = 0;
};

class SmallVectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestObject::reset_counters();
        TrackingAllocator<int>::reset_counters();
    }

    void TearDown() override {}

    // Utility to verify vector contents
    template <typename T, std::size_t N, typename Alloc, typename Container>
    void ExpectVectorContent(const SmallVector<T, N, Alloc>& sv,
                             const Container& expected) {
        ASSERT_EQ(sv.size(), expected.size());
        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(sv[i], expected[i]);
        }
    }

    // Verify if a vector is using inline storage
    template <typename T, std::size_t N, typename Alloc>
    void ExpectInlineStorage(const SmallVector<T, N, Alloc>& sv,
                             bool expected = true) {
        EXPECT_EQ(sv.isUsingInlineStorage(), expected);
    }
};

// Construction Tests
TEST_F(SmallVectorTest, DefaultConstructor) {
    SmallVector<int, 4> sv;
    EXPECT_TRUE(sv.empty());
    EXPECT_EQ(sv.size(), 0u);
    EXPECT_EQ(sv.capacity(), 4u);
    ExpectInlineStorage(sv);
}

TEST_F(SmallVectorTest, ConstructorWithAllocator) {
    TrackingAllocator<int> alloc;
    SmallVector<int, 4, TrackingAllocator<int>> sv(alloc);

    EXPECT_TRUE(sv.empty());
    EXPECT_EQ(sv.size(), 0u);
    EXPECT_EQ(sv.capacity(), 4u);
    ExpectInlineStorage(sv);
    EXPECT_EQ(sv.get_allocator().id(), alloc.id());
}

TEST_F(SmallVectorTest, CountValueConstructor) {
    // Small enough to fit in buffer
    SmallVector<int, 10> sv1(5, 42);
    EXPECT_EQ(sv1.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(sv1[i], 42);
    }
    ExpectInlineStorage(sv1);

    // Large enough to use dynamic storage
    SmallVector<int, 3> sv2(5, 42);
    EXPECT_EQ(sv2.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(sv2[i], 42);
    }
    ExpectInlineStorage(sv2, false);
}

TEST_F(SmallVectorTest, DefaultElementCountConstructor) {
    // Small enough to fit in buffer
    SmallVector<int, 10> sv1(5);
    EXPECT_EQ(sv1.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(sv1[i], 0);  // Default value for int is 0
    }
    ExpectInlineStorage(sv1);

    // Large enough to use dynamic storage
    SmallVector<int, 3> sv2(5);
    EXPECT_EQ(sv2.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(sv2[i], 0);
    }
    ExpectInlineStorage(sv2, false);
}

TEST_F(SmallVectorTest, RangeConstructor) {
    std::vector<int> source{1, 2, 3, 4, 5};

    // Small enough to fit in buffer
    SmallVector<int, 10> sv1(source.begin(), source.end());
    EXPECT_EQ(sv1.size(), 5u);
    ExpectVectorContent(sv1, source);
    ExpectInlineStorage(sv1);

    // Large enough to use dynamic storage
    SmallVector<int, 3> sv2(source.begin(), source.end());
    EXPECT_EQ(sv2.size(), 5u);
    ExpectVectorContent(sv2, source);
    ExpectInlineStorage(sv2, false);
}

TEST_F(SmallVectorTest, CopyConstructor) {
    SmallVector<int, 5> source{1, 2, 3};

    // Copy construction
    SmallVector<int, 5> sv1(source);
    EXPECT_EQ(sv1.size(), 3u);
    ExpectVectorContent(sv1, source);
    ExpectInlineStorage(sv1);

    // Fill source to force dynamic storage
    source.insert(source.end(), {4, 5, 6, 7, 8});

    // Copy with dynamic storage
    SmallVector<int, 5> sv2(source);
    EXPECT_EQ(sv2.size(), 8u);
    ExpectVectorContent(sv2, source);
    ExpectInlineStorage(sv2, false);

    // Different inline capacities
    SmallVector<int, 10> sv3(sv2);
    EXPECT_EQ(sv3.size(), 8u);
    ExpectVectorContent(sv3, sv2);
    ExpectInlineStorage(sv3);  // Should fit in sv3's larger inline storage
}

TEST_F(SmallVectorTest, CopyConstructorWithAllocator) {
    TrackingAllocator<int> alloc1;
    TrackingAllocator<int> alloc2;

    SmallVector<int, 3, TrackingAllocator<int>> source({1, 2, 3, 4, 5}, alloc1);

    // Copy with different allocator
    SmallVector<int, 3, TrackingAllocator<int>> sv(source, alloc2);

    EXPECT_EQ(sv.size(), 5u);
    ExpectVectorContent(sv, source);
    EXPECT_NE(sv.get_allocator().id(), source.get_allocator().id());
    EXPECT_EQ(sv.get_allocator().id(), alloc2.id());
}

TEST_F(SmallVectorTest, MoveConstructor) {
    // Move from inline storage
    {
        SmallVector<int, 5> source{1, 2, 3};

        SmallVector<int, 5> sv(std::move(source));
        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3});
        ExpectInlineStorage(sv);

        // Source should be in valid but unspecified state
        // In our implementation, it should be empty
        EXPECT_TRUE(source.empty());
    }

    // Move from dynamic storage
    {
        SmallVector<int, 3> source{1, 2, 3, 4, 5};

        SmallVector<int, 3> sv(std::move(source));
        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5});
        ExpectInlineStorage(sv, false);

        // Source should be in valid but reset state
        EXPECT_TRUE(source.empty());
        ExpectInlineStorage(source);
    }

    // Move between different capacities (inline to inline)
    {
        SmallVector<int, 3> source{1, 2, 3};

        SmallVector<int, 5> sv(std::move(source));
        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3});
        ExpectInlineStorage(sv);

        EXPECT_TRUE(source.empty());
        ExpectInlineStorage(source);
    }

    // Move between different capacities (dynamic to inline possible)
    {
        SmallVector<int, 3> source{1, 2, 3, 4, 5};

        SmallVector<int, 10> sv(std::move(source));
        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5});
        // Implementation defined - could be either depending on how moveFrom is
        // implemented We're copying elements from source's dynamic to sv's
        // inline
        ExpectInlineStorage(sv);

        EXPECT_TRUE(source.empty());
        ExpectInlineStorage(source);
    }
}

TEST_F(SmallVectorTest, MoveConstructorWithAllocator) {
    TrackingAllocator<int> alloc1;
    TrackingAllocator<int> alloc2;

    // When allocators are the same (equivalent)
    {
        SmallVector<int, 3, TrackingAllocator<int>> source({1, 2, 3, 4, 5},
                                                           alloc1);

        SmallVector<int, 3, TrackingAllocator<int>> sv(std::move(source),
                                                       alloc1);

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5});
        EXPECT_EQ(sv.get_allocator().id(), alloc1.id());

        // Source should be empty but have the same allocator
        EXPECT_TRUE(source.empty());
        EXPECT_EQ(source.get_allocator().id(), alloc1.id());
    }

    // When allocators are different
    {
        SmallVector<int, 3, TrackingAllocator<int>> source({1, 2, 3, 4, 5},
                                                           alloc1);

        int alloc_count_before = TrackingAllocator<int>::allocation_count();
        SmallVector<int, 3, TrackingAllocator<int>> sv(std::move(source),
                                                       alloc2);

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5});
        EXPECT_EQ(sv.get_allocator().id(), alloc2.id());

        // Should have allocated new memory with alloc2
        EXPECT_GT(TrackingAllocator<int>::allocation_count(),
                  alloc_count_before);
    }
}

TEST_F(SmallVectorTest, InitializerListConstructor) {
    // Small enough to fit in buffer
    SmallVector<int, 5> sv1{1, 2, 3};
    EXPECT_EQ(sv1.size(), 3u);
    ExpectVectorContent(sv1, std::vector<int>{1, 2, 3});
    ExpectInlineStorage(sv1);

    // Large enough to use dynamic storage
    SmallVector<int, 2> sv2{1, 2, 3, 4, 5};
    EXPECT_EQ(sv2.size(), 5u);
    ExpectVectorContent(sv2, std::vector<int>{1, 2, 3, 4, 5});
    ExpectInlineStorage(sv2, false);
}

// Assignment Tests
TEST_F(SmallVectorTest, CopyAssignment) {
    // Both inline
    {
        SmallVector<int, 5> source{1, 2, 3};
        SmallVector<int, 5> sv{4, 5};

        sv = source;

        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, source);
        ExpectInlineStorage(sv);
    }

    // Source dynamic, target inline
    {
        SmallVector<int, 3> source{1, 2, 3, 4, 5};
        SmallVector<int, 5> sv{4, 5};

        sv = source;

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, source);
        ExpectInlineStorage(sv);
    }

    // Both dynamic
    {
        SmallVector<int, 3> source{1, 2, 3, 4, 5};
        SmallVector<int, 3> sv{4, 5, 6, 7, 8, 9};

        sv = source;

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, source);
        ExpectInlineStorage(sv, false);
    }

    // Source inline, target dynamic
    {
        SmallVector<int, 5> source{1, 2, 3};
        SmallVector<int, 3> sv{4, 5, 6, 7, 8};

        sv = source;

        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, source);
        // Could be either inline or dynamic depending on implementation
        // Our implementation shrinks to fit when assigning smaller number of
        // elements
    }

    // Self assignment
    {
        SmallVector<int, 5> sv{1, 2, 3};
        SmallVector<int, 5>& ref = sv;

        ref = sv;

        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3});
        ExpectInlineStorage(sv);
    }
}

TEST_F(SmallVectorTest, MoveAssignment) {
    // Both inline
    {
        SmallVector<int, 5> source{1, 2, 3};
        SmallVector<int, 5> sv{4, 5};

        sv = std::move(source);

        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3});
        ExpectInlineStorage(sv);

        // Source should be in valid but unspecified state
        EXPECT_TRUE(source.empty());
    }

    // Source dynamic, target inline
    {
        SmallVector<int, 3> source{1, 2, 3, 4, 5};
        SmallVector<int, 5> sv{4, 5};

        sv = std::move(source);

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5});

        // Source should be empty
        EXPECT_TRUE(source.empty());
        ExpectInlineStorage(source);
    }

    // Self move-assignment (should be handled safely)
    {
        SmallVector<int, 5> sv{1, 2, 3};
        SmallVector<int, 5>& ref = sv;

        ref = std::move(sv);  // Self-move is undefined behavior, but should be
                              // handled safely

        // State should still be valid
        EXPECT_EQ(sv.size(), 3u);
    }
}

TEST_F(SmallVectorTest, InitializerListAssignment) {
    // Assign to inline storage
    {
        SmallVector<int, 5> sv{1, 2};

        sv = {3, 4, 5};

        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, std::vector<int>{3, 4, 5});
        ExpectInlineStorage(sv);
    }

    // Assign requiring dynamic storage
    {
        SmallVector<int, 3> sv{1, 2};

        sv = {3, 4, 5, 6, 7};

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{3, 4, 5, 6, 7});
        ExpectInlineStorage(sv, false);
    }

    // Assign to empty
    {
        SmallVector<int, 5> sv;

        sv = {1, 2, 3};

        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3});
        ExpectInlineStorage(sv);
    }
}

// Assign Method Tests
TEST_F(SmallVectorTest, AssignCountValue) {
    // Assign to smaller size
    {
        SmallVector<int, 5> sv{1, 2, 3, 4};

        sv.assign(2, 42);

        EXPECT_EQ(sv.size(), 2u);
        ExpectVectorContent(sv, std::vector<int>{42, 42});
        ExpectInlineStorage(sv);
    }

    // Assign to larger size within inline capacity
    {
        SmallVector<int, 5> sv{1, 2};

        sv.assign(4, 42);

        EXPECT_EQ(sv.size(), 4u);
        ExpectVectorContent(sv, std::vector<int>{42, 42, 42, 42});
        ExpectInlineStorage(sv);
    }

    // Assign to size requiring dynamic storage
    {
        SmallVector<int, 3> sv{1, 2};

        sv.assign(5, 42);

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{42, 42, 42, 42, 42});
        ExpectInlineStorage(sv, false);
    }

    // Assign to empty
    {
        SmallVector<int, 5> sv;

        sv.assign(3, 42);

        EXPECT_EQ(sv.size(), 3u);
        ExpectVectorContent(sv, std::vector<int>{42, 42, 42});
        ExpectInlineStorage(sv);
    }
}

TEST_F(SmallVectorTest, AssignRange) {
    std::vector<int> source{10, 20, 30, 40, 50};

    // Assign to smaller size
    {
        SmallVector<int, 10> sv{1, 2, 3, 4, 5, 6};

        sv.assign(source.begin(), source.end());

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, source);
        ExpectInlineStorage(sv);
    }

    // Assign to larger size within inline capacity
    {
        SmallVector<int, 10> sv{1, 2};

        sv.assign(source.begin(), source.end());

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, source);
        ExpectInlineStorage(sv);
    }

    // Assign to size requiring dynamic storage
    {
        SmallVector<int, 3> sv{1, 2};

        sv.assign(source.begin(), source.end());

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, source);
        ExpectInlineStorage(sv, false);
    }

    // Assign to empty
    {
        SmallVector<int, 10> sv;

        sv.assign(source.begin(), source.end());

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, source);
        ExpectInlineStorage(sv);
    }
}

TEST_F(SmallVectorTest, AssignInitializerList) {
    // Assign to smaller size
    {
        SmallVector<int, 10> sv{1, 2, 3, 4, 5, 6};

        sv.assign({10, 20, 30, 40, 50});

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{10, 20, 30, 40, 50});
        ExpectInlineStorage(sv);
    }

    // Assign to size requiring dynamic storage
    {
        SmallVector<int, 3> sv{1, 2};

        sv.assign({10, 20, 30, 40, 50});

        EXPECT_EQ(sv.size(), 5u);
        ExpectVectorContent(sv, std::vector<int>{10, 20, 30, 40, 50});
        ExpectInlineStorage(sv, false);
    }
}

// Element Access Tests
TEST_F(SmallVectorTest, At) {
    SmallVector<int, 5> sv{1, 2, 3, 4, 5};

    // Valid access
    EXPECT_EQ(sv.at(0), 1);
    EXPECT_EQ(sv.at(4), 5);

    // Out of bounds access
    EXPECT_THROW(sv.at(5), std::out_of_range);
    EXPECT_THROW(sv.at(10), std::out_of_range);

    // Modify through at()
    sv.at(2) = 30;
    EXPECT_EQ(sv[2], 30);

    // Const version
    const SmallVector<int, 5>& csv = sv;
    EXPECT_EQ(csv.at(0), 1);
    EXPECT_THROW(csv.at(5), std::out_of_range);
}

TEST_F(SmallVectorTest, SubscriptOperator) {
    SmallVector<int, 5> sv{1, 2, 3, 4, 5};

    // Access
    EXPECT_EQ(sv[0], 1);
    EXPECT_EQ(sv[4], 5);

    // Modify
    sv[2] = 30;
    EXPECT_EQ(sv[2], 30);

    // Const version
    const SmallVector<int, 5>& csv = sv;
    EXPECT_EQ(csv[0], 1);
    EXPECT_EQ(csv[4], 5);
}

TEST_F(SmallVectorTest, Front) {
    // Non-empty vector
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};

        EXPECT_EQ(sv.front(), 1);

        // Modify
        sv.front() = 10;
        EXPECT_EQ(sv[0], 10);

        // Const version
        const SmallVector<int, 5>& csv = sv;
        EXPECT_EQ(csv.front(), 10);
    }

    // Empty vector - undefined behavior but should assert in debug mode
    {
        SmallVector<int, 5> sv;
        // We can't test this with EXPECT_DEATH because it depends on whether
        // asserts are enabled EXPECT_DEATH(sv.front(), "");
    }
}

TEST_F(SmallVectorTest, Back) {
    // Non-empty vector
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};

        EXPECT_EQ(sv.back(), 5);

        // Modify
        sv.back() = 50;
        EXPECT_EQ(sv[4], 50);

        // Const version
        const SmallVector<int, 5>& csv = sv;
        EXPECT_EQ(csv.back(), 50);
    }

    // Single element
    {
        SmallVector<int, 5> sv{42};
        EXPECT_EQ(sv.back(), 42);
    }

    // Empty vector - undefined behavior but should assert in debug mode
    {
        SmallVector<int, 5> sv;
        // EXPECT_DEATH(sv.back(), "");
    }
}

TEST_F(SmallVectorTest, Data) {
    SmallVector<int, 5> sv{1, 2, 3, 4, 5};

    // Access through data()
    int* data = sv.data();
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[4], 5);

    // Modify through data()
    data[2] = 30;
    EXPECT_EQ(sv[2], 30);

    // Const version
    const SmallVector<int, 5>& csv = sv;
    const int* cdata = csv.data();
    EXPECT_EQ(cdata[0], 1);
    EXPECT_EQ(cdata[4], 5);
}

// Iterator Tests
TEST_F(SmallVectorTest, Iterators) {
    SmallVector<int, 5> sv{1, 2, 3, 4, 5};

    // begin/end
    auto it = sv.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);

    // Iterate through all elements
    int sum = 0;
    for (auto i : sv) {
        sum += i;
    }
    EXPECT_EQ(sum, 15);  // 1+2+3+4+5

    // Const iterators
    const SmallVector<int, 5>& csv = sv;
    auto cit = csv.begin();
    EXPECT_EQ(*cit, 1);

    // cbegin/cend
    auto cit2 = sv.cbegin();
    EXPECT_EQ(*cit2, 1);

    // Modify through iterator
    *sv.begin() = 10;
    EXPECT_EQ(sv[0], 10);

    // Reverse iterators
    auto rit = sv.rbegin();
    EXPECT_EQ(*rit, 5);
    ++rit;
    EXPECT_EQ(*rit, 4);

    // Const reverse iterators
    auto crit = csv.rbegin();
    EXPECT_EQ(*crit, 5);

    // crbegin/crend
    auto crit2 = sv.crbegin();
    EXPECT_EQ(*crit2, 5);

    // Use standard algorithms with iterators
    std::sort(sv.begin(), sv.end(), std::greater<>());
    ExpectVectorContent(sv, std::vector<int>{10, 5, 4, 3, 2});
}

// Capacity Tests
TEST_F(SmallVectorTest, Empty) {
    // Default constructed
    SmallVector<int, 5> sv1;
    EXPECT_TRUE(sv1.empty());

    // After adding elements
    sv1.pushBack(1);
    EXPECT_FALSE(sv1.empty());

    // After clearing
    sv1.clear();
    EXPECT_TRUE(sv1.empty());

    // After resizing to zero
    SmallVector<int, 5> sv2{1, 2, 3};
    sv2.resize(0);
    EXPECT_TRUE(sv2.empty());
}

TEST_F(SmallVectorTest, Size) {
    // Default constructed
    SmallVector<int, 5> sv;
    EXPECT_EQ(sv.size(), 0u);

    // After adding elements
    sv.pushBack(1);
    EXPECT_EQ(sv.size(), 1u);

    sv.pushBack(2);
    EXPECT_EQ(sv.size(), 2u);

    // After inserting multiple elements
    sv.insert(sv.begin(), 3, 42);
    EXPECT_EQ(sv.size(), 5u);

    // After erasing elements
    sv.erase(sv.begin(), sv.begin() + 2);
    EXPECT_EQ(sv.size(), 3u);

    // After clearing
    sv.clear();
    EXPECT_EQ(sv.size(), 0u);
}

TEST_F(SmallVectorTest, MaxSize) {
    SmallVector<int, 5> sv;
    // Practical check (can't know exact value, but should be reasonably large)
    EXPECT_GT(sv.maxSize(), 1000000u);
}

TEST_F(SmallVectorTest, Reserve) {
    SmallVector<int, 5> sv;

    // Reserve within inline capacity
    sv.reserve(3);
    EXPECT_EQ(sv.capacity(), 5u);  // Still using inline capacity
    ExpectInlineStorage(sv);

    // Reserve beyond inline capacity
    sv.reserve(10);
    EXPECT_EQ(sv.capacity(), 10u);
    ExpectInlineStorage(sv, false);

    // Reserve less than current capacity (should be no-op)
    sv.reserve(8);
    EXPECT_EQ(sv.capacity(), 10u);  // Capacity unchanged

    // Test with contents
    SmallVector<int, 3> sv2{1, 2, 3};
    sv2.reserve(6);
    EXPECT_EQ(sv2.size(), 3u);
    ExpectVectorContent(sv2, std::vector<int>{1, 2, 3});
    EXPECT_EQ(sv2.capacity(), 6u);
    ExpectInlineStorage(sv2, false);
}

TEST_F(SmallVectorTest, Capacity) {
    // Default constructed
    SmallVector<int, 5> sv;
    EXPECT_EQ(sv.capacity(), 5u);

    // After reserve
    sv.reserve(10);
    EXPECT_EQ(sv.capacity(), 10u);

    // After pushBack beyond inline
    SmallVector<int, 3> sv2;
    EXPECT_EQ(sv2.capacity(), 3u);

    sv2.pushBack(1);
    sv2.pushBack(2);
    sv2.pushBack(3);
    EXPECT_EQ(sv2.capacity(), 3u);

    sv2.pushBack(4);  // This should trigger growth
    EXPECT_GT(sv2.capacity(), 3u);
}

TEST_F(SmallVectorTest, ShrinkToFit) {
    // Dynamic storage with excess capacity
    {
        SmallVector<int, 3> sv;
        sv.reserve(10);
        sv.pushBack(1);
        sv.pushBack(2);
        sv.pushBack(4);
        sv.pushBack(5);

        EXPECT_EQ(sv.capacity(), 10u);

        sv.shrinkToFit();

        EXPECT_EQ(sv.capacity(), 4u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 4, 5});
        ExpectInlineStorage(sv, false);
    }

    // Dynamic storage that can fit in inline storage
    {
        SmallVector<int, 5> sv;
        sv.reserve(10);
        sv.pushBack(1);
        sv.pushBack(2);
        sv.pushBack(3);

        EXPECT_EQ(sv.capacity(), 10u);
        ExpectInlineStorage(sv, false);

        sv.shrinkToFit();

        EXPECT_EQ(sv.capacity(), 5u);  // Back to inline capacity
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3});
        ExpectInlineStorage(sv);
    }

    // Already at capacity
    {
        SmallVector<int, 3> sv{1, 2, 3};
        sv.shrinkToFit();
        EXPECT_EQ(sv.capacity(), 3u);
        ExpectInlineStorage(sv);
    }

    // Empty with excess capacity
    {
        SmallVector<int, 3> sv;
        sv.reserve(10);
        sv.shrinkToFit();
        EXPECT_EQ(sv.capacity(), 3u);
        ExpectInlineStorage(sv);
    }
}

// Modifier Tests
TEST_F(SmallVectorTest, Clear) {
    // Inline storage
    {
        SmallVector<int, 5> sv{1, 2, 3};
        sv.clear();
        EXPECT_TRUE(sv.empty());
        EXPECT_EQ(sv.size(), 0u);
        EXPECT_EQ(sv.capacity(), 5u);  // Capacity unchanged
        ExpectInlineStorage(sv);
    }

    // Dynamic storage
    {
        SmallVector<int, 3> sv{1, 2, 3, 4, 5};
        sv.clear();
        EXPECT_TRUE(sv.empty());
        EXPECT_EQ(sv.size(), 0u);
        EXPECT_GT(sv.capacity(), 3u);  // Capacity unchanged
        ExpectInlineStorage(sv, false);
    }

    // Clear empty vector
    {
        SmallVector<int, 5> sv;
        sv.clear();
        EXPECT_TRUE(sv.empty());
    }

    // Verify destructor calls
    {
        TestObject::reset_counters();
        SmallVector<TestObject, 5> sv;
        sv.emplaceBack(1);
        sv.emplaceBack(2);
        sv.emplaceBack(3);

        sv.clear();

        EXPECT_EQ(TestObject::destructor_count(), 3);
    }
}

TEST_F(SmallVectorTest, Insert) {
    // Insert single element at beginning
    {
        SmallVector<int, 5> sv{2, 3, 4};
        auto it = sv.insert(sv.begin(), 1);

        EXPECT_EQ(*it, 1);
        EXPECT_EQ(it - sv.begin(), 0);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4});
    }

    // Insert single element at middle
    {
        SmallVector<int, 5> sv{1, 2, 4, 5};
        auto it = sv.insert(sv.begin() + 2, 3);

        EXPECT_EQ(*it, 3);
        EXPECT_EQ(it - sv.begin(), 2);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5});
    }

    // Insert single element at end
    {
        SmallVector<int, 5> sv{1, 2, 3, 4};
        auto it = sv.insert(sv.end(), 5);

        EXPECT_EQ(*it, 5);
        EXPECT_EQ(it - sv.begin(), 4);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5});
    }

    // Insert that requires reallocation
    {
        SmallVector<int, 3> sv{1, 2, 3};
        auto it = sv.insert(sv.begin() + 1, 42);

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 1);
        ExpectVectorContent(sv, std::vector<int>{1, 42, 2, 3});
        ExpectInlineStorage(sv, false);
    }

    // Insert rvalue
    {
        SmallVector<std::string, 5> sv{"aaa", "ccc"};
        std::string str = "bbb";
        auto it = sv.insert(sv.begin() + 1, std::move(str));

        EXPECT_EQ(*it, "bbb");
        EXPECT_EQ(it - sv.begin(), 1);
        EXPECT_TRUE(str.empty());  // Moved from
    }
}

TEST_F(SmallVectorTest, InsertCount) {
    // Insert multiple elements at beginning
    {
        SmallVector<int, 10> sv{4, 5, 6};
        auto it = sv.insert(sv.begin(), 3, 42);

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 0);
        ExpectVectorContent(sv, std::vector<int>{42, 42, 42, 4, 5, 6});
    }

    // Insert multiple elements in middle
    {
        SmallVector<int, 10> sv{1, 2, 6, 7};
        auto it = sv.insert(sv.begin() + 2, 3, 42);

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 2);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 42, 42, 42, 6, 7});
    }

    // Insert multiple elements at end
    {
        SmallVector<int, 10> sv{1, 2, 3};
        auto it = sv.insert(sv.end(), 3, 42);

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 3);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 42, 42, 42});
    }

    // Insert that requires reallocation
    {
        SmallVector<int, 5> sv{1, 2, 3, 4};
        auto it = sv.insert(sv.begin() + 2, 3, 42);

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 2);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 42, 42, 42, 3, 4});
        ExpectInlineStorage(sv, false);
    }

    // Insert zero elements (no-op)
    {
        SmallVector<int, 5> sv{1, 2, 3};
        auto it = sv.insert(sv.begin() + 1, 0, 42);

        EXPECT_EQ(it - sv.begin(), 1);
        EXPECT_EQ(*it, 2);  // Points to the original element at position 1
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3});
    }

    // Insert into empty vector
    {
        SmallVector<int, 5> sv;
        auto it = sv.insert(sv.begin(), 3, 42);

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 0);
        ExpectVectorContent(sv, std::vector<int>{42, 42, 42});
    }
}

TEST_F(SmallVectorTest, InsertRange) {
    std::vector<int> source{42, 43, 44};

    // Insert at beginning
    {
        SmallVector<int, 10> sv{1, 2, 3};
        auto it = sv.insert(sv.begin(), source.begin(), source.end());

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 0);
        ExpectVectorContent(sv, std::vector<int>{42, 43, 44, 1, 2, 3});
    }

    // Insert in middle
    {
        SmallVector<int, 10> sv{1, 2, 6, 7};
        auto it = sv.insert(sv.begin() + 2, source.begin(), source.end());

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 2);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 42, 43, 44, 6, 7});
    }

    // Insert at end
    {
        SmallVector<int, 10> sv{1, 2, 3};
        auto it = sv.insert(sv.end(), source.begin(), source.end());

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 3);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 42, 43, 44});
    }

    // Insert that requires reallocation
    {
        SmallVector<int, 4> sv{1, 2, 3};
        auto it = sv.insert(sv.begin() + 1, source.begin(), source.end());

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 1);
        ExpectVectorContent(sv, std::vector<int>{1, 42, 43, 44, 2, 3});
        ExpectInlineStorage(sv, false);
    }

    // Insert empty range (no-op)
    {
        SmallVector<int, 5> sv{1, 2, 3};
        auto it = sv.insert(sv.begin() + 1, source.begin(), source.begin());

        EXPECT_EQ(it - sv.begin(), 1);
        EXPECT_EQ(*it, 2);  // Points to the original element
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3});
    }

    // Insert into empty vector
    {
        SmallVector<int, 5> sv;
        auto it = sv.insert(sv.begin(), source.begin(), source.end());

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 0);
        ExpectVectorContent(sv, std::vector<int>{42, 43, 44});
    }
}

TEST_F(SmallVectorTest, InsertInitializerList) {
    // Insert at beginning
    {
        SmallVector<int, 10> sv{4, 5, 6};
        auto it = sv.insert(sv.begin(), {1, 2, 3});

        EXPECT_EQ(*it, 1);
        EXPECT_EQ(it - sv.begin(), 0);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5, 6});
    }

    // Insert in middle
    {
        SmallVector<int, 10> sv{1, 2, 6, 7};
        auto it = sv.insert(sv.begin() + 2, {3, 4, 5});

        EXPECT_EQ(*it, 3);
        EXPECT_EQ(it - sv.begin(), 2);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5, 6, 7});
    }

    // Insert requiring reallocation
    {
        SmallVector<int, 3> sv{1, 2, 3};
        auto it = sv.insert(sv.begin() + 1, {42, 43, 44});

        EXPECT_EQ(*it, 42);
        EXPECT_EQ(it - sv.begin(), 1);
        ExpectVectorContent(sv, std::vector<int>{1, 42, 43, 44, 2, 3});
        ExpectInlineStorage(sv, false);
    }
}

TEST_F(SmallVectorTest, Emplace) {
    // Emplace at beginning
    {
        SmallVector<TestObject, 5> sv;
        sv.emplaceBack(1);
        sv.emplaceBack(2);

        TestObject::reset_counters();
        auto it = sv.emplace(sv.begin(), 42);

        EXPECT_EQ(it->value(), 42);
        EXPECT_EQ(it - sv.begin(), 0);
        EXPECT_EQ(sv.size(), 3u);
        EXPECT_EQ(sv[0].value(), 42);
        EXPECT_EQ(sv[1].value(), 1);
        EXPECT_EQ(sv[2].value(), 2);

        // Should construct in-place without extra copies
        EXPECT_EQ(TestObject::copy_count(), 0);
    }

    // Emplace in middle
    {
        SmallVector<TestObject, 5> sv;
        sv.emplaceBack(1);
        sv.emplaceBack(3);

        TestObject::reset_counters();
        auto it = sv.emplace(sv.begin() + 1, 2);

        EXPECT_EQ(it->value(), 2);
        EXPECT_EQ(it - sv.begin(), 1);
        EXPECT_EQ(sv.size(), 3u);
        EXPECT_EQ(sv[0].value(), 1);
        EXPECT_EQ(sv[1].value(), 2);
        EXPECT_EQ(sv[2].value(), 3);
    }

    // Emplace requiring reallocation
    {
        SmallVector<TestObject, 2> sv;
        sv.emplaceBack(1);
        sv.emplaceBack(3);

        TestObject::reset_counters();
        auto it = sv.emplace(sv.begin() + 1, 2);

        EXPECT_EQ(it->value(), 2);
        EXPECT_EQ(it - sv.begin(), 1);
        EXPECT_EQ(sv.size(), 3u);
        EXPECT_EQ(sv[0].value(), 1);
        EXPECT_EQ(sv[1].value(), 2);
        EXPECT_EQ(sv[2].value(), 3);
        ExpectInlineStorage(sv, false);
    }

    // Emplace at end
    {
        SmallVector<TestObject, 5> sv;
        sv.emplaceBack(1);
        sv.emplaceBack(2);

        TestObject::reset_counters();
        auto it = sv.emplace(sv.end(), 3);

        EXPECT_EQ(it->value(), 3);
        EXPECT_EQ(it - sv.begin(), 2);
        EXPECT_EQ(sv.size(), 3u);
        EXPECT_EQ(sv[0].value(), 1);
        EXPECT_EQ(sv[1].value(), 2);
        EXPECT_EQ(sv[2].value(), 3);
    }
}

TEST_F(SmallVectorTest, Erase) {
    // Erase from beginning
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};
        auto it = sv.erase(sv.begin());

        EXPECT_EQ(*it, 2);
        EXPECT_EQ(it - sv.begin(), 0);
        ExpectVectorContent(sv, std::vector<int>{2, 3, 4, 5});
    }

    // Erase from middle
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};
        auto it = sv.erase(sv.begin() + 2);

        EXPECT_EQ(*it, 4);
        EXPECT_EQ(it - sv.begin(), 2);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 4, 5});
    }

    // Erase from end
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};
        auto it = sv.erase(sv.end() - 1);

        EXPECT_EQ(it, sv.end());
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4});
    }

    // Erase range from beginning
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};
        auto it = sv.erase(sv.begin(), sv.begin() + 2);

        EXPECT_EQ(*it, 3);
        EXPECT_EQ(it - sv.begin(), 0);
        ExpectVectorContent(sv, std::vector<int>{3, 4, 5});
    }

    // Erase range from middle
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};
        auto it = sv.erase(sv.begin() + 1, sv.begin() + 4);

        EXPECT_EQ(*it, 5);
        EXPECT_EQ(it - sv.begin(), 1);
        ExpectVectorContent(sv, std::vector<int>{1, 5});
    }

    // Erase range to end
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};
        auto it = sv.erase(sv.begin() + 2, sv.end());

        EXPECT_EQ(it, sv.end());
        ExpectVectorContent(sv, std::vector<int>{1, 2});
    }

    // Erase all
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};
        auto it = sv.erase(sv.begin(), sv.end());

        EXPECT_EQ(it, sv.end());
        EXPECT_TRUE(sv.empty());
    }

    // Erase empty range (no-op)
    {
        SmallVector<int, 5> sv{1, 2, 3, 4, 5};
        auto it = sv.erase(sv.begin() + 2, sv.begin() + 2);

        EXPECT_EQ(*it, 3);
        EXPECT_EQ(it - sv.begin(), 2);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4, 5});
    }

    // Verify destructor calls
    {
        TestObject::reset_counters();
        SmallVector<TestObject, 5> sv;
        sv.emplaceBack(1);
        sv.emplaceBack(2);
        sv.emplaceBack(3);

        sv.erase(sv.begin() + 1);

        EXPECT_EQ(TestObject::destructor_count(), 1);
        EXPECT_EQ(sv.size(), 2u);
        EXPECT_EQ(sv[0].value(), 1);
        EXPECT_EQ(sv[1].value(), 3);
    }
}

TEST_F(SmallVectorTest, PushBack) {
    // Push back to inline storage
    {
        SmallVector<int, 5> sv;

        sv.pushBack(1);
        EXPECT_EQ(sv.size(), 1u);
        EXPECT_EQ(sv[0], 1);

        sv.pushBack(2);
        EXPECT_EQ(sv.size(), 2u);
        EXPECT_EQ(sv[1], 2);

        ExpectInlineStorage(sv);
    }

    // Push back that requires reallocation
    {
        SmallVector<int, 3> sv{1, 2, 3};

        sv.pushBack(4);
        EXPECT_EQ(sv.size(), 4u);
        EXPECT_EQ(sv[3], 4);
        ExpectInlineStorage(sv, false);

        // Check that all elements were moved correctly
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4});
    }

    // Push back with move semantics
    {
        SmallVector<std::string, 3> sv;

        std::string str = "hello";
        sv.pushBack(std::move(str));

        EXPECT_EQ(sv.size(), 1u);
        EXPECT_EQ(sv[0], "hello");
        EXPECT_TRUE(str.empty());  // Moved from
    }
}

TEST_F(SmallVectorTest, EmplaceBack) {
    // Emplace back to inline storage
    {
        SmallVector<TestObject, 5> sv;

        TestObject::reset_counters();
        auto& ref1 = sv.emplaceBack(1);

        EXPECT_EQ(sv.size(), 1u);
        EXPECT_EQ(sv[0].value(), 1);
        EXPECT_EQ(ref1.value(), 1);

        auto& ref2 = sv.emplaceBack(2);

        EXPECT_EQ(sv.size(), 2u);
        EXPECT_EQ(sv[1].value(), 2);
        EXPECT_EQ(ref2.value(), 2);

        // Should construct in-place without copies
        EXPECT_EQ(TestObject::copy_count(), 0);
        EXPECT_EQ(TestObject::move_count(), 0);

        ExpectInlineStorage(sv);
    }

    // Emplace back that requires reallocation
    {
        SmallVector<TestObject, 2> sv;
        sv.emplaceBack(1);
        sv.emplaceBack(2);

        TestObject::reset_counters();
        auto& ref = sv.emplaceBack(3);

        EXPECT_EQ(sv.size(), 3u);
        EXPECT_EQ(sv[2].value(), 3);
        EXPECT_EQ(ref.value(), 3);
        ExpectInlineStorage(sv, false);

        // Check that elements were moved during reallocation
        EXPECT_EQ(TestObject::move_count(), 2);  // First two elements moved
    }
}

TEST_F(SmallVectorTest, PopBack) {
    // Pop back from inline storage
    {
        SmallVector<int, 5> sv{1, 2, 3};

        sv.popBack();
        EXPECT_EQ(sv.size(), 2u);
        ExpectVectorContent(sv, std::vector<int>{1, 2});

        sv.popBack();
        EXPECT_EQ(sv.size(), 1u);
        ExpectVectorContent(sv, std::vector<int>{1});

        sv.popBack();
        EXPECT_TRUE(sv.empty());

        ExpectInlineStorage(sv);
    }

    // Pop back from dynamic storage
    {
        SmallVector<int, 3> sv{1, 2, 3, 4, 5};

        sv.popBack();
        EXPECT_EQ(sv.size(), 4u);
        ExpectVectorContent(sv, std::vector<int>{1, 2, 3, 4});
        ExpectInlineStorage(sv, false);
    }

    // Verify destructor calls
    {
        TestObject::reset_counters();
        SmallVector<TestObject, 5> sv;
        sv.emplaceBack(1);
        sv.emplaceBack(2);

        sv.popBack();

        EXPECT_EQ(TestObject::destructor_count(), 1);
        EXPECT_EQ(sv.size(), 1u);
    }
}
