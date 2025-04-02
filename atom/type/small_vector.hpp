/*
 * small_vector.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-17

Description: A Small Vector Implementation with optional Boost support

**************************************************/

#ifndef ATOM_TYPE_SMALL_VECTOR_HPP
#define ATOM_TYPE_SMALL_VECTOR_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/copy.hpp>
#include <boost/core/noinit_adaptor.hpp>
#include <boost/move/move.hpp>
#include <boost/range/algorithm.hpp>
#endif

#if defined(__cpp_lib_hardware_interference_size) && \
    !defined(ATOM_DISABLE_CACHE_OPTIMIZATION)
constexpr std::size_t ATOM_CACHELINE_SIZE =
    std::hardware_destructive_interference_size;
#else
constexpr std::size_t ATOM_CACHELINE_SIZE = 64;  // Common cache line size
#endif

#if defined(__cpp_lib_parallel_algorithm) && !defined(ATOM_DISABLE_PARALLEL)
#include <execution>
#define ATOM_PARALLEL_COPY(src, dest, size) \
    std::copy(std::execution::par_unseq, src, src + size, dest)
#else
#define ATOM_PARALLEL_COPY(src, dest, size) std::copy(src, src + size, dest)
#endif

/**
 * @brief A small vector implementation with small buffer optimization
 *
 * @tparam T Type of elements
 * @tparam N Size of the internal buffer
 * @tparam Allocator Allocator type (default: std::allocator<T>)
 */
template <typename T, std::size_t N, typename Allocator = std::allocator<T>>
class SmallVector {
public:
    using value_type = T;
    using allocator_type = Allocator;
    using allocator_traits = std::allocator_traits<allocator_type>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename allocator_traits::pointer;
    using const_pointer = typename allocator_traits::const_pointer;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr bool is_trivially_relocatable =
        std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

    // Constructors
    SmallVector() noexcept(noexcept(allocator_type())) : alloc_{} {
        initializeFromEmpty();
    }

    explicit SmallVector(const allocator_type& alloc) noexcept : alloc_{alloc} {
        initializeFromEmpty();
    }

    SmallVector(size_type count, const T& value,
                const allocator_type& alloc = allocator_type())
        : alloc_{alloc} {
        try {
            initializeFromEmpty();
            assign(count, value);
        } catch (...) {
            deallocate();
            throw;
        }
    }

    explicit SmallVector(size_type count,
                         const allocator_type& alloc = allocator_type())
        : alloc_{alloc} {
        try {
            initializeFromEmpty();
            resize(count);
        } catch (...) {
            deallocate();
            throw;
        }
    }

    template <typename InputIt,
              typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
    SmallVector(InputIt first, InputIt last,
                const allocator_type& alloc = allocator_type())
        : alloc_{alloc} {
        try {
            initializeFromEmpty();
            assign(first, last);
        } catch (...) {
            deallocate();
            throw;
        }
    }

    // 添加在类定义中其他构造函数后面:

    // 支持从不同 buffer size 的 SmallVector 进行复制构造
    template <std::size_t M>
    SmallVector(const SmallVector<T, M, Allocator>& other)
        : alloc_{allocator_traits::select_on_container_copy_construction(
              other.get_allocator())} {
        try {
            initializeFromEmpty();
            assign(other.begin(), other.end());
        } catch (...) {
            deallocate();
            throw;
        }
    }

    template <std::size_t M>
    SmallVector(SmallVector<T, M, Allocator>&& other) noexcept
        : alloc_{std::move(other.get_allocator())} {
        initializeFromEmpty();
        assign(std::make_move_iterator(other.begin()),
               std::make_move_iterator(other.end()));
        other.clear();
    }

