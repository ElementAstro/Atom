#ifndef ATOM_META_ANY_HPP
#define ATOM_META_ANY_HPP

#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"
#include "atom/meta/concept.hpp"

namespace atom::meta {

/**
 * @brief A type-safe container for values of any type.
 *
 * The Any class provides a type-safe container for single values of any type.
 * It uses small buffer optimization for small objects and dynamically allocates
 * memory for larger objects.
 */
class Any {
#ifdef TEST_F
public:
#endif
    struct ATOM_ALIGNAS(64) VTable {
        void (*destroy)(void*) noexcept;
        void (*copy)(const void*, void*);
        void (*move)(void*, void*) noexcept;
        const std::type_info& (*type)() noexcept;
        std::string (*toString)(const void*);
        size_t (*size)() noexcept;
        void (*invoke)(const void*, const std::function<void(const void*)>&);
        void (*foreach)(const void*, const std::function<void(const Any&)>&);
        bool (*equals)(const void*, const void*) noexcept;
        size_t (*hash)(const void*) noexcept;
    };

    template <typename T>
    static auto defaultToString(const void* ptr) -> std::string {
        if constexpr (std::is_same_v<T, std::string>) {
            return *static_cast<const std::string*>(ptr);
        } else if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(*static_cast<const T*>(ptr));
        } else if constexpr (requires(const T& t, std::ostream& os) {
                                 os << t;
                             }) {
            std::ostringstream oss;
            oss << *static_cast<const T*>(ptr);
            return oss.str();
        } else {
            return "Object of type " + std::string(typeid(T).name());
        }
    }

    template <typename T>
    static void defaultInvoke(const void* ptr,
                              const std::function<void(const void*)>& func) {
        func(ptr);
    }

    template <typename T>
    static void defaultForeach(const void* ptr,
                               const std::function<void(const Any&)>& func) {
        if constexpr (Iterable<T>) {
            const auto& container = *static_cast<const T*>(ptr);
            for (const auto& item : container) {
                func(Any(item));
            }
        } else {
            THROW_INVALID_ARGUMENT("Type is not iterable");
        }
    }

    template <typename T>
    static bool defaultEquals(const void* lhs, const void* rhs) noexcept {
        if constexpr (std::equality_comparable<T>) {
            return *static_cast<const T*>(lhs) == *static_cast<const T*>(rhs);
        } else {
            return lhs == rhs;
        }
    }

    template <typename T>
    static size_t defaultHash(const void* ptr) noexcept {
        if constexpr (requires(const T& t) { std::hash<T>{}(t); }) {
            return std::hash<T>{}(*static_cast<const T*>(ptr));
        } else {
            return reinterpret_cast<std::uintptr_t>(ptr);
        }
    }

    template <typename T>
    static constexpr VTable K_V_TABLE = {
        [](void* ptr) noexcept {
            if (ptr)
                static_cast<T*>(ptr)->~T();
        },
        [](const void* src, void* dst) {
            if (src && dst)
                new (dst) T(*static_cast<const T*>(src));
        },
        [](void* src, void* dst) noexcept {
            if (src && dst)
                new (dst) T(std::move(*static_cast<T*>(src)));
        },
        []() noexcept -> const std::type_info& { return typeid(T); },
        &defaultToString<T>,
        []() noexcept -> size_t { return sizeof(T); },
        &defaultInvoke<T>,
        &defaultForeach<T>,
        &defaultEquals<T>,
        &defaultHash<T>};

    static constexpr size_t kSmallObjectSize = 3 * sizeof(void*);

    union {
        alignas(std::max_align_t) std::array<char, kSmallObjectSize> storage;
        void* ptr;
    };

    const VTable* vptr_ = nullptr;
    bool is_small_ = true;

    template <typename T>
    static constexpr bool kIsSmallObject =
        sizeof(T) <= kSmallObjectSize &&
        std::is_nothrow_move_constructible_v<T> &&
        alignof(T) <= alignof(std::max_align_t);

    auto getPtr() noexcept -> void* {
        return is_small_ ? static_cast<void*>(storage.data()) : ptr;
    }

    [[nodiscard]] auto getPtr() const noexcept -> const void* {
        return is_small_ ? static_cast<const void*>(storage.data()) : ptr;
    }

    template <typename T>
    auto as() noexcept -> T* {
        return static_cast<T*>(getPtr());
    }

