#include "atom/function/decorate.hpp"
#include <iostream>
#include <string>

// 基础函数
int add(int a, int b) { return a + b; }
void printHello() { std::cout << "Hello!" << std::endl; }
void printGoodbye() { std::cout << "Goodbye!" << std::endl; }
std::string greet(const std::string& name) { return "Hello, " + name + "!"; }

int main() {
    // 示例1: 基础装饰器
    using AddFunc = int(int, int);
    auto decoratedAdd =
        atom::meta::makeDecorator<AddFunc>([](int a, int b) -> int {
            std::cout << "Before addition" << std::endl;
            int result = add(a, b);
            std::cout << "After addition: " << result << std::endl;
            return result;
        });

    int result = decoratedAdd(3, 4);
    std::cout << "Result: " << result << std::endl;

    // 示例2: 循环装饰器
    auto loopedAdd = atom::meta::makeLoopDecorator(
        [](int a, int b) -> int { return add(a, b); });

    int loopCount = 5;
    int loopedResult = loopedAdd(loopCount, 1, 2);
    std::cout << "Looped result: " << loopedResult << std::endl;

    // 示例3: 条件检查装饰器
    auto conditionCheckedGreet = atom::meta::makeConditionCheckDecorator(greet);

    bool condition = true;
    std::string greeting = conditionCheckedGreet(
        [condition]() -> bool { return condition; }, "Alice");
    std::cout << greeting << std::endl;

    // 示例4: 装饰器步进器
    auto stepper = atom::meta::makeDecorateStepper(add);

    // 添加装饰器
    stepper.addDecorator(atom::meta::makeDecorator<AddFunc>(
        [](const std::function<int(int, int)>& func, int a, int b) -> int {
            std::cout << "Before call" << std::endl;
            int result = func(a, b);
            std::cout << "After call: " << result << std::endl;
            return result;
        }));

    stepper.addDecorator(atom::meta::makeLoopDecorator(add));

    int stepperResult = stepper.execute(5, 3);
    std::cout << "Stepper result: " << stepperResult << std::endl;

    return 0;
}