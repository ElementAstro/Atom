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

#ifdef _MSC_VER
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <ffi.h>

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"
#include "atom/type/expected.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#endif

namespace atom::meta {

template <typename T>
class LibraryObject;
class DynamicLibrary;
class CallbackRegistry;

/**
 * \brief Enumeration of FFI error codes
 */
enum class FFIError {
    None,                ///< No error occurred
    LibraryLoadFailed,   ///< Failed to load dynamic library
    SymbolNotFound,      ///< Symbol not found in library
    FunctionCallFailed,  ///< Function call failed
    InvalidArgument,     ///< Invalid argument provided
    Timeout,             ///< Operation timed out
    CallbackNotFound,    ///< Callback function not found
    TypeMismatch,        ///< Type mismatch in function call
    OutOfMemory,         ///< Out of memory
    InternalError        ///< Internal FFI error
};

/**
 * \brief Convert FFI error code to string representation
 * \param error The error code to convert
 * \return String representation of the error
 */
inline auto to_string(FFIError error) -> std::string {
    static constexpr std::array<std::string_view, 10> error_strings = {
        "No error",
        "Failed to load dynamic library",
        "Symbol not found in library",
        "Function call failed",
        "Invalid argument provided",
        "Operation timed out",
        "Callback function not found",
        "Type mismatch in function call",
        "Out of memory",
        "Internal FFI error"};

    const auto index = static_cast<size_t>(error);
    return index < error_strings.size() ? std::string(error_strings[index])
                                        : "Unknown error";
}

/**
 * \brief FFI-specific exception class with enhanced error information
 */
class FFIException : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;

    /**
     * \brief Construct FFI exception with source location
     * \param message Error message
     * \param error_code Specific FFI error code
     * \param location Source location where error occurred
     */
    explicit FFIException(
        std::string_view message, FFIError error_code = FFIError::InternalError,
        std::source_location location = std::source_location::current())
        : atom::error::Exception(
              location.file_name(), location.line(), location.function_name(),
              std::format("{}: {}", message, to_string(error_code))),
          error_code_(error_code) {}

    /**
     * \brief Get the specific FFI error code
     * \return The FFI error code
     */
    [[nodiscard]] auto error_code() const noexcept -> FFIError {
        return error_code_;
    }

private:
    FFIError error_code_ = FFIError::None;
};

/**
 * \brief Type alias for FFI result using expected
 * \tparam T The expected result type
 */
template <typename T>
using FFIResult = type::expected<T, FFIError>;

#define THROW_FFI_EXCEPTION(...)                                       \
    throw FFIException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                       __VA_ARGS__)

/**
 * \brief Concept for basic FFI-compatible types
 * \tparam T Type to check
 */
template <typename T>
concept FFIBasicType =
    std::is_same_v<T, int> || std::is_same_v<T, float> ||
    std::is_same_v<T, double> || std::is_same_v<T, uint8_t> ||
    std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> ||
    std::is_same_v<T, uint64_t> || std::is_same_v<T, int8_t> ||
    std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t> ||
    std::is_same_v<T, int64_t> || std::is_same_v<T, void>;

/**
 * \brief Concept for pointer types in FFI
 * \tparam T Type to check
 */
template <typename T>
concept FFIPointerType =
    std::is_pointer_v<T> || std::is_same_v<T, const char*> ||
    std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>;

/**
 * \brief Concept for struct types that can be used in FFI
 * \tparam T Type to check
 */
template <typename T>
concept FFIStructType = std::is_class_v<T> && requires(T t) {
    { T::getFFITypeLayout() } -> std::convertible_to<ffi_type>;
};

/**
 * \brief Get FFI type for template parameter
 * \tparam T The C++ type to map to FFI type
 * \return Pointer to corresponding ffi_type
 */
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
        static ffi_type customStructType = T::getFFITypeLayout();
        return &customStructType;
    } else {
        static_assert(FFIBasicType<T> || FFIPointerType<T> || FFIStructType<T>,
                      "Unsupported type passed to getFFIType");
        return nullptr;
    }
}

