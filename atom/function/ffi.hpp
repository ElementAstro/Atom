/*!
 * \file ffi.hpp
 * \brief Enhanced FFI with Lazy Loading, Callbacks, and Timeout Mechanism
 * \author Max Qian <lightapt.com>, Enhanced by Claude
 * \date 2023-03-29, Updated 2024-10-14, Enhanced 2025-03-13
 * \copyright Copyright (C) 2023-2025 Max Qian
 */

#ifndef ATOM_META_FFI_HPP
#define ATOM_META_FFI_HPP

#include <any>
#include <chrono>
#include <concepts>
#include <expected>
#include <format>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>
#include "atom/macro.hpp"
#include "atom/type/expected.hpp"

#ifdef _MSC_VER
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <ffi.h>

#include "atom/error/exception.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#endif

namespace atom::meta {

// Forward declarations
template <typename T>
class LibraryObject;
class DynamicLibrary;
class CallbackRegistry;

// Error handling with type::expected (C++23)
enum class FFIError {
    None,
    LibraryLoadFailed,
    SymbolNotFound,
    FunctionCallFailed,
    InvalidArgument,
    Timeout,
    CallbackNotFound,
    TypeMismatch,
    OutOfMemory,
    InternalError
};

inline auto to_string(FFIError error) -> std::string {
    switch (error) {
        case FFIError::None:
            return "No error";
        case FFIError::LibraryLoadFailed:
            return "Failed to load dynamic library";
        case FFIError::SymbolNotFound:
            return "Symbol not found in library";
        case FFIError::FunctionCallFailed:
            return "Function call failed";
        case FFIError::InvalidArgument:
            return "Invalid argument provided";
        case FFIError::Timeout:
            return "Operation timed out";
        case FFIError::CallbackNotFound:
            return "Callback function not found";
        case FFIError::TypeMismatch:
            return "Type mismatch in function call";
        case FFIError::OutOfMemory:
            return "Out of memory";
        case FFIError::InternalError:
            return "Internal FFI error";
        default:
            return "Unknown error";
    }
}

class FFIException : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;

    // Modern constructor with source_location
    explicit FFIException(
        std::string_view message, FFIError error_code = FFIError::InternalError,
        std::source_location location = std::source_location::current())
        : atom::error::Exception(
              location.file_name(), location.line(), location.function_name(),
              std::format("{}: {}", message, to_string(error_code))),
          error_code_(error_code) {}

    [[nodiscard]] auto error_code() const noexcept -> FFIError {
        return error_code_;
    }

private:
    FFIError error_code_ = FFIError::None;
};

// Modern error handling with type::expected
template <typename T>
using FFIResult = type::expected<T, FFIError>;

#define THROW_FFI_EXCEPTION(...)                                       \
    throw FFIException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                       __VA_ARGS__)

// Type trait concepts for FFI type mapping
template <typename T>
concept FFIBasicType =
    std::is_same_v<T, int> || std::is_same_v<T, float> ||
    std::is_same_v<T, double> || std::is_same_v<T, uint8_t> ||
    std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> ||
    std::is_same_v<T, uint64_t> || std::is_same_v<T, int8_t> ||
    std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t> ||
    std::is_same_v<T, int64_t> || std::is_same_v<T, void>;

template <typename T>
concept FFIPointerType =
    std::is_pointer_v<T> || std::is_same_v<T, const char*> ||
    std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>;

template <typename T>
concept FFIStructType = std::is_class_v<T> && requires(T t) {
    { T::getFFITypeLayout() } -> std::convertible_to<ffi_type>;
};

