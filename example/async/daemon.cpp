#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <signal.h>
#include <functional>
#include <stdexcept>
#include <cstdlib>

#include "atom/async/daemon.hpp"

// 为示例代码定义一个命名空间
namespace examples {

// 简单的任务回调函数 - 传统方式
int simpleTask(int argc, char** argv) {
    std::cout << "简单任务开始执行" << std::endl;
    std::cout << "参数数量: " << argc << std::endl;
    
    for (int i = 0; i < argc; ++i) {
        std::cout << "参数[" << i << "]: " << (argv[i] ? argv[i] : "nullptr") << std::endl;
    }
    
    // 模拟工作
    std::cout << "任务正在执行..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "简单任务执行完成" << std::endl;
    
    return 0;
}

// 简单的任务回调函数 - 现代方式 (使用 std::span// filepath: examples/daemon_examples.cpp
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <signal.h>
#include <functional>
#include <stdexcept>
#include <cstdlib>

#include "atom/async/daemon.hpp"

// 为示例代码定义一个命名空间
namespace examples {

// 简单的任务回调函数 - 传统方式
int simpleTask(int argc, char** argv) {
    std::cout << "简单任务开始执行" << std::endl;
    std::cout << "参数数量: " << argc << std::endl;
    
    for (int i = 0; i < argc; ++i) {
        std::cout << "参数[" << i << "]: " << (argv[i] ? argv[i] : "nullptr") << std::endl;
    }
    
    // 模拟工作
    std::cout << "任务正在执行..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "简单任务执行完成" << std::endl;
    
    return 0;
}

// 简单的任务回调函数 - 现代方式 (使用 std::span