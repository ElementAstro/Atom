#ifndef ATOM_TYPE_TEST_POD_VECTOR_HPP
#define ATOM_TYPE_TEST_POD_VECTOR_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "atom/type/pod_vector.hpp"

namespace atom::type::test {

// 定义一些用于测试的简单 POD 结构
struct SmallPod {
    int value;
    double ratio;

    bool operator==(const SmallPod& other) const {
        return value == other.value && ratio == other.ratio;
    }
};

struct LargePod {
    int id;
    char name[64];
    double values[8];

    bool operator==(const LargePod& other) const {
        return id == other.id && std::strcmp(name, other.name) == 0 &&
               std::memcmp(values, other.values, sizeof(values)) == 0;
    }
};

using IntVector = PodVector<int>;
using SmallPodVector = PodVector<SmallPod>;
using LargePodVector = PodVector<LargePod>;

// 基本功能测试类
class PodVectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 准备一些测试数据
        for (int i = 0; i < 10; ++i) {
            test_data.push_back(i);
        }

        // 初始化测试向量
        for (int i = 0; i < 5; ++i) {
            vec.pushBack(i);
        }
    }

    std::vector<int> test_data;
    IntVector vec;
};

// 构造函数测试
TEST_F(PodVectorTest, Constructor) {
    // 默认构造函数
    IntVector empty_vec;
    EXPECT_TRUE(empty_vec.empty());
    EXPECT_EQ(empty_vec.size(), 0);

    // 带初始大小的构造函数
    IntVector sized_vec(10);
    EXPECT_EQ(sized_vec.size(), 10);
    EXPECT_GE(sized_vec.capacity(), 10);

    // 初始化列表构造函数
    IntVector init_list_vec = {1, 2, 3, 4, 5};
    EXPECT_EQ(init_list_vec.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(init_list_vec[i], i + 1);
    }

    // 复制构造函数
    IntVector copy_vec(vec);
    EXPECT_EQ(copy_vec.size(), vec.size());
    for (int i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(copy_vec[i], vec[i]);
    }

    // 移动构造函数
    IntVector original({1, 2, 3});
    IntVector moved_vec(std::move(original));
    EXPECT_EQ(moved_vec.size(), 3);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(moved_vec[i], i + 1);
    }
    // 原向量应该被清空
    EXPECT_EQ(original.data(), nullptr);
}

// 元素访问和修改测试
TEST_F(PodVectorTest, ElementAccess) {
    // 下标操作符
    EXPECT_EQ(vec[0], 0);
    EXPECT_EQ(vec[4], 4);

    // 修改元素
    vec[2] = 42;
    EXPECT_EQ(vec[2], 42);

    // back() 方法
    EXPECT_EQ(vec.back(), 4);
    vec.back() = 100;
    EXPECT_EQ(vec.back(), 100);

    // 常量引用访问
    const IntVector& const_vec = vec;
    EXPECT_EQ(const_vec[0], 0);
    EXPECT_EQ(const_vec.back(), 100);
}

// 容量和大小管理测试
TEST_F(PodVectorTest, CapacityAndSize) {
    EXPECT_EQ(vec.size(), 5);
    EXPECT_GE(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());

    // reserve 测试
    int original_capacity = vec.capacity();
    vec.reserve(original_capacity * 2);
    EXPECT_GE(vec.capacity(), original_capacity * 2);
    EXPECT_EQ(vec.size(), 5);  // size 不应改变

    // resize 测试
    vec.resize(10);
    EXPECT_EQ(vec.size(), 10);

    // 缩小测试
    vec.resize(3);
    EXPECT_EQ(vec.size(), 3);

    // clear 测试
    vec.clear();
    EXPECT_EQ(vec.size(), 0);
    EXPECT_TRUE(vec.empty());
    EXPECT_GE(vec.capacity(), original_capacity * 2);  // capacity 不应改变
}

