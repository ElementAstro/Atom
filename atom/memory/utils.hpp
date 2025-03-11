#ifndef ATOM_MEMORY_UTILS_HPP
#define ATOM_MEMORY_UTILS_HPP

#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace atom::memory {
struct Config {
    static constexpr size_t DefaultAlignment = alignof(std::max_align_t);
    static constexpr bool EnableMemoryTracking =
#ifdef ATOM_MEMORY_TRACKING
        true;
#else
        false;
#endif
};

template <typename T, typename... Args>
struct IsConstructible {
    static constexpr bool value = std::is_constructible_v<T, Args...>;
};

template <typename T, typename... Args>
using ConstructorArguments_t =
    std::enable_if_t<IsConstructible<T, Args...>::value, std::shared_ptr<T>>;

template <typename T, typename... Args>
using UniqueConstructorArguments_t =
    std::enable_if_t<IsConstructible<T, Args...>::value, std::unique_ptr<T>>;

/**
 * @brief Creates a std::shared_ptr object and validates constructor arguments
 * @return shared_ptr to type T
 */
template <typename T, typename... Args>
auto makeShared(Args&&... args) -> ConstructorArguments_t<T, Args...> {
    if constexpr (IsConstructible<T, Args...>::value) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    } else {
        static_assert(IsConstructible<T, Args...>::value,
                      "Arguments do not match any constructor of the type T");
        return nullptr;
    }
}

/**
 * @brief Creates a std::unique_ptr object and validates constructor arguments
 * @return unique_ptr to type T
 */
template <typename T, typename... Args>
auto makeUnique(Args&&... args) -> UniqueConstructorArguments_t<T, Args...> {
    if constexpr (IsConstructible<T, Args...>::value) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    } else {
        static_assert(IsConstructible<T, Args...>::value,
                      "Arguments do not match any constructor of the type T");
        return nullptr;
    }
}

/**
 * @brief Creates a shared_ptr with custom deleter
 */
template <typename T, typename Deleter, typename... Args>
auto makeSharedWithDeleter(Deleter&& deleter, Args&&... args)
    -> std::enable_if_t<IsConstructible<T, Args...>::value,
                        std::shared_ptr<T>> {
    if constexpr (IsConstructible<T, Args...>::value) {
        return std::shared_ptr<T>(new T(std::forward<Args>(args)...),
                                  std::forward<Deleter>(deleter));
    } else {
        static_assert(IsConstructible<T, Args...>::value,
                      "Arguments do not match any constructor of the type T");
        return nullptr;
    }
}

/**
 * @brief Creates a unique_ptr with custom deleter
 */
template <typename T, typename Deleter, typename... Args>
auto makeUniqueWithDeleter(Deleter&& deleter, Args&&... args)
    -> std::enable_if_t<IsConstructible<T, Args...>::value,
                        std::unique_ptr<T, std::decay_t<Deleter>>> {
    if constexpr (IsConstructible<T, Args...>::value) {
        return std::unique_ptr<T, std::decay_t<Deleter>>(
            new T(std::forward<Args>(args)...), std::forward<Deleter>(deleter));
    } else {
        static_assert(IsConstructible<T, Args...>::value,
                      "Arguments do not match any constructor of the type T");
        return nullptr;
    }
}

/**
 * @brief Creates an array type shared_ptr
 */
template <typename T>
std::shared_ptr<T[]> makeSharedArray(size_t size) {
    return std::shared_ptr<T[]>(new T[size]());
}

/**
 * @brief Creates an array type unique_ptr
 */
template <typename T>
std::unique_ptr<T[]> makeUniqueArray(size_t size) {
    return std::make_unique<T[]>(size);
}

/**
 * @brief Thread-safe singleton template
 */
template <typename T>
class ThreadSafeSingleton {
public:
    static std::shared_ptr<T> getInstance() {
        std::shared_ptr<T> instance = instance_weak_.lock();
        if (!instance) {
            std::lock_guard<std::mutex> lock(mutex_);
            instance = instance_weak_.lock();
            if (!instance) {
                instance = std::make_shared<T>();
                instance_weak_ = instance;
            }
        }
        return instance;
    }

private:
    static std::weak_ptr<T> instance_weak_;
    static std::mutex mutex_;
};

template <typename T>
std::weak_ptr<T> ThreadSafeSingleton<T>::instance_weak_;

template <typename T>
std::mutex ThreadSafeSingleton<T>::mutex_;

/**
 * @brief Check weak reference and lock it
 * @return If weak reference is valid, returns the locked shared_ptr, otherwise
 * returns nullptr
 */
template <typename T>
std::shared_ptr<T> lockWeak(const std::weak_ptr<T>& weak) {
    return weak.lock();
}

/**
 * @brief Check weak reference and lock it, create new object if invalid
 */
template <typename T, typename... Args>
std::shared_ptr<T> lockWeakOrCreate(std::weak_ptr<T>& weak, Args&&... args) {
    auto ptr = weak.lock();
    if (!ptr) {
        ptr = std::make_shared<T>(std::forward<Args>(args)...);
        weak = ptr;
    }
    return ptr;
}

}  // namespace atom::memory

#endif  // ATOM_MEMORY_UTILS_HPP