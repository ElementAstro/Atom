/*!
 * \file any.hpp
 * \brief Enhanced BoxedValue using C++20 features
 * \author Max Qian <lightapt.com>
 * \date 2023-12-28
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_ANY_HPP
#define ATOM_META_ANY_HPP

#include <any>
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
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "atom/macro.hpp"
#include "type_info.hpp"

namespace atom::meta {

/*!
 * \class BoxedValue
 * \brief A class that encapsulates a value of any type with additional
 * metadata.
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
     */
    struct ATOM_ALIGNAS(128) Data {
        std::any mObj;       ///< The encapsulated value.
        TypeInfo mTypeInfo;  ///< Type information of the value.
        std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<Data>>>
            mAttrs;           ///< Attributes associated with the value.
        bool mIsRef = false;  ///< Indicates if the value is a reference.
        bool mReturnValue =
            false;               ///< Indicates if the value is a return value.
        bool mReadonly = false;  ///< Indicates if the value is read-only.
        const void* mConstDataPtr = nullptr;  ///< Pointer to the constant data.
        std::chrono::time_point<std::chrono::system_clock>
            mCreationTime;  ///< Creation time.
        std::chrono::time_point<std::chrono::system_clock>
            mModificationTime;  ///< Modification time.
        int mAccessCount = 0;   ///< Access count.

        /*!
         * \brief Constructor for non-void types.
         * \tparam T The type of the value.
         * \param obj The value to be encapsulated.
         * \param is_ref Indicates if the value is a reference.
         * \param return_value Indicates if the value is a return value.
         * \param readonly Indicates if the value is read-only.
         */
        template <typename T>
            requires(!std::is_same_v<std::decay_t<T>, VoidType>)
        Data(T&& obj, bool is_ref, bool return_value, bool readonly)
            : mObj(std::forward<T>(obj)),
              mTypeInfo(userType<std::decay_t<T>>()),
              mAttrs(nullptr),
              mIsRef(is_ref),
              mReturnValue(return_value),
              mReadonly(readonly),
              mConstDataPtr(
                  std::is_const_v<std::remove_reference_t<T>> ? &obj : nullptr),
              mCreationTime(std::chrono::system_clock::now()),
              mModificationTime(std::chrono::system_clock::now()) {}

        /*!
         * \brief Constructor for void type.
         * \tparam T The type of the value.
         * \param obj The value to be encapsulated.
         * \param is_ref Indicates if the value is a reference.
         * \param return_value Indicates if the value is a return value.
         * \param readonly Indicates if the value is read-only.
         */
        template <typename T>
            requires(std::is_same_v<std::decay_t<T>, VoidType>)
        Data([[maybe_unused]] T&& obj, bool is_ref, bool return_value,
             bool readonly)
            : mTypeInfo(userType<std::decay_t<T>>()),
              mAttrs(nullptr),
              mIsRef(is_ref),
              mReturnValue(return_value),
              mReadonly(readonly),
              mCreationTime(std::chrono::system_clock::now()),
              mModificationTime(std::chrono::system_clock::now()) {}
    };

    std::shared_ptr<Data> m_data_;  ///< Shared pointer to the internal data.
    mutable std::shared_mutex m_mutex_;  ///< Mutex for thread-safe access.