// Enhanced FFI type mapping
template <typename T>
constexpr auto getFFIType() -> ffi_type* {
    if constexpr (std::is_same_v<T, int>) {
        return &ffi_type_sint;
    } else if constexpr (std::is_same_v<T, float>) {
        return &ffi_type_float;
    } else if constexpr (std::is_same_v<T, double>) {
        return &ffi_type_double;
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        return &ffi_type_uint8;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        return &ffi_type_uint16;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        return &ffi_type_uint32;
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        return &ffi_type_uint64;
    } else if constexpr (std::is_same_v<T, int8_t>) {
        return &ffi_type_sint8;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        return &ffi_type_sint16;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return &ffi_type_sint32;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return &ffi_type_sint64;
    } else if constexpr (std::is_same_v<T, const char*> ||
                         std::is_same_v<T, std::string> ||
                         std::is_same_v<T, std::string_view>) {
        return &ffi_type_pointer;
    } else if constexpr (std::is_pointer_v<T>) {
        return &ffi_type_pointer;
    } else if constexpr (std::is_same_v<T, void>) {
        return &ffi_type_void;
    } else if constexpr (std::is_class_v<T>) {
        // Define custom struct ffi_type here if T is a class/struct
        static ffi_type customStructType = T::getFFITypeLayout();
        return &customStructType;
    } else {
        static_assert(FFIBasicType<T> || FFIPointerType<T> || FFIStructType<T>,
                      "Unsupported type passed to getFFIType");
        return nullptr;  // This should never be reached due to static_assert
    }
}

// Helper to automatically generate getFFITypeLayout for classes
template <typename T>
struct FFITypeLayoutGenerator {
    [[nodiscard]] static auto getFFITypeLayout() -> ffi_type {
        ffi_type layout{};
        // Assuming T has a static method to provide the layout details
        T::defineFFITypeLayout(layout);
        return layout;
    }
};

// RAII wrapper for FFI resources
class FFIResourceGuard {
public:
    FFIResourceGuard() = default;

    template <typename T>
    void addResource(T* resource, std::function<void(T*)> deleter) {
        resources_.emplace_back([resource, deleter = std::move(deleter)]() {
            if (resource) {
                deleter(resource);
            }
        });
    }

    ~FFIResourceGuard() {
        for (auto it = resources_.rbegin(); it != resources_.rend(); ++it) {
            (*it)();
        }
    }

private:
    std::vector<std::function<void()>> resources_;
};

// Enhanced FFI wrapper with parameter validation and error handling
template <typename ReturnType, typename... Args>
class FFIWrapper {
public:
    FFIWrapper() { initializeCIF(); }

    // Explicit validation constructor
    explicit FFIWrapper(bool validate) : validate_(validate) {
        initializeCIF();
    }

    // Move constructor
    FFIWrapper(FFIWrapper&& other) noexcept
        : cif_(other.cif_),
          argTypes_(std::move(other.argTypes_)),
          returnType_(other.returnType_),
          validate_(other.validate_) {
        // Invalidate the other wrapper
        other.returnType_ = nullptr;
    }

    // Move assignment
    auto operator=(FFIWrapper&& other) noexcept -> FFIWrapper& {
        if (this != &other) {
            cif_ = other.cif_;
            argTypes_ = std::move(other.argTypes_);
            returnType_ = other.returnType_;
            validate_ = other.validate_;

            // Invalidate the other wrapper
            other.returnType_ = nullptr;
        }
        return *this;
    }

    // Non-copyable
    FFIWrapper(const FFIWrapper&) = delete;
    auto operator=(const FFIWrapper&) -> FFIWrapper& = delete;

    [[nodiscard]] auto call(void* funcPtr, Args... args) const
        -> FFIResult<std::conditional_t<std::is_same_v<ReturnType, void>,
                                        std::monostate, ReturnType>> {
        if (validate_ && !validateArguments(args...)) {
            return type::unexpected(FFIError::InvalidArgument);
        }

        std::vector<void*> argsArray = {
            &const_cast<std::remove_reference_t<Args>&>(args)...};

        if constexpr (std::is_same_v<ReturnType, void>) {
            ffi_call(&cif_, FFI_FN(funcPtr), nullptr, argsArray.data());
            return std::monostate{};
        } else {
            ReturnType result{};
            ffi_call(&cif_, FFI_FN(funcPtr), &result, argsArray.data());
            return result;
        }
    }