/**
 * \brief Helper to automatically generate getFFITypeLayout for classes
 * \tparam T The class type
 */
template <typename T>
struct FFITypeLayoutGenerator {
    /**
     * \brief Get FFI type layout for class T
     * \return FFI type layout structure
     */
    [[nodiscard]] static auto getFFITypeLayout() -> ffi_type {
        ffi_type layout{};
        T::defineFFITypeLayout(layout);
        return layout;
    }
};

/**
 * \brief RAII wrapper for FFI resources with automatic cleanup
 */
class FFIResourceGuard {
public:
    FFIResourceGuard() = default;

    /**
     * \brief Add a resource with custom deleter
     * \tparam T Resource type
     * \param resource Pointer to resource
     * \param deleter Custom deleter function
     */
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

/**
 * \brief Enhanced FFI wrapper with parameter validation and error handling
 * \tparam ReturnType Function return type
 * \tparam Args Function argument types
 */
template <typename ReturnType, typename... Args>
class FFIWrapper {
private:
    using ResultType = std::conditional_t<std::is_same_v<ReturnType, void>,
                                          std::monostate, ReturnType>;

public:
    /**
     * \brief Default constructor with validation enabled
     */
    FFIWrapper() { initializeCIF(); }

    /**
     * \brief Constructor with explicit validation setting
     * \param validate Enable or disable argument validation
     */
    explicit FFIWrapper(bool validate) : validate_(validate) {
        initializeCIF();
    }

    FFIWrapper(FFIWrapper&& other) noexcept
        : cif_(other.cif_),
          argTypes_(std::move(other.argTypes_)),
          returnType_(other.returnType_),
          validate_(other.validate_) {
        other.returnType_ = nullptr;
    }

    auto operator=(FFIWrapper&& other) noexcept -> FFIWrapper& {
        if (this != &other) {
            cif_ = other.cif_;
            argTypes_ = std::move(other.argTypes_);
            returnType_ = other.returnType_;
            validate_ = other.validate_;
            other.returnType_ = nullptr;
        }
        return *this;
    }

    FFIWrapper(const FFIWrapper&) = delete;
    auto operator=(const FFIWrapper&) -> FFIWrapper& = delete;

    /**
     * \brief Call function with given arguments
     * \param funcPtr Pointer to the function to call
     * \param args Function arguments
     * \return Result or error
     */
    [[nodiscard]] auto call(void* funcPtr, Args... args) const
        -> FFIResult<ResultType> {
        if (validate_ && !validateArguments(args...)) {
            return type::unexpected(FFIError::InvalidArgument);
        }

        std::array<void*, sizeof...(Args)> argsArray = {
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

    /**
     * \brief Call function with timeout
     * \param funcPtr Pointer to the function to call
     * \param timeout Maximum time to wait for function completion
     * \param args Function arguments
     * \return Result or error (including timeout)
     */
    [[nodiscard]] auto callWithTimeout(void* funcPtr,
                                       std::chrono::milliseconds timeout,
                                       Args... args) const
        -> FFIResult<ResultType> {
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
        } catch (const std::exception&) {
            return type::unexpected(FFIError::FunctionCallFailed);
        }
#endif
    }

private:
    void initializeCIF() {
        if constexpr (sizeof...(Args) > 0) {
            argTypes_.reserve(sizeof...(Args));
            (argTypes_.push_back(getFFIType<std::remove_cvref_t<Args>>()), ...);
        }
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
            return arg != nullptr;
        } else if constexpr (std::is_same_v<T, std::string> ||
                             std::is_same_v<T, std::string_view>) {
            return arg.length() < std::numeric_limits<size_t>::max() - 1;
        } else {
            return true;
        }
    }

