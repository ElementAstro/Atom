/**
 * Comprehensive examples for atom::meta::ffi utilities
 *
 * This file demonstrates the use of all FFI functionality:
 * 1. Basic library loading
 * 2. Function calls
 * 3. Callbacks
 * 4. Timeouts
 * 5. Error handling
 * 6. Library objects
 * 7. Asynchronous operations
 * 8. Type mapping
 * 9. Resource management
 * 10. Advanced features
 */

#include "atom/meta/ffi.hpp"
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

using namespace atom::meta;
using namespace atom::type;  // 添加type命名空间

// Helper function to print section headers
void printHeader(const std::string& title) {
    std::cout << "\n=========================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "=========================================================="
              << std::endl;
}

std::string errorToString(const FFIError& error) {
    switch (error) {
        case FFIError::None:
            return "None";
        case FFIError::SymbolNotFound:
            return "SymbolNotFound";
        case FFIError::InvalidArgument:
            return "InvalidArgument";
        case FFIError::FunctionCallFailed:
            return "FunctionCallFailed";
        case FFIError::Timeout:
            return "Timeout";
        default:
            return "Undefined";
    }
}

// Helper to print error results
template <typename T>
void printResult(const FFIResult<T>& result, const std::string& description) {
    std::cout << std::left << std::setw(40) << description;
    if (result) {
        std::cout << ": Success" << std::endl;
    } else {
        std::cout << ": Error - " << errorToString(result.error().error())
                  << std::endl;
    }
}

// Example custom struct that implements FFI type layout
struct ExampleStruct : public FFITypeLayoutGenerator<ExampleStruct> {
    int x;
    double y;
    char* z;

    static void defineFFITypeLayout(ffi_type& layout) {
        static ffi_type* elements[] = {
            &ffi_type_sint32,   // for int x
            &ffi_type_double,   // for double y
            &ffi_type_pointer,  // for char* z
            nullptr             // terminator
        };

        layout.size = 0;
        layout.alignment = 0;
        layout.type = FFI_TYPE_STRUCT;
        layout.elements = elements;
    }
};

// Sample callback function
int sampleCallback(int a, int b) {
    std::cout << "Callback called with: " << a << ", " << b << std::endl;
    return a + b;
}

// Define a mock library interface for demonstration
class MockLibraryInterface {
public:
    virtual ~MockLibraryInterface() = default;
    virtual int add(int a, int b) = 0;
    virtual void performTask(const char* taskName) = 0;
};

// Mock implementation for example
class MockLibraryImpl : public MockLibraryInterface {
public:
    int add(int a, int b) override { return a + b; }

    void performTask(const char* taskName) override {
        std::cout << "Performing task: " << taskName << std::endl;
    }
};

// Factory function
extern "C" MockLibraryInterface* createMockLibrary() {
    return new MockLibraryImpl();
}

