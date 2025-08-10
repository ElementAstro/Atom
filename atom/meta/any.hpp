/*!
 * \file any.hpp
 * \brief Enhanced BoxedValue using C++20 features - OPTIMIZED VERSION
 * \author Max Qian <lightapt.com>
 * \date 2023-12-28
 * \updated 2025-01-22 - Performance optimizations by AI Assistant
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * OPTIMIZATIONS APPLIED:
 * - Reduced memory alignment from 128 to 64 bytes for better cache usage
 * - Packed boolean flags into single byte structure
 * - Converted time storage to compact uint64_t microseconds format
 * - Added atomic access count for lock-free performance monitoring
 * - Added helper methods for time conversion and access tracking
 * - Optimized copy/move operations and reduced unnecessary allocations
 */

#ifndef ATOM_META_ANY_HPP
#define ATOM_META_ANY_HPP

#include <any>
#include <atomic>
#include <chrono>
#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <format>

#include "type_info.hpp"

namespace atom::meta {

/*!
 * \brief Serialization format enumeration
 */
enum class SerializationFormat {
    JSON,
    BINARY,
    XML,
    YAML
};

/*!
 * \brief Serialization result structure
 */
struct SerializationResult {
    bool success = false;
    std::string data;
    std::string error_message;

    explicit operator bool() const noexcept { return success; }
};

/*!
 * \brief Performance statistics for BoxedValue
 */
struct PerformanceStats {
    uint32_t access_count = 0;
    uint32_t copy_count = 0;
    uint32_t move_count = 0;
    uint64_t creation_time_micros = 0;
    uint64_t last_access_time_micros = 0;
    uint64_t total_access_time_micros = 0;

    [[nodiscard]] auto averageAccessTime() const noexcept -> double {
        return access_count > 0 ? static_cast<double>(total_access_time_micros) / access_count : 0.0;
    }
};

/*!
 * \brief Attribute metadata for enhanced attribute system
 */
struct AttributeMetadata {
    std::string description;
    std::string category;
    bool is_readonly = false;
    bool is_system = false;  // System attributes cannot be removed by user
    uint64_t creation_time = 0;
    uint64_t modification_time = 0;
};

/*!
 * \class BoxedValue
 * \brief A class that encapsulates a value of any type with additional
 * metadata. Enhanced with serialization, debugging, and performance features.
 */
class BoxedValue {
public:
    /*!
     * \struct VoidType
     * \brief A placeholder type representing void.
     */
    struct VoidType {};

private:
    /*!
     * \struct Data
     * \brief Internal data structure to hold the value and its metadata.
     * Optimized for better memory layout and cache performance.
     */
    struct alignas(64) Data {  // Reduced from 128 to 64 bytes for better cache usage
        std::any obj;
        TypeInfo typeInfo;

        // Simplified attribute storage - keep existing interface but optimize later
        std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<Data>>> attrs;

        // Pack boolean flags into a single byte for better memory efficiency
        struct Flags {
            bool isRef : 1;
            bool returnValue : 1;
            bool readonly : 1;
            bool isConst : 1;
            uint8_t reserved : 4;    // Reserved for future use
        } flags = {};

        const void* constDataPtr = nullptr;

        // Use more compact time representation
        uint64_t creationTime;      // Microseconds since epoch
        uint64_t modificationTime;  // Microseconds since epoch
        mutable std::atomic<uint32_t> accessCount{0};  // Atomic for lock-free access

        /*!
         * \brief Constructor for non-void types.
         * \tparam T The type of the value.
         * \param object The value to be encapsulated.
         * \param is_ref Indicates if the value is a reference.
         * \param return_value Indicates if the value is a return value.
         * \param is_readonly Indicates if the value is read-only.
         */
        template <typename T>
            requires(!std::is_same_v<std::decay_t<T>, VoidType>)
        Data(T&& object, bool is_ref, bool return_value, bool is_readonly)
            : obj(std::forward<T>(object)),
              typeInfo(userType<std::decay_t<T>>()),
              attrs{},
              constDataPtr(std::is_const_v<std::remove_reference_t<T>>
                               ? &object
                               : nullptr),
              creationTime(getCurrentTimeMicros()),
              modificationTime(getCurrentTimeMicros()) {
            flags.isRef = is_ref;
            flags.returnValue = return_value;
            flags.readonly = is_readonly;
            flags.isConst = std::is_const_v<std::remove_reference_t<T>>;
        }

        /*!
         * \brief Constructor for void type.
         * \tparam T The type of the value.
         * \param object The value to be encapsulated.
         * \param is_ref Indicates if the value is a reference.
         * \param return_value Indicates if the value is a return value.
         * \param is_readonly Indicates if the value is read-only.
         */
        template <typename T>
            requires(std::is_same_v<std::decay_t<T>, VoidType>)
        Data([[maybe_unused]] T&& object, bool is_ref, bool return_value,
             bool is_readonly)
            : typeInfo(userType<std::decay_t<T>>()),
              attrs{},
              creationTime(getCurrentTimeMicros()),
              modificationTime(getCurrentTimeMicros()) {
            flags.isRef = is_ref;
            flags.returnValue = return_value;
            flags.readonly = is_readonly;
            flags.isConst = false;
        }