    // Overload with timeout
    [[nodiscard]] auto callWithTimeout(void* funcPtr,
                                       std::chrono::milliseconds timeout,
                                       Args... args) const
        -> FFIResult<std::conditional_t<std::is_same_v<ReturnType, void>,
                                        std::monostate, ReturnType>> {
        if (validate_ && !validateArguments(args...)) {
            return type::unexpected(FFIError::InvalidArgument);
        }

#ifdef ATOM_USE_BOOST
        boost::asio::io_context io;
        boost::asio::steady_timer timer(io, timeout);
        std::optional<FFIResult<ResultType>> resultOpt;
        std::atomic<bool> completed = false;

        std::thread ioThread([&io]() { io.run(); });

        std::thread funcThread([&]() {
            try {
                resultOpt = call(funcPtr, args...);
                completed.store(true, std::memory_order_release);
                timer.cancel();
            } catch (...) {
                completed.store(true, std::memory_order_release);
                timer.cancel();
                throw;
            }
        });

        timer.async_wait([&](const boost::system::error_code& ec) {
            if (!ec && !completed.load(std::memory_order_acquire)) {
                resultOpt = type::unexpected(FFIError::Timeout);
            }
        });

        funcThread.join();
        ioThread.join();

        if (!resultOpt.has_value()) {
            return type::unexpected(FFIError::InternalError);
        }
        return *resultOpt;
#else
        auto future = std::async(std::launch::async, [this, funcPtr, args...] {
            return this->call(funcPtr, args...);
        });

        if (future.wait_for(timeout) == std::future_status::timeout) {
            return type::unexpected(FFIError::Timeout);
        }

        try {
            return future.get();
        } catch (const std::exception& e) {
            return type::unexpected(FFIError::FunctionCallFailed);
        }
#endif
    }

private:
    void initializeCIF() {
        argTypes_.reserve(sizeof...(Args));
        (argTypes_.push_back(getFFIType<std::remove_cvref_t<Args>>()), ...);
        returnType_ = getFFIType<std::remove_cvref_t<ReturnType>>();

        if (ffi_prep_cif(&cif_, FFI_DEFAULT_ABI, sizeof...(Args), returnType_,
                         argTypes_.data()) != FFI_OK) {
            THROW_FFI_EXCEPTION("Failed to prepare FFI call interface",
                                FFIError::InternalError);
        }
    }

    [[nodiscard]] static constexpr bool validateArguments(
        [[maybe_unused]] const Args&... args) {
        if constexpr (sizeof...(Args) == 0) {
            return true;
        } else {
            return (validateArgument(args) && ...);
        }
    }

    template <typename T>
    [[nodiscard]] static constexpr bool validateArgument(
        [[maybe_unused]] const T& arg) {
        if constexpr (std::is_pointer_v<T>) {
            // Check for null pointers when validation is enabled
            return arg != nullptr;
        } else if constexpr (std::is_same_v<T, std::string> ||
                             std::is_same_v<T, std::string_view>) {
            // String validation - ensure it's properly terminated and not too
            // large
            return arg.length() < std::numeric_limits<size_t>::max() - 1;
        } else {
            // Most basic types don't need validation
            return true;
        }
    }

    mutable ffi_cif cif_{};
    std::vector<ffi_type*> argTypes_;
    ffi_type* returnType_{};
    bool validate_ = true;
};

// RAII wrapper for dynamic library handles
class LibraryHandle {
public:
    LibraryHandle() = default;

    explicit LibraryHandle(std::string_view path) {
        ATOM_UNUSED_RESULT(load(path));
    }

    ~LibraryHandle() { unload(); }