    template <typename T>
    [[nodiscard]] auto as() const noexcept -> const T* {
        return static_cast<const T*>(getPtr());
    }

    template <typename T = void>
    static auto allocateAligned(size_t size, size_t alignment) -> T* {
        if (alignment & (alignment - 1)) {
            alignment = std::bit_ceil(alignment);
        }

        alignment = std::max(alignment, sizeof(void*));

#ifdef _WIN32
        void* ptr = _aligned_malloc(size, alignment);
#else
        void* ptr = std::aligned_alloc(alignment, size);
#endif
        if (!ptr) {
            throw std::bad_alloc();
        }

        return static_cast<T*>(ptr);
    }

public:
    /**
     * @brief Default constructor creates an empty Any.
     */
    Any() noexcept : storage{} {}

    /**
     * @brief Copy constructor.
     * @param other The Any object to copy from.
     * @throws std::bad_alloc If memory allocation fails.
     */
    Any(const Any& other) : vptr_(other.vptr_), is_small_(other.is_small_) {
        if (vptr_ != nullptr) {
            try {
                if (is_small_) {
                    std::memcpy(storage.data(), other.getPtr(), vptr_->size());
                } else {
                    ptr = std::malloc(vptr_->size());
                    if (ptr == nullptr) {
                        throw std::bad_alloc();
                    }
                    vptr_->copy(other.getPtr(), ptr);
                }
            } catch (...) {
                if (!is_small_ && ptr != nullptr) {
                    std::free(ptr);
                    ptr = nullptr;
                }
                vptr_ = nullptr;
                is_small_ = true;
                throw;
            }
        }
    }

    /**
     * @brief Move constructor.
     * @param other The Any object to move from.
     */
    Any(Any&& other) noexcept : vptr_(other.vptr_), is_small_(other.is_small_) {
        if (vptr_ != nullptr) {
            if (is_small_) {
                vptr_->move(other.storage.data(), storage.data());
            } else {
                ptr = other.ptr;
                other.ptr = nullptr;
            }
            other.vptr_ = nullptr;
            other.is_small_ = true;
        }
    }

    /**
     * @brief Constructor from any value.
     * @param value The value to store.
     * @throws std::bad_alloc If memory allocation fails.
     */
    template <typename T, typename = std::enable_if_t<
                              !std::is_same_v<std::decay_t<T>, Any>>>
    explicit Any(T&& value) {
        using ValueType = std::remove_cvref_t<T>;

        try {
            if constexpr (kIsSmallObject<ValueType>) {
                alignas(std::max_align_t) char* addr = storage.data();
                new (addr) ValueType(std::forward<T>(value));
                is_small_ = true;
            } else {
                auto temp = allocateAligned<ValueType>(
                    sizeof(ValueType),
                    std::max(alignof(ValueType), alignof(std::max_align_t)));

                try {
                    new (temp) ValueType(std::forward<T>(value));
                    ptr = temp;
                    is_small_ = false;
                } catch (...) {
                    std::free(temp);
                    throw;
                }
            }

            vptr_ = &K_V_TABLE<ValueType>;
        } catch (...) {
            reset();
            throw;
        }
    }

    /**
     * @brief Destructor.
     */
    ~Any() noexcept { reset(); }