        /*!
         * \brief Copy constructor for Data struct
         * \param other The other Data to copy from
         */
        Data(const Data& other)
            : obj(other.obj),
              typeInfo(other.typeInfo),
              attrs(other.attrs),
              flags(other.flags),
              constDataPtr(other.constDataPtr),
              creationTime(getCurrentTimeMicros()),
              modificationTime(getCurrentTimeMicros()),
              accessCount(other.accessCount.load()) {
        }

        /*!
         * \brief Copy assignment operator for Data struct
         * \param other The other Data to copy from
         * \return Reference to this Data
         */
        Data& operator=(const Data& other) {
            if (this != &other) {
                obj = other.obj;
                typeInfo = other.typeInfo;
                attrs = other.attrs;
                flags = other.flags;
                constDataPtr = other.constDataPtr;
                creationTime = getCurrentTimeMicros();
                modificationTime = getCurrentTimeMicros();
                accessCount.store(other.accessCount.load());
            }
            return *this;
        }
    };

    std::shared_ptr<Data> data_;
    mutable std::shared_mutex mutex_;

private:
    /*!
     * \brief Helper method to get current time in microseconds
     * \return Current time as microseconds since epoch
     */
    static auto getCurrentTimeMicros() noexcept -> uint64_t {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /*!
     * \brief Increment access count atomically (lock-free)
     */
    void incrementAccessCount() const noexcept {
        data_->accessCount.fetch_add(1, std::memory_order_relaxed);
    }

    /*!
     * \brief Get current access count (lock-free)
     * \return Current access count
     */
    [[nodiscard]] auto getAccessCount() const noexcept -> uint32_t {
        return data_->accessCount.load(std::memory_order_relaxed);
    }

    /*!
     * \brief Convert microseconds since epoch to time_point
     * \param micros Microseconds since epoch
     * \return time_point representation
     */
    static auto microsToTimePoint(uint64_t micros) noexcept
        -> std::chrono::system_clock::time_point {
        return std::chrono::system_clock::time_point(
            std::chrono::microseconds(micros));
    }

public:
    /*!
     * \brief Constructor for any type.
     * \tparam T The type of the value.
     * \param value The value to be encapsulated.
     * \param return_value Indicates if the value is a return value.
     * \param is_readonly Indicates if the value is read-only.
     */
    // clang-tidy: disable=hicpp-explicit-constructor
    template <typename T>
        requires(!std::same_as<BoxedValue, std::decay_t<T>>)
    BoxedValue(T&& value, bool return_value = false, bool is_readonly = false)
        : data_(std::make_shared<Data>(
              std::forward<T>(value),
              std::is_reference_v<T> ||
                  std::is_same_v<
                      std::decay_t<T>,
                      std::reference_wrapper<std::remove_reference_t<T>>>,
              return_value, is_readonly)) {
        if constexpr (std::is_same_v<
                          std::decay_t<T>,
                          std::reference_wrapper<std::remove_reference_t<T>>>) {
            data_->flags.isRef = true;
        }
    }

    /*!
     * \brief Default constructor for VoidType.
     */
    BoxedValue()
        : data_(std::make_shared<Data>(VoidType{}, false, false, false)) {}

    /*!
     * \brief Constructor with shared data pointer.
     * \param data Shared pointer to the internal data.
     */
    explicit BoxedValue(std::shared_ptr<Data> data) : data_(std::move(data)) {}

    /*!
     * \brief Copy constructor.
     * \param other The other BoxedValue to copy from.
     */
    BoxedValue(const BoxedValue& other) {
        std::shared_lock lock(other.mutex_);
        if (other.data_) {
            data_ = std::make_shared<Data>(*other.data_);
        }
    }

    /*!
     * \brief Move constructor.
     * \param other The other BoxedValue to move from.
     */
    BoxedValue(BoxedValue&& other) noexcept {
        std::unique_lock lock(other.mutex_);
        data_ = std::move(other.data_);
    }

    /*!
     * \brief Copy assignment operator.
     * \param other The other BoxedValue to copy from.
     * \return Reference to this BoxedValue.
     */
    auto operator=(const BoxedValue& other) -> BoxedValue& {
        if (this != &other) {
            std::scoped_lock lock(mutex_, other.mutex_);
            if (other.data_) {
                data_ = std::make_shared<Data>(*other.data_);
            }
        }
        return *this;
    }

    /*!
     * \brief Move assignment operator.
     * \param other The other BoxedValue to move from.
     * \return Reference to this BoxedValue.
     */
    auto operator=(BoxedValue&& other) noexcept -> BoxedValue& {
        if (this != &other) {
            std::scoped_lock lock(mutex_, other.mutex_);
            data_ = std::move(other.data_);
        }
        return *this;
    }

    /*!
     * \brief Assignment operator for any type.
     * \tparam T The type of the value.
     * \param value The value to be assigned.
     * \return Reference to this BoxedValue.
     */
    template <typename T>
        requires(!std::same_as<BoxedValue, std::decay_t<T>>)
    auto operator=(T&& value) -> BoxedValue& {
        std::unique_lock lock(mutex_);
        data_->obj = std::forward<T>(value);
        data_->typeInfo = userType<std::decay_t<T>>();
        data_->modificationTime = getCurrentTimeMicros();
        return *this;
    }

    /*!
     * \brief Assignment operator for constant values.
     * \tparam T The type of the value.
     * \param value The constant value to be assigned.
     * \return Reference to this BoxedValue.
     */
    template <typename T>
    auto operator=(const T& value) -> BoxedValue& {
        std::unique_lock lock(mutex_);
        data_->obj = value;
        data_->typeInfo = userType<T>();
        data_->flags.readonly = true;
        data_->modificationTime = getCurrentTimeMicros();
        return *this;
    }

    /*!
     * \brief Constructor for constant values.
     * \tparam T The type of the value.
     * \param value The constant value to be encapsulated.
     */
    template <typename T>
    BoxedValue(const T& value)
        : data_(std::make_shared<Data>(value, false, false, true)) {}

    /*!
     * \brief Swap function.
     * \param rhs The other BoxedValue to swap with.
     */
    void swap(BoxedValue& rhs) noexcept {
        if (this != &rhs) {
            std::scoped_lock lock(mutex_, rhs.mutex_);
            std::swap(data_, rhs.data_);
        }
    }

    /*!
     * \brief Destructor.
     */
    ~BoxedValue() = default;

    template <typename T>
    auto isType() const -> bool {
        std::shared_lock lock(mutex_);
        return data_->typeInfo == userType<T>();
    }

    /*!
     * \brief Check if the value is undefined.
     * \return True if the value is undefined, false otherwise.
     */
    [[nodiscard]] auto isUndef() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return !data_ || data_->obj.type() == typeid(VoidType) ||
               !data_->obj.has_value();
    }

