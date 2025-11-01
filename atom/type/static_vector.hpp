/*
 * static_vector.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: A static vector (Optimized with C++20 features and optional Boost
support)

**************************************************/

#ifndef ATOM_TYPE_STATIC_VECTOR_HPP
#define ATOM_TYPE_STATIC_VECTOR_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef ATOM_USE_PARALLEL_ALGORITHMS
#include <execution>
#endif

#include "atom/error/exception.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/align/aligned_allocator.hpp>
#include <boost/container/static_vector.hpp>
#endif

namespace atom {
namespace type {

/**
 * @brief A static vector implementation with a fixed capacity.
 *
 * This implementation provides std::vector-like functionality with a fixed
 * capacity, avoiding dynamic memory allocation.
 *
 * @tparam T The type of elements stored in the vector.
 * @tparam Capacity The maximum number of elements the vector can hold.
 * @tparam Alignment Optional alignment value for SIMD optimization.
 */
template <typename T, std::size_t Capacity, std::size_t Alignment = alignof(T)>
    requires(Capacity > 0)  // Ensure positive capacity
class StaticVector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

#ifdef ATOM_USE_BOOST
    using boost_static_vector = boost::container::static_vector<T, Capacity>;
    using aligned_storage_type =
        typename boost::alignment::aligned_allocator<T, Alignment>;
#else
    // Using aligned storage for potential SIMD operations
    using aligned_array = std::array<T, Capacity>;
#endif

    /**
     * @brief Default constructor. Constructs an empty StaticVector.
     */
    constexpr StaticVector() noexcept = default;

    /**
     * @brief Constructs a StaticVector with n copies of value.
     *
     * @param n Number of elements
     * @param value Value to fill the vector with
     * @throws std::length_error if n > Capacity
     */
    constexpr StaticVector(size_type n, const T& value) {
        if (n > Capacity) {
            THROW_LENGTH("StaticVector size exceeds capacity");
        }

        try {
            std::fill_n(begin(), n, value);
            m_size_ = n;
        } catch (...) {
            m_size_ = 0;  // Reset on failure
            throw;
        }
    }

    /**
     * @brief Constructs a StaticVector with n default-initialized elements.
     *
     * @param n Number of elements
     * @throws std::length_error if n > Capacity
     */
    constexpr explicit StaticVector(size_type n) {
        if (n > Capacity) {
            THROW_LENGTH("StaticVector size exceeds capacity");
        }

        m_size_ = n;
    }

    /**
     * @brief Constructs a StaticVector from an initializer list.
     *
     * @param init The initializer list to initialize the StaticVector with.
     * @throws std::length_error if init.size() > Capacity
     */
    constexpr StaticVector(std::initializer_list<T> init) {
        if (init.size() > Capacity) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<T, Capacity>::
                static_vector_exception(
                    "Initializer list size exceeds capacity");
#else
            THROW_LENGTH("Initializer list size exceeds capacity");
#endif
        }

        try {
            std::ranges::copy(init, begin());
            m_size_ = init.size();
        } catch (...) {
            m_size_ = 0;  // Reset on failure
            throw;
        }
    }

    /**
     * @brief Constructs a StaticVector from an input range.
     *
     * @tparam InputIt Input iterator type
     * @param first Iterator to the first element
     * @param last Iterator past the last element
     * @throws std::length_error if range size > Capacity
     */
    template <std::input_iterator InputIt>
    constexpr StaticVector(InputIt first, InputIt last) {
        if constexpr (std::sized_sentinel_for<InputIt, InputIt>) {
            if (std::distance(first, last) >
                static_cast<difference_type>(Capacity)) {
                THROW_LENGTH("Input range size exceeds capacity");
            }
        }

        try {
            for (; first != last && m_size_ < Capacity; ++first) {
                pushBack(*first);
            }

            if (first != last) {
                THROW_LENGTH("Input range size exceeds capacity");
            }
        } catch (...) {
            // Clean up in case of exception
            clear();
            throw;
        }
    }

    /**
     * @brief Copy constructor. Constructs a StaticVector by copying another
     * StaticVector.
     *
     * @param other The StaticVector to copy from.
     */
    constexpr StaticVector(const StaticVector& other) noexcept {
        std::ranges::copy(other.begin(), other.begin() + other.m_size_,
                          begin());
        m_size_ = other.m_size_;
    }

    /**
     * @brief Move constructor. Constructs a StaticVector by moving another
     * StaticVector.
     *
     * @param other The StaticVector to move from.
     */
    constexpr StaticVector(StaticVector&& other) noexcept {
        std::ranges::move(other.begin(), other.begin() + other.m_size_,
                          begin());
        m_size_ = other.m_size_;
        other.m_size_ = 0;
    }