    template <std::size_t M>
    auto operator=(const SmallVector<T, M, Allocator>& other) -> SmallVector& {
        if (static_cast<const void*>(this) !=
            static_cast<const void*>(&other)) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    template <std::size_t M>
    auto operator=(SmallVector<T, M, Allocator>&& other) noexcept
        -> SmallVector& {
        if (static_cast<const void*>(this) !=
            static_cast<const void*>(&other)) {
            clear();
            assign(std::make_move_iterator(other.begin()),
                   std::make_move_iterator(other.end()));
            other.clear();
        }
        return *this;
    }

    SmallVector(const SmallVector& other)
        : alloc_{allocator_traits::select_on_container_copy_construction(
              other.alloc_)} {
        try {
            initializeFromEmpty();
            assign(other.begin(), other.end());
        } catch (...) {
            deallocate();
            throw;
        }
    }

    SmallVector(const SmallVector& other, const allocator_type& alloc)
        : alloc_{alloc} {
        try {
            initializeFromEmpty();
            assign(other.begin(), other.end());
        } catch (...) {
            deallocate();
            throw;
        }
    }

    SmallVector(SmallVector&& other) noexcept
        : alloc_{std::move(other.alloc_)} {
        initializeFromEmpty();
        moveFrom(std::move(other));
    }

    SmallVector(SmallVector&& other, const allocator_type& alloc)
        : alloc_{alloc} {
        initializeFromEmpty();

        if (alloc_ == other.alloc_) {
            // Fast path - take ownership of other's memory
            moveFrom(std::move(other));
        } else {
            // Slow path - must copy elements
            try {
                assign(std::make_move_iterator(other.begin()),
                       std::make_move_iterator(other.end()));
            } catch (...) {
                deallocate();
                throw;
            }
        }
    }

    SmallVector(std::initializer_list<T> init,
                const allocator_type& alloc = allocator_type())
        : alloc_{alloc} {
        try {
            initializeFromEmpty();
            assign(init);
        } catch (...) {
            deallocate();
            throw;
        }
    }

    // Destructor
    ~SmallVector() {
        clear();
        deallocate();
    }

    // Assignment operators
    auto operator=(const SmallVector& other) -> SmallVector& {
        if (this != &other) {
            if constexpr (allocator_traits::
                              propagate_on_container_copy_assignment::value) {
                // Must handle potential change of allocator
                SmallVector tmp(other);  // Copy using other's allocator
                swap(tmp);  // Swap with tmp including the allocator
            } else {
                // Keep our allocator
                assign(other.begin(), other.end());
            }
        }
        return *this;
    }

    auto operator=(SmallVector&& other) noexcept(
        allocator_traits::propagate_on_container_move_assignment::value ||
        allocator_traits::is_always_equal::value) -> SmallVector& {
        if (this != &other) {
            if constexpr (allocator_traits::
                              propagate_on_container_move_assignment::value) {
                // Take other's allocator and swap
                clear();
                deallocate();
                alloc_ = std::move(other.alloc_);
                moveFrom(std::move(other));
            } else if (alloc_ == other.alloc_) {
                // Same allocator, can move efficiently
                clear();
                deallocate();
                moveFrom(std::move(other));
            } else {
                // Different allocators, must move individual elements
                assign(std::make_move_iterator(other.begin()),
                       std::make_move_iterator(other.end()));
            }
        }
        return *this;
    }

    auto operator=(std::initializer_list<T> init) -> SmallVector& {
        assign(init);
        return *this;
    }

    // Assign methods
    void assign(size_type count, const T& value) {
        try {
            if (count > capacity()) {
                // Need to reallocate
                SmallVector tmp(count, value, alloc_);
                swap(tmp);
            } else {
                // Can reuse existing storage
                clear();
                constructElements(begin(), count, value);
                size_ = count;
            }
        } catch (...) {
            // Keep vector in a valid state
            clear();
            throw;
        }
    }

    template <typename InputIt,
              typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
    void assign(InputIt first, InputIt last) {
        try {
            const size_type count = std::distance(first, last);

            if (count > capacity()) {
                // Need to reallocate
                SmallVector tmp(first, last, alloc_);
                swap(tmp);
            } else {
                // Can reuse existing storage
                clear();
                constructRange(begin(), first, last);
                size_ = count;
            }
        } catch (...) {
            // Keep vector in a valid state
            clear();
            throw;
        }
    }

    void assign(std::initializer_list<T> init) {
        assign(init.begin(), init.end());
    }

    // Element access
    auto at(size_type pos) -> reference {
        if (pos >= size()) {
            throw std::out_of_range("SmallVector::at: index out of range");
        }
        return (*this)[pos];
    }

    auto at(size_type pos) const -> const_reference {
        if (pos >= size()) {
            throw std::out_of_range("SmallVector::at: index out of range");
        }
        return (*this)[pos];
    }

    auto operator[](size_type pos) -> reference {
        assert(pos < size() && "Index out of bounds");
        return *(begin() + pos);
    }

    auto operator[](size_type pos) const -> const_reference {
        assert(pos < size() && "Index out of bounds");
        return *(begin() + pos);
    }

    auto front() -> reference {
        assert(!empty() && "Cannot call front() on empty vector");
        return *begin();
    }

    auto front() const -> const_reference {
        assert(!empty() && "Cannot call front() on empty vector");
        return *begin();
    }

    auto back() -> reference {
        assert(!empty() && "Cannot call back() on empty vector");
        return *(end() - 1);
    }

    auto back() const -> const_reference {
        assert(!empty() && "Cannot call back() on empty vector");
        return *(end() - 1);
    }

    auto data() noexcept -> T* { return begin(); }
    auto data() const noexcept -> const T* { return begin(); }

    // Iterators
    auto begin() noexcept -> iterator { return data_; }
    auto begin() const noexcept -> const_iterator { return data_; }
    auto cbegin() const noexcept -> const_iterator { return begin(); }

    auto end() noexcept -> iterator { return begin() + size(); }
    auto end() const noexcept -> const_iterator { return begin() + size(); }
    auto cend() const noexcept -> const_iterator { return end(); }

    auto rbegin() noexcept -> reverse_iterator {
        return reverse_iterator(end());
    }
    auto rbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }
    auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }

    auto rend() noexcept -> reverse_iterator {
        return reverse_iterator(begin());
    }
    auto rend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }
    auto crend() const noexcept -> const_reverse_iterator { return rend(); }

    // Capacity
    [[nodiscard]] auto empty() const noexcept -> bool { return size() == 0; }
    [[nodiscard]] auto size() const noexcept -> size_type { return size_; }

    [[nodiscard]] auto maxSize() const noexcept -> size_type {
        return std::min<size_type>(allocator_traits::max_size(alloc_),
                                   std::numeric_limits<difference_type>::max());
    }

    void reserve(size_type new_cap) {
        if (new_cap <= capacity()) {
            return;
        }

        if (new_cap > maxSize()) {
            throw std::length_error(
                "SmallVector::reserve: capacity exceeded maximum size");
        }

        try {
            reallocate(new_cap);
        } catch (...) {
            // Keep vector in a valid state
            throw;
        }
    }

    [[nodiscard]] auto capacity() const noexcept -> size_type {
        return capacity_;
    }

    void shrinkToFit() {
        if (size() == capacity() || isUsingInlineStorage()) {
            return;
        }

        if (size() <= N) {
            // Move back to inline storage
            pointer newData = reinterpret_cast<pointer>(static_buffer_.data());
            try {
                relocateElements(begin(), end(), newData);
                deallocate();
                data_ = newData;
                capacity_ = N;
            } catch (...) {
                // Original data still intact
                throw;
            }
        } else if (size() < capacity()) {
            // Reduce heap allocation size
            try {
                reallocate(size());
            } catch (...) {
                // Keep vector in a valid state
                throw;
            }
        }
    }

    // Modifiers
    void clear() noexcept {
        destroyElements(begin(), end());
        size_ = 0;
    }

    auto insert(const_iterator pos, const T& value) -> iterator {
        return emplace(pos, value);
    }

    auto insert(const_iterator pos, T&& value) -> iterator {
        return emplace(pos, std::move(value));
    }

    auto insert(const_iterator pos, size_type count, const T& value)
        -> iterator {
        if (count == 0) {
            return const_cast<iterator>(pos);
        }

        const size_type index = pos - begin();

        try {
            if (size() + count > capacity()) {
                // Need to reallocate
                const size_type new_cap = growthSize(size() + count);

                // Allocate new buffer
                pointer new_data = allocateMemory(new_cap);

                // Move elements before insertion point
                relocateElements(begin(), begin() + index, new_data);

                // Insert new elements
                constructElements(new_data + index, count, value);

                // Move elements after insertion point
                relocateElements(begin() + index, end(),
                                 new_data + index + count);

                // Update size before cleanup to ensure proper destruction in
                // case of exception
                const size_type new_size = size() + count;

                // Clean up old storage
                destroyElements(begin(), end());
                deallocate();

                // Update members
                data_ = new_data;
                size_ = new_size;
                capacity_ = new_cap;
            } else {
                // Can use existing allocation
                const size_type elems_after = size() - index;

                if (elems_after > count) {
                    // Move last 'count' elements to the end
                    relocateElements(end() - count, end(), end());

                    // Move the rest of elements after pos backward
                    std::move_backward(begin() + index, end() - count, end());

                    // Fill the hole with copies of value
                    std::fill_n(begin() + index, count, value);
                } else {
                    // Move elements after pos to the new end position
                    relocateElements(begin() + index, end(),
                                     begin() + index + count);

                    // Fill the hole with copies of value
                    std::fill_n(begin() + index, elems_after, value);

                    // Construct additional elements
                    constructElements(end(), count - elems_after, value);
                }

                size_ += count;
            }

            return begin() + index;
        } catch (...) {
            // Keep vector in a valid state
            throw;
        }
    }

    template <typename InputIt,
              typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
    auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator {
        const size_type count = std::distance(first, last);
        if (count == 0) {
            return const_cast<iterator>(pos);
        }

        const size_type index = pos - begin();

        try {
            if (size() + count > capacity()) {
                // Need to reallocate
                const size_type new_cap = growthSize(size() + count);

                // Allocate new buffer
                pointer new_data = allocateMemory(new_cap);

                // Move elements before insertion point
                relocateElements(begin(), begin() + index, new_data);

                // Insert new elements
                constructRange(new_data + index, first, last);

                // Move elements after insertion point
                relocateElements(begin() + index, end(),
                                 new_data + index + count);

                // Update size before cleanup to ensure proper destruction in
                // case of exception
                const size_type new_size = size() + count;

                // Clean up old storage
                destroyElements(begin(), end());
                deallocate();

                // Update members
                data_ = new_data;
                size_ = new_size;
                capacity_ = new_cap;
            } else {
                // Can use existing allocation
                const size_type elems_after = size() - index;

                if (elems_after > count) {
                    // Move last 'count' elements to the end
                    relocateElements(end() - count, end(), end());

                    // Move the rest of elements after pos backward
                    std::move_backward(begin() + index, end() - count, end());

                    // Copy new elements into the hole
                    std::copy(first, last, begin() + index);
                } else {
                    // Move elements after pos to the new end position
                    relocateElements(begin() + index, end(),
                                     begin() + index + count);

                    // Split the range to be inserted
                    auto mid = first;
                    std::advance(mid, elems_after);

                    // Copy the first part to the existing slots
                    std::copy(first, mid, begin() + index);

                    // Construct the remaining part in new slots
                    constructRange(end(), mid, last);
                }

                size_ += count;
            }

            return begin() + index;
        } catch (...) {
            // Keep vector in a valid state
            throw;
        }
    }

    auto insert(const_iterator pos, std::initializer_list<T> init) -> iterator {
        return insert(pos, init.begin(), init.end());
    }

    template <typename... Args>
    auto emplace(const_iterator pos, Args&&... args) -> iterator {
        const size_type index = pos - begin();

        try {
            if (size() == capacity()) {
                // Need to reallocate
                const size_type new_cap = growthSize(size() + 1);

                // Allocate new buffer
                pointer new_data = allocateMemory(new_cap);

                // Move elements before insertion point
                relocateElements(begin(), begin() + index, new_data);

                // Construct new element
                allocator_traits::construct(alloc_, new_data + index,
                                            std::forward<Args>(args)...);

                // Move elements after insertion point
                relocateElements(begin() + index, end(), new_data + index + 1);

                // Update size before cleanup to ensure proper destruction in
                // case of exception
                const size_type new_size = size() + 1;

                // Clean up old storage
                destroyElements(begin(), end());
                deallocate();

                // Update members
                data_ = new_data;
                size_ = new_size;
                capacity_ = new_cap;
            } else {
                // Can use existing allocation
                if (index < size()) {
                    // Move the last element to a new position at the end
                    allocator_traits::construct(alloc_, end(),
                                                std::move(back()));

                    // Move elements after pos backwards
                    std::move_backward(begin() + index, end() - 1, end());

                    // Destroy the element at pos
                    destroyElements(begin() + index, begin() + index + 1);

                    // Construct new element at pos
                    allocator_traits::construct(alloc_, begin() + index,
                                                std::forward<Args>(args)...);
                } else {
                    // Simply construct at the end
                    allocator_traits::construct(alloc_, end(),
                                                std::forward<Args>(args)...);
                }

                ++size_;
            }

            return begin() + index;
        } catch (...) {
            // Keep vector in a valid state
            throw;
        }
    }

    auto erase(const_iterator pos) -> iterator { return erase(pos, pos + 1); }

    auto erase(const_iterator first, const_iterator last) -> iterator {
        if (first == last) {
            return const_cast<iterator>(first);
        }

        const size_type index = first - begin();
        const size_type count = last - first;

        // Move elements after 'last' to 'first' position
        std::move(begin() + index + count, end(), begin() + index);

        // Destroy the elements at the end
        destroyElements(end() - count, end());

        // Update size
        size_ -= count;

        return begin() + index;
    }

    void pushBack(const T& value) {
        try {
            emplaceBack(value);
        } catch (...) {
            throw;
        }
    }

    void pushBack(T&& value) {
        try {
            emplaceBack(std::move(value));
        } catch (...) {
            throw;
        }
    }

    template <typename... Args>
    auto emplaceBack(Args&&... args) -> reference {
        try {
            if (size() == capacity()) {
                const size_type new_cap = growthSize(size() + 1);
                reallocate(new_cap);
            }

            allocator_traits::construct(alloc_, end(),
                                        std::forward<Args>(args)...);
            ++size_;
            return back();
        } catch (...) {
            // Keep vector in a valid state
            throw;
        }
    }

    void popBack() {
        assert(!empty() && "Cannot pop from an empty vector");

        destroyElements(end() - 1, end());
        --size_;
    }

    void resize(size_type count) {
        if (count < size()) {
            // Shrink
            destroyElements(begin() + count, end());
            size_ = count;
        } else if (count > size()) {
            // Grow
            try {
                reserve(count);
                constructDefaultElements(end(), count - size());
                size_ = count;
            } catch (...) {
                // Keep vector in a valid state
                throw;
            }
        }
    }

    void resize(size_type count, const T& value) {
        if (count < size()) {
            // Shrink
            destroyElements(begin() + count, end());
            size_ = count;
        } else if (count > size()) {
            // Grow
            try {
                reserve(count);
                constructElements(end(), count - size(), value);
                size_ = count;
            } catch (...) {
                // Keep vector in a valid state
                throw;
            }
        }
    }

    void swap(SmallVector& other) noexcept(
        allocator_traits::propagate_on_container_swap::value ||
        allocator_traits::is_always_equal::value) {
        if (this == &other) {
            return;
        }

        using std::swap;

        if constexpr (allocator_traits::propagate_on_container_swap::value) {
            swap(alloc_, other.alloc_);
        } else {
            // If allocators are not swappable, they must be equal for
            // well-defined behavior
            assert(alloc_ == other.alloc_ &&
                   "Swapping containers with unequal allocators and "
                   "!propagate_on_container_swap");
        }

        // Handle inline storage case
        const bool this_inline = isUsingInlineStorage();
        const bool other_inline = other.isUsingInlineStorage();

        if (this_inline && other_inline) {
            // Both using inline storage - need element-wise swap
            if (size() <= other.size()) {
                // Swap common elements
                for (size_type i = 0; i < size(); ++i) {
                    swap((*this)[i], other[i]);
                }

                // Move construct remaining elements from other
                for (size_type i = size(); i < other.size(); ++i) {
                    allocator_traits::construct(alloc_, data_ + i,
                                                std::move(other[i]));
                }

                // Destroy remaining elements in other
                for (size_type i = size(); i < other.size(); ++i) {
                    destroyElements(other.data_ + i, other.data_ + i + 1);
                }

                // Update sizes
                swap(size_, other.size_);
            } else {
                // Same logic but reverse roles
                for (size_type i = 0; i < other.size(); ++i) {
                    swap((*this)[i], other[i]);
                }

                for (size_type i = other.size(); i < size(); ++i) {
                    allocator_traits::construct(other.alloc_, other.data_ + i,
                                                std::move((*this)[i]));
                }

                for (size_type i = other.size(); i < size(); ++i) {
                    destroyElements(data_ + i, data_ + i + 1);
                }

                swap(size_, other.size_);
            }
        } else if (!this_inline && !other_inline) {
            // Both using dynamic storage - easy case
            swap(data_, other.data_);
            swap(size_, other.size_);
            swap(capacity_, other.capacity_);
        } else {
            // One is using inline storage, the other is using dynamic storage
            // Temporarily move to local variables, then reassign
            SmallVector temp(std::move(*this));
            *this = std::move(other);
            other = std::move(temp);
        }
    }

    // Utility
    [[nodiscard]] auto get_allocator() const noexcept -> allocator_type {
        return alloc_;
    }

    [[nodiscard]] bool isUsingInlineStorage() const noexcept {
        return data_ == reinterpret_cast<pointer>(
                            const_cast<unsigned char*>(static_buffer_.data()));
    }