    /*!
     * \brief Check if the value is constant.
     * \return True if the value is constant, false otherwise.
     */
    [[nodiscard]] auto isConst() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return data_->flags.isConst || data_->typeInfo.isConst();
    }

    /*!
     * \brief Check if the value is of a specific type.
     * \param type_info The type information to check against.
     * \return True if the value is of the specified type, false otherwise.
     */
    [[nodiscard]] auto isType(const TypeInfo& type_info) const noexcept
        -> bool {
        std::shared_lock lock(mutex_);
        return data_->typeInfo == type_info;
    }

    /*!
     * \brief Check if the value is a reference.
     * \return True if the value is a reference, false otherwise.
     */
    [[nodiscard]] auto isRef() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return data_->flags.isRef;
    }

    /*!
     * \brief Check if the value is a return value.
     * \return True if the value is a return value, false otherwise.
     */
    [[nodiscard]] auto isReturnValue() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return data_->flags.returnValue;
    }

    /*!
     * \brief Reset the return value flag.
     */
    void resetReturnValue() noexcept {
        std::unique_lock lock(mutex_);
        data_->flags.returnValue = false;
    }

    /*!
     * \brief Check if the value is read-only.
     * \return True if the value is read-only, false otherwise.
     */
    [[nodiscard]] auto isReadonly() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return data_->flags.readonly;
    }

    /*!
     * \brief Check if the value is void.
     * \return True if the value is void, false otherwise.
     */
    [[nodiscard]] auto isVoid() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return data_->typeInfo == userType<VoidType>();
    }

    /*!
     * \brief Check if the value is a constant data pointer.
     * \return True if the value is a constant data pointer, false otherwise.
     */
    [[nodiscard]] auto isConstDataPtr() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return data_->constDataPtr != nullptr;
    }

    /*!
     * \brief Get the encapsulated value.
     * \return The encapsulated value.
     */
    [[nodiscard]] auto get() const noexcept -> const std::any& {
        std::shared_lock lock(mutex_);
        ++data_->accessCount;
        return data_->obj;
    }

    /*!
     * \brief Get the type information of the value.
     * \return The type information of the value.
     */
    [[nodiscard]] auto getTypeInfo() const noexcept -> const TypeInfo& {
        std::shared_lock lock(mutex_);
        return data_->typeInfo;
    }

    /*!
     * \brief Set an attribute.
     * \param name The name of the attribute.
     * \param value The value of the attribute.
     * \return Reference to this BoxedValue.
     */
    auto setAttr(const std::string& name, const BoxedValue& value)
        -> BoxedValue& {
        std::unique_lock lock(mutex_);
        if (!data_->attrs) {
            data_->attrs = std::make_shared<
                std::unordered_map<std::string, std::shared_ptr<Data>>>();
        }
        (*data_->attrs)[name] = value.data_;
        data_->modificationTime = getCurrentTimeMicros();
        return *this;
    }

    /*!
     * \brief Get an attribute.
     * \param name The name of the attribute.
     * \return The value of the attribute.
     */
    [[nodiscard]] auto getAttr(const std::string& name) const -> BoxedValue {
        std::shared_lock lock(mutex_);
        if (data_->attrs) {
            if (auto iter = data_->attrs->find(name);
                iter != data_->attrs->end()) {
                return BoxedValue(iter->second);
            }
        }
        return {};
    }

    /*!
     * \brief List all attributes.
     * \return A vector of attribute names.
     */
    [[nodiscard]] auto listAttrs() const -> std::vector<std::string> {
        std::shared_lock lock(mutex_);
        std::vector<std::string> attrs;
        if (data_->attrs) {
            attrs.reserve(data_->attrs->size());
            for (const auto& [key, value] : *data_->attrs) {
                attrs.push_back(key);
            }
        }
        return attrs;
    }

    /*!
     * \brief Check if an attribute exists.
     * \param name The name of the attribute.
     * \return True if the attribute exists, false otherwise.
     */
    [[nodiscard]] auto hasAttr(const std::string& name) const -> bool {
        std::shared_lock lock(mutex_);
        return data_->attrs && data_->attrs->contains(name);
    }

    /*!
     * \brief Remove an attribute.
     * \param name The name of the attribute.
     */
    void removeAttr(const std::string& name) {
        std::unique_lock lock(mutex_);
        if (data_->attrs) {
            data_->attrs->erase(name);
            data_->modificationTime = getCurrentTimeMicros();
        }
    }

    /*!
     * \brief Check if the BoxedValue is null (i.e., contains an unset value).
     * \return True if the BoxedValue is null, false otherwise.
     */
    [[nodiscard]] auto isNull() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return !data_->obj.has_value();
    }

    /*!
     * \brief Get the pointer to the contained data.
     * \return Pointer to the contained data.
     */
    [[nodiscard]] auto getPtr() const noexcept -> void* {
        std::shared_lock lock(mutex_);
        return const_cast<void*>(data_->constDataPtr);
    }

    /*!
     * \brief Try to cast the internal value to a specified type.
     * \tparam T The type to cast to.
     * \return An optional containing the casted value if successful,
     * std::nullopt otherwise.
     */
    template <typename T>
    [[nodiscard]] auto tryCast() const noexcept -> std::optional<T> {
        std::shared_lock lock(mutex_);
        incrementAccessCount();  // Track access for performance monitoring
        try {
            if constexpr (std::is_reference_v<T>) {
                if (data_->obj.type() ==
                    typeid(
                        std::reference_wrapper<std::remove_reference_t<T>>)) {
                    return std::any_cast<std::reference_wrapper<
                        std::remove_reference_t<T>>>(data_->obj)
                        .get();
                }
            }
            if (data_->obj.type() == typeid(std::reference_wrapper<T>)) {
                return std::any_cast<std::reference_wrapper<T>>(data_->obj)
                    .get();
            }
            if (isConst() || isReadonly()) {
                using ConstT = std::add_const_t<T>;
                return std::any_cast<ConstT>(data_->obj);
            }
            return std::any_cast<T>(data_->obj);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

    /*!
     * \brief Check if the internal value can be cast to a specified type.
     * \tparam T The type to check.
     * \return True if the value can be cast to the specified type, false
     * otherwise.
     */
    template <typename T>
    [[nodiscard]] auto canCast() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        try {
            if constexpr (std::is_reference_v<T>) {
                return data_->obj.type() ==
                       typeid(
                           std::reference_wrapper<std::remove_reference_t<T>>);
            } else {
                std::any_cast<T>(data_->obj);
                return true;
            }
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    /*!
     * \brief Get creation time
     * \return Creation time as time_point
     */
    [[nodiscard]] auto getCreationTime() const noexcept
        -> std::chrono::system_clock::time_point {
        std::shared_lock lock(mutex_);
        return microsToTimePoint(data_->creationTime);
    }

    /*!
     * \brief Get modification time
     * \return Modification time as time_point
     */
    [[nodiscard]] auto getModificationTime() const noexcept
        -> std::chrono::system_clock::time_point {
        std::shared_lock lock(mutex_);
        return microsToTimePoint(data_->modificationTime);
    }

    /*!
     * \brief Get performance statistics
     * \return Performance statistics structure
     */
    [[nodiscard]] auto getPerformanceStats() const noexcept -> PerformanceStats {
        std::shared_lock lock(mutex_);
        PerformanceStats stats;
        stats.access_count = getAccessCount();
        stats.creation_time_micros = data_->creationTime;
        stats.last_access_time_micros = data_->modificationTime;
        // Note: copy_count, move_count, and total_access_time would need additional tracking
        return stats;
    }

    /*!
     * \brief Set attribute with metadata
     * \param name Attribute name
     * \param value Attribute value
     * \param metadata Attribute metadata
     * \return Reference to this BoxedValue
     */
    auto setAttrWithMetadata(const std::string& name, const BoxedValue& value,
                           const AttributeMetadata& metadata = {}) -> BoxedValue& {
        std::unique_lock lock(mutex_);
        if (!data_->attrs) {
            data_->attrs = std::make_shared<
                std::unordered_map<std::string, std::shared_ptr<Data>>>();
        }
        (*data_->attrs)[name] = value.data_;

        // Store metadata in a special attribute
        auto meta_copy = metadata;
        meta_copy.creation_time = getCurrentTimeMicros();
        meta_copy.modification_time = meta_copy.creation_time;

        // Create a BoxedValue for the metadata and store it
        auto metadata_key = "__meta_" + name;
        (*data_->attrs)[metadata_key] = std::make_shared<Data>(
            meta_copy, false, false, true);

        data_->modificationTime = getCurrentTimeMicros();
        return *this;
    }

    /*!
     * \brief Get attribute metadata
     * \param name Attribute name
     * \return Optional containing metadata if found
     */
    [[nodiscard]] auto getAttrMetadata(const std::string& name) const
        -> std::optional<AttributeMetadata> {
        std::shared_lock lock(mutex_);
        if (!data_->attrs) {
            return std::nullopt;
        }

        auto metadata_key = "__meta_" + name;
        auto it = data_->attrs->find(metadata_key);
        if (it != data_->attrs->end()) {
            try {
                return std::any_cast<AttributeMetadata>(it->second->obj);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    /*!
     * \brief Create a deep clone of this BoxedValue
     * \param copy_attributes Whether to copy attributes as well
     * \return New BoxedValue instance
     */
    [[nodiscard]] auto clone(bool copy_attributes = true) const -> BoxedValue {
        std::shared_lock lock(mutex_);

        // Create new BoxedValue with same data
        BoxedValue result;
        result.data_ = std::make_shared<Data>(*data_);

        // Reset timing information for the clone
        result.data_->creationTime = getCurrentTimeMicros();
        result.data_->modificationTime = result.data_->creationTime;
        result.data_->accessCount.store(0, std::memory_order_relaxed);

        // Optionally copy attributes
        if (!copy_attributes && result.data_->attrs) {
            result.data_->attrs.reset();
        }

        return result;
    }

    /*!
     * \brief Reset performance counters
     */
    void resetPerformanceCounters() noexcept {
        std::unique_lock lock(mutex_);
        data_->accessCount.store(0, std::memory_order_relaxed);
        data_->creationTime = getCurrentTimeMicros();
        data_->modificationTime = data_->creationTime;
    }

    /*!
     * \brief Get a debug string representation of the BoxedValue.
     * \return A string representing the BoxedValue.
     */
    [[nodiscard]] auto debugString() const -> std::string {
        std::ostringstream oss;
        oss << "BoxedValue<" << data_->typeInfo.name() << ">: ";
        std::shared_lock lock(mutex_);
        if (auto* intPtr = std::any_cast<int>(&data_->obj)) {
            oss << *intPtr;
        } else if (auto* doublePtr = std::any_cast<double>(&data_->obj)) {
            oss << *doublePtr;
        } else if (auto* strPtr = std::any_cast<std::string>(&data_->obj)) {
            oss << *strPtr;
        } else {
            oss << "unknown type";
        }
        return oss.str();
    }

    /*!
     * \brief Enhanced debug string with detailed metadata
     * \return Comprehensive debug information
     */
    [[nodiscard]] auto detailedDebugString() const -> std::string {
        std::ostringstream oss;
        std::shared_lock lock(mutex_);

        oss << "=== BoxedValue Debug Info ===\n";
        oss << "Type: " << data_->typeInfo.name() << "\n";
        oss << "Bare Type: " << data_->typeInfo.bareName() << "\n";
        oss << "Type Traits: ";
        oss << (data_->typeInfo.isArithmetic() ? "ARITHMETIC " : "");
        oss << (data_->typeInfo.isClass() ? "CLASS " : "");
        oss << (data_->typeInfo.isPointer() ? "POINTER " : "");
        oss << (data_->typeInfo.isEnum() ? "ENUM " : "");
        oss << "\n";
        oss << "Flags: ";
        oss << (data_->flags.isRef ? "REF " : "");
        oss << (data_->flags.returnValue ? "RETURN " : "");
        oss << (data_->flags.readonly ? "READONLY " : "");
        oss << (data_->flags.isConst ? "CONST " : "");
        oss << "\n";
        oss << "Access Count: " << getAccessCount() << "\n";
        oss << "Creation Time: " << std::format("{:%Y-%m-%d %H:%M:%S}", getCreationTime()) << "\n";
        oss << "Modification Time: " << std::format("{:%Y-%m-%d %H:%M:%S}", getModificationTime()) << "\n";
        oss << "Has Attributes: " << (data_->attrs ? "Yes" : "No") << "\n";
        if (data_->attrs) {
            oss << "Attribute Count: " << data_->attrs->size() << "\n";
        }
        oss << "Value: ";

        // Try to display the value
        if (auto* intPtr = std::any_cast<int>(&data_->obj)) {
            oss << *intPtr;
        } else if (auto* doublePtr = std::any_cast<double>(&data_->obj)) {
            oss << *doublePtr;
        } else if (auto* strPtr = std::any_cast<std::string>(&data_->obj)) {
            oss << "\"" << *strPtr << "\"";
        } else if (auto* boolPtr = std::any_cast<bool>(&data_->obj)) {
            oss << (*boolPtr ? "true" : "false");
        } else {
            oss << "[" << data_->typeInfo.name() << " object]";
        }
        oss << "\n========================\n";

        return oss.str();
    }

    /*!
     * \brief Serialize the BoxedValue to specified format
     * \param format The serialization format
     * \return Serialization result
     */
    [[nodiscard]] auto serialize(SerializationFormat format = SerializationFormat::JSON) const
        -> SerializationResult {
        std::shared_lock lock(mutex_);
        SerializationResult result;

        try {
            switch (format) {
                case SerializationFormat::JSON:
                    result = serializeToJson();
                    break;
                case SerializationFormat::BINARY:
                    result = serializeToBinary();
                    break;
                case SerializationFormat::XML:
                    result = serializeToXml();
                    break;
                case SerializationFormat::YAML:
                    result = serializeToYaml();
                    break;
                default:
                    result.error_message = "Unsupported serialization format";
                    return result;
            }
        } catch (const std::exception& e) {
            result.error_message = std::string("Serialization error: ") + e.what();
        }

        return result;
    }

    /*!
     * \brief Visit the value in BoxedValue with a visitor
     * \tparam Visitor The type of visitor
     * \param visitor The visitor function object
     * \return The return value of the visitor
     */
    template <typename Visitor>
    auto visit(Visitor&& visitor) const {
        using ResultType = std::invoke_result_t<Visitor, int&>;

        std::shared_lock lock(mutex_);
        if (isUndef() || isNull()) {
            if constexpr (requires { visitor.fallback(); }) {
                return visitor.fallback();
            } else if constexpr (std::is_default_constructible_v<ResultType>) {
                return ResultType{};
            } else {
                throw std::bad_any_cast();
            }
        }

        return visitImpl(std::forward<Visitor>(visitor));
    }

    /*!
     * \brief Visit and possibly modify the value in BoxedValue with a visitor
     * \tparam Visitor The type of visitor
     * \param visitor The visitor function object
     * \return The return value of the visitor
     */
    template <typename Visitor>
    auto visit(Visitor&& visitor) {
        using ResultType = std::invoke_result_t<Visitor, int&>;

        std::unique_lock lock(mutex_);
        if (isUndef() || isNull() || isReadonly()) {
            if constexpr (requires { visitor.fallback(); }) {
                return visitor.fallback();
            } else if constexpr (std::is_default_constructible_v<ResultType>) {
                return ResultType{};
            } else {
                throw std::bad_any_cast();
            }
        }

        auto result = visitImpl(std::forward<Visitor>(visitor));
        data_->modificationTime = getCurrentTimeMicros();
        return result;
    }

private:
    using MapStringInt = std::map<std::string, int>;
    using MapStringDouble = std::map<std::string, double>;
    using MapStringString = std::map<std::string, std::string>;
    using UMapStringInt = std::unordered_map<std::string, int>;
    using UMapStringDouble = std::unordered_map<std::string, double>;
    using UMapStringString = std::unordered_map<std::string, std::string>;
    using PairIntInt = std::pair<int, int>;
    using PairIntString = std::pair<int, std::string>;
    using PairStringString = std::pair<std::string, std::string>;
    using TupleInt = std::tuple<int>;
    using TupleIntInt = std::tuple<int, int>;
    using TupleIntString = std::tuple<int, std::string>;
    using TupleStringString = std::tuple<std::string, std::string>;
    using VariantTypes = std::variant<int, double, std::string>;

    template <typename Visitor>
    auto visitImpl(Visitor&& visitor) const {
        using ResultType = std::invoke_result_t<Visitor, int&>;

#define VISIT_TYPE(Type)                                             \
    if (data_->obj.type() == typeid(Type)) {                         \
        if (isConst() || isReadonly()) {                             \
            return visitor(*std::any_cast<const Type>(&data_->obj)); \
        } else {                                                     \
            return visitor(*std::any_cast<Type>(&data_->obj));       \
        }                                                            \
    }

        VISIT_TYPE(int)
        VISIT_TYPE(unsigned int)
        VISIT_TYPE(long)
        VISIT_TYPE(unsigned long)
        VISIT_TYPE(long long)
        VISIT_TYPE(unsigned long long)
        VISIT_TYPE(short)
        VISIT_TYPE(unsigned short)
        VISIT_TYPE(char)
        VISIT_TYPE(unsigned char)
        VISIT_TYPE(signed char)
        VISIT_TYPE(wchar_t)
        VISIT_TYPE(char16_t)
        VISIT_TYPE(char32_t)
        VISIT_TYPE(float)
        VISIT_TYPE(double)
        VISIT_TYPE(long double)
        VISIT_TYPE(bool)

        VISIT_TYPE(std::string)
        VISIT_TYPE(std::wstring)
        VISIT_TYPE(std::u16string)
        VISIT_TYPE(std::u32string)
        VISIT_TYPE(std::string_view)
        VISIT_TYPE(std::wstring_view)
        VISIT_TYPE(std::u16string_view)
        VISIT_TYPE(std::u32string_view)

        VISIT_TYPE(std::vector<int>)
        VISIT_TYPE(std::vector<double>)
        VISIT_TYPE(std::vector<std::string>)
        VISIT_TYPE(std::vector<bool>)
        VISIT_TYPE(std::list<int>)
        VISIT_TYPE(std::list<double>)
        VISIT_TYPE(std::list<std::string>)

        VISIT_TYPE(MapStringInt)
        VISIT_TYPE(MapStringDouble)
        VISIT_TYPE(MapStringString)
        VISIT_TYPE(UMapStringInt)
        VISIT_TYPE(UMapStringDouble)
        VISIT_TYPE(UMapStringString)
        VISIT_TYPE(std::set<int>)
        VISIT_TYPE(std::set<double>)
        VISIT_TYPE(std::set<std::string>)
        VISIT_TYPE(std::unordered_set<int>)
        VISIT_TYPE(std::unordered_set<double>)
        VISIT_TYPE(std::unordered_set<std::string>)

        VISIT_TYPE(std::shared_ptr<int>)
        VISIT_TYPE(std::shared_ptr<double>)
        VISIT_TYPE(std::shared_ptr<std::string>)
        VISIT_TYPE(std::unique_ptr<int>)
        VISIT_TYPE(std::unique_ptr<double>)
        VISIT_TYPE(std::unique_ptr<std::string>)

        VISIT_TYPE(std::chrono::seconds)
        VISIT_TYPE(std::chrono::milliseconds)
        VISIT_TYPE(std::chrono::microseconds)
        VISIT_TYPE(std::chrono::nanoseconds)
        VISIT_TYPE(std::chrono::minutes)
        VISIT_TYPE(std::chrono::hours)
        VISIT_TYPE(std::chrono::system_clock::time_point)
        VISIT_TYPE(std::chrono::steady_clock::time_point)
        VISIT_TYPE(std::chrono::high_resolution_clock::time_point)

        VISIT_TYPE(std::optional<int>)
        VISIT_TYPE(std::optional<double>)
        VISIT_TYPE(std::optional<std::string>)

        VISIT_TYPE(PairIntInt)
        VISIT_TYPE(PairIntString)
        VISIT_TYPE(PairStringString)
        VISIT_TYPE(TupleInt)
        VISIT_TYPE(TupleIntInt)
        VISIT_TYPE(TupleIntString)
        VISIT_TYPE(TupleStringString)

        VISIT_TYPE(VariantTypes)

#define VISIT_REF_TYPE(Type)                                                \
    if (data_->obj.type() == typeid(std::reference_wrapper<Type>)) {        \
        return visitor(                                                     \
            std::any_cast<std::reference_wrapper<Type>>(data_->obj).get()); \
    }                                                                       \
    if (data_->obj.type() == typeid(std::reference_wrapper<const Type>)) {  \
        return visitor(                                                     \
            std::any_cast<std::reference_wrapper<const Type>>(data_->obj)   \
                .get());                                                    \
    }

        VISIT_REF_TYPE(int)
        VISIT_REF_TYPE(unsigned int)
        VISIT_REF_TYPE(long)
        VISIT_REF_TYPE(unsigned long)
        VISIT_REF_TYPE(long long)
        VISIT_REF_TYPE(unsigned long long)
        VISIT_REF_TYPE(short)
        VISIT_REF_TYPE(unsigned short)
        VISIT_REF_TYPE(char)
        VISIT_REF_TYPE(unsigned char)
        VISIT_REF_TYPE(signed char)
        VISIT_REF_TYPE(wchar_t)
        VISIT_REF_TYPE(char16_t)
        VISIT_REF_TYPE(char32_t)
        VISIT_REF_TYPE(float)
        VISIT_REF_TYPE(double)
        VISIT_REF_TYPE(long double)
        VISIT_REF_TYPE(bool)

        VISIT_REF_TYPE(std::string)
        VISIT_REF_TYPE(std::wstring)
        VISIT_REF_TYPE(std::u16string)
        VISIT_REF_TYPE(std::u32string)
        VISIT_REF_TYPE(std::string_view)
        VISIT_REF_TYPE(std::wstring_view)
        VISIT_REF_TYPE(std::u16string_view)
        VISIT_REF_TYPE(std::u32string_view)

        VISIT_REF_TYPE(std::vector<int>)
        VISIT_REF_TYPE(std::vector<double>)
        VISIT_REF_TYPE(std::vector<std::string>)

        VISIT_REF_TYPE(MapStringInt)
        VISIT_REF_TYPE(MapStringString)
        VISIT_REF_TYPE(UMapStringInt)
        VISIT_REF_TYPE(UMapStringString)

#undef VISIT_TYPE
#undef VISIT_REF_TYPE

        if constexpr (requires { visitor.fallback(); }) {
            return visitor.fallback();
        } else if constexpr (std::is_default_constructible_v<ResultType>) {
            return ResultType{};
        } else {
            throw std::bad_any_cast();
        }
    }

    /*!
     * \brief Serialize to JSON format
     * \return JSON serialization result
     */
    [[nodiscard]] auto serializeToJson() const -> SerializationResult {
        SerializationResult result;
        std::ostringstream oss;

        try {
            oss << "{\n";
            oss << "  \"type\": \"" << data_->typeInfo.name() << "\",\n";
            oss << "  \"flags\": {\n";
            oss << "    \"isRef\": " << (data_->flags.isRef ? "true" : "false") << ",\n";
            oss << "    \"returnValue\": " << (data_->flags.returnValue ? "true" : "false") << ",\n";
            oss << "    \"readonly\": " << (data_->flags.readonly ? "true" : "false") << ",\n";
            oss << "    \"isConst\": " << (data_->flags.isConst ? "true" : "false") << "\n";
            oss << "  },\n";
            oss << "  \"metadata\": {\n";
            oss << "    \"creationTime\": " << data_->creationTime << ",\n";
            oss << "    \"modificationTime\": " << data_->modificationTime << ",\n";
            oss << "    \"accessCount\": " << getAccessCount() << "\n";
            oss << "  },\n";
            oss << "  \"value\": ";

            // Serialize the actual value based on type
            if (auto* intPtr = std::any_cast<int>(&data_->obj)) {
                oss << *intPtr;
            } else if (auto* doublePtr = std::any_cast<double>(&data_->obj)) {
                oss << *doublePtr;
            } else if (auto* strPtr = std::any_cast<std::string>(&data_->obj)) {
                oss << "\"" << *strPtr << "\"";
            } else if (auto* boolPtr = std::any_cast<bool>(&data_->obj)) {
                oss << (*boolPtr ? "true" : "false");
            } else {
                oss << "\"[" << data_->typeInfo.name() << " object]\"";
            }

            oss << "\n}";

            result.success = true;
            result.data = oss.str();
        } catch (const std::exception& e) {
            result.error_message = std::string("JSON serialization failed: ") + e.what();
        }

        return result;
    }

    /*!
     * \brief Serialize to binary format (simplified)
     * \return Binary serialization result
     */
    [[nodiscard]] auto serializeToBinary() const -> SerializationResult {
        SerializationResult result;
        result.error_message = "Binary serialization not yet implemented";
        return result;
    }

    /*!
     * \brief Serialize to XML format
     * \return XML serialization result
     */
    [[nodiscard]] auto serializeToXml() const -> SerializationResult {
        SerializationResult result;
        result.error_message = "XML serialization not yet implemented";
        return result;
    }

    /*!
     * \brief Serialize to YAML format
     * \return YAML serialization result
     */
    [[nodiscard]] auto serializeToYaml() const -> SerializationResult {
        SerializationResult result;
        result.error_message = "YAML serialization not yet implemented";
        return result;
    }
};

/*!
 * \brief Helper function to create a BoxedValue instance.
 * \tparam T The type of the value.
 * \param value The value to be encapsulated.
 * \return A BoxedValue instance.
 */
template <typename T>
auto var(T&& value) -> BoxedValue {
    using DecayedType = std::decay_t<T>;
    constexpr bool IS_REF_WRAPPER =
        std::is_same_v<DecayedType,
                       std::reference_wrapper<std::remove_reference_t<T>>>;
    return BoxedValue(std::forward<T>(value), IS_REF_WRAPPER, false);
}

/*!
 * \brief Helper function to create a constant BoxedValue instance.
 * \tparam T The type of the value.
 * \param value The constant value to be encapsulated.
 * \return A BoxedValue instance.
 */
template <typename T>
auto constVar(const T& value) -> BoxedValue {
    using DecayedType = std::decay_t<T>;
    constexpr bool IS_REF_WRAPPER =
        std::is_same_v<DecayedType,
                       std::reference_wrapper<std::remove_reference_t<T>>>;
    return BoxedValue(std::cref(value), IS_REF_WRAPPER, true);
}

/*!
 * \brief Helper function to create a void BoxedValue instance.
 * \return A BoxedValue instance representing void.
 */
inline auto voidVar() -> BoxedValue { return {}; }

/*!
 * \brief Helper function to create a BoxedValue instance with description.
 * \tparam T The type of the value.
 * \param value The value to be encapsulated.
 * \param description Description of the value.
 * \return A BoxedValue instance with description attribute.
 */
template <typename T>
auto varWithDesc(T&& value, std::string_view description) -> BoxedValue {
    auto result = var(std::forward<T>(value));
    result.setAttr("description", BoxedValue(std::string(description)));
    return result;
}

/*!
 * \brief Helper function to create a BoxedValue instance with additional
 * options.
 * \tparam T The type of the value.
 * \param value The value to be encapsulated.
 * \param is_return_value Indicates if the value is a return value.
 * \param is_readonly Indicates if the value is read-only.
 * \return A BoxedValue instance.
 */
template <typename T>
auto makeBoxedValue(T&& value, bool is_return_value = false,
                    bool is_readonly = false) -> BoxedValue {
    if constexpr (std::is_reference_v<T>) {
        return BoxedValue(std::ref(value), is_return_value, is_readonly);
    } else {
        return BoxedValue(std::forward<T>(value), is_return_value, is_readonly);
    }
}

}  // namespace atom::meta

#endif  // ATOM_META_ANY_HPP