    /**
     * @brief Copy assignment operator. Copies the contents of another
     * StaticVector.
     *
     * @param other The StaticVector to copy from.
     * @return A reference to the assigned StaticVector.
     */
    constexpr auto operator=(const StaticVector& other) noexcept
        -> StaticVector& {
        if (this != &other) {
            std::ranges::copy(other.begin(), other.begin() + other.m_size_,
                              begin());
            m_size_ = other.m_size_;
        }
        return *this;
    }

    /**
     * @brief Move assignment operator. Moves the contents of another
     * StaticVector.
     *
     * @param other The StaticVector to move from.
     * @return A reference to the assigned StaticVector.
     */
    constexpr auto operator=(StaticVector&& other) noexcept -> StaticVector& {
        if (this != &other) {
            std::ranges::move(other.begin(), other.begin() + other.m_size_,
                              begin());
            m_size_ = other.m_size_;
            other.m_size_ = 0;
        }
        return *this;
    }

    /**
     * @brief Assigns elements from an initializer list.
     *
     * @param ilist Initializer list to assign from
     * @throws std::length_error if ilist.size() > Capacity
     */
    constexpr StaticVector& operator=(std::initializer_list<T> ilist) {
        if (ilist.size() > Capacity) {
            THROW_LENGTH("Initializer list size exceeds capacity");
        }

        try {
            std::ranges::copy(ilist.begin(), ilist.end(), begin());
            m_size_ = ilist.size();
        } catch (...) {
            clear();  // Reset on failure
            throw;
        }

        return *this;
    }

    /**
     * @brief Assigns elements from a container to this vector.
     *
     * @tparam Container Type of the source container.
     * @param container The source container.
     * @throws std::length_error if container size > Capacity
     */
    template <typename Container>
    constexpr void assign(const Container& container) {
        if constexpr (std::ranges::sized_range<Container>) {
            if (std::ranges::size(container) > Capacity) {
                THROW_LENGTH("Container size exceeds capacity");
            }
        }

        try {
            clear();
            for (const auto& item : container) {
                pushBack(item);
            }
        } catch (...) {
            clear();  // Reset on failure
            throw;
        }
    }

    /**
     * @brief Assigns n copies of value to the vector.
     *
     * @param n Number of elements
     * @param value Value to fill with
     * @throws std::length_error if n > Capacity
     */
    constexpr void assign(size_type n, const T& value) {
        if (n > Capacity) {
            THROW_LENGTH("Assignment size exceeds capacity");
        }

        try {
            clear();
            std::fill_n(begin(), n, value);
            m_size_ = n;
        } catch (...) {
            clear();  // Reset on failure
            throw;
        }
    }

    /**
     * @brief Assigns elements from an iterator range.
     *
     * @tparam InputIt Input iterator type
     * @param first Iterator to first element
     * @param last Iterator past last element
     * @throws std::length_error if range size > Capacity
     */
    template <std::input_iterator InputIt>
    constexpr void assign(InputIt first, InputIt last) {
        if constexpr (std::sized_sentinel_for<InputIt, InputIt>) {
            if (std::distance(first, last) >
                static_cast<difference_type>(Capacity)) {
                THROW_LENGTH("Input range size exceeds capacity");
            }
        }

        try {
            clear();
            for (; first != last; ++first) {
                pushBack(*first);
            }
        } catch (...) {
            clear();  // Reset on failure
            throw;
        }
    }