    mutable ffi_cif cif_{};
    std::vector<ffi_type*> argTypes_;
    ffi_type* returnType_{};
    bool validate_ = true;
};

/**
 * \brief RAII wrapper for dynamic library handles with automatic cleanup
 */
class LibraryHandle {
public:
    LibraryHandle() = default;

    /**
     * \brief Constructor that loads library immediately
     * \param path Path to the library file
     */
    explicit LibraryHandle(std::string_view path) {
        ATOM_UNUSED_RESULT(load(path));
    }

    ~LibraryHandle() { unload(); }

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

    LibraryHandle(const LibraryHandle&) = delete;
    auto operator=(const LibraryHandle&) -> LibraryHandle& = delete;

    /**
     * \brief Load dynamic library from path
     * \param path Path to the library file
     * \return Success or error
     */
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

    /**
     * \brief Unload the library if loaded
     */
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

    /**
     * \brief Get raw library handle
     * \return Raw handle pointer
     */
    [[nodiscard]] auto get() const noexcept -> void* { return handle_; }

    /**
     * \brief Check if library is loaded
     * \return True if library is loaded
     */
    [[nodiscard]] auto isLoaded() const noexcept -> bool {
        return handle_ != nullptr;
    }

    /**
     * \brief Get symbol from loaded library
     * \param name Symbol name to lookup
     * \return Symbol pointer or error
     */
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

/**
 * \brief Enhanced DynamicLibrary with modern C++ features and performance
 * optimizations
 */
class DynamicLibrary {
public:
    /**
     * \brief Library loading strategies
     */
    enum class LoadStrategy {
        Immediate,  ///< Load immediately when DynamicLibrary is created
        Lazy,       ///< Load on first function access
        OnDemand    ///< Load only when explicitly requested
    };

    /**
     * \brief Configuration options for DynamicLibrary
     */
    struct Options {
        LoadStrategy strategy = LoadStrategy::Immediate;
        bool cacheSymbols = true;
        bool validateCalls = true;
        std::chrono::milliseconds defaultTimeout = std::chrono::seconds(30);
    };

    /**
     * \brief Constructor with library path and options
     * \param libraryPath Path to the dynamic library
     * \param options Configuration options
     */
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

    DynamicLibrary(const DynamicLibrary&) = delete;
    auto operator=(const DynamicLibrary&) -> DynamicLibrary& = delete;

    /**
     * \brief Load the library if not already loaded
     * \return Success or error
     */
    [[nodiscard]] auto loadLibrary() -> FFIResult<void> {
        std::unique_lock lock(mutex_);
        return handle_.load(libraryPath_);
    }

    /**
     * \brief Unload the library and clear function cache
     */
    void unloadLibrary() {
        std::unique_lock lock(mutex_);
        handle_.unload();
        functionMap_.clear();
    }

    /**
     * \brief Get function from library with type safety
     * \tparam Func Function signature type
     * \param functionName Name of the function to retrieve
     * \return Function wrapper or error
     */
    template <typename Func>
    [[nodiscard]] auto getFunction(std::string_view functionName)
        -> FFIResult<std::function<Func>> {
        ensureLibraryLoaded();

        std::shared_lock lock(mutex_);
        if (!handle_.isLoaded()) {
            return type::unexpected(FFIError::LibraryLoadFailed);
        }

        if (options_.cacheSymbols) {
            if (auto it = functionMap_.find(std::string(functionName));
                it != functionMap_.end()) {
                return std::function<Func>(reinterpret_cast<Func*>(it->second));
            }
        }

        auto symbolResult = handle_.getSymbol(functionName);
        if (!symbolResult) {
            return type::unexpected(symbolResult.error());
        }

        void* symbol = symbolResult.value();

        if (options_.cacheSymbols) {
            functionMap_.emplace(std::string(functionName), symbol);
        }

        return std::function<Func>(reinterpret_cast<Func*>(symbol));
    }

