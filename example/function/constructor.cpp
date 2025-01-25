#include "atom/function/constructor.hpp"
#include <iostream>
#include <string>

class Example {
public:
    Example() { std::cout << "默认构造函数" << std::endl; }

    Example(int a, double b, const std::string& c) : a_(a), b_(b), c_(c) {
        std::cout << "参数化构造函数: " << a_ << ", " << b_ << ", " << c_
                  << std::endl;
    }

    Example(const Example& other) : a_(other.a_), b_(other.b_), c_(other.c_) {
        std::cout << "复制构造函数" << std::endl;
    }

    Example(Example&& other) noexcept
        : a_(std::move(other.a_)),
          b_(std::move(other.b_)),
          c_(std::move(other.c_)) {
        std::cout << "移动构造函数" << std::endl;
    }

    Example(std::initializer_list<int> list) {
        std::cout << "初始化列表构造函数" << std::endl;
        if (list.size() > 0) {
            a_ = *list.begin();
        }
    }

    void print() const {
        std::cout << "值: " << a_ << ", " << b_ << ", " << c_ << std::endl;
    }

private:
    int a_ = 0;
    double b_ = 0.0;
    std::string c_ = "default";
};

int main() {
    // 1. 默认构造器
    std::cout << "\n=== 默认构造器 ===" << std::endl;
    auto default_ctor = atom::meta::defaultConstructor<Example>();
    Example example1 = default_ctor();

    // 2. 参数化构造器
    std::cout << "\n=== 参数化构造器 ===" << std::endl;
    auto param_ctor =
        atom::meta::buildConstructor<Example, int, double, std::string>();
    auto example2 = param_ctor(42, 3.14, "Hello");
    example2->print();

    // 3. 复制构造器
    std::cout << "\n=== 复制构造器 ===" << std::endl;
    auto copy_ctor = atom::meta::buildCopyConstructor<Example>();
    Example example3 = *example2;
    example3.print();

    // 4. 移动构造器
    std::cout << "\n=== 移动构造器 ===" << std::endl;
    auto move_ctor = atom::meta::buildMoveConstructor<Example>();
    Example example4 = move_ctor(Example(1, 2.0, "Moved"));

    // 5. 异步构造器
    std::cout << "\n=== 异步构造器 ===" << std::endl;
    auto async_ctor = atom::meta::asyncConstructor<Example>();
    auto future_example = async_ctor(100, 99.9, "Async");
    auto example5 = future_example.get();
    example5->print();

    // 6. 单例构造器
    std::cout << "\n=== 单例构造器 ===" << std::endl;
    auto singleton_ctor = atom::meta::singletonConstructor<Example>();
    auto singleton1 = singleton_ctor();
    auto singleton2 = singleton_ctor();
    std::cout << "单例相同?: " << (singleton1 == singleton2) << std::endl;

    // 7. 初始化列表构造器
    std::cout << "\n=== 初始化列表构造器 ===" << std::endl;
    auto init_list_ctor =
        atom::meta::buildInitializerListConstructor<Example, int>();
    Example example6 = init_list_ctor({1, 2, 3});

    // 8. 自定义构造器
    std::cout << "\n=== 自定义构造器 ===" << std::endl;
    auto custom_ctor = atom::meta::customConstructor<Example>(
        [](int x) { return Example(x, x * 2.0, "Custom"); });
    auto example7 = custom_ctor(50);
    example7.print();

    return 0;
}