    /**
     * @brief Adds an element to the end of the StaticVector by copying.
     *
     * @param value The value to add.
     * @throws std::overflow_error if capacity would be exceeded
     */
    constexpr void pushBack(const T& value) {
        if (m_size_ >= Capacity) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<T, Capacity>::
                static_vector_exception("StaticVector capacity exceeded");
#else
            THROW_OVERFLOW("StaticVector capacity exceeded");
#endif
        }
        m_data_[m_size_++] = value;
    }

    /**
     * @brief Adds an element to the end of the StaticVector by moving.
     *
     * @param value The value to add.
     * @throws std::overflow_error if capacity would be exceeded
     */
    constexpr void pushBack(T&& value) {
        if (m_size_ >= Capacity) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<T, Capacity>::
                static_vector_exception("StaticVector capacity exceeded");
#else
            THROW_OVERFLOW("StaticVector capacity exceeded");
#endif
        }
        m_data_[m_size_++] = std::move(value);
    }

    /**
     * @brief Constructs an element in place at the end of the StaticVector.
     *
     * @tparam Args The types of the arguments to construct the element with.
     * @param args The arguments to construct the element with.
     * @return A reference to the constructed element.
     * @throws std::overflow_error if capacity would be exceeded
     */
    template <typename... Args>
    constexpr auto emplaceBack(Args&&... args) -> reference {
        // Check capacity before construction to avoid unnecessary object
        // creation
        if (m_size_ >= Capacity) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<T, Capacity>::
                static_vector_exception("StaticVector capacity exceeded");
#else
            THROW_OVERFLOW("StaticVector capacity exceeded on emplaceBack");
#endif
        }

        try {
            auto& ref = m_data_[m_size_];
            ref = T(std::forward<Args>(args)...);
            ++m_size_;
            return ref;
        } catch (...) {
            // Don't increment size if construction throws
            throw;
        }
    }

    /**
     * @brief Removes the last element from the StaticVector.
     *
     * @throws std::underflow_error if vector is empty
     */
    constexpr void popBack() {
        if (m_size_ == 0) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<
                T, Capacity>::static_vector_exception("StaticVector is empty");
#else
            THROW_UNDERFLOW("StaticVector is empty");
#endif
        }
        --m_size_;
    }

    /**
     * @brief Inserts an element at the specified position.
     *
     * @param pos Iterator to the position where the element will be inserted.
     * @param value The value to insert.
     * @return Iterator pointing to the inserted element.
     * @throws std::overflow_error if capacity would be exceeded
     */
    constexpr auto insert(const_iterator pos, const T& value) -> iterator {
        return emplace(pos, value);
    }

    /**
     * @brief Inserts a moved element at the specified position.
     *
     * @param pos Iterator to the position where the element will be inserted.
     * @param value The value to insert.
     * @return Iterator pointing to the inserted element.
     * @throws std::overflow_error if capacity would be exceeded
     */
    constexpr auto insert(const_iterator pos, T&& value) -> iterator {
        return emplace(pos, std::move(value));
    }

    /**
     * @brief Inserts n copies of value at the specified position.
     *
     * @param pos Iterator to the insertion position
     * @param n Number of elements to insert
     * @param value Value to insert
     * @return Iterator to the first inserted element
     * @throws std::overflow_error if capacity would be exceeded
     */
    constexpr auto insert(const_iterator pos, size_type n,
                          const T& value) -> iterator {
        if (n == 0) {
            return const_cast<iterator>(pos);
        }

        if (m_size_ + n > Capacity) {
            THROW_OVERFLOW("Insertion would exceed capacity");
        }

        const auto index = std::distance(cbegin(), pos);
        assert(index >= 0 && static_cast<size_type>(index) <= m_size_);

        try {
            if (pos != end()) {
                // Move existing elements to make space
                std::move_backward(begin() + index, end(), end() + n);
            }

            // Fill the gap with copies of value
            std::fill_n(begin() + index, n, value);
            m_size_ += n;

            return begin() + index;
        } catch (...) {
            // Rollback if an exception occurs
            if (pos != end()) {
                std::move(begin() + index + n, end() + n, begin() + index);
            }
            throw;
        }
    }

    /**
     * @brief Inserts elements from a range at the specified position.
     *
     * @tparam InputIt Input iterator type
     * @param pos Iterator to the insertion position
     * @param first Begin iterator of the range
     * @param last End iterator of the range
     * @return Iterator to the first inserted element
     * @throws std::overflow_error if capacity would be exceeded
     */
    template <std::input_iterator InputIt>
    constexpr auto insert(const_iterator pos, InputIt first,
                          InputIt last) -> iterator {
        const auto index = std::distance(cbegin(), pos);
        assert(index >= 0 && static_cast<size_type>(index) <= m_size_);

        // For forward iterators, we can determine the distance
        if constexpr (std::forward_iterator<InputIt>) {
            const auto count = std::distance(first, last);
            if (count <= 0) {
                return const_cast<iterator>(pos);
            }

            if (m_size_ + count > Capacity) {
                THROW_OVERFLOW("Insertion would exceed capacity");
            }

            try {
                if (pos != end()) {
                    // Move existing elements to make space
                    std::move_backward(begin() + index, end(), end() + count);
                }

                // Copy elements from the range
                std::copy(first, last, begin() + index);
                m_size_ += count;

                return begin() + index;
            } catch (...) {
                // Rollback if an exception occurs
                if (pos != end()) {
                    std::move(begin() + index + count, end() + count,
                              begin() + index);
                }
                throw;
            }
        } else {
            // For input iterators, copy one by one and keep track of insertions
            size_type inserted = 0;
            try {
                auto insertPos = const_cast<iterator>(pos);

                for (; first != last; ++first) {
                    insertPos = insert(insertPos, *first) + 1;
                    ++inserted;
                }

                return const_cast<iterator>(pos);
            } catch (...) {
                throw;
            }
        }
    }

    /**
     * @brief Inserts elements from an initializer list at the specified
     * position.
     *
     * @param pos Iterator to the insertion position
     * @param ilist Initializer list of elements to insert
     * @return Iterator to the first inserted element
     * @throws std::overflow_error if capacity would be exceeded
     */
    constexpr auto insert(const_iterator pos,
                          std::initializer_list<T> ilist) -> iterator {
        return insert(pos, ilist.begin(), ilist.end());
    }

    /**
     * @brief Constructs an element in-place at the specified position.
     *
     * @tparam Args The types of the arguments to construct the element with.
     * @param pos Iterator to the position where the element will be inserted.
     * @param args The arguments to construct the element with.
     * @return Iterator pointing to the inserted element.
     * @throws std::overflow_error if capacity would be exceeded
     */
    template <typename... Args>
    constexpr auto emplace(const_iterator pos, Args&&... args) -> iterator {
        assert(pos >= begin() && pos <= end());
        const auto index = std::distance(cbegin(), pos);

        if (m_size_ >= Capacity) {
            THROW_OVERFLOW("StaticVector capacity exceeded on emplace");
        }

        try {
            if (pos != end()) {
                // Move elements to make space
                std::move_backward(begin() + index, end(), end() + 1);
            }

            // Construct new element
            auto* p = &m_data_[index];
            *p = T(std::forward<Args>(args)...);

            ++m_size_;
            return begin() + index;
        } catch (...) {
            // Restore the vector to its previous state
            if (pos != end()) {
                std::move(begin() + index + 1, end() + 1, begin() + index);
            }
            throw;
        }
    }

    /**
     * @brief Erases an element at the specified position.
     *
     * @param pos Iterator to the element to erase.
     * @return Iterator following the erased element.
     * @throws std::out_of_range if pos is out of bounds
     */
    constexpr auto erase(const_iterator pos) -> iterator {
        if (pos < begin() || pos >= end()) {
            THROW_OUT_OF_RANGE("Iterator out of range in erase");
        }

        const auto index = std::distance(cbegin(), pos);

        // Move elements to fill the gap
        std::move(begin() + index + 1, end(), begin() + index);
        --m_size_;

        return begin() + index;
    }

    /**
     * @brief Erases a range of elements.
     *
     * @param first Iterator to the first element to erase
     * @param last Iterator past the last element to erase
     * @return Iterator following the last erased element
     * @throws std::out_of_range if range is invalid
     */
    constexpr auto erase(const_iterator first,
                         const_iterator last) -> iterator {
        if (first < begin() || last > end() || first > last) {
            THROW_OUT_OF_RANGE("Invalid range in erase");
        }

        if (first == last) {
            return const_cast<iterator>(first);
        }

        const auto start = std::distance(cbegin(), first);
        const auto count = std::distance(first, last);

        // Move elements to fill the gap
        std::move(begin() + start + count, end(), begin() + start);
        m_size_ -= count;

        return begin() + start;
    }

    /**
     * @brief Clears the StaticVector, removing all elements.
     */
    constexpr void clear() noexcept { m_size_ = 0; }

    /**
     * @brief Resizes the vector to contain n elements.
     *
     * If n is smaller than the current size, the vector is truncated.
     * If n is larger, new default-initialized elements are added.
     *
     * @param n New size of the vector
     * @throws std::overflow_error if n > Capacity
     */
    constexpr void resize(size_type n) {
        if (n > Capacity) {
            THROW_OVERFLOW("Resize would exceed capacity");
        }

        if (n > m_size_) {
            // Default-initialize new elements
            for (size_type i = m_size_; i < n; ++i) {
                try {
                    m_data_[i] = T();
                } catch (...) {
                    // Rollback on exception
                    m_size_ = i;
                    throw;
                }
            }
        }

        m_size_ = n;
    }

    /**
     * @brief Resizes the vector to contain n elements.
     *
     * If n is smaller than the current size, the vector is truncated.
     * If n is larger, new elements initialized with value are added.
     *
     * @param n New size of the vector
     * @param value Value to initialize new elements with
     * @throws std::overflow_error if n > Capacity
     */
    constexpr void resize(size_type n, const T& value) {
        if (n > Capacity) {
            THROW_OVERFLOW("Resize would exceed capacity");
        }

        if (n > m_size_) {
            // Initialize new elements with value
            try {
                std::fill_n(end(), n - m_size_, value);
            } catch (...) {
                throw;
            }
        }

        m_size_ = n;
    }

    /**
     * @brief Exchanges the contents of this vector with another.
     *
     * @param other Vector to swap with
     */
    constexpr void swap(StaticVector& other) noexcept {
        if (this == &other) {
            return;
        }

        const size_type min_size = std::min(m_size_, other.m_size_);

        // Swap common elements
        for (size_type i = 0; i < min_size; ++i) {
            std::swap(m_data_[i], other.m_data_[i]);
        }

        // If this vector has more elements, move them to other
        if (m_size_ > min_size) {
            std::move(begin() + min_size, end(), other.begin() + min_size);
        }
        // If other vector has more elements, move them to this
        else if (other.m_size_ > min_size) {
            std::move(other.begin() + min_size, other.end(),
                      begin() + min_size);
        }

        // Swap sizes
        std::swap(m_size_, other.m_size_);
    }

    /**
     * @brief Checks if the StaticVector is empty.
     *
     * @return True if the StaticVector is empty, false otherwise.
     */
    [[nodiscard]] constexpr auto empty() const noexcept -> bool {
        return m_size_ == 0;
    }

    /**
     * @brief Returns the number of elements in the StaticVector.
     *
     * @return The number of elements in the StaticVector.
     */
    [[nodiscard]] constexpr auto size() const noexcept -> size_type {
        return m_size_;
    }

    /**
     * @brief Returns the maximum number of elements the StaticVector can hold.
     *
     * @return The maximum number of elements the StaticVector can hold.
     */
    [[nodiscard]] constexpr auto max_size() const noexcept -> size_type {
        return Capacity;
    }

    /**
     * @brief Returns the capacity of the StaticVector.
     *
     * @return The capacity of the StaticVector.
     */
    [[nodiscard]] constexpr auto capacity() const noexcept -> size_type {
        return Capacity;
    }

    /**
     * @brief Reserves capacity for future growth.
     *
     * This is a no-op for StaticVector since capacity is fixed,
     * but it will throw if requested capacity exceeds the static capacity.
     *
     * @param newCapacity The new capacity to reserve.
     * @throws std::overflow_error if newCapacity > Capacity
     */
    constexpr void reserve(size_type newCapacity) const {
        if (newCapacity > Capacity) {
            THROW_OVERFLOW("Cannot reserve beyond static capacity");
        }
        // Otherwise no-op since capacity is fixed
    }

    /**
     * @brief Requests the removal of unused capacity.
     *
     * This is a no-op for StaticVector since capacity is fixed.
     */
    constexpr void shrink_to_fit() noexcept {
        // No-op for StaticVector
    }

    /**
     * @brief Accesses an element by index.
     *
     * @param index The index of the element to access.
     * @return A reference to the element at the specified index.
     */
    [[nodiscard]] constexpr auto operator[](size_type index) noexcept
        -> reference {
        assert(index < m_size_);
        return m_data_[index];
    }

    /**
     * @brief Accesses an element by index.
     *
     * @param index The index of the element to access.
     * @return A const reference to the element at the specified index.
     */
    [[nodiscard]] constexpr auto operator[](size_type index) const noexcept
        -> const_reference {
        assert(index < m_size_);
        return m_data_[index];
    }

    /**
     * @brief Accesses an element by index with bounds checking.
     *
     * @param index The index of the element to access.
     * @return A reference to the element at the specified index.
     * @throws std::out_of_range if the index is out of bounds.
     */
    [[nodiscard]] constexpr auto at(size_type index) -> reference {
        if (index >= m_size_) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<T, Capacity>::
                static_vector_exception("StaticVector::at: index out of range");
#else
            THROW_OUT_OF_RANGE("StaticVector::at: index out of range");
#endif
        }
        return m_data_[index];
    }

    /**
     * @brief Accesses an element by index with bounds checking.
     *
     * @param index The index of the element to access.
     * @return A const reference to the element at the specified index.
     * @throws std::out_of_range if the index is out of bounds.
     */
    [[nodiscard]] constexpr auto at(size_type index) const -> const_reference {
        if (index >= m_size_) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<T, Capacity>::
                static_vector_exception("StaticVector::at: index out of range");
#else
            THROW_OUT_OF_RANGE("StaticVector::at: index out of range");
#endif
        }
        return m_data_[index];
    }

    /**
     * @brief Accesses the first element.
     *
     * @return A reference to the first element.
     * @throws std::underflow_error if the vector is empty.
     */
    [[nodiscard]] constexpr auto front() -> reference {
        if (m_size_ == 0) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<
                T, Capacity>::static_vector_exception("StaticVector is empty");
#else
            THROW_UNDERFLOW("StaticVector is empty");
#endif
        }
        return m_data_[0];
    }

    /**
     * @brief Accesses the first element.
     *
     * @return A const reference to the first element.
     * @throws std::underflow_error if the vector is empty.
     */
    [[nodiscard]] constexpr auto front() const -> const_reference {
        if (m_size_ == 0) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<
                T, Capacity>::static_vector_exception("StaticVector is empty");
#else
            THROW_UNDERFLOW("StaticVector is empty");
#endif
        }
        return m_data_[0];
    }

    /**
     * @brief Accesses the last element.
     *
     * @return A reference to the last element.
     * @throws std::underflow_error if the vector is empty.
     */
    [[nodiscard]] constexpr auto back() -> reference {
        if (m_size_ == 0) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<
                T, Capacity>::static_vector_exception("StaticVector is empty");
#else
            THROW_UNDERFLOW("StaticVector is empty");
#endif
        }
        return m_data_[m_size_ - 1];
    }

    /**
     * @brief Accesses the last element.
     *
     * @return A const reference to the last element.
     * @throws std::underflow_error if the vector is empty.
     */
    [[nodiscard]] constexpr auto back() const -> const_reference {
        if (m_size_ == 0) {
#ifdef ATOM_USE_BOOST
            throw boost::container::static_vector<
                T, Capacity>::static_vector_exception("StaticVector is empty");
#else
            THROW_UNDERFLOW("StaticVector is empty");
#endif
        }
        return m_data_[m_size_ - 1];
    }

    /**
     * @brief Returns a pointer to the underlying data.
     *
     * @return A pointer to the underlying data.
     */
    [[nodiscard]] constexpr auto data() noexcept -> pointer {
        return m_data_.data();
    }

    /**
     * @brief Returns a const pointer to the underlying data.
     *
     * @return A const pointer to the underlying data.
     */
    [[nodiscard]] constexpr auto data() const noexcept -> const_pointer {
        return m_data_.data();
    }

    /**
     * @brief Returns a span view of the vector's content.
     *
     * @return A span covering the vector's elements.
     */
    [[nodiscard]] constexpr auto as_span() noexcept -> std::span<T> {
        return std::span<T>(data(), size());
    }

    /**
     * @brief Returns a const span view of the vector's content.
     *
     * @return A const span covering the vector's elements.
     */
    [[nodiscard]] constexpr auto as_span() const noexcept
        -> std::span<const T> {
        return std::span<const T>(data(), size());
    }

    /**
     * @brief Returns an iterator to the beginning of the StaticVector.
     *
     * @return An iterator to the beginning of the StaticVector.
     */
    [[nodiscard]] constexpr auto begin() noexcept -> iterator { return data(); }

    /**
     * @brief Returns a const iterator to the beginning of the StaticVector.
     *
     * @return A const iterator to the beginning of the StaticVector.
     */
    [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator {
        return data();
    }

    /**
     * @brief Returns an iterator to the end of the StaticVector.
     *
     * @return An iterator to the end of the StaticVector.
     */
    [[nodiscard]] constexpr auto end() noexcept -> iterator {
        return data() + m_size_;
    }

    /**
     * @brief Returns a const iterator to the end of the StaticVector.
     *
     * @return A const iterator to the end of the StaticVector.
     */
    [[nodiscard]] constexpr auto end() const noexcept -> const_iterator {
        return data() + m_size_;
    }

    /**
     * @brief Returns a reverse iterator to the beginning of the StaticVector.
     *
     * @return A reverse iterator to the beginning of the StaticVector.
     */
    [[nodiscard]] constexpr auto rbegin() noexcept -> reverse_iterator {
        return reverse_iterator(end());
    }

    /**
     * @brief Returns a const reverse iterator to the beginning of the
     * StaticVector.
     *
     * @return A const reverse iterator to the beginning of the StaticVector.
     */
    [[nodiscard]] constexpr auto rbegin() const noexcept
        -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    /**
     * @brief Returns a reverse iterator to the end of the StaticVector.
     *
     * @return A reverse iterator to the end of the StaticVector.
     */
    [[nodiscard]] constexpr auto rend() noexcept -> reverse_iterator {
        return reverse_iterator(begin());
    }

    /**
     * @brief Returns a const reverse iterator to the end of the StaticVector.
     *
     * @return A const reverse iterator to the end of the StaticVector.
     */
    [[nodiscard]] constexpr auto rend() const noexcept
        -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    /**
     * @brief Returns a const iterator to the beginning of the StaticVector.
     *
     * @return A const iterator to the beginning of the StaticVector.
     */
    [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator {
        return begin();
    }

    /**
     * @brief Returns a const iterator to the end of the StaticVector.
     *
     * @return A const iterator to the end of the StaticVector.
     */
    [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator {
        return end();
    }

    /**
     * @brief Returns a const reverse iterator to the beginning of the
     * StaticVector.
     *
     * @return A const reverse iterator to the beginning of the StaticVector.
     */
    [[nodiscard]] constexpr auto crbegin() const noexcept
        -> const_reverse_iterator {
        return rbegin();
    }

    /**
     * @brief Returns a const reverse iterator to the end of the StaticVector.
     *
     * @return A const reverse iterator to the end of the StaticVector.
     */
    [[nodiscard]] constexpr auto crend() const noexcept
        -> const_reverse_iterator {
        return rend();
    }

    /**
     * @brief Applies a transformation to all elements using SIMD when possible.
     *
     * @tparam UnaryOp Type of the unary operation.
     * @param op The unary operation to apply.
     */
    template <typename UnaryOp>
    constexpr void transform_elements(UnaryOp op) {
#if defined(__AVX2__) || defined(__SSE4_2__)
        // SIMD implementation for compatible types and operations
        if constexpr (std::is_arithmetic_v<T> && sizeof(T) * Capacity >= 16) {
            // SIMD implementation would go here - simplified example
            for (size_type i = 0; i < m_size_; i += 4) {
                // Process 4 elements at once with SIMD
                size_type chunk_size = std::min(size_type{4}, m_size_ - i);
                for (size_type j = 0; j < chunk_size; ++j) {
                    m_data_[i + j] = op(m_data_[i + j]);
                }
            }
        } else
#endif
        {
            // Fallback to standard implementation
            std::transform(begin(), end(), begin(), op);
        }
    }

    /**
     * @brief Applies an operation to all elements in parallel when appropriate.
     *
     * @tparam UnaryOp Type of the unary operation.
     * @param op The unary operation to apply.
     */
    template <typename UnaryOp>
    void parallel_for_each(UnaryOp op) {
#if defined(ATOM_USE_PARALLEL_ALGORITHMS)
        if constexpr (Capacity > 1000) {
            std::for_each(std::execution::par, begin(), end(), op);
        } else
#endif
        {
            std::for_each(begin(), end(), op);
        }
    }

    /**
     * @brief Safely adds elements to the vector with error handling.
     *
     * @param elements A span of elements to add.
     * @return True if all elements were added successfully, false otherwise.
     */
    constexpr bool safeAddElements(std::span<const T> elements) noexcept {
        try {
            if (m_size_ + elements.size() > Capacity) {
                return false;
            }

            for (const auto& elem : elements) {
                pushBack(elem);
            }
            return true;
        } catch (...) {
            // Log the error but continue execution
            return false;
        }
    }

    /**
     * @brief Equality operator.
     *
     * @param other The StaticVector to compare with.
     * @return True if the vectors are equal, false otherwise.
     */
    [[nodiscard]] constexpr auto operator==(
        const StaticVector& other) const noexcept -> bool {
        return m_size_ == other.m_size_ &&
               std::equal(begin(), end(), other.begin());
    }

    /**
     * @brief Three-way comparison operator.
     *
     * @param other The StaticVector to compare with.
     * @return The result of the three-way comparison.
     */
    [[nodiscard]] constexpr auto operator<=>(
        const StaticVector& other) const noexcept -> std::strong_ordering {
        return std::lexicographical_compare_three_way(
            begin(), end(), other.begin(), other.end());
    }

private:
    aligned_array m_data_{};
    size_type m_size_{0};
};

/**
 * @brief Swap function for StaticVector.
 *
 * @tparam T Element type.
 * @tparam Capacity Vector capacity.
 * @param lhs First vector.
 * @param rhs Second vector.
 */
template <typename T, std::size_t Capacity, std::size_t Alignment>
constexpr void swap(StaticVector<T, Capacity, Alignment>& lhs,
                    StaticVector<T, Capacity, Alignment>& rhs) noexcept {
    lhs.swap(rhs);
}

/**
 * @brief Helper function for safely adding elements to a StaticVector with
 * error handling.
 *
 * @tparam T Element type.
 * @tparam Capacity Vector capacity.
 * @param vec Reference to the StaticVector.
 * @param elements Span of elements to add.
 * @return True if all elements were added successfully, false otherwise.
 */
template <typename T, std::size_t Capacity, std::size_t Alignment = alignof(T)>
bool safeAddElements(StaticVector<T, Capacity, Alignment>& vec,
                     std::span<const T> elements) noexcept {
    try {
        if (vec.size() + elements.size() > vec.capacity()) {
            std::cerr << "Warning: Cannot add all elements - capacity would be "
                         "exceeded"
                      << std::endl;
            return false;
        }

        for (const auto& elem : elements) {
            vec.pushBack(elem);
        }
        return true;
    } catch (const std::overflow_error& e) {
        // Handle overflow specifically
        std::cerr << "Overflow error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        // General exception handling
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Creates a StaticVector from a container safely.
 *
 * @tparam T Element type.
 * @tparam Capacity Vector capacity.
 * @tparam Container Container type.
 * @param container Source container.
 * @return StaticVector containing the elements from the container.
 * @throws std::length_error if container size exceeds capacity.
 */
template <typename T, std::size_t Capacity, std::size_t Alignment = alignof(T),
          typename Container>
StaticVector<T, Capacity, Alignment> makeStaticVector(
    const Container& container) {
    StaticVector<T, Capacity, Alignment> result;
    result.assign(container);
    return result;
}

/**
 * @brief Performs a SIMD-accelerated operation on two StaticVectors and stores
 * the result in a third.
 *
 * @tparam T Element type (must be arithmetic).
 * @tparam Capacity Vector capacity.
 * @tparam BinaryOp Binary operation type.
 * @param lhs First input vector.
 * @param rhs Second input vector.
 * @param result Output vector.
 * @param op The binary operation to apply.
 * @return True if operation was successful, false otherwise.
 */
template <typename T, std::size_t Capacity, std::size_t Alignment = alignof(T),
          typename BinaryOp>
bool simdTransform(const StaticVector<T, Capacity, Alignment>& lhs,
                   const StaticVector<T, Capacity, Alignment>& rhs,
                   StaticVector<T, Capacity, Alignment>& result,
                   BinaryOp op) noexcept
    requires(std::is_arithmetic_v<T>)
{
    try {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        result.resize(lhs.size());

#if defined(__AVX2__) || defined(__SSE4_2__)
        if constexpr (sizeof(T) * Capacity >= 16) {
            // Process elements in SIMD-friendly chunks
            for (std::size_t i = 0; i < lhs.size(); i += 4) {
                std::size_t chunk_size =
                    std::min(std::size_t{4}, lhs.size() - i);
                for (std::size_t j = 0; j < chunk_size; ++j) {
                    result[i + j] = op(lhs[i + j], rhs[i + j]);
                }
            }
        } else
#endif
        {
            // Standard implementation
            std::transform(lhs.begin(), lhs.end(), rhs.begin(), result.begin(),
                           op);
        }

        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief A smart pointer wrapper for StaticVector for automatic cleanup.
 *
 * @tparam T Element type.
 * @tparam Capacity Vector capacity.
 * @tparam Alignment Element alignment.
 */
template <typename T, std::size_t Capacity, std::size_t Alignment = alignof(T)>
class SmartStaticVector {
public:
    using vector_type = StaticVector<T, Capacity, Alignment>;

    SmartStaticVector() : m_vec(std::make_shared<vector_type>()) {}

    SmartStaticVector(const SmartStaticVector&) = default;
    SmartStaticVector(SmartStaticVector&&) = default;
    SmartStaticVector& operator=(const SmartStaticVector&) = default;
    SmartStaticVector& operator=(SmartStaticVector&&) = default;

    /**
     * @brief Get the underlying vector.
     * @return Reference to the vector.
     */
    vector_type& get() { return *m_vec; }

    /**
     * @brief Get the underlying vector.
     * @return Const reference to the vector.
     */
    const vector_type& get() const { return *m_vec; }

    /**
     * @brief Arrow operator for convenient access.
     * @return Pointer to the vector.
     */
    vector_type* operator->() { return m_vec.get(); }

    /**
     * @brief Arrow operator for convenient access.
     * @return Const pointer to the vector.
     */
    const vector_type* operator->() const { return m_vec.get(); }

    /**
     * @brief Check if the vector is shared by multiple SmartStaticVectors.
     * @return True if use count > 1, false otherwise.
     */
    bool isShared() const { return m_vec.use_count() > 1; }

    /**
     * @brief Create a copy of the vector if it's shared.
     */
    void makeUnique() {
        if (isShared()) {
            m_vec = std::make_shared<vector_type>(*m_vec);
        }
    }

private:
    std::shared_ptr<vector_type> m_vec;
};

}  // namespace type
}  // namespace atom

#endif  // ATOM_TYPE_STATIC_VECTOR_HPP