    /**
     * \brief Call function with timeout support
     * \tparam ReturnType Function return type
     * \tparam Args Function argument types
     * \param functionName Name of the function to call
     * \param timeout Maximum time to wait for completion
     * \param args Function arguments
     * \return Function result or error
     */
    template <typename ReturnType, typename... Args>
    [[nodiscard]] auto callFunctionWithTimeout(
        std::string_view functionName, std::chrono::milliseconds timeout,
        Args&&... args)
        -> FFIResult<std::conditional_t<std::is_same_v<ReturnType, void>,
                                        std::monostate, ReturnType>> {
        ensureLibraryLoaded();

        std::shared_lock lock(mutex_);

        void* funcPtr = nullptr;
        if (options_.cacheSymbols) {
            if (auto it = functionMap_.find(std::string(functionName));
                it != functionMap_.end()) {
                funcPtr = it->second;
            }
        }

        if (funcPtr == nullptr) {
            auto symbolResult = handle_.getSymbol(functionName);
            if (!symbolResult) {
                return type::unexpected(symbolResult.error());
            }

            funcPtr = symbolResult.value();

            if (options_.cacheSymbols) {
                functionMap_.emplace(std::string(functionName), funcPtr);
            }
        }

        FFIWrapper<ReturnType, std::remove_cvref_t<Args>...> wrapper(
            options_.validateCalls);
        return wrapper.callWithTimeout(funcPtr, timeout,
                                       std::forward<Args>(args)...);
    }

    /**
     * \brief Pre-load function into cache
     * \tparam FuncType Function type signature
     * \param functionName Name of the function to cache
     * \return Success or error
     */
    template <typename FuncType>
    [[nodiscard]] auto addFunction(std::string_view functionName)
        -> FFIResult<void> {
        ensureLibraryLoaded();

        std::unique_lock lock(mutex_);

        auto symbolResult = handle_.getSymbol(functionName);
        if (!symbolResult) {
            return type::unexpected(symbolResult.error().error());
        }

        void* funcPtr = symbolResult.value();
        functionMap_.emplace(std::string(functionName), funcPtr);

        return {};
    }

    /**
     * \brief Check if function is cached
     * \param functionName Function name to check
     * \return True if function is in cache
     */
    [[nodiscard]] auto hasFunction(std::string_view functionName) const
        -> bool {
        std::shared_lock lock(mutex_);
        return functionMap_.contains(std::string(functionName));
    }

    /**
     * \brief Reload library with optional new path
     * \param newLibraryPath Optional new library path
     * \return Success or error
     */
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

    /**
     * \brief Get raw library handle for advanced usage
     * \return Library handle or error
     */
    [[nodiscard]] auto getHandle() const -> FFIResult<void*> {
        std::shared_lock lock(mutex_);

        if (!handle_.isLoaded()) {
            return type::unexpected(FFIError::LibraryLoadFailed);
        }

        return handle_.get();
    }

    /**
     * \brief Create managed library object
     * \tparam T Object type to create
     * \param factoryFuncName Name of factory function
     * \return Library object or error
     */
    template <typename T>
    [[nodiscard]] auto createObject(std::string_view factoryFuncName)
        -> FFIResult<LibraryObject<T>> {
        return LibraryObject<T>::create(*this, factoryFuncName);
    }

    /**
     * \brief Update library options
     * \param options New options to apply
     */
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

/**
 * \brief Enhanced callback registry with type safety and performance
 * optimizations
 */
class CallbackRegistry {
public:
    CallbackRegistry() = default;

    CallbackRegistry(const CallbackRegistry&) = delete;
    auto operator=(const CallbackRegistry&) -> CallbackRegistry& = delete;

