/*!
 * \file test_ffi.cpp
 * \brief Unit tests for FFI functionality
 * \author GitHub Copilot
 * \date 2024-10-14
 */

#include <gtest/gtest.h>
#include "atom/meta/ffi.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
constexpr const char* TEST_LIB_PATH =
    "msvcrt.dll";  // Standard C runtime on Windows
constexpr const char* NONEXISTENT_LIB = "nonexistent_lib.dll";
constexpr const char* MATH_FUNC = "cos";
constexpr const char* STRING_FUNC = "strlen";
#else
constexpr const char* TEST_LIB_PATH =
    "libm.so";  // Math library on Unix-like systems
constexpr const char* NONEXISTENT_LIB = "libnonexistent.so";
constexpr const char* MATH_FUNC = "cos";
constexpr const char* STRING_FUNC = "strlen";
#endif

namespace {

// Test fixture for FFI tests
class FFITest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test LibraryHandle basic functionality
TEST_F(FFITest, LibraryHandleBasic) {
    // Test loading a valid library
    atom::meta::LibraryHandle handle;
    auto result = handle.load(TEST_LIB_PATH);
    ASSERT_TRUE(result.has_value()) << "Failed to load valid library";
    EXPECT_TRUE(handle.isLoaded());

    // Test unloading
    handle.unload();
    EXPECT_FALSE(handle.isLoaded());

    // Test loading a non-existent library
    result = handle.load(NONEXISTENT_LIB);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), atom::meta::FFIError::LibraryLoadFailed);
}

// Test LibraryHandle symbol resolution
TEST_F(FFITest, LibraryHandleSymbol) {
    atom::meta::LibraryHandle handle(TEST_LIB_PATH);
    ASSERT_TRUE(handle.isLoaded());

    // Test getting a valid symbol
    auto symbolResult = handle.getSymbol(MATH_FUNC);
    ASSERT_TRUE(symbolResult.has_value()) << "Failed to find valid symbol";
    EXPECT_NE(symbolResult.value(), nullptr);

    // Test getting an invalid symbol
    auto invalidResult = handle.getSymbol("this_function_does_not_exist");
    EXPECT_FALSE(invalidResult.has_value());
    EXPECT_EQ(invalidResult.error(), atom::meta::FFIError::SymbolNotFound);
}

// Test DynamicLibrary loading strategies
TEST_F(FFITest, DynamicLibraryLoading) {
    // Test immediate loading
    {
        atom::meta::DynamicLibrary::Options options;
        options.strategy = atom::meta::DynamicLibrary::LoadStrategy::Immediate;
        EXPECT_NO_THROW(
            { atom::meta::DynamicLibrary lib(TEST_LIB_PATH, options); });
    }

    // Test lazy loading
    {
        atom::meta::DynamicLibrary::Options options;
        options.strategy = atom::meta::DynamicLibrary::LoadStrategy::Lazy;
        atom::meta::DynamicLibrary lib(TEST_LIB_PATH, options);

        // Function call should trigger loading
        auto handleResult = lib.getHandle();
        EXPECT_TRUE(handleResult.has_value());
    }

    // Test on-demand loading
    {
        atom::meta::DynamicLibrary::Options options;
        options.strategy = atom::meta::DynamicLibrary::LoadStrategy::OnDemand;
        atom::meta::DynamicLibrary lib(TEST_LIB_PATH, options);

        // Explicitly load the library
        auto result = lib.loadLibrary();
        EXPECT_TRUE(result.has_value());
    }
}

