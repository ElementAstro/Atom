#ifndef ATOM_META_ANY_HPP
#define ATOM_META_ANY_HPP

#include <array>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/function/concept.hpp"
#include "atom/macro.hpp"

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
    // VTable for type erasure
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

    // Default toString implementation for different types
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
            std::ostringstream oss;
            oss << "Object of type " << typeid(T).name();
            return oss.str();
        }
    }

    // Default invoke implementation
    template <typename T>
    static void defaultInvoke(const void* ptr,
                              const std::function<void(const void*)>& func) {
        func(ptr);
    }

    // Default foreach implementation
    template <typename T>
    static void defaultForeach(const void* ptr,
                               const std::function<void(const Any&)>& func) {
        if constexpr (Iterable<T>) {
            for (const auto& item : *static_cast<const T*>(ptr)) {
                func(Any(item));
            }
        } else {
            THROW_INVALID_ARGUMENT("Type is not iterable");
        }
    }

    // Default equals implementation
    template <typename T>
    static bool defaultEquals(const void* lhs, const void* rhs) noexcept {
        if constexpr (std::equality_comparable<T>) {
            return *static_cast<const T*>(lhs) == *static_cast<const T*>(rhs);
        } else {
            return lhs == rhs;  // Fallback to pointer comparison
        }
    }

    // Default hash implementation
    template <typename T>
    static size_t defaultHash(const void* ptr) noexcept {
        if constexpr (requires(const T& t) { std::hash<T>{}(t); }) {
            return std::hash<T>{}(*static_cast<const T*>(ptr));
        } else {
            // Fallback to pointer value as hash
            return reinterpret_cast<std::uintptr_t>(ptr);
        }
    }

    // Static VTable for each type
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

    // Size of small objects that can be stored inline
    static constexpr size_t kSmallObjectSize = 3 * sizeof(void*);

    // Storage for the object
    union {
        alignas(std::max_align_t) std::array<char, kSmallObjectSize> storage;
        void* ptr;
    };

    // Pointer to the VTable
    const VTable* vptr_ = nullptr;

    // Whether the object is stored inline
    bool is_small_ = true;

    // Check if a type can be stored inline
    template <typename T>
    static constexpr bool kIsSmallObject =
        sizeof(T) <= kSmallObjectSize &&
        std::is_nothrow_move_constructible_v<T> &&
        alignof(T) <= alignof(std::max_align_t);

    // Get the pointer to the stored object
    auto getPtr() noexcept -> void* {
        return is_small_ ? static_cast<void*>(storage.data()) : ptr;
    }

    // Get the const pointer to the stored object
    [[nodiscard]] auto getPtr() const noexcept -> const void* {
        return is_small_ ? static_cast<const void*>(storage.data()) : ptr;
    }

    // Cast the stored object to the specified type
    template <typename T>
    auto as() noexcept -> T* {
        return static_cast<T*>(getPtr());
    }

    // Cast the stored object to the specified type (const version)
    template <typename T>
    [[nodiscard]] auto as() const noexcept -> const T* {
        return static_cast<const T*>(getPtr());
    }

    // Allocate aligned memory
    template <typename T = void>
    static auto allocateAligned(size_t size, size_t alignment) -> T* {
        // Ensure alignment is a power of 2
        if (alignment & (alignment - 1)) {
            alignment = std::bit_ceil(alignment);
        }

        // Ensure alignment is at least sizeof(void*)
        alignment = std::max(alignment, sizeof(void*));

        void* ptr = std::aligned_alloc(alignment, size);
        if (!ptr) {
            throw std::bad_alloc();
        }

        return static_cast<T*>(ptr);
    }

    // Reset the object, freeing resources
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
                    // Small object: copy directly to storage
                    std::memcpy(storage.data(), other.getPtr(), vptr_->size());
                } else {
                    // Large object: allocate memory and copy
                    ptr = std::malloc(vptr_->size());
                    if (ptr == nullptr) {
                        throw std::bad_alloc();
                    }
                    vptr_->copy(other.getPtr(), ptr);
                }
            } catch (...) {
                // Clean up on error
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
                // For small objects, use move constructor via vtable
                vptr_->move(other.storage.data(), storage.data());
            } else {
                // For large objects, transfer ownership of the pointer
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
                // Store small objects inline
                alignas(std::max_align_t) char* addr = storage.data();
                new (addr) ValueType(std::forward<T>(value));
                is_small_ = true;
            } else {
                // Allocate memory for large objects
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

            // Set up vtable
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
                    // Use move operation from vtable for small objects
                    vptr_->move(other.storage.data(), storage.data());
                } else {
                    // Transfer pointer ownership for large objects
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

        // Quick swap for empty objects
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

        // Full swap needed
        Any temp(std::move(*this));
        *this = std::move(other);
        other = std::move(temp);
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_ANY_HPP