// 元素添加和删除测试
TEST_F(PodVectorTest, ElementAdditionAndRemoval) {
    int original_size = vec.size();

    // pushBack 测试
    vec.pushBack(100);
    EXPECT_EQ(vec.size(), original_size + 1);
    EXPECT_EQ(vec.back(), 100);

    // emplaceBack 测试
    vec.emplaceBack(200);
    EXPECT_EQ(vec.size(), original_size + 2);
    EXPECT_EQ(vec.back(), 200);

    // popBack 测试
    vec.popBack();
    EXPECT_EQ(vec.size(), original_size + 1);
    EXPECT_EQ(vec.back(), 100);

    // popxBack 测试 (返回并移除元素)
    int popped = vec.popxBack();
    EXPECT_EQ(popped, 100);
    EXPECT_EQ(vec.size(), original_size);

    // insert 测试
    vec.insert(2, 42);
    EXPECT_EQ(vec.size(), original_size + 1);
    EXPECT_EQ(vec[2], 42);

    // erase 测试
    vec.erase(2);
    EXPECT_EQ(vec.size(), original_size);
    EXPECT_EQ(vec[2], 2);  // 元素被移除后，后面的元素应该前移
}

// 迭代器测试
TEST_F(PodVectorTest, Iterators) {
    // begin/end 测试
    int sum = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 0 + 1 + 2 + 3 + 4);

    // 修改值通过迭代器
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        *it *= 2;
    }

    EXPECT_EQ(vec[0], 0);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 4);
    EXPECT_EQ(vec[3], 6);
    EXPECT_EQ(vec[4], 8);

    // const_iterator 测试
    const IntVector& const_vec = vec;
    sum = 0;
    for (auto it = const_vec.begin(); it != const_vec.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 0 + 2 + 4 + 6 + 8);

    // 范围for循环测试
    sum = 0;
    for (const auto& val : vec) {
        sum += val;
    }
    EXPECT_EQ(sum, 0 + 2 + 4 + 6 + 8);
}

// 扩展方法测试
TEST_F(PodVectorTest, ExtendMethods) {
    IntVector vec1 = {1, 2, 3};
    IntVector vec2 = {4, 5, 6};

    // extend 另一个 PodVector
    vec1.extend(vec2);
    EXPECT_EQ(vec1.size(), 6);
    EXPECT_EQ(vec1[3], 4);
    EXPECT_EQ(vec1[4], 5);
    EXPECT_EQ(vec1[5], 6);

    // extend 指针范围
    int arr[] = {7, 8, 9};
    vec1.extend(arr, arr + 3);
    EXPECT_EQ(vec1.size(), 9);
    EXPECT_EQ(vec1[6], 7);
    EXPECT_EQ(vec1[7], 8);
    EXPECT_EQ(vec1[8], 9);
}

// 其他操作测试
TEST_F(PodVectorTest, OtherOperations) {
    // reverse 测试
    IntVector rev_vec = {1, 2, 3, 4, 5};
    rev_vec.reverse();
    EXPECT_EQ(rev_vec[0], 5);
    EXPECT_EQ(rev_vec[1], 4);
    EXPECT_EQ(rev_vec[2], 3);
    EXPECT_EQ(rev_vec[3], 2);
    EXPECT_EQ(rev_vec[4], 1);

    // detach 测试
    IntVector detach_vec = {10, 20, 30};
    auto [ptr, size] = detach_vec.detach();

    // 验证 detach 结果
    EXPECT_EQ(size, 3);
    EXPECT_EQ(ptr[0], 10);
    EXPECT_EQ(ptr[1], 20);
    EXPECT_EQ(ptr[2], 30);

    // 原向量应该被清空
    EXPECT_EQ(detach_vec.size(), 0);
    EXPECT_EQ(detach_vec.data(), nullptr);

    // 需要手动释放分离出来的内存
    std::allocator<int> alloc;
    alloc.deallocate(ptr, size);
}