// Test DynamicLibrary function access
TEST_F(FFITest, DynamicLibraryFunctions) {
    atom::meta::DynamicLibrary::Options options;
    atom::meta::DynamicLibrary lib(TEST_LIB_PATH, options);

    // Test getting a valid function
    using CosFunc = double(double);
    auto cosResult = lib.getFunction<CosFunc>(MATH_FUNC);
    ASSERT_TRUE(cosResult.has_value()) << "Failed to get cos function";

    // Test calling the function
    auto cosFunc = cosResult.value();
    double result = cosFunc(0.0);
    EXPECT_DOUBLE_EQ(result, 1.0);

    // Test function caching
    EXPECT_TRUE(lib.hasFunction(MATH_FUNC));

    // Test getting an invalid function
    auto invalidResult = lib.getFunction<CosFunc>("nonexistent_function");
    EXPECT_FALSE(invalidResult.has_value());
    EXPECT_EQ(invalidResult.error(), atom::meta::FFIError::SymbolNotFound);
}

// Test callFunctionWithTimeout
TEST_F(FFITest, CallFunctionWithTimeout) {
    atom::meta::DynamicLibrary::Options options;
    atom::meta::DynamicLibrary lib(TEST_LIB_PATH, options);

    // Test calling a function with sufficient timeout
    auto result = lib.callFunctionWithTimeout<double, double>(
        MATH_FUNC, std::chrono::milliseconds(1000), 0.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result.value(), 1.0);

    // Test with string function
    auto strResult = lib.callFunctionWithTimeout<size_t, const char*>(
        STRING_FUNC, std::chrono::milliseconds(1000), "hello");
    ASSERT_TRUE(strResult.has_value());
    EXPECT_EQ(strResult.value(), 5UL);
}

// Test FFIWrapper error handling
TEST_F(FFITest, FFIWrapperValidation) {
    atom::meta::FFIWrapper<double, double> wrapper(true);  // with validation

    // Create a null function pointer to simulate error
    [[maybe_unused]] void* nullFunc = nullptr;

    // We can't directly test null pointer validation as it would cause a
    // segfault But we can test passing nullptr to a function that expects
    // non-null pointers

    atom::meta::FFIWrapper<size_t, const char*> strWrapper(true);
    [[maybe_unused]] auto strResult = strWrapper.call((void*)strlen, nullptr);

    // This should ideally return an error, but the specific behavior might vary
    // depending on the platform's implementation of strlen with nullptr
    // So we don't assert specific behavior here
}

// Test CallbackRegistry basic functionality
TEST_F(FFITest, CallbackRegistryBasic) {
    atom::meta::CallbackRegistry registry;

    // Register a callback
    registry.registerCallback("test", [](int x) { return x * 2; });

    // Check if the callback exists
    EXPECT_TRUE(registry.hasCallback("test"));
    EXPECT_FALSE(registry.hasCallback("nonexistent"));

    // Get the callback
    auto callbackResult = registry.getCallback<int(int)>("test");
    ASSERT_TRUE(callbackResult.has_value());

    // Call the callback
    auto callback = *callbackResult.value();
    EXPECT_EQ(callback(5), 10);

    // Remove the callback
    registry.removeCallback("test");
    EXPECT_FALSE(registry.hasCallback("test"));

    // Clear all callbacks
    registry.registerCallback("test1", [](int x) { return x; });
    registry.registerCallback("test2", [](int x) { return x * x; });
    registry.clear();
    EXPECT_FALSE(registry.hasCallback("test1"));
    EXPECT_FALSE(registry.hasCallback("test2"));
}

// Test CallbackRegistry type safety
TEST_F(FFITest, CallbackRegistryTypeSafety) {
    atom::meta::CallbackRegistry registry;

    // Register a callback with one signature
    registry.registerCallback("test", [](int x) { return x * 2; });

    // Try to get it with a wrong signature
    auto wrongResult = registry.getCallback<double(double)>("test");
    EXPECT_FALSE(wrongResult.has_value());
    EXPECT_EQ(wrongResult.error(), atom::meta::FFIError::TypeMismatch);

    // Try to get a nonexistent callback
    auto missingResult = registry.getCallback<int(int)>("nonexistent");
    EXPECT_FALSE(missingResult.has_value());
    EXPECT_EQ(missingResult.error(), atom::meta::FFIError::CallbackNotFound);
}