public:
    /*!
     * \brief Constructor for any type.
     * \tparam T The type of the value.
     * \param value The value to be encapsulated.
     * \param return_value Indicates if the value is a return value.
     * \param readonly Indicates if the value is read-only.
     */
    // clang-tidy: disable=hicpp-explicit-constructor
    template <typename T>
        requires(!std::same_as<BoxedValue, std::decay_t<T>>)
    BoxedValue(T&& value, bool return_value = false, bool readonly = false)
        : m_data_(std::make_shared<Data>(
              std::forward<T>(value),
              std::is_reference_v<T> ||
                  std::is_same_v<
                      std::decay_t<T>,
                      std::reference_wrapper<std::remove_reference_t<T>>>,
              return_value, readonly)) {
        if constexpr (std::is_same_v<
                          std::decay_t<T>,
                          std::reference_wrapper<std::remove_reference_t<T>>>) {
            m_data_->mIsRef = true;
        }
    }

    /*!
     * \brief Default constructor for VoidType.
     */
    BoxedValue()
        : m_data_(std::make_shared<Data>(VoidType{}, false, false, false)) {}

    /*!
     * \brief Constructor with shared data pointer.
     * \param data Shared pointer to the internal data.
     */
    BoxedValue(std::shared_ptr<Data> data) : m_data_(std::move(data)) {}

    /*!
     * \brief Copy constructor.
     * \param other The other BoxedValue to copy from.
     */
    BoxedValue(const BoxedValue& other) {
        std::shared_lock lock(other.m_mutex_);
        if (other.m_data_) {
            m_data_ = std::make_shared<Data>(*other.m_data_);
        } else {
            m_data_ = nullptr;
        }
    }

    /*!
     * \brief Move constructor.
     * \param other The other BoxedValue to move from.
     */
    BoxedValue(BoxedValue&& other) noexcept {
        std::unique_lock lock(other.m_mutex_);
        m_data_ = std::move(other.m_data_);
        other.m_data_ = nullptr;
    }

    /*!
     * \brief Copy assignment operator.
     * \param other The other BoxedValue to copy from.
     * \return Reference to this BoxedValue.
     */
    auto operator=(const BoxedValue& other) -> BoxedValue& {
        if (this != &other) {
            std::unique_lock lock(m_mutex_);
            std::shared_lock otherLock(other.m_mutex_);
            m_data_ = std::make_shared<Data>(*other.m_data_);
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
            std::unique_lock lock(m_mutex_);
            std::unique_lock otherLock(other.m_mutex_);
            m_data_ = std::move(other.m_data_);
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
        std::unique_lock lock(m_mutex_);
        m_data_->mObj = std::forward<T>(value);
        m_data_->mTypeInfo = userType<T>();
        m_data_->mModificationTime = std::chrono::system_clock::now();
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
        std::unique_lock lock(m_mutex_);
        m_data_->mObj = value;
        m_data_->mTypeInfo = userType<T>();
        m_data_->mReadonly = true;
        m_data_->mModificationTime = std::chrono::system_clock::now();
        return *this;
    }

    /*!
     * \brief Constructor for constant values.
     * \tparam T The type of the value.
     * \param value The constant value to be encapsulated.
     */
    template <typename T>
    BoxedValue(const T& value)
        : m_data_(std::make_shared<Data>(value, false, false, true)) {}

    /*!
     * \brief Swap function.
     * \param rhs The other BoxedValue to swap with.
     */
    void swap(BoxedValue& rhs) noexcept {
        if (this != &rhs) {
            std::scoped_lock lock(m_mutex_, rhs.m_mutex_);
            std::swap(m_data_, rhs.m_data_);
        }
    }

    /*!
     * \brief Destructor.
     */
    ~BoxedValue() = default;

    template <typename T>
    auto isType() const -> bool {
        std::shared_lock lock(m_mutex_);
        return m_data_->mTypeInfo == userType<T>();
    }

    /*!
     * \brief Check if the value is undefined.
     * \return True if the value is undefined, false otherwise.
     */
    [[nodiscard]] auto isUndef() const noexcept -> bool {
        std::shared_lock lock(m_mutex_);
        return !m_data_ || m_data_->mObj.type() == typeid(VoidType) ||
               !m_data_->mObj.has_value();
    }

    /*!
     * \brief Check if the value is constant.
     * \return True if the value is constant, false otherwise.
     */
    [[nodiscard]] auto isConst() const noexcept -> bool {
        std::shared_lock lock(m_mutex_);
        return m_data_->mTypeInfo.isConst();
    }

    /*!
     * \brief Check if the value is of a specific type.
     * \param type_info The type information to check against.
     * \return True if the value is of the specified type, false otherwise.
     */
    [[nodiscard]] auto isType(const TypeInfo& type_info) const noexcept
        -> bool {
        std::shared_lock lock(m_mutex_);
        return m_data_->mTypeInfo == type_info;
    }

    /*!
     * \brief Check if the value is a reference.
     * \return True if the value is a reference, false otherwise.
     */
    [[nodiscard]] auto isRef() const noexcept -> bool {
        std::shared_lock lock(m_mutex_);
        return m_data_->mIsRef;
    }

    /*!
     * \brief Check if the value is a return value.
     * \return True if the value is a return value, false otherwise.
     */
    [[nodiscard]] auto isReturnValue() const noexcept -> bool {
        std::shared_lock lock(m_mutex_);
        return m_data_->mReturnValue;
    }

    /*!
     * \brief Reset the return value flag.
     */
    void resetReturnValue() noexcept {
        std::unique_lock lock(m_mutex_);
        m_data_->mReturnValue = false;
    }

    /*!
     * \brief Check if the value is read-only.
     * \return True if the value is read-only, false otherwise.
     */
    [[nodiscard]] auto isReadonly() const noexcept -> bool {
        std::shared_lock lock(m_mutex_);
        return m_data_->mReadonly;
    }

    /*!
     * \brief Check if the value is void.
     * \return True if the value is void, false otherwise.
     */
    [[nodiscard]] auto isVoid() const noexcept -> bool {
        std::shared_lock lock(m_mutex_);
        return m_data_->mTypeInfo == userType<VoidType>();
    }

    /*!
     * \brief Check if the value is a constant data pointer.
     * \return True if the value is a constant data pointer, false
     * otherwise.
     */
    [[nodiscard]] auto isConstDataPtr() const noexcept -> bool {
        std::shared_lock lock(m_mutex_);
        return m_data_->mConstDataPtr != nullptr;
    }

    /*!
     * \brief Get the encapsulated value.
     * \return The encapsulated value.
     */
    [[nodiscard]] auto get() const noexcept -> const std::any& {
        std::shared_lock lock(m_mutex_);
        m_data_->mAccessCount++;
        return m_data_->mObj;
    }

    /*!
     * \brief Get the type information of the value.
     * \return The type information of the value.
     */
    [[nodiscard]] auto getTypeInfo() const noexcept -> const TypeInfo& {
        std::shared_lock lock(m_mutex_);
        return m_data_->mTypeInfo;
    }

    /*!
     * \brief Set an attribute.
     * \param name The name of the attribute.
     * \param value The value of the attribute.
     * \return Reference to this BoxedValue.
     */
    auto setAttr(const std::string& name, const BoxedValue& value)
        -> BoxedValue& {
        std::unique_lock lock(m_mutex_);
        if (!m_data_->mAttrs) {
            m_data_->mAttrs = std::make_shared<
                std::unordered_map<std::string, std::shared_ptr<Data>>>();
        }
        (*m_data_->mAttrs)[name] = value.m_data_;
        m_data_->mModificationTime = std::chrono::system_clock::now();
        return *this;
    }

    /*!
     * \brief Get an attribute.
     * \param name The name of the attribute.
     * \return The value of the attribute.
     */
    [[nodiscard]] auto getAttr(const std::string& name) const -> BoxedValue {
        std::shared_lock lock(m_mutex_);
        if (m_data_->mAttrs) {
            if (auto iter = m_data_->mAttrs->find(name);
                iter != m_data_->mAttrs->end()) {
                return BoxedValue(iter->second);
            }
        }
        return {};  // Undefined BoxedValue
    }

    /*!
     * \brief List all attributes.
     * \return A vector of attribute names.
     */
    [[nodiscard]] auto listAttrs() const -> std::vector<std::string> {
        std::shared_lock lock(m_mutex_);
        std::vector<std::string> attrs;
        if (m_data_->mAttrs) {
            attrs.reserve(m_data_->mAttrs->size());
            for (const auto& entry : *m_data_->mAttrs) {
                attrs.push_back(entry.first);
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
        std::shared_lock lock(m_mutex_);
        return m_data_->mAttrs &&
               m_data_->mAttrs->find(name) != m_data_->mAttrs->end();
    }

    /*!
     * \brief Remove an attribute.
     * \param name The name of the attribute.
     */
    void removeAttr(const std::string& name) {
        std::unique_lock lock(m_mutex_);
        if (m_data_->mAttrs) {
            m_data_->mAttrs->erase(name);
            m_data_->mModificationTime = std::chrono::system_clock::now();
        }
    }

    /*!
     * \brief Check if the BoxedValue is null (i.e., contains an unset
     * value). \return True if the BoxedValue is null, false otherwise.
     */
    [[nodiscard]] auto isNull() const noexcept -> bool {
        std::shared_lock lock(m_mutex_);
        return !m_data_->mObj.has_value();
    }

    /*!
     * \brief Get the pointer to the contained data.
     * \return Pointer to the contained data.
     */
    [[nodiscard]] auto getPtr() const noexcept -> void* {
        std::shared_lock lock(m_mutex_);
        return const_cast<void*>(m_data_->mConstDataPtr);
    }

    /*!
     * \brief Try to cast the internal value to a specified type.
     * \tparam T The type to cast to.
     * \return An optional containing the casted value if successful,
     * std::nullopt otherwise.
     */
    template <typename T>
    [[nodiscard]] auto tryCast() const noexcept -> std::optional<T> {
        std::shared_lock lock(m_mutex_);
        try {
            if constexpr (std::is_reference_v<T>) {
                if (m_data_->mObj.type() ==
                    typeid(
                        std::reference_wrapper<std::remove_reference_t<T>>)) {
                    return std::any_cast<std::reference_wrapper<
                        std::remove_reference_t<T>>>(m_data_->mObj)
                        .get();
                }
            }
            if (m_data_->mObj.type() == typeid(std::reference_wrapper<T>)) {
                return std::any_cast<std::reference_wrapper<T>>(m_data_->mObj)
                    .get();
            }
            if (isConst() || isReadonly()) {
                using constT = std::add_const_t<T>;
                return std::any_cast<constT>(m_data_->mObj);
            }
            return std::any_cast<T>(m_data_->mObj);
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
        std::shared_lock lock(m_mutex_);
        try {
            if constexpr (std::is_reference_v<T>) {
                return m_data_->mObj.type() ==
                       typeid(
                           std::reference_wrapper<std::remove_reference_t<T>>);
            } else {
                std::any_cast<T>(m_data_->mObj);
                return true;
            }
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    /*!
     * \brief Get a debug string representation of the BoxedValue.
     * \return A string representing the BoxedValue.
     */
    [[nodiscard]] auto debugString() const -> std::string {
        std::ostringstream oss;
        oss << "BoxedValue<" << m_data_->mTypeInfo.name() << ">: ";
        std::shared_lock lock(m_mutex_);
        if (auto* intPtr = std::any_cast<int>(&m_data_->mObj)) {
            oss << *intPtr;
        } else if (auto* doublePtr = std::any_cast<double>(&m_data_->mObj)) {
            oss << *doublePtr;
        } else if (auto* strPtr = std::any_cast<std::string>(&m_data_->mObj)) {
            oss << *strPtr;
        } else {
            oss << "unknown type";
        }
        return oss.str();
    }

    /*!
     * \brief Visit the value in BoxedValue with a visitor
     * \tparam Visitor The type of visitor
     * \param visitor The visitor function object
     * \return The return value of the visitor
     *
     * The visitor should be a callable object that accepts arguments of any
     * type and returns a consistent type. If BoxedValue is empty or cannot be
     * converted to the required type, the visitor's fallback will be called.
     */
    template <typename Visitor>
    auto visit(Visitor&& visitor) const {
        using result_type =
            std::invoke_result_t<Visitor,
                                 int&>;  // Example type, actual call depends on
                                         // content

        std::shared_lock lock(m_mutex_);
        if (isUndef() || isNull()) {
            if constexpr (requires { visitor.fallback(); }) {
                return visitor.fallback();
            } else if constexpr (std::is_default_constructible_v<result_type>) {
                return result_type{};
            } else {
                throw std::bad_any_cast();
            }
        }

        // Try to call the visitor using type information
        return visitImpl(std::forward<Visitor>(visitor));
    }

    /*!
     * \brief Visit and possibly modify the value in BoxedValue with a visitor
     * \tparam Visitor The type of visitor
     * \param visitor The visitor function object
     * \return The return value of the visitor
     *
     * Non-const version, allows modification of internal value
     */
    template <typename Visitor>
    auto visit(Visitor&& visitor) {
        using result_type = std::invoke_result_t<Visitor, int&>;

        std::unique_lock lock(m_mutex_);
        if (isUndef() || isNull() || isReadonly()) {
            if constexpr (requires { visitor.fallback(); }) {
                return visitor.fallback();
            } else if constexpr (std::is_default_constructible_v<result_type>) {
                return result_type{};
            } else {
                throw std::bad_any_cast();
            }
        }

        auto result = visitImpl(std::forward<Visitor>(visitor));
        m_data_->mModificationTime = std::chrono::system_clock::now();
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
        using result_type = std::invoke_result_t<Visitor, int&>;

#define VISIT_TYPE(Type)                                                \
    if (m_data_->mObj.type() == typeid(Type)) {                         \
        if (isConst() || isReadonly()) {                                \
            return visitor(*std::any_cast<const Type>(&m_data_->mObj)); \
        } else {                                                        \
            return visitor(*std::any_cast<Type>(&m_data_->mObj));       \
        }                                                               \
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

        // 变体类型
        VISIT_TYPE(VariantTypes)

#define VISIT_REF_TYPE(Type)                                                   \
    if (m_data_->mObj.type() == typeid(std::reference_wrapper<Type>)) {        \
        return visitor(                                                        \
            std::any_cast<std::reference_wrapper<Type>>(m_data_->mObj).get()); \
    }                                                                          \
    if (m_data_->mObj.type() == typeid(std::reference_wrapper<const Type>)) {  \
        return visitor(                                                        \
            std::any_cast<std::reference_wrapper<const Type>>(m_data_->mObj)   \
                .get());                                                       \
    }

        // 基本数值类型引用
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

        // 字符串类型引用
        VISIT_REF_TYPE(std::string)
        VISIT_REF_TYPE(std::wstring)
        VISIT_REF_TYPE(std::u16string)
        VISIT_REF_TYPE(std::u32string)
        VISIT_REF_TYPE(std::string_view)
        VISIT_REF_TYPE(std::wstring_view)
        VISIT_REF_TYPE(std::u16string_view)
        VISIT_REF_TYPE(std::u32string_view)

        // 常用容器类型引用
        VISIT_REF_TYPE(std::vector<int>)
        VISIT_REF_TYPE(std::vector<double>)
        VISIT_REF_TYPE(std::vector<std::string>)

        // 使用类型别名
        VISIT_REF_TYPE(MapStringInt)
        VISIT_REF_TYPE(MapStringString)
        VISIT_REF_TYPE(UMapStringInt)
        VISIT_REF_TYPE(UMapStringString)

#undef VISIT_TYPE
#undef VISIT_REF_TYPE

        // 处理未能匹配到的类型
        if constexpr (requires { visitor.fallback(); }) {
            return visitor.fallback();
        } else if constexpr (std::is_default_constructible_v<result_type>) {
            return result_type{};
        } else {
            throw std::bad_any_cast();
        }
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

inline auto voidVar() -> BoxedValue { return {}; }

template <typename T>
auto varWithDesc(T&& value, std::string_view description) -> BoxedValue {
    auto result = var(std::forward<T>(value));
    result.setAttr("description", BoxedValue(std::string(description)));
    return result;
}

/*!
 * \brief Helper function to create a BoxedValue instance with additional
 * options. \tparam T The type of the value. \param value The value to be
 * encapsulated. \param is_return_value Indicates if the value is a return
 * value. \param readonly Indicates if the value is read-only. \return A
 * BoxedValue instance.
 */
template <typename T>
auto makeBoxedValue(T&& value, bool is_return_value = false,
                    bool readonly = false) -> BoxedValue {
    if constexpr (std::is_reference_v<T>) {
        return BoxedValue(std::ref(value), is_return_value, readonly);
    } else {
        return BoxedValue(std::forward<T>(value), is_return_value, readonly);
    }
}

}  // namespace atom::meta

#endif  // ATOM_META_ANY_HPP