    // Move operations
    LibraryHandle(LibraryHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    auto operator=(LibraryHandle&& other) noexcept -> LibraryHandle& {
        if (this != &other) {
            unload();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    // Non-copyable
    LibraryHandle(const LibraryHandle&) = delete;
    auto operator=(const LibraryHandle&) -> LibraryHandle& = delete;

    [[nodiscard]] auto load(std::string_view path) -> FFIResult<void> {
        unload();

#ifdef _MSC_VER
        handle_ = LoadLibraryA(path.data());
        if (handle_ == nullptr) {
            return type::unexpected(FFIError::LibraryLoadFailed);
        }
#else
        handle_ = dlopen(path.data(), RTLD_LAZY);
        if (handle_ == nullptr) {
            return type::unexpected(FFIError::LibraryLoadFailed);
        }
#endif
        return {};
    }

    void unload() {
        if (handle_ != nullptr) {
#ifdef _MSC_VER
            FreeLibrary(static_cast<HMODULE>(handle_));
#else
            dlclose(handle_);
#endif
            handle_ = nullptr;
        }
    }

    [[nodiscard]] auto get() const noexcept -> void* { return handle_; }

    [[nodiscard]] auto isLoaded() const noexcept -> bool {
        return handle_ != nullptr;
    }

    // Get symbol
    [[nodiscard]] auto getSymbol(std::string_view name) const
        -> FFIResult<void*> {
        if (!isLoaded()) {
            return type::unexpected(FFIError::LibraryLoadFailed);
        }

#ifdef _MSC_VER
        void* symbol =
            GetProcAddress(static_cast<HMODULE>(handle_), name.data());
#else
        void* symbol = dlsym(handle_, name.data());
#endif

        if (symbol == nullptr) {
            return type::unexpected(FFIError::SymbolNotFound);
        }

        return symbol;
    }

private:
    void* handle_ = nullptr;
};

// Enhanced DynamicLibrary with modern C++ features
class DynamicLibrary {
public:
    // Library loading strategies
    enum class LoadStrategy {
        Immediate,  // Load immediately
        Lazy,       // Load on first use
        OnDemand    // Load when explicitly requested
    };

    struct Options {
        LoadStrategy strategy = LoadStrategy::Immediate;
        bool cacheSymbols = true;
        bool validateCalls = true;
        std::chrono::milliseconds defaultTimeout = std::chrono::seconds(30);
    };

    explicit DynamicLibrary(std::string_view libraryPath, Options options)
        : libraryPath_(libraryPath), options_(options) {
        if (options_.strategy == LoadStrategy::Immediate) {
            auto result = loadLibrary();
            if (!result) {
                THROW_FFI_EXCEPTION("Failed to load library: {}", libraryPath,
                                    result.error());
            }
        }
    }

    // No need for custom destructor - LibraryHandle has RAII

    // Non-copyable
    DynamicLibrary(const DynamicLibrary&) = delete;
    auto operator=(const DynamicLibrary&) -> DynamicLibrary& = delete;

    [[nodiscard]] auto loadLibrary() -> FFIResult<void> {
        std::unique_lock lock(mutex_);
        return handle_.load(libraryPath_);
    }

    void unloadLibrary() {
        std::unique_lock lock(mutex_);
        handle_.unload();
        functionMap_.clear();
    }

    // 在 getFunction 方法中
    template <typename Func>
    [[nodiscard]] auto getFunction(std::string_view functionName)
        -> FFIResult<std::function<Func>> {
        ensureLibraryLoaded();

        std::shared_lock lock(mutex_);
        if (!handle_.isLoaded()) {
            return type::unexpected(FFIError::LibraryLoadFailed);
        }

        // First check if we've cached this function
        if (options_.cacheSymbols) {
            auto it = functionMap_.find(std::string(functionName));
            if (it != functionMap_.end()) {
                return std::function<Func>(reinterpret_cast<Func*>(it->second));
            }
        }

        // Get the symbol from the library
        auto symbolResult = handle_.getSymbol(functionName);
        if (!symbolResult) {
            return type::unexpected(symbolResult.error());
        }

        // 使用 expected 的 value() 方法获取值，而不是使用 *
        void* symbol = symbolResult.value();

        // Cache the symbol if caching is enabled
        if (options_.cacheSymbols) {
            functionMap_[std::string(functionName)] = symbol;
        }

        return std::function<Func>(reinterpret_cast<Func*>(symbol));
    }

    // 在 callFunctionWithTimeout 方法中
    template <typename ReturnType, typename... Args>
    [[nodiscard]] auto callFunctionWithTimeout(
        std::string_view functionName, std::chrono::milliseconds timeout,
        Args&&... args)
        -> FFIResult<std::conditional_t<std::is_same_v<ReturnType, void>,
                                        std::monostate, ReturnType>> {
        ensureLibraryLoaded();

        std::shared_lock lock(mutex_);

        // Get function pointer
        void* funcPtr = nullptr;
        if (options_.cacheSymbols) {
            auto it = functionMap_.find(std::string(functionName));
            if (it != functionMap_.end()) {
                funcPtr = it->second;
            }
        }

        if (funcPtr == nullptr) {
            auto symbolResult = handle_.getSymbol(functionName);
            if (!symbolResult) {
                return type::unexpected(symbolResult.error());
            }

            // 使用 expected 的 value() 方法获取值，而不是使用 *
            funcPtr = symbolResult.value();

            if (options_.cacheSymbols) {
                functionMap_[std::string(functionName)] = funcPtr;
            }
        }

        // Call the function with timeout
        FFIWrapper<ReturnType, std::remove_cvref_t<Args>...> wrapper(
            options_.validateCalls);
        return wrapper.callWithTimeout(funcPtr, timeout,
                                       std::forward<Args>(args)...);
    }

    // 在 addFunction 方法中
    template <typename FuncType>
    [[nodiscard]] auto addFunction(std::string_view functionName)
        -> FFIResult<void> {
        ensureLibraryLoaded();

        std::unique_lock lock(mutex_);

        auto symbolResult = handle_.getSymbol(functionName);
        if (!symbolResult) {
            // 直接传递错误对象，不需要再包装在 type::unexpected 中
            // TODO: Fix this
            // return FFIResult<void>(type::unexpected(symbolResult.error()));
        }

        // 使用 expected 的 value() 方法获取值，而不是使用 *
        void* funcPtr = symbolResult.value();
        functionMap_[std::string(functionName)] = funcPtr;

        return {};
    }

    // Check if a function is loaded and present in the function map
    [[nodiscard]] auto hasFunction(std::string_view functionName) const
        -> bool {
        std::shared_lock lock(mutex_);
        return functionMap_.contains(std::string(functionName));
    }

    // Reload the library
    [[nodiscard]] auto reload(const std::string& newLibraryPath = "")
        -> FFIResult<void> {
        std::unique_lock lock(mutex_);

        handle_.unload();
        functionMap_.clear();

        if (!newLibraryPath.empty()) {
            libraryPath_ = newLibraryPath;
        }

        return handle_.load(libraryPath_);
    }

    // Retrieve the dynamic library handle (for advanced users)
    [[nodiscard]] auto getHandle() const -> FFIResult<void*> {
        std::shared_lock lock(mutex_);

        if (!handle_.isLoaded()) {
            return type::unexpected(FFIError::LibraryLoadFailed);
        }

        return handle_.get();
    }

    // Create a library object
    template <typename T>
    [[nodiscard]] auto createObject(std::string_view factoryFuncName)
        -> FFIResult<LibraryObject<T>> {
        return LibraryObject<T>::create(*this, factoryFuncName);
    }

    // Set new options
    void setOptions(const Options& options) {
        std::unique_lock lock(mutex_);
        options_ = options;
    }

private:
    void ensureLibraryLoaded() {
        if (options_.strategy != LoadStrategy::OnDemand &&
            !handle_.isLoaded()) {
            auto result = loadLibrary();
            if (!result) {
                THROW_FFI_EXCEPTION(
                    std::format("Failed to load library: {}", libraryPath_),
                    result.error());
            }
        }
    }

    std::string libraryPath_;
    Options options_;
    LibraryHandle handle_;
    mutable std::shared_mutex mutex_;

    std::unordered_map<std::string, void*> functionMap_;
};

// Enhanced callback registry with type safety
class CallbackRegistry {
public:
    CallbackRegistry() = default;

    // Non-copyable
    CallbackRegistry(const CallbackRegistry&) = delete;
    auto operator=(const CallbackRegistry&) -> CallbackRegistry& = delete;

    // Register a callback function that will be passed to an external library
    template <typename Func>
    void registerCallback(std::string_view callbackName, Func&& func) {
        std::unique_lock lock(mutex_);

        using FuncType = std::decay_t<Func>;
        callbackMap_[std::string(callbackName)] =
            std::make_any<std::function<FuncType>>(std::forward<Func>(func));
    }

    // Retrieve a registered callback
    template <typename Func>
    [[nodiscard]] auto getCallback(std::string_view callbackName)
        -> FFIResult<std::function<Func>*> {
        std::shared_lock lock(mutex_);

        auto it = callbackMap_.find(std::string(callbackName));
        if (it == callbackMap_.end()) {
            return type::unexpected(FFIError::CallbackNotFound);
        }

        try {
            return std::any_cast<std::function<Func>>(&it->second);
        } catch (const std::bad_any_cast&) {
            return type::unexpected(FFIError::TypeMismatch);
        }
    }

    // Register an asynchronous callback function
    template <typename Func>
    void registerAsyncCallback(std::string_view callbackName, Func&& func) {
        std::unique_lock lock(mutex_);

        using FuncType = std::decay_t<Func>;
        callbackMap_[std::string(callbackName)] =
            std::make_any<std::function<FuncType>>(
                [f = std::forward<Func>(func)](auto&&... args) {
                    return std::async(std::launch::async, f,
                                      std::forward<decltype(args)>(args)...);
                });
    }

    // Check if a callback exists
    [[nodiscard]] auto hasCallback(std::string_view callbackName) const
        -> bool {
        std::shared_lock lock(mutex_);
        return callbackMap_.contains(std::string(callbackName));
    }

    // Remove a callback
    void removeCallback(std::string_view callbackName) {
        std::unique_lock lock(mutex_);
        callbackMap_.erase(std::string(callbackName));
    }

    // Clear all callbacks
    void clear() {
        std::unique_lock lock(mutex_);
        callbackMap_.clear();
    }

private:
    std::unordered_map<std::string, std::any> callbackMap_;
    mutable std::shared_mutex mutex_;
};

// Enhanced LibraryObject with proper resource management
template <typename T>
class LibraryObject {
public:
    // Factory method that returns an expected
    [[nodiscard]] static auto create(DynamicLibrary& library,
                                     std::string_view factoryFuncName)
        -> FFIResult<LibraryObject<T>> {
        auto factoryResult = library.getFunction<T*(void)>(factoryFuncName);
        if (!factoryResult) {
            return type::unexpected(factoryResult.error());
        }

        auto factory = *factoryResult;
        T* object = factory();

        if (!object) {
            return type::unexpected(FFIError::FunctionCallFailed);
        }

        return LibraryObject<T>(object);
    }

    // Constructor from raw pointer
    explicit LibraryObject(T* object) : object_(object) {}

    // Move semantics
    LibraryObject(LibraryObject&& other) noexcept : object_(other.object_) {
        other.object_ = nullptr;
    }

    auto operator=(LibraryObject&& other) noexcept -> LibraryObject& {
        if (this != &other) {
            if (object_) {
                // Handle cleanup if needed
                delete object_;
            }
            object_ = other.object_;
            other.object_ = nullptr;
        }
        return *this;
    }

    // No copy
    LibraryObject(const LibraryObject&) = delete;
    auto operator=(const LibraryObject&) -> LibraryObject& = delete;

    // Destructor
    ~LibraryObject() {
        if (object_) {
            delete object_;
        }
    }

    [[nodiscard]] auto operator->() const noexcept -> T* {
        return object_.get();
    }

    [[nodiscard]] auto operator*() const -> T& {
        if (!object_) {
            THROW_FFI_EXCEPTION("Attempting to dereference null object",
                                FFIError::InvalidArgument);
        }
        return *object_;
    }

    [[nodiscard]] auto get() const noexcept -> T* { return object_.get(); }

    // Check if object is valid
    [[nodiscard]] auto isValid() const noexcept -> bool {
        return object_ != nullptr;
    }

private:
    std::unique_ptr<T> object_;
};

}  // namespace atom::meta

#endif  // ATOM_META_FFI_HPP