// 测试小型 POD 类型
TEST(PodVectorTypeTest, SmallPodType) {
    SmallPodVector vec;

    // 添加元素
    SmallPod pod1{1, 1.5};
    SmallPod pod2{2, 2.5};
    vec.pushBack(pod1);
    vec.pushBack(pod2);
    vec.emplaceBack(3, 3.5);

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0].value, 1);
    EXPECT_DOUBLE_EQ(vec[0].ratio, 1.5);
    EXPECT_EQ(vec[2].value, 3);
    EXPECT_DOUBLE_EQ(vec[2].ratio, 3.5);

    // 修改元素
    vec[1].value = 42;
    vec[1].ratio = 42.5;

    EXPECT_EQ(vec[1].value, 42);
    EXPECT_DOUBLE_EQ(vec[1].ratio, 42.5);

    // 迭代
    for (auto& item : vec) {
        item.value *= 2;
    }

    EXPECT_EQ(vec[0].value, 2);
    EXPECT_EQ(vec[1].value, 84);
    EXPECT_EQ(vec[2].value, 6);
}

// 测试大型 POD 类型
TEST(PodVectorTypeTest, LargePodType) {
    LargePodVector vec;

    // 创建测试数据
    LargePod pod1;
    pod1.id = 1;
    std::strcpy(pod1.name, "Test Pod 1");
    for (int i = 0; i < 8; ++i) {
        pod1.values[i] = i * 1.1;
    }

    LargePod pod2;
    pod2.id = 2;
    std::strcpy(pod2.name, "Test Pod 2");
    for (int i = 0; i < 8; ++i) {
        pod2.values[i] = i * 2.2;
    }

    // 添加元素
    vec.pushBack(pod1);
    vec.pushBack(pod2);

    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0].id, 1);
    EXPECT_STREQ(vec[0].name, "Test Pod 1");
    EXPECT_DOUBLE_EQ(vec[0].values[3], 3 * 1.1);

    EXPECT_EQ(vec[1].id, 2);
    EXPECT_STREQ(vec[1].name, "Test Pod 2");
    EXPECT_DOUBLE_EQ(vec[1].values[3], 3 * 2.2);

    // 复制构造测试
    LargePodVector vec_copy(vec);
    EXPECT_EQ(vec_copy.size(), 2);
    EXPECT_EQ(vec_copy[0].id, 1);
    EXPECT_STREQ(vec_copy[0].name, "Test Pod 1");

    // 修改副本不应影响原始向量
    std::strcpy(vec_copy[0].name, "Modified");
    EXPECT_STREQ(vec_copy[0].name, "Modified");
    EXPECT_STREQ(vec[0].name, "Test Pod 1");
}

// 测试自动增长能力
TEST(PodVectorGrowthTest, AutoGrowth) {
    PodVector<int> vec;
    int initial_capacity = vec.capacity();

    // 添加足够多的元素触发多次增长
    for (int i = 0; i < initial_capacity * 10; ++i) {
        vec.pushBack(i);
    }

    EXPECT_GE(vec.capacity(),
              initial_capacity * 8);  // 应该至少增长了3次 (2^3 = 8)
    EXPECT_EQ(vec.size(), initial_capacity * 10);

    // 验证添加的所有元素
    for (int i = 0; i < initial_capacity * 10; ++i) {
        EXPECT_EQ(vec[i], i);
    }
}

// 测试性能 (大量元素操作)
TEST(PodVectorPerformanceTest, LargeNumberOfElements) {
    PodVector<int> vec;
    constexpr int num_elements = 100000;

    // 预留空间
    vec.reserve(num_elements);
    EXPECT_GE(vec.capacity(), num_elements);

    // 添加大量元素
    for (int i = 0; i < num_elements; ++i) {
        vec.pushBack(i);
    }

    EXPECT_EQ(vec.size(), num_elements);

    // 验证部分元素
    EXPECT_EQ(vec[0], 0);
    EXPECT_EQ(vec[num_elements / 2], num_elements / 2);
    EXPECT_EQ(vec[num_elements - 1], num_elements - 1);

    // 测试随机访问性能
    int sum = 0;
    for (int i = 0; i < 1000; ++i) {
        int idx = (i * 97) % num_elements;  // 伪随机索引
        sum += vec[idx];
    }

    // 我们不关心具体的和，只要确保操作完成
    EXPECT_GE(sum, 0);
}

