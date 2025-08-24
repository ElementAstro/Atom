#include <iostream>
#include <string>
#include <future>
#include "atom/io/async/async_io.hpp"

using namespace atom::async::io;

// 定义一个简单的函数来演示异步文件操作
void example_async_operations() {
    std::string filename = "example.txt";
    std::string data_to_write = "Hello, World!";

    // Create async context
    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    // 异步写入文件
    std::promise<AsyncResult<void>> writePromise;
    fileManager.asyncWrite(filename, std::span<const char>(data_to_write.data(), data_to_write.size()),
                          [&](AsyncResult<void> result) {
                              writePromise.set_value(std::move(result));
                          });
    auto writeResult = writePromise.get_future().get();
    if (!writeResult.success) {
        std::cerr << "Failed to write file: " << writeResult.error_message << std::endl;
        return;
    }
    std::cout << "Data written to file: " << filename << std::endl;

    // 异步读取文件
    std::promise<AsyncResult<std::string>> readPromise;
    fileManager.asyncRead(filename, [&](AsyncResult<std::string> result) {
        readPromise.set_value(std::move(result));
    });
    auto readResult = readPromise.get_future().get();
    if (!readResult.success) {
        std::cerr << "Failed to read file: " << readResult.error_message << std::endl;
        return;
    }
    std::cout << "Data read from file: " << readResult.value << std::endl;

    // 异步删除文件
    std::promise<AsyncResult<void>> delp;
    fileManager.asyncDelete(filename, [&](AsyncResult<void> r){ delp.set_value(std::move(r)); });
    auto deleteResult = delp.get_future().get();
    if (!deleteResult.success) {
        std::cerr << "Failed to delete file: " << deleteResult.error_message << std::endl;
        return;
    }
    std::cout << "File deleted: " << filename << std::endl;
}

int main() {
    try {
        example_async_operations();
        std::cout << "All async operations completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
