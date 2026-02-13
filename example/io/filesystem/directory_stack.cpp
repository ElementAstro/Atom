/**
 * @file directory_stack.cpp
 * @brief Comprehensive demonstration of directory stack operations (pushd/popd)
 *
 * This example demonstrates:
 * - Async pushd/popd operations (callback-based)
 * - Directory stack management (dirs, peek, size, isEmpty)
 * - Stack persistence (asyncSaveStackToFile / asyncLoadStackFromFile)
 * - Stack manipulation (swap, remove, clear)
 * - Error handling for invalid directories
 * - Cross-platform directory handling
 *
 * @note DirectoryStack uses async callbacks (asyncPushd/asyncPopd) or
 *       coroutines (pushd/popd). This example uses async callbacks.
 */

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include "atom/io/filesystem/pushd.hpp"

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

namespace fs = std::filesystem;

/**
 * @brief Creates test directories for demonstration
 */
void createTestDirectories() {
    std::cout << "Creating test directories..." << std::endl;
    try {
        fs::create_directories("test_dirs/dir1");
        fs::create_directories("test_dirs/dir2/subdir");
        fs::create_directories("test_dirs/dir3");
        std::cout << "  Test directories created successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  Failed to create test directories: " << e.what()
                  << std::endl;
    }
}

/**
 * @brief Cleans up test directories
 */