int main() {
    std::cout << "================================================="
              << std::endl;
    std::cout << "   Comprehensive FFI Utilities Examples           "
              << std::endl;
    std::cout << "================================================="
              << std::endl;

    //=========================================================================
    // 1. Basic Library Loading
    //=========================================================================
    printHeader("1. Basic Library Loading");

    // Path to the dynamic library (modify as needed)
    // Note: In a real application, you would use an actual shared library
    std::string libraryPath = "/usr/lib/libm.so";  // Math library on Linux

    try {
        // Immediate loading
        DynamicLibrary::Options immediateOptions;
        immediateOptions.strategy = DynamicLibrary::LoadStrategy::Immediate;
        DynamicLibrary mathLibImmediate(libraryPath, immediateOptions);
        std::cout << "Immediate library loading: Success" << std::endl;

        // Lazy loading
        DynamicLibrary::Options lazyOptions;
        lazyOptions.strategy = DynamicLibrary::LoadStrategy::Lazy;
        DynamicLibrary mathLibLazy(libraryPath, lazyOptions);
        std::cout << "Lazy library loading initialized: Success" << std::endl;

        // On-demand loading
        DynamicLibrary::Options onDemandOptions;
        onDemandOptions.strategy = DynamicLibrary::LoadStrategy::OnDemand;
        onDemandOptions.cacheSymbols = true;
        DynamicLibrary mathLibOnDemand(libraryPath, onDemandOptions);
        std::cout << "On-demand library object created: Success" << std::endl;

        // Explicitly load the on-demand library
        auto loadResult = mathLibOnDemand.loadLibrary();
        printResult(loadResult, "Explicit library loading");

        // Retrieve library handle
        auto handleResult = mathLibOnDemand.getHandle();
        printResult(handleResult, "Getting library handle");

    } catch (const FFIException& e) {
        std::cerr << "FFI Exception: " << e.what() << std::endl;
        std::cerr << "Error code: " << errorToString(e.error_code())
                  << std::endl;
    }

    //=========================================================================
    // 2. Function Calls
    //=========================================================================
    printHeader("2. Function Calls");

    try {
        DynamicLibrary mathLib(libraryPath, {});

        // Get a function from the library
        auto sinFuncResult = mathLib.getFunction<double(double)>("sin");
        printResult(sinFuncResult, "Getting 'sin' function");

        if (sinFuncResult) {
            auto sinFunc = sinFuncResult.value();
            double result = sinFunc(3.14159265358979323846 / 2);
            std::cout << "sin(π/2) = " << result << std::endl;
        }

        // Check if a function exists
        bool hasCos = mathLib.hasFunction("cos");
        std::cout << "Library has 'cos' function: " << (hasCos ? "Yes" : "No")
                  << std::endl;

        // Add a function to the cache
        auto addResult = mathLib.addFunction<double(double)>("cos");
        printResult(addResult, "Adding 'cos' function to cache");

        // Direct function call with timeout
        auto cosResult = mathLib.callFunctionWithTimeout<double>(
            "cos", std::chrono::seconds(1), 0.0);

        if (cosResult) {
            std::cout << "cos(0) = " << cosResult.value() << std::endl;
        } else {
            std::cout << "Error calling cos: "
                      << errorToString(cosResult.error().error()) << std::endl;
        }

    } catch (const FFIException& e) {
        std::cerr << "FFI Exception: " << e.what() << std::endl;
    }

    //=========================================================================
    // 3. Callbacks
    //=========================================================================
    printHeader("3. Callbacks");

    try {
        CallbackRegistry registry;

        // Register a callback
        registry.registerCallback("add", sampleCallback);
        std::cout << "Callback registered: Success" << std::endl;

        // Check if callback exists
        bool hasCallback = registry.hasCallback("add");
        std::cout << "Has 'add' callback: " << (hasCallback ? "Yes" : "No")
                  << std::endl;

        // Get the callback
        auto callbackResult = registry.getCallback<int(int, int)>("add");
        printResult(callbackResult, "Getting 'add' callback");

        if (callbackResult) {
            auto callback = *callbackResult.value();
            int sum = callback(5, 7);
            std::cout << "Callback result: 5 + 7 = " << sum << std::endl;
        }

        // Register an asynchronous callback
        registry.registerAsyncCallback("asyncAdd", [](int a, int b) -> int {
            std::cout << "Async callback processing: " << a << " + " << b
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return a + b;
        });
        std::cout << "Async callback registered: Success" << std::endl;

        // Get and call the async callback
        auto asyncCallbackResult =
            registry.getCallback<std::future<int>(int, int)>("asyncAdd");
        if (asyncCallbackResult) {
            auto asyncCallback = *asyncCallbackResult.value();
            std::future<int> futureResult = asyncCallback(10, 20);
            std::cout << "Async operation started..." << std::endl;
            int asyncSum = futureResult.get();
            std::cout << "Async callback result: 10 + 20 = " << asyncSum
                      << std::endl;
        }

        // Remove a callback
        registry.removeCallback("add");
        std::cout << "Removed 'add' callback" << std::endl;
        std::cout << "Has 'add' callback after removal: "
                  << (registry.hasCallback("add") ? "Yes" : "No") << std::endl;

        // Clear all callbacks
        registry.clear();
        std::cout << "Cleared all callbacks" << std::endl;

    } catch (const FFIException& e) {
        std::cerr << "FFI Exception: " << e.what() << std::endl;
    }

    //=========================================================================
    // 4. Timeouts
    //=========================================================================
    printHeader("4. Timeouts");

    try {
        DynamicLibrary::Options timeoutOptions;
        timeoutOptions.defaultTimeout = std::chrono::milliseconds(500);
        DynamicLibrary mathLib(libraryPath, timeoutOptions);

        // Function call with default timeout
        std::cout << "Calling function with default timeout (500ms)..."
                  << std::endl;
        auto result1 = mathLib.callFunctionWithTimeout<double>(
            "sin", timeoutOptions.defaultTimeout, 1.0);
        printResult(result1, "Function call with default timeout");

        // Function call with custom timeout
        std::cout << "Calling function with custom timeout (2s)..."
                  << std::endl;
        auto result2 = mathLib.callFunctionWithTimeout<double>(
            "cos", std::chrono::seconds(2), 0.0);
        printResult(result2, "Function call with custom timeout");

        // Simulate a timeout with a long-running operation
        // Note: This is just for demonstration - we're not actually calling a
        // slow function
        std::cout << "Simulating a function call that would time out..."
                  << std::endl;
        std::cout << "(In practice, this would attempt to call a function that "
                     "takes too long)"
                  << std::endl;

    } catch (const FFIException& e) {
        std::cerr << "FFI Exception: " << e.what() << std::endl;
    }

    //=========================================================================
    // 5. Error Handling
    //=========================================================================
    printHeader("5. Error Handling");

    try {
        // Attempt to load a non-existent library
        DynamicLibrary badLib("/nonexistent/library.so", {});
        std::cout << "This line should not be reached!" << std::endl;
    } catch (const FFIException& e) {
        std::cout << "Caught expected exception: " << e.what() << std::endl;
        std::cout << "Error code: " << errorToString(e.error_code())
                  << std::endl;
    }

    // Using expected-based error handling
    DynamicLibrary::Options options;
    options.strategy = DynamicLibrary::LoadStrategy::OnDemand;
    DynamicLibrary mathLib(libraryPath, options);

    // Try to get a non-existent function
    auto nonExistentFunc = mathLib.getFunction<void()>("non_existent_function");
    if (!nonExistentFunc) {
        std::cout << "Error getting non-existent function: "
                  << errorToString(nonExistentFunc.error().error())
                  << std::endl;
    }

    // Checking error types
    if (nonExistentFunc.error().error() == FFIError::SymbolNotFound) {
        std::cout << "Confirmed error type is SymbolNotFound" << std::endl;
    }

    //=========================================================================
    // 6. Library Objects
    //=========================================================================
    printHeader("6. Library Objects (Mock Implementation)");

    // Note: In a real application, you would use an actual shared library with
    // these functions
    std::cout << "This is a demonstration of the API - it would normally work "
                 "with actual shared libraries"
              << std::endl;
    std::cout << "In this example, we're showing the code pattern without "
                 "actually loading the objects"
              << std::endl;

    // Demonstrate the API pattern (this won't actually run correctly without
    // the proper library)
    try {
        // Code pattern for working with library objects
        std::cout << "Code pattern for library objects:" << std::endl;
        std::cout << R"(
         DynamicLibrary myLibrary("path/to/library.so", {});
         auto mockObjectResult = myLibrary.createObject<MockLibraryInterface>("createMockLibrary");

         if (mockObjectResult) {
             MockLibraryInterface& mockObj = *mockObjectResult.value();
             int sum = mockObj.add(10, 20);
             mockObj.performTask("Important Task");
         }
         )" << std::endl;

    } catch (const FFIException& e) {
        std::cerr << "FFI Exception (expected in demo): " << e.what()
                  << std::endl;
    }

    //=========================================================================
    // 7. FFI Type Mapping
    //=========================================================================
    printHeader("7. FFI Type Mapping");

    // Demonstrate FFI type mapping
    std::cout << "FFI type mapping demonstration:" << std::endl;

    std::cout << "Basic types:" << std::endl;
    std::cout << "- int maps to: "
              << (getFFIType<int>() == &ffi_type_sint ? "ffi_type_sint"
                                                      : "other")
              << std::endl;
    std::cout << "- float maps to: "
              << (getFFIType<float>() == &ffi_type_float ? "ffi_type_float"
                                                         : "other")
              << std::endl;
    std::cout << "- double maps to: "
              << (getFFIType<double>() == &ffi_type_double ? "ffi_type_double"
                                                           : "other")
              << std::endl;

    std::cout << "\nPointer types:" << std::endl;
    std::cout << "- char* maps to: "
              << (getFFIType<char*>() == &ffi_type_pointer ? "ffi_type_pointer"
                                                           : "other")
              << std::endl;
    std::cout << "- void* maps to: "
              << (getFFIType<void*>() == &ffi_type_pointer ? "ffi_type_pointer"
                                                           : "other")
              << std::endl;
    std::cout << "- std::string maps to: "
              << (getFFIType<std::string>() == &ffi_type_pointer
                      ? "ffi_type_pointer"
                      : "other")
              << std::endl;

    std::cout << "\nCustom struct type:" << std::endl;
    auto exampleStructType = getFFIType<ExampleStruct>();
    std::cout
        << "- ExampleStruct has FFI type with fields for: int, double, char*"
        << std::endl;
    std::cout << "- Type is struct: "
              << (exampleStructType->type == FFI_TYPE_STRUCT ? "Yes" : "No")
              << std::endl;

    //=========================================================================
    // 8. Resource Management
    //=========================================================================
    printHeader("8. Resource Management");

    // Demonstrate RAII resource management
    {
        FFIResourceGuard guard;

        // 修复: 使用泛型模板或具体类型函数
        int* intResource = new int(42);
        guard.addResource<int>(intResource, [](int* p) {
            std::cout << "Cleaning up int resource: " << *p << std::endl;
            delete p;
        });

        char* charResource = new char[10];
        snprintf(charResource, 10, "Hello");
        guard.addResource<char>(charResource, [](char* p) {
            std::cout << "Cleaning up char resource: " << p << std::endl;
            delete[] p;
        });

        std::cout << "Resources allocated and registered with guard"
                  << std::endl;
        std::cout << "Resources will be automatically cleaned up when guard "
                     "goes out of scope"
                  << std::endl;
    }  // guard is destroyed here, which cleans up resources
    std::cout << "Scope ended, resources should be cleaned up" << std::endl;

    //=========================================================================
    // 9. Library Reloading
    //=========================================================================
    printHeader("9. Library Reloading");

    try {
        DynamicLibrary libraryObj(libraryPath, {});  // 修复变量名
        std::cout << "Initial library loaded" << std::endl;

        // Reload the same library
        auto reloadResult = libraryObj.reload();  // 修复变量名
        printResult(reloadResult, "Reloading the same library");

        // Reload with a different path (this would typically fail in this
        // example)
        std::cout << "Attempting to reload with a different path (may fail)..."
                  << std::endl;
        auto reloadAltResult =
            libraryObj.reload("/usr/lib/libz.so");  // 修复变量名
        printResult(reloadAltResult, "Reloading with different library");

    } catch (const FFIException& e) {
        std::cerr << "FFI Exception: " << e.what() << std::endl;
    }

    //=========================================================================
    // 10. FFI Wrapper
    //=========================================================================
    printHeader("10. FFI Wrapper");

    try {
        // Load the math library
        DynamicLibrary mathLib(libraryPath, {});
        auto cosSymbolResult =
            mathLib.getHandle().and_then([](void* handle) -> FFIResult<void*> {
#ifdef _MSC_VER
                void* symbol =
                    GetProcAddress(static_cast<HMODULE>(handle), "cos");
#else
                void* symbol = dlsym(handle, "cos");
#endif
                if (symbol == nullptr) {
                    return atom::type::unexpected(
                        FFIError::SymbolNotFound);  // 修复命名空间
                }
                return symbol;
            });

        if (cosSymbolResult) {
            void* cosFunc = cosSymbolResult.value();

            // Create an FFI wrapper for cos function
            FFIWrapper<double, double> cosWrapper(true);  // With validation

            // Call through the wrapper
            auto result = cosWrapper.call(cosFunc, 0.0);
            if (result) {
                std::cout << "cos(0.0) = " << result.value() << std::endl;
            } else {
                std::cout << "Error calling function: "
                          << errorToString(result.error().error()) << std::endl;
            }

            // Call with timeout
            auto timeoutResult = cosWrapper.callWithTimeout(
                cosFunc, std::chrono::seconds(1), 3.14159);
            if (timeoutResult) {
                std::cout << "cos(π) = " << timeoutResult.value() << std::endl;
            } else {
                std::cout << "Error with timeout call: "
                          << errorToString(timeoutResult.error().error())
                          << std::endl;
            }
        } else {
            std::cout << "Could not get cos function: "
                      << errorToString(cosSymbolResult.error().error())
                      << std::endl;
        }

    } catch (const FFIException& e) {
        std::cerr << "FFI Exception: " << e.what() << std::endl;
    }

    //=========================================================================
    // Final Summary
    //=========================================================================
    printHeader("FFI Utilities Summary");

    std::cout << "This example demonstrated the following FFI capabilities:"
              << std::endl;
    std::cout << "1. Dynamic library loading with different strategies"
              << std::endl;
    std::cout << "2. Function lookup and calling" << std::endl;
    std::cout << "3. Callback registration and invocation" << std::endl;
    std::cout << "4. Timeout mechanisms for function calls" << std::endl;
    std::cout << "5. Robust error handling with exceptions and expected"
              << std::endl;
    std::cout << "6. Library object creation and management" << std::endl;
    std::cout << "7. FFI type mapping for various C++ types" << std::endl;
    std::cout << "8. RAII-based resource management" << std::endl;
    std::cout << "9. Library reloading capabilities" << std::endl;
    std::cout << "10. Low-level FFI wrapper for direct control" << std::endl;

    return 0;
}