    /**
     * @brief Copy assignment operator.
     * @param other The Any object to copy from.
     * @return Reference to this.
     */
    auto operator=(const Any& other) -> Any& {
        if (this != &other) {
            Any temp(other);
            swap(temp);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     * @param other The Any object to move from.
     * @return Reference to this.
     */
    auto operator=(Any&& other) noexcept -> Any& {
        if (this != &other) {
            reset();
            vptr_ = other.vptr_;
            is_small_ = other.is_small_;

            if (vptr_ != nullptr) {
                if (is_small_) {
                    vptr_->move(other.storage.data(), storage.data());
                } else {
                    ptr = other.ptr;
                    other.ptr = nullptr;
                }
                other.vptr_ = nullptr;
                other.is_small_ = true;
            }
        }
        return *this;
    }

    /**
     * @brief Assignment operator from any value.
     * @param value The value to store.
     * @return Reference to this.
     */
    template <typename T, typename = std::enable_if_t<
                              !std::is_same_v<std::decay_t<T>, Any>>>
    auto operator=(T&& value) -> Any& {
        *this = Any(std::forward<T>(value));
        return *this;
    }

    /**
     * @brief Swap with another Any.
     * @param other The Any to swap with.
     */
    void swap(Any& other) noexcept {
        if (this == &other)
            return;

        if (!vptr_ && !other.vptr_)
            return;
        if (!vptr_) {
            *this = std::move(other);
            return;
        }
        if (!other.vptr_) {
            other = std::move(*this);
            return;
        }

        Any temp(std::move(*this));
        *this = std::move(other);
        other = std::move(temp);
    }

    /**
     * @brief Check if the Any object is empty (holds no value).
     * @return True if empty, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept { return vptr_ == nullptr; }

    /**
     * @brief Get the type information of the contained value.
     * @return The type_info of the contained value or typeid(void) if empty.
     */
    [[nodiscard]] const std::type_info& type() const noexcept {
        return vptr_ ? vptr_->type() : typeid(void);
    }

    /**
     * @brief Convert the contained value to a string representation.
     * @return String representation of the value.
     */
    [[nodiscard]] std::string toString() const {
        if (empty()) {
            return "[empty]";
        }
        return vptr_->toString(getPtr());
    }

    /**
     * @brief Check if the Any contains a value of the specified type.
     * @tparam T The type to check for.
     * @return True if the contained value is of type T, false otherwise.
     */
    template <typename T>
    [[nodiscard]] bool is() const noexcept {
        return !empty() && vptr_->type() == typeid(T);
    }

    /**
     * @brief Cast the contained value to the specified type with type checking.
     * @tparam T The type to cast to.
     * @return The value cast to type T.
     * @throws std::bad_cast if the contained value is not of type T.
     */
    template <typename T>
    [[nodiscard]] T cast() const {
        if (!is<T>()) {
            throw std::bad_cast();
        }
        return *static_cast<const T*>(getPtr());
    }

    /**
     * @brief Cast the contained value to the specified type without type
     * checking.
     * @tparam T The type to cast to.
     * @return The value cast to type T.
     * @warning This method does not perform type checking and may cause
     * undefined behavior.
     */
    template <typename T>
    [[nodiscard]] T unsafeCast() const noexcept {
        return *static_cast<const T*>(getPtr());
    }

    /**
     * @brief Invoke a function with the contained value.
     * @param func The function to invoke.
     * @throws std::runtime_error if Any is empty.
     */
    void invoke(const std::function<void(const void*)>& func) const {
        if (empty()) {
            throw std::runtime_error("Cannot invoke function on empty Any");
        }
        vptr_->invoke(getPtr(), func);
    }

    /**
     * @brief Iterate over the contained value if it's iterable.
     * @param func The function to invoke for each element.
     * @throws std::runtime_error if Any is empty.
     * @throws std::invalid_argument if the contained value is not iterable.
     */
    void foreach (const std::function<void(const Any&)>& func) const {
        if (empty()) {
            throw std::runtime_error("Cannot iterate over empty Any");
        }
        vptr_->foreach (getPtr(), func);
    }

    /**
     * @brief Compare two Any objects for equality.
     * @param other The other Any object to compare with.
     * @return True if the objects are equal, false otherwise.
     */
    bool operator==(const Any& other) const noexcept {
        if (empty() && other.empty())
            return true;
        if (empty() || other.empty())
            return false;
        if (vptr_->type() != other.vptr_->type())
            return false;
        return vptr_->equals(getPtr(), other.getPtr());
    }

    /**
     * @brief Compare two Any objects for inequality.
     * @param other The other Any object to compare with.
     * @return True if the objects are not equal, false otherwise.
     */
    bool operator!=(const Any& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Get the hash value of the contained value.
     * @return The hash value.
     */
    [[nodiscard]] size_t hash() const noexcept {
        if (empty())
            return 0;
        return vptr_->hash(getPtr());
    }

    /**
     * @brief Check if the object is stored using small object optimization.
     * @return True if stored inline, false if dynamically allocated.
     */
    [[nodiscard]] bool isSmallObject() const noexcept { return is_small_; }

    /**
     * @brief Reset the object, freeing resources.
     */
    void reset() noexcept {
        if (vptr_ != nullptr) {
            vptr_->destroy(getPtr());

            if (!is_small_ && ptr != nullptr) {
                std::free(ptr);
                ptr = nullptr;
            }

            vptr_ = nullptr;
            is_small_ = true;
        }
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_ANY_HPP