private:
    void initializeFromEmpty() noexcept {
        data_ = reinterpret_cast<pointer>(static_buffer_.data());
        size_ = 0;
        capacity_ = N;
    }

    auto allocateMemory(size_type n) -> pointer {
        if (n <= N) {
            return reinterpret_cast<pointer>(static_buffer_.data());
        } else {
            try {
                return allocator_traits::allocate(alloc_, n);
            } catch (const std::bad_alloc&) {
                throw std::bad_alloc();
            } catch (...) {
                throw;
            }
        }
    }

    void deallocate() noexcept {
        if (!isUsingInlineStorage()) {
            allocator_traits::deallocate(alloc_, data_, capacity_);
            data_ = reinterpret_cast<pointer>(static_buffer_.data());
            capacity_ = N;
        }
    }

    void reallocate(size_type new_cap) {
        // Allocate new memory
        pointer new_data = allocateMemory(new_cap);

        // Move existing elements to new location
        relocateElements(begin(), end(), new_data);

        // Remember the new size before cleanup in case of exceptions
        const size_type current_size = size();

        // Clean up old storage
        destroyElements(begin(), end());
        deallocate();

        // Update members
        data_ = new_data;
        size_ = current_size;
        capacity_ = new_cap;
    }

    void moveFrom(SmallVector&& other) noexcept {
        if (other.isUsingInlineStorage()) {
            // Source is using inline storage, must move elements individually
            data_ = reinterpret_cast<pointer>(static_buffer_.data());
            size_ = 0;
            capacity_ = N;

            // Move elements one by one
            for (auto it = other.begin(); it != other.end(); ++it) {
                if constexpr (std::is_nothrow_move_constructible_v<T>) {
                    allocator_traits::construct(alloc_, data_ + size_,
                                                std::move(*it));
                } else {
                    try {
                        allocator_traits::construct(alloc_, data_ + size_,
                                                    std::move(*it));
                    } catch (...) {
                        // Roll back
                        destroyElements(begin(), begin() + size_);
                        size_ = 0;
                        throw;
                    }
                }
                ++size_;
            }
        } else {
            // Source is using dynamic storage, take ownership
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;

            // Reset other to use its inline storage
            other.data_ =
                reinterpret_cast<pointer>(other.static_buffer_.data());
            other.size_ = 0;
            other.capacity_ = N;
        }
    }

    // Optimization for trivially copyable types
    template <typename Iter>
    void relocateElements(Iter first, Iter last, pointer dest) {
        const size_type count = std::distance(first, last);

        if (count == 0) {
            return;
        }

        if constexpr (is_trivially_relocatable) {
            // Bulk relocate using memcpy for trivial types
            std::memcpy(static_cast<void*>(dest),
                        static_cast<const void*>(std::addressof(*first)),
                        count * sizeof(T));
        } else if (std::is_nothrow_move_constructible_v<T>) {
            // Use uninitialized_move for nothrow move constructible types
            std::uninitialized_move(first, last, dest);
        } else {
            // Fallback to copy for types with potentially throwing moves
            std::uninitialized_copy(first, last, dest);
        }
    }

    void destroyElements(iterator first, iterator last) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (; first != last; ++first) {
                allocator_traits::destroy(alloc_, std::addressof(*first));
            }
        }
    }

    void constructElements(iterator pos, size_type count, const T& value) {
        for (size_type i = 0; i < count; ++i) {
            allocator_traits::construct(alloc_, pos + i, value);
        }
    }

    void constructDefaultElements(iterator pos, size_type count) {
        for (size_type i = 0; i < count; ++i) {
            allocator_traits::construct(alloc_, pos + i);
        }
    }

    template <typename InputIt>
    void constructRange(iterator dest, InputIt first, InputIt last) {
        iterator current = dest;
        try {
            for (; first != last; ++first, ++current) {
                allocator_traits::construct(alloc_, current, *first);
            }
        } catch (...) {
            // Cleanup partially constructed elements
            destroyElements(dest, current);
            throw;
        }
    }

    // Growth strategy
    auto growthSize(size_type min_size) const -> size_type {
        const size_type max = maxSize();

        if (min_size > max) {
            throw std::length_error(
                "SmallVector capacity exceeded maximum size");
        }

        // Growth factor of 1.5 is generally a good balance
        const size_type new_cap = capacity_ + (capacity_ / 2);

        if (new_cap < min_size) {
            return min_size;
        }
        if (new_cap > max) {
            return max;
        }
        return new_cap;
    }

    alignas(
        alignof(T)) std::array<unsigned char, N * sizeof(T)> static_buffer_{};
    pointer data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
    [[no_unique_address]] allocator_type alloc_{};
};