// 测试不同增长因子
TEST(PodVectorGrowthFactorTest, CustomGrowthFactor) {
    // 默认增长因子为 2
    PodVector<int> default_vec;
    int default_initial = default_vec.capacity();
    default_vec.reserve(default_initial + 1);  // 触发增长
    EXPECT_GE(default_vec.capacity(), default_initial * 2);

    // 增长因子为 3
    PodVector<int, 3> custom_vec;
    int custom_initial = custom_vec.capacity();
    custom_vec.reserve(custom_initial + 1);  // 触发增长
    EXPECT_GE(custom_vec.capacity(), custom_initial * 3);
}

// 测试边缘情况
TEST(PodVectorEdgeCaseTest, EdgeCases) {
    // 空向量上的操作
    PodVector<int> empty_vec;
    EXPECT_TRUE(empty_vec.empty());
    EXPECT_THROW(empty_vec.back(),
                 std::runtime_error);    // 应该抛出异常，因为向量为空
    EXPECT_NO_THROW(empty_vec.clear());  // 清空空向量应该安全

    // 零容量预留
    EXPECT_NO_THROW(empty_vec.reserve(0));

    // pushBack 后立即 popBack
    empty_vec.pushBack(42);
    EXPECT_EQ(empty_vec.size(), 1);
    empty_vec.popBack();
    EXPECT_TRUE(empty_vec.empty());

    // 试图访问无效索引
    PodVector<int> vec = {1, 2, 3};
    EXPECT_THROW(vec[3], std::out_of_range);  // 索引越界

    // 移动空向量
    PodVector<int> moved_vec(std::move(empty_vec));
    EXPECT_TRUE(moved_vec.empty());
}

// 测试移动语义
TEST(PodVectorMoveTest, MoveSemantics) {
    // 创建一个含有数据的向量
    PodVector<int> original({1, 2, 3, 4, 5});

    // 移动赋值
    PodVector<int> vec_move_assigned;
    vec_move_assigned = std::move(original);

    // 验证移动赋值结果
    EXPECT_EQ(vec_move_assigned.size(), 5);
    EXPECT_EQ(vec_move_assigned[0], 1);
    EXPECT_EQ(vec_move_assigned[4], 5);

    // 原向量应该被清空
    EXPECT_EQ(original.data(), nullptr);

    // 对移后源进行操作应该安全
    EXPECT_NO_THROW(original.pushBack(10));  // 应该重新分配内存并工作
    EXPECT_EQ(original.size(), 1);
    EXPECT_EQ(original[0], 10);
}

// 测试自定义POD类型
struct CustomPod {
    int id;
    float value;
    char code[4];

    bool operator==(const CustomPod& other) const {
        return id == other.id && std::abs(value - other.value) < 0.001f &&
               std::memcmp(code, other.code, sizeof(code)) == 0;
    }
};

static_assert(PodType<CustomPod>, "CustomPod should be a POD type");

TEST(PodVectorCustomTypeTest, CustomPodType) {
    PodVector<CustomPod> vec;

    // 添加一些元素
    CustomPod p1{1, 1.5f, {'A', 'B', 'C', 'D'}};
    CustomPod p2{2, 2.5f, {'E', 'F', 'G', 'H'}};

    vec.pushBack(p1);
    vec.pushBack(p2);

    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], p1);
    EXPECT_EQ(vec[1], p2);

    // 通过引用修改
    vec[0].id = 100;
    vec[0].value = 100.5f;

    EXPECT_EQ(vec[0].id, 100);
    EXPECT_FLOAT_EQ(vec[0].value, 100.5f);
    EXPECT_NE(vec[0], p1);  // 现在应该不相等
}

#ifdef ATOM_USE_BOOST
// Boost 特定功能测试
TEST(PodVectorBoostTest, BoostFunctionality) {
    PodVector<int> vec = {1, 2, 3};

    // 测试 Boost 迭代器
    int sum = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 6);

    // 测试异常处理
    EXPECT_THROW(
        {
            // 故意创建一个可能抛出异常的情况
            PodVector<int> huge_vec(std::numeric_limits<int>::max());
        },
        PodVectorException);
}
#endif

}  // namespace atom::type::test

#endif  // ATOM_TYPE_TEST_POD_VECTOR_HPP
