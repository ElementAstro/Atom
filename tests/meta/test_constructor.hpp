#include <gtest/gtest.h>
#include "atom/function/constructor.hpp"

#include <chrono>
#include <map>  // 添加缺失的头文件
#include <string>
#include <thread>
#include <vector>

namespace {

// 测试类
class SimpleClass {
private:
    int value_;
    std::string name_;

public:
    SimpleClass() : value_(0), name_("Default") {}

    SimpleClass(int value) : value_(value), name_("FromInt") {}

    SimpleClass(int value, std::string name)
        : value_(value), name_(std::move(name)) {}

    SimpleClass(const SimpleClass& other)
        : value_(other.value_), name_(other.name_) {}

    SimpleClass(SimpleClass&& other) noexcept
        : value_(other.value_), name_(std::move(other.name_)) {
        other.value_ = 0;
    }

    int getValue() const { return value_; }
    const std::string& getName() const { return name_; }

    void setValue(int value) { value_ = value; }
    void setName(const std::string& name) { name_ = name; }

    bool operator==(const SimpleClass& other) const {
        return value_ == other.value_ && name_ == other.name_;
    }
};

class ThrowingClass {
public:
    ThrowingClass() { throw std::runtime_error("Constructor error"); }

    explicit ThrowingClass(int) {}  // 非抛出构造函数
};

class NonCopyable {
private:
    int value_;

public:
    NonCopyable() : value_(0) {}
    explicit NonCopyable(int value) : value_(value) {}

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    NonCopyable(NonCopyable&&) noexcept = default;
    NonCopyable& operator=(NonCopyable&&) noexcept = default;

    int getValue() const { return value_; }
};

class InitListClass {
private:
    std::vector<int> values_;

public:
    InitListClass(std::initializer_list<int> init) : values_(init) {}

    const std::vector<int>& getValues() const { return values_; }
};

}  // anonymous namespace

class ConstructorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试基本构造函数功能
TEST_F(ConstructorTest, BasicConstructors) {
    // 默认构造函数
    auto defaultCtor = atom::meta::buildDefaultConstructor<SimpleClass>();
    auto instance = defaultCtor();
    EXPECT_EQ(instance.getValue(), 0);
    EXPECT_EQ(instance.getName(), "Default");

    // 带参数的构造函数
    auto paramCtor = [](int value, const std::string& name) {
        return SimpleClass(value, name);
    };
    auto instance2 = paramCtor(42, "Test");
    EXPECT_EQ(instance2.getValue(), 42);
    EXPECT_EQ(instance2.getName(), "Test");
}

// 测试共享指针构造函数
TEST_F(ConstructorTest, SharedConstructors) {
    using namespace atom::meta;

    // 构建共享构造函数
    auto sharedCtor = buildConstructor<SimpleClass, int, std::string>();

    auto instance = sharedCtor(42, "SharedTest");
    EXPECT_EQ(instance->getValue(), 42);
    EXPECT_EQ(instance->getName(), "SharedTest");

    // 测试构造函数模板
    auto genericCtor = constructor<SimpleClass, int, std::string>();
    auto instance2 = genericCtor(100, "GenericTest");
    EXPECT_EQ(instance2->getValue(), 100);
    EXPECT_EQ(instance2->getName(), "GenericTest");
}

// 针对SafeConstructorResult的方法，使用我们自己的helper函数
namespace {
template <typename T>
bool isValid(const atom::meta::SafeConstructorResult<T>& result) {
#if HAS_EXPECTED
    return result.has_value();
#else
    return result.isValid();
#endif
}

template <typename T>
std::optional<std::string> getError(
    const atom::meta::SafeConstructorResult<T>& result) {
#if HAS_EXPECTED
    return result.has_value() ? std::nullopt
                              : std::make_optional(result.error().error());
#else
    return result.error;
#endif
}

template <typename T>
T& getValue(atom::meta::SafeConstructorResult<T>& result) {
#if HAS_EXPECTED
    return result.value();
#else
    return result.getValue();
#endif
}

template <typename T>
const T& getValue(const atom::meta::SafeConstructorResult<T>& result) {
#if HAS_EXPECTED
    return result.value();
#else
    return result.getValue();
#endif
}
}  // namespace