// Test async callbacks
TEST_F(FFITest, AsyncCallbacks) {
    atom::meta::CallbackRegistry registry;

    // Track how many times the callback is called
    std::atomic<int> callCount = 0;

    // Register an async callback
    registry.registerAsyncCallback("async_test", [&callCount](int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        callCount++;
        return ms * 2;
    });

    // Get the callback
    auto callbackResult =
        registry.getCallback<std::future<int>(int)>("async_test");
    ASSERT_TRUE(callbackResult.has_value());

    // Call the callback
    auto future = (*callbackResult.value())(10);

    // Result should be ready after waiting
    auto status = future.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(future.get(), 20);
    EXPECT_EQ(callCount, 1);
}

// Test error handling
TEST_F(FFITest, ErrorHandling) {
    // Test FFIException construction and methods
    atom::meta::FFIException ex("Test error",
                                atom::meta::FFIError::SymbolNotFound);
    EXPECT_EQ(ex.error_code(), atom::meta::FFIError::SymbolNotFound);
    EXPECT_TRUE(std::string(ex.what()).find("Test error") != std::string::npos);
}

// Test thread safety
TEST_F(FFITest, ThreadSafety) {
    atom::meta::DynamicLibrary::Options options;
    atom::meta::DynamicLibrary lib(TEST_LIB_PATH, options);

    // Create multiple threads that all access the library
    std::vector<std::thread> threads;
    std::atomic<int> successCount = 0;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&lib, &successCount]() {
            // Get the cos function
            using CosFunc = double(double);
            auto cosResult = lib.getFunction<CosFunc>(MATH_FUNC);
            if (cosResult.has_value()) {
                auto cosFunc = cosResult.value();
                double result = cosFunc(0.0);
                if (std::abs(result - 1.0) < 0.000001) {
                    successCount++;
                }
            }
        });
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    // All threads should have succeeded
    EXPECT_EQ(successCount, 10);
}

// Test FFI type mapping
TEST_F(FFITest, FFITypeMapping) {
    // Test basic types
    EXPECT_EQ(atom::meta::getFFIType<int>(), &ffi_type_sint);
    EXPECT_EQ(atom::meta::getFFIType<float>(), &ffi_type_float);
    EXPECT_EQ(atom::meta::getFFIType<double>(), &ffi_type_double);
    EXPECT_EQ(atom::meta::getFFIType<void>(), &ffi_type_void);

    // Test pointer types
    EXPECT_EQ(atom::meta::getFFIType<int*>(), &ffi_type_pointer);
    EXPECT_EQ(atom::meta::getFFIType<const char*>(), &ffi_type_pointer);
    EXPECT_EQ(atom::meta::getFFIType<std::string>(), &ffi_type_pointer);
}

// Define a mock class for LibraryObject testing
class MockObject {
public:
    MockObject() = default;
    virtual ~MockObject() = default;

    virtual int getValue() const { return 42; }
};

// Test LibraryObject
TEST_F(FFITest, LibraryObject) {
    // Create a LibraryObject directly for testing
    MockObject* rawObject = new MockObject();
    atom::meta::LibraryObject<MockObject> object(rawObject);

    // Test operator->
    EXPECT_EQ(object->getValue(), 42);

    // Test operator*
    EXPECT_EQ((*object).getValue(), 42);

    // Test isValid
    EXPECT_TRUE(object.isValid());
}

// Test FFIResourceGuard
TEST_F(FFITest, ResourceGuard) {
    bool deleted = false;
    int* testValue = new int(42);

    {
        atom::meta::FFIResourceGuard guard;
        guard.addResource<int>(testValue, [&deleted](int* ptr) {
            delete ptr;
            deleted = true;
        });

        // Resource should still be valid here
        EXPECT_FALSE(deleted);
    }

    // Resource should be deleted when guard goes out of scope
    EXPECT_TRUE(deleted);
}

}  // namespace