// Global relational operators
template <typename T, std::size_t N, typename Alloc>
auto operator==(const SmallVector<T, N, Alloc>& lhs,
                const SmallVector<T, N, Alloc>& rhs) -> bool {
    return lhs.size() == rhs.size() &&
           std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename T, std::size_t N, typename Alloc>
auto operator!=(const SmallVector<T, N, Alloc>& lhs,
                const SmallVector<T, N, Alloc>& rhs) -> bool {
    return !(lhs == rhs);
}

template <typename T, std::size_t N, typename Alloc>
auto operator<(const SmallVector<T, N, Alloc>& lhs,
               const SmallVector<T, N, Alloc>& rhs) -> bool {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(),
                                        rhs.end());
}

template <typename T, std::size_t N, typename Alloc>
auto operator<=(const SmallVector<T, N, Alloc>& lhs,
                const SmallVector<T, N, Alloc>& rhs) -> bool {
    return !(rhs < lhs);
}

template <typename T, std::size_t N, typename Alloc>
auto operator>(const SmallVector<T, N, Alloc>& lhs,
               const SmallVector<T, N, Alloc>& rhs) -> bool {
    return rhs < lhs;
}

template <typename T, std::size_t N, typename Alloc>
auto operator>=(const SmallVector<T, N, Alloc>& lhs,
                const SmallVector<T, N, Alloc>& rhs) -> bool {
    return !(lhs < rhs);
}

// Global swap
template <typename T, std::size_t N, typename Alloc>
void swap(SmallVector<T, N, Alloc>& lhs,
          SmallVector<T, N, Alloc>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

#endif  // ATOM_TYPE_SMALL_VECTOR_HPP