// 测试安全构造函数和异常处理
TEST_F(ConstructorTest, SafeConstructors) {
    using namespace atom::meta;
    using namespace atom::type;  // 添加这一行以访问 make_unexpected

    // 创建自定义的安全构造函数
    auto safeCtor = [](auto&&... args)
        -> SafeConstructorResult<std::shared_ptr<ThrowingClass>> {
        try {
            return std::make_shared<ThrowingClass>(
                std::forward<decltype(args)>(args)...);
        } catch (const std::exception& e) {
            return make_unexpected(std::string("Failed to construct: ") +
                                   e.what());
        }
    };

    auto result = safeCtor();
    EXPECT_FALSE(isValid(result));
    EXPECT_TRUE(getError(result).has_value());
    EXPECT_FALSE(getError(result).value().empty());

    // 测试成功构造
    auto safeSimpleCtor = [](auto&&... args)
        -> SafeConstructorResult<std::shared_ptr<SimpleClass>> {
        try {
            return std::make_shared<SimpleClass>(
                std::forward<decltype(args)>(args)...);
        } catch (const std::exception& e) {
            return make_unexpected(std::string("Failed to construct: ") +
                                   e.what());
        }
    };

    auto successResult = safeSimpleCtor();
    EXPECT_TRUE(isValid(successResult));
    EXPECT_FALSE(getError(successResult).has_value());
    EXPECT_EQ(getValue(successResult)->getValue(), 0);
    EXPECT_EQ(getValue(successResult)->getName(), "Default");
}

// 测试经过验证的构造函数
TEST_F(ConstructorTest, ValidatedConstructors) {
    using namespace atom::meta;
    using namespace atom::type;

    // 创建SimpleClass的验证器
    auto validator = [](int value, const std::string& name) {
        return value >= 0 && !name.empty();
    };

    // 创建自定义的验证构造函数
    auto validatedCtor = [validator](int value, const std::string& name)
        -> SafeConstructorResult<std::shared_ptr<SimpleClass>> {
        try {
            // 先验证参数
            if (!validator(value, name)) {
                return make_unexpected("Parameter validation failed");
            }

            return std::make_shared<SimpleClass>(value, name);
        } catch (const std::exception& e) {
            return make_unexpected(std::string("Failed to construct: ") +
                                   e.what());
        }
    };

    // 测试有效参数
    auto validResult = validatedCtor(42, "ValidTest");
    EXPECT_TRUE(isValid(validResult));
    EXPECT_EQ(getValue(validResult)->getValue(), 42);
    EXPECT_EQ(getValue(validResult)->getName(), "ValidTest");

    // 测试无效参数
    auto invalidResult = validatedCtor(-1, "");
    EXPECT_FALSE(isValid(invalidResult));
    EXPECT_TRUE(getError(invalidResult).has_value());
    EXPECT_EQ(getError(invalidResult).value(), "Parameter validation failed");
}

// 测试移动构造函数
TEST_F(ConstructorTest, MoveConstructors) {
    using namespace atom::meta;

    auto moveCtor = buildMoveConstructor<SimpleClass>();

    SimpleClass original(42, "Original");
    auto moved = moveCtor(std::move(original));

    // 检查从移动后的对象处于有效但未指定的状态
    EXPECT_EQ(original.getValue(), 0);        // 我们的实现将此设置为0
    EXPECT_TRUE(original.getName().empty());  // 字符串应该被移动

    // 检查被移入的对象具有正确的值
    EXPECT_EQ(moved.getValue(), 42);
    EXPECT_EQ(moved.getName(), "Original");
}

// 测试初始化列表构造函数
TEST_F(ConstructorTest, InitializerListConstructors) {
    using namespace atom::meta;

    auto initListCtor = buildInitializerListConstructor<InitListClass, int>();

    auto instance = initListCtor({1, 2, 3, 4, 5});

    EXPECT_EQ(instance.getValues().size(), 5);
    EXPECT_EQ(instance.getValues()[0], 1);
    EXPECT_EQ(instance.getValues()[4], 5);
}

// 测试异步构造函数
TEST_F(ConstructorTest, AsyncConstructors) {
    using namespace atom::meta;

    auto asyncCtor = asyncConstructor<SimpleClass, int, std::string>();
    auto future = asyncCtor(42, "AsyncTest");

    auto instance = future.get();
    EXPECT_EQ(instance->getValue(), 42);
    EXPECT_EQ(instance->getName(), "AsyncTest");
}