void cleanupTestDirectories() {
    try {
        if (fs::exists("test_dirs")) {
            fs::remove_all("test_dirs");
            std::cout << "  Test directories cleaned up" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "  Warning: Failed to cleanup: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrates async pushd/popd operations
 */
void demonstrateBasicOperations() {
    std::cout << "\n=== Basic Directory Stack Operations ===" << std::endl;

#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    atom::io::DirectoryStack dirStack(ioCtx);
#else
    atom::io::DirectoryStack dirStack(static_cast<void*>(nullptr));
#endif

    std::cout << "Initial directory: " << fs::current_path() << std::endl;
    std::cout << "Stack size: " << dirStack.size() << std::endl;
    std::cout << "Is empty: " << (dirStack.isEmpty() ? "Yes" : "No")
              << std::endl;

    // Push to test directories using asyncPushd with std::string
    std::cout << "\n1. Async pushd to test_dirs/dir1..." << std::endl;
    dirStack.asyncPushd(std::string("test_dirs/dir1"),
                        [](const std::error_code& ec) {
                            if (!ec) {
                                std::cout << "  Changed to: "
                                          << fs::current_path() << std::endl;
                            } else {
                                std::cerr << "  pushd error: " << ec.message()
                                          << std::endl;
                            }
                        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "  Stack size: " << dirStack.size() << std::endl;

    std::cout << "\n2. Async pushd to ../dir2..." << std::endl;
    dirStack.asyncPushd(std::string("../dir2"),
                        [](const std::error_code& ec) {
                            if (!ec) {
                                std::cout << "  Changed to: "
                                          << fs::current_path() << std::endl;
                            } else {
                                std::cerr << "  pushd error: " << ec.message()
                                          << std::endl;
                            }
                        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "  Stack size: " << dirStack.size() << std::endl;

    std::cout << "\n3. Async pushd to subdir..." << std::endl;
    dirStack.asyncPushd(std::string("subdir"),
                        [](const std::error_code& ec) {
                            if (!ec) {
                                std::cout << "  Changed to: "
                                          << fs::current_path() << std::endl;
                            } else {
                                std::cerr << "  pushd error: " << ec.message()
                                          << std::endl;
                            }
                        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "  Stack size: " << dirStack.size() << std::endl;

    // Show stack contents
    std::cout << "\n4. Current stack contents:" << std::endl;
    auto stackList = dirStack.dirs();
    for (size_t i = 0; i < stackList.size(); ++i) {
        std::cout << "  [" << i << "] " << stackList[i] << std::endl;
    }

    // Peek at top
    if (!dirStack.isEmpty()) {
        auto top = dirStack.peek();
        std::cout << "  Top of stack: " << top << std::endl;
    }

    // Pop back through directories
    std::cout << "\n5. Popping back through directories..." << std::endl;
    while (!dirStack.isEmpty()) {
        dirStack.asyncPopd([](const std::error_code& ec) {
            if (!ec) {
                std::cout << "  Popped to: " << fs::current_path() << std::endl;
            } else {
                std::cerr << "  popd error: " << ec.message() << std::endl;
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "  Stack size: " << dirStack.size() << std::endl;

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates stack persistence operations
 */
void demonstrateStackPersistence() {
    std::cout << "\n=== Stack Persistence Operations ===" << std::endl;

#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    atom::io::DirectoryStack dirStack(ioCtx);
#else
    atom::io::DirectoryStack dirStack(static_cast<void*>(nullptr));
#endif
    const std::string stackFile = "directory_stack.txt";

    // Build up a stack
    std::cout << "Building directory stack..." << std::endl;
    dirStack.asyncPushd(std::string("test_dirs/dir1"),
                        [](const std::error_code&) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    dirStack.asyncPushd(std::string("../dir2"),
                        [](const std::error_code&) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    dirStack.asyncPushd(std::string("../dir3"),
                        [](const std::error_code&) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "  Stack size: " << dirStack.size() << std::endl;

    // Async save stack to file
    std::cout << "\nSaving stack to file: " << stackFile << std::endl;
    dirStack.asyncSaveStackToFile(
        stackFile, [](const std::error_code& ec) {
            if (!ec) {
                std::cout << "  Stack saved successfully" << std::endl;
            } else {
                std::cerr << "  Save error: " << ec.message() << std::endl;
            }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Clear the stack via popd
    std::cout << "\nClearing current stack..." << std::endl;
    while (!dirStack.isEmpty()) {
        dirStack.asyncPopd([](const std::error_code&) {});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << "  Stack size after clearing: " << dirStack.size() << std::endl;

    // Async load stack from file
    std::cout << "\nLoading stack from file: " << stackFile << std::endl;
    dirStack.asyncLoadStackFromFile(
        stackFile, [](const std::error_code& ec) {
            if (!ec) {
                std::cout << "  Stack loaded successfully" << std::endl;
            } else {
                std::cerr << "  Load error: " << ec.message() << std::endl;
            }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "  Stack size after loading: " << dirStack.size() << std::endl;
    auto stackList = dirStack.dirs();
    for (size_t i = 0; i < stackList.size(); ++i) {
        std::cout << "  [" << i << "] " << stackList[i] << std::endl;
    }

    // Clean up
    while (!dirStack.isEmpty()) {
        dirStack.asyncPopd([](const std::error_code&) {});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (fs::exists(stackFile)) {
        fs::remove(stackFile);
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates error handling scenarios
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling Scenarios ===" << std::endl;

#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    atom::io::DirectoryStack dirStack(ioCtx);
#else
    atom::io::DirectoryStack dirStack(static_cast<void*>(nullptr));
#endif

    // Try to push to non-existent directory
    std::cout << "1. Async pushd to non-existent directory..." << std::endl;
    dirStack.asyncPushd(
        std::string("non_existent_directory_xyz"),
        [](const std::error_code& ec) {
            if (ec) {
                std::cout << "  Expected error: " << ec.message() << std::endl;
            } else {
                std::cout << "  Unexpected success" << std::endl;
            }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Try to pop from empty stack
    std::cout << "\n2. Async popd from empty stack..." << std::endl;
    dirStack.asyncPopd([](const std::error_code& ec) {
        if (ec) {
            std::cout << "  Expected error: " << ec.message() << std::endl;
        } else {
            std::cout << "  Unexpected success" << std::endl;
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Try to peek empty stack
    std::cout << "\n3. Checking empty stack..." << std::endl;
    std::cout << "  Stack is empty: " << (dirStack.isEmpty() ? "Yes" : "No")
              << std::endl;

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates async directory operations: goto, get current dir
 */
void demonstrateAsyncOperations() {
    std::cout << "\n=== Async Directory Stack Operations ===" << std::endl;

#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    atom::io::DirectoryStack dirStack(ioCtx);
#else
    atom::io::DirectoryStack dirStack(static_cast<void*>(nullptr));
#endif

    // Build stack
    dirStack.asyncPushd(std::string("test_dirs/dir1"),
                        [](const std::error_code&) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    dirStack.asyncPushd(std::string("../dir2"),
                        [](const std::error_code&) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Async get current directory
    std::cout << "1. Async get current directory..." << std::endl;
    dirStack.asyncGetCurrentDirectory(
        [](const fs::path& path, const std::error_code& ec) {
            if (!ec) {
                std::cout << "  Current directory: " << path << std::endl;
            } else {
                std::cerr << "  Error: " << ec.message() << std::endl;
            }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Async goto index
    std::cout << "\n2. Async goto index 0..." << std::endl;
    if (dirStack.size() > 0) {
        dirStack.asyncGotoIndex(
            0, [](const std::error_code& ec) {
                if (!ec) {
                    std::cout << "  Jumped to index 0: "
                              << fs::current_path() << std::endl;
                } else {
                    std::cerr << "  Goto error: " << ec.message() << std::endl;
                }
            });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Async save and load
    std::cout << "\n3. Async save stack to file..." << std::endl;
    dirStack.asyncSaveStackToFile(
        "async_stack.txt", [](const std::error_code& ec) {
            if (!ec) {
                std::cout << "  Stack saved asynchronously" << std::endl;
            } else {
                std::cerr << "  Save error: " << ec.message() << std::endl;
            }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Pop all
    while (!dirStack.isEmpty()) {
        dirStack.asyncPopd([](const std::error_code&) {});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Load back
    std::cout << "\n4. Async load stack from file..." << std::endl;
    dirStack.asyncLoadStackFromFile(
        "async_stack.txt", [](const std::error_code& ec) {
            if (!ec) {
                std::cout << "  Stack loaded asynchronously" << std::endl;
            } else {
                std::cerr << "  Load error: " << ec.message() << std::endl;
            }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "  Loaded stack:" << std::endl;
    auto stackList = dirStack.dirs();
    for (size_t i = 0; i < stackList.size(); ++i) {
        std::cout << "    [" << i << "] " << stackList[i] << std::endl;
    }

    // Clean up
    while (!dirStack.isEmpty()) {
        dirStack.asyncPopd([](const std::error_code&) {});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (fs::exists("async_stack.txt")) {
        fs::remove("async_stack.txt");
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates stack manipulation operations (swap, remove, clear)
 */
void demonstrateStackManipulation() {
    std::cout << "\n=== Stack Manipulation ===" << std::endl;

#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    atom::io::DirectoryStack dirStack(ioCtx);
#else
    atom::io::DirectoryStack dirStack(static_cast<void*>(nullptr));
#endif

    // Build up stack
    dirStack.asyncPushd(std::string("test_dirs/dir1"),
                        [](const std::error_code&) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    dirStack.asyncPushd(std::string("../dir2"),
                        [](const std::error_code&) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    dirStack.asyncPushd(std::string("../dir3"),
                        [](const std::error_code&) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Initial stack:" << std::endl;
    auto stackList = dirStack.dirs();
    for (size_t i = 0; i < stackList.size(); ++i) {
        std::cout << "  [" << i << "] " << stackList[i] << std::endl;
    }

    // Swap two entries
    std::cout << "\n1. Swapping indices 0 and 2..." << std::endl;
    if (dirStack.size() >= 3) {
        dirStack.swap(0, 2);
        stackList = dirStack.dirs();
        for (size_t i = 0; i < stackList.size(); ++i) {
            std::cout << "  [" << i << "] " << stackList[i] << std::endl;
        }
    }

    // Remove an entry
    std::cout << "\n2. Removing index 1..." << std::endl;
    if (dirStack.size() >= 2) {
        dirStack.remove(1);
        stackList = dirStack.dirs();
        for (size_t i = 0; i < stackList.size(); ++i) {
            std::cout << "  [" << i << "] " << stackList[i] << std::endl;
        }
    }

    // Clear
    std::cout << "\n3. Clearing stack..." << std::endl;
    dirStack.clear();
    std::cout << "  Stack size: " << dirStack.size() << std::endl;
    std::cout << "  Is empty: " << (dirStack.isEmpty() ? "Yes" : "No")
              << std::endl;

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

int main() {
    try {
        std::cout << "Atom I/O Directory Stack Examples" << std::endl;
        std::cout << "=================================" << std::endl;

        createTestDirectories();
        auto originalDir = fs::current_path();

        demonstrateBasicOperations();
        fs::current_path(originalDir);

        demonstrateStackPersistence();
        fs::current_path(originalDir);

        demonstrateErrorHandling();
        fs::current_path(originalDir);

        demonstrateAsyncOperations();
        fs::current_path(originalDir);

        demonstrateStackManipulation();
        fs::current_path(originalDir);

        std::cout << "\nRestored to original directory: "
                  << fs::current_path() << std::endl;

        cleanupTestDirectories();

        std::cout << "\nAll directory stack examples completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal exception: " << e.what() << std::endl;
        cleanupTestDirectories();
        return 1;
    }
}
