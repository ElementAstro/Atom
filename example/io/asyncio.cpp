#include <iostream>
#include <string>
#include <coroutine>
#include "atom/io/async/async_io.hpp"

using namespace atom::async::io;

// 定义一个简单的协程函数来演示异步文件操作
AsyncResult<void> example_async_operations() {
    std::string filename = "example.txt";
    std::string data_to_write = "Hello, World!";

    // Create async context
    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    // 异步写入文件
    auto writeResult = fileManager.writeFile(filename, std::span<const char>(data_to_write.data(), data_to_write.size())).get();
    if (!writeResult.success) {
        std::cerr << "Failed to write file: " << writeResult.error_message << std::endl;
        return AsyncResult<void>::error_result(writeResult.error_message);
    }
    std::cout << "Data written to file: " << filename << std::endl;

    // 异步读取文件
    auto readResult = fileManager.readFile(filename).get();
    if (!readResult.success) {
        std::cerr << "Failed to read file: " << readResult.error_message << std::endl;
        return AsyncResult<void>::error_result(readResult.error_message);
    }
    std::cout << "Data read from file: " << readResult.value << std::endl;

    // 异步删除文件
    std::promise<AsyncResult<void>> delp;
    fileManager.asyncDelete(filename, [&](AsyncResult<void> r){ delp.set_value(std::move(r)); });
    auto deleteResult = delp.get_future().get();
    if (!deleteResult.success) {
        std::cerr << "Failed to delete file: " << deleteResult.error_message << std::endl;
        return AsyncResult<void>::error_result(deleteResult.error_message);
    }
    std::cout << "File deleted: " << filename << std::endl;

    return AsyncResult<void>::success_result();
}

int main() {
    try {
        auto result = example_async_operations();

        if (!result.success) {
            std::cerr << "Async operations failed with error: " << result.error_message << std::endl;
            return 1;
        }

        std::cout << "All async operations completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