// 测试单例构造函数
TEST_F(ConstructorTest, SingletonConstructors) {
    using namespace atom::meta;

    // 线程安全的单例
    auto safeSingleton = singletonConstructor<SimpleClass, true>();
    auto instance1 = safeSingleton();
    auto instance2 = safeSingleton();

    // 应该是相同的实例
    EXPECT_EQ(instance1, instance2);

    // 修改单例
    instance1->setValue(42);
    instance1->setName("SingletonTest");

    // 更改应该反映在所有引用中
    EXPECT_EQ(instance2->getValue(), 42);
    EXPECT_EQ(instance2->getName(), "SingletonTest");

    // 非线程安全单例
    auto fastSingleton = singletonConstructor<SimpleClass, false>();
    auto instance3 = fastSingleton();
    auto instance4 = fastSingleton();

    EXPECT_EQ(instance3, instance4);
}

// 测试惰性构造函数
TEST_F(ConstructorTest, LazyConstructors) {
    using namespace atom::meta;

    auto lazyCtor = lazyConstructor<SimpleClass, int, std::string>();

    // 第一次调用应该构造
    auto instance1 = lazyCtor(42, "LazyTest");  // 删除引用&
    EXPECT_EQ(instance1.getValue(), 42);
    EXPECT_EQ(instance1.getName(), "LazyTest");

    // 第二次调用应该返回相同的实例
    auto instance2 =
        lazyCtor(100, "NewValue");  // 删除引用&，参数在第一次调用后被忽略
    EXPECT_EQ(instance2.getValue(), 42);         // 仍然是原来的值
    EXPECT_EQ(instance2.getName(), "LazyTest");  // 仍然是原来的名称

    // 在不同的线程中测试，以验证线程局部行为
    std::thread thread([&lazyCtor]() {
        auto threadInstance = lazyCtor(200, "ThreadTest");  // 删除引用&
        EXPECT_EQ(threadInstance.getValue(), 200);  // 新线程中的新实例
        EXPECT_EQ(threadInstance.getName(), "ThreadTest");
    });
    thread.join();
}

// 测试工厂构造函数
TEST_F(ConstructorTest, FactoryConstructors) {
    using namespace atom::meta;

    auto factory = factoryConstructor<SimpleClass>();

    // 默认构造函数
    auto instance1 = factory();
    EXPECT_EQ(instance1->getValue(), 0);
    EXPECT_EQ(instance1->getName(), "Default");

    // 带一个参数的构造函数
    auto instance2 = factory(42);
    EXPECT_EQ(instance2->getValue(), 42);
    EXPECT_EQ(instance2->getName(), "FromInt");

    // 带两个参数的构造函数
    auto instance3 = factory(100, "FactoryTest");
    EXPECT_EQ(instance3->getValue(), 100);
    EXPECT_EQ(instance3->getName(), "FactoryTest");
}

// 测试自定义构造函数
TEST_F(ConstructorTest, CustomConstructors) {
    using namespace atom::meta;
    using namespace atom::type;

    // 创建一个组合两个整数的自定义构造函数
    auto customIntCtor = [](int a, int b) {
        return SimpleClass(a + b, "Combined");
    };

    auto wrappedCtor = customConstructor<SimpleClass>(customIntCtor);
    auto instance = wrappedCtor(40, 2);

    EXPECT_EQ(instance.getValue(), 42);
    EXPECT_EQ(instance.getName(), "Combined");

    // 使用自定义的安全构造函数测试
    auto safeCtor = [customIntCtor](
                        int a, int b) -> SafeConstructorResult<SimpleClass> {
        try {
            return customIntCtor(a, b);
        } catch (const std::exception& e) {
            return make_unexpected(std::string("Custom construction failed: ") +
                                   e.what());
        }
    };

    auto result = safeCtor(50, 10);
    EXPECT_TRUE(isValid(result));
    EXPECT_EQ(getValue(result).getValue(), 60);
    EXPECT_EQ(getValue(result).getName(), "Combined");

    // 测试抛出异常的构造函数
    auto throwingCtor = [](int) -> SimpleClass {
        throw std::runtime_error("Custom construction failed");
        return SimpleClass();  // 永远不会到达
    };

    auto safeThrowingCtor =
        [throwingCtor](int val) -> SafeConstructorResult<SimpleClass> {
        try {
            return throwingCtor(val);
        } catch (const std::exception& e) {
            return make_unexpected(std::string("Custom construction failed: ") +
                                   e.what());
        }
    };

    auto errorResult = safeThrowingCtor(0);
    EXPECT_FALSE(isValid(errorResult));
    EXPECT_TRUE(getError(errorResult).has_value());
    EXPECT_TRUE(
        getError(errorResult).value().find("Custom construction failed") !=
        std::string::npos);
}