    /**
     * \brief Register a callback function for external library use
     * \tparam Func Function type
     * \param callbackName Name to register callback under
     * \param func Function to register
     */
    template <typename Func>
    void registerCallback(std::string_view callbackName, Func&& func) {
        std::unique_lock lock(mutex_);

        using FuncType = std::decay_t<Func>;
        callbackMap_.emplace(
            std::string(callbackName),
            std::make_any<std::function<FuncType>>(std::forward<Func>(func)));
    }

    /**
     * \brief Retrieve registered callback
     * \tparam Func Expected function type
     * \param callbackName Name of callback to retrieve
     * \return Pointer to callback function or error
     */
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

    /**
     * \brief Register asynchronous callback function
     * \tparam Func Function type
     * \param callbackName Name to register callback under
     * \param func Function to register as async
     */
    template <typename Func>
    void registerAsyncCallback(std::string_view callbackName, Func&& func) {
        std::unique_lock lock(mutex_);

        using FuncType = std::decay_t<Func>;
        callbackMap_.emplace(
            std::string(callbackName),
            std::make_any<std::function<FuncType>>(
                [f = std::forward<Func>(func)](auto&&... args) {
                    return std::async(std::launch::async, f,
                                      std::forward<decltype(args)>(args)...);
                }));
    }

    /**
     * \brief Check if callback exists
     * \param callbackName Name of callback to check
     * \return True if callback exists
     */
    [[nodiscard]] auto hasCallback(std::string_view callbackName) const
        -> bool {
        std::shared_lock lock(mutex_);
        return callbackMap_.contains(std::string(callbackName));
    }

    /**
     * \brief Remove registered callback
     * \param callbackName Name of callback to remove
     */
    void removeCallback(std::string_view callbackName) {
        std::unique_lock lock(mutex_);
        callbackMap_.erase(std::string(callbackName));
    }

    /**
     * \brief Clear all registered callbacks
     */
    void clear() {
        std::unique_lock lock(mutex_);
        callbackMap_.clear();
    }

private:
    std::unordered_map<std::string, std::any> callbackMap_;
    mutable std::shared_mutex mutex_;
};

/**
 * \brief Enhanced LibraryObject with proper resource management and RAII
 * \tparam T Object type managed by this wrapper
 */
template <typename T>
class LibraryObject {
public:
    /**
     * \brief Factory method to create library object
     * \param library Reference to dynamic library
     * \param factoryFuncName Name of factory function
     * \return LibraryObject instance or error
     */
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

    /**
     * \brief Constructor from raw pointer
     * \param object Pointer to object to manage
     */
    explicit LibraryObject(T* object) : object_(object) {}

    LibraryObject(LibraryObject&& other) noexcept
        : object_(std::move(other.object_)) {}

    auto operator=(LibraryObject&& other) noexcept -> LibraryObject& {
        if (this != &other) {
            object_ = std::move(other.object_);
        }
        return *this;
    }

    LibraryObject(const LibraryObject&) = delete;
    auto operator=(const LibraryObject&) -> LibraryObject& = delete;

    /**
     * \brief Access object member
     * \return Pointer to managed object
     */
    [[nodiscard]] auto operator->() const noexcept -> T* {
        return object_.get();
    }

    /**
     * \brief Dereference object
     * \return Reference to managed object
     * \throws FFIException if object is null
     */
    [[nodiscard]] auto operator*() const -> T& {
        if (!object_) {
            THROW_FFI_EXCEPTION("Attempting to dereference null object",
                                FFIError::InvalidArgument);
        }
        return *object_;
    }

    /**
     * \brief Get raw pointer to managed object
     * \return Raw pointer to object
     */
    [[nodiscard]] auto get() const noexcept -> T* { return object_.get(); }

    /**
     * \brief Check if object is valid (non-null)
     * \return True if object is valid
     */
    [[nodiscard]] auto isValid() const noexcept -> bool {
        return object_ != nullptr;
    }

private:
    std::unique_ptr<T> object_;
};

}  // namespace atom::meta

#endif  // ATOM_META_FFI_HPP