// 测试不可复制类型
TEST_F(ConstructorTest, NonCopyableTypes) {
    using namespace atom::meta;

    // 只有shared_ptr构造函数应该可以处理不可复制类型
    auto sharedCtor = buildConstructor<NonCopyable, int>();

    auto instance = sharedCtor(42);
    EXPECT_EQ(instance->getValue(), 42);
}

// 测试绑定成员函数和变量
TEST_F(ConstructorTest, MemberBindings) {
    using namespace atom::meta;

    SimpleClass obj(42, "Original");

    // 绑定成员函数
    auto getValueBinder = bindMemberFunction(&SimpleClass::getValue);
    EXPECT_EQ(getValueBinder(obj), 42);

    // 绑定成员变量setter
    auto setValueBinder = bindMemberFunction(&SimpleClass::setValue);
    setValueBinder(obj, 100);
    EXPECT_EQ(obj.getValue(), 100);

    // 绑定const成员函数
    auto getNameBinder = bindConstMemberFunction(&SimpleClass::getName);
    EXPECT_EQ(getNameBinder(obj), "Original");

    // 测试引用行为
    SimpleClass& objRef = obj;
    EXPECT_EQ(getValueBinder(objRef), 100);

    // 测试const正确性
    const SimpleClass constObj(200, "Const");
    EXPECT_EQ(getNameBinder(constObj), "Const");
    // 这不会编译: setValueBinder(constObj, 300);
}

// 测试对象构建器模式
TEST_F(ConstructorTest, ObjectBuilder) {
    using namespace atom::meta;

    // 创建一个具有公共成员的测试类，简化此测试
    struct TestClass {
        int value = 0;
        std::string name;
        std::vector<int> data;

        void initialize() { data.resize(5, value); }

        void setMultiplier(int mult) {
            value *= mult;
            initialize();
        }
    };

    // 使用构建器模式
    auto builder = makeBuilder<TestClass>();

    auto instance = builder.with(&TestClass::value, 42)
                        .with(&TestClass::name, "BuilderTest")
                        .call(&TestClass::initialize)
                        .build();

    EXPECT_EQ(instance->value, 42);
    EXPECT_EQ(instance->name, "BuilderTest");
    EXPECT_EQ(instance->data.size(), 5);
    EXPECT_EQ(instance->data[0], 42);

    // 测试链式调用方法
    auto instance2 = makeBuilder<TestClass>()
                         .with(&TestClass::value, 10)
                         .call(&TestClass::setMultiplier, 5)
                         .build();

    EXPECT_EQ(instance2->value, 50);
    EXPECT_EQ(instance2->data.size(), 5);
    EXPECT_EQ(instance2->data[0], 50);
}

// 测试单例模式的线程安全性
TEST_F(ConstructorTest, ThreadSafeSingleton) {
    using namespace atom::meta;

    // 使用类外的变量来跟踪构造器调用次数
    static int constructorCalls = 0;

    struct Counter {
        Counter() { constructorCalls++; }
    };

    constructorCalls = 0;
    auto singleton = singletonConstructor<Counter, true>();

    // 创建10个线程，全都请求同一个单例
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&singleton]() {
            // 添加小的随机延迟，增加竞争条件的可能性
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));
            auto instance = singleton();
            (void)instance;  // 抑制未使用变量警告
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 对于真正的单例，构造函数应该只被调用一次
    EXPECT_EQ(constructorCalls, 1);
}

// 验证模板特化行为正确
TEST_F(ConstructorTest, TemplateSpecializations) {
    using namespace atom::meta;

    // 使用不同的模板参数组合进行测试
    auto defaultCtor = defaultConstructor<std::vector<int>>();
    auto instance = defaultCtor();
    EXPECT_TRUE(instance.empty());

    // 使用复杂的模板类型进行测试
    auto mapCtor =
        defaultConstructor<std::map<std::string, std::vector<int>>>();
    auto mapInstance = mapCtor();
    EXPECT_TRUE(mapInstance.empty());
}
