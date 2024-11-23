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
#include <initializer_list>
#include <limits>
#include <memory>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/copy.hpp>
#include <boost/core/noinit_adaptor.hpp>
#include <boost/move/move.hpp>
#include <boost/range/algorithm.hpp>
#endif

template <typename T, std::size_t N>
class SmallVector {
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

    SmallVector() = default;

    template <typename InputIt>
    SmallVector(InputIt first, InputIt last) {
        assign(first, last);
    }

    explicit SmallVector(size_type count, const T& value = T()) {
        assign(count, value);
    }

    SmallVector(std::initializer_list<T> init) { assign(init); }

    SmallVector(const SmallVector& other) {
        assign(other.begin(), other.end());
    }

    SmallVector(SmallVector&& other) ATOM_NOEXCEPT {
        moveFrom(std::move(other));
    }

    ~SmallVector() {
        clear();
        deallocate();
    }

    auto operator=(const SmallVector& other) -> SmallVector& {
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    auto operator=(SmallVector&& other) ATOM_NOEXCEPT->SmallVector& {
        if (this != &other) {
            clear();
            deallocate();
            moveFrom(std::move(other));
        }
        return *this;
    }

    auto operator=(std::initializer_list<T> init) -> SmallVector& {
        assign(init);
        return *this;
    }

    void assign(size_type count, const T& value) {
        clear();
        if (count > capacity()) {
            deallocate();
            allocateMemory(count);
        }
#ifdef ATOM_USE_BOOST
        boost::uninitialized_fill_n(begin(), count, value);
#else
        std::uninitialized_fill_n(begin(), count, value);
#endif
        size_ = count;
    }

    template <typename InputIt>
    void assign(InputIt first, InputIt last) {
        clear();
        size_type count = std::distance(first, last);
        if (count > capacity()) {
            deallocate();
            allocateMemory(count);
        }
#ifdef ATOM_USE_BOOST
        boost::uninitialized_copy(first, last, begin());
#else
        std::uninitialized_copy(first, last, begin());
#endif
        size_ = count;
    }

    void assign(std::initializer_list<T> init) {
        assign(init.begin(), init.end());
    }

    auto at(size_type pos) -> reference {
        if (pos >= size()) {
            THROW_OUT_OF_RANGE("SmallVector::at");
        }
        return (*this)[pos];
    }

    auto at(size_type pos) const -> const_reference {
        if (pos >= size()) {
            THROW_OUT_OF_RANGE("SmallVector::at");
        }
        return (*this)[pos];
    }

    auto operator[](size_type pos) -> reference { return *(begin() + pos); }

    auto operator[](size_type pos) const -> const_reference {
        return *(begin() + pos);
    }

    auto front() -> reference { return *begin(); }

    auto front() const -> const_reference { return *begin(); }

    auto back() -> reference { return *(end() - 1); }

    auto back() const -> const_reference { return *(end() - 1); }

    auto data() ATOM_NOEXCEPT -> T* { return begin(); }

    auto data() const ATOM_NOEXCEPT -> const T* { return begin(); }

    auto begin() ATOM_NOEXCEPT -> iterator { return data_; }

    auto begin() const ATOM_NOEXCEPT -> const_iterator { return data_; }

    auto cbegin() const ATOM_NOEXCEPT -> const_iterator { return begin(); }

    auto end() ATOM_NOEXCEPT -> iterator { return begin() + size(); }

    auto end() const ATOM_NOEXCEPT -> const_iterator {
        return begin() + size();
    }

    auto cend() const ATOM_NOEXCEPT -> const_iterator { return end(); }

    auto rbegin() ATOM_NOEXCEPT -> reverse_iterator {
        return reverse_iterator(end());
    }

    auto rbegin() const ATOM_NOEXCEPT -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    auto crbegin() const ATOM_NOEXCEPT -> const_reverse_iterator {
        return rbegin();
    }

    auto rend() ATOM_NOEXCEPT -> reverse_iterator {
        return reverse_iterator(begin());
    }

    auto rend() const ATOM_NOEXCEPT -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    auto crend() const ATOM_NOEXCEPT -> const_reverse_iterator {
        return rend();
    }

    ATOM_NODISCARD auto empty() const ATOM_NOEXCEPT -> bool {
        return size() == 0;
    }

    ATOM_NODISCARD auto size() const ATOM_NOEXCEPT -> size_type {
        return size_;
    }

    ATOM_NODISCARD auto maxSize() const ATOM_NOEXCEPT -> size_type {
        return std::numeric_limits<size_type>::max();
    }

    void reserve(size_type new_cap) {
        if (new_cap > capacity()) {
            allocateMemory(new_cap);
            T* newData = data_;
#ifdef ATOM_USE_BOOST
            boost::uninitialized_move(begin(), end(), newData);
#else
            std::uninitialized_move(begin(), end(), newData);
#endif
            clear();
            deallocate();
            data_ = newData;
            capacity_ = new_cap;
        }
    }

    ATOM_NODISCARD auto capacity() const ATOM_NOEXCEPT -> size_type {
        return capacity_;
    }

    void clear() ATOM_NOEXCEPT {
        destroy(begin(), end());
        size_ = 0;
    }

    auto insert(const_iterator pos, const T& value) -> iterator {
        return emplace(pos, value);
    }

    auto insert(const_iterator pos, T&& value) -> iterator {
        return emplace(pos, std::move(value));
    }

    auto insert(const_iterator pos, size_type count,
                const T& value) -> iterator {
        size_type index = pos - begin();
        if (size() + count > capacity()) {
            reserve(std::max(size() + count, capacity() * 2));
        }

        iterator insertPos = begin() + index;
        if (insertPos != end()) {
#ifdef ATOM_USE_BOOST
            boost::uninitialized_move(insertPos, end(), end() + count);
#else
            std::uninitialized_move_n(insertPos, end() - insertPos,
                                      end() + count);
#endif
            destroy(insertPos, insertPos + count);
        }
#ifdef ATOM_USE_BOOST
        boost::uninitialized_fill_n(insertPos, count, value);
#else
        std::uninitialized_fill_n(insertPos, count, value);
#endif
        size_ += count;
        return insertPos;
    }

    template <typename InputIt>
    auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator {
        size_type index = pos - begin();
        size_type count = std::distance(first, last);
        if (size() + count > capacity()) {
            reserve(std::max(size() + count, capacity() * 2));
        }

        iterator insertPos = begin() + index;
        if (insertPos != end()) {
#ifdef ATOM_USE_BOOST
            boost::uninitialized_move(insertPos, end(), end() + count);
#else
            std::uninitialized_move_n(insertPos, end() - insertPos,
                                      end() + count);
#endif
            destroy(insertPos, insertPos + count);
        }
#ifdef ATOM_USE_BOOST
        boost::uninitialized_copy(first, last, insertPos);
#else
        std::uninitialized_copy(first, last, insertPos);
#endif
        size_ += count;
        return insertPos;
    }

    auto insert(const_iterator pos, std::initializer_list<T> init) -> iterator {
        return insert(pos, init.begin(), init.end());
    }

    template <typename... Args>
    auto emplace(const_iterator pos, Args&&... args) -> iterator {
        size_type index = pos - begin();
        if (size() == capacity()) {
            reserve(capacity() == 0 ? 1 : capacity() * 2);
        }

        iterator insertPos = begin() + index;
        if (insertPos != end()) {
#ifdef ATOM_USE_BOOST
            boost::uninitialized_move(insertPos, end(), end() + 1);
#else
            std::uninitialized_move_n(insertPos, end() - insertPos, end() + 1);
#endif
            destroy(insertPos);
        }
        ::new (static_cast<void*>(insertPos)) T(std::forward<Args>(args)...);
        ++size_;
        return insertPos;
    }

    auto erase(const_iterator pos) -> iterator { return erase(pos, pos + 1); }

    auto erase(const_iterator first, const_iterator last) -> iterator {
        auto nonConstFirst = const_cast<iterator>(first);
        auto nonConstLast = const_cast<iterator>(last);
        size_type count = nonConstLast - nonConstFirst;
        destroy(nonConstFirst, nonConstLast);
#ifdef ATOM_USE_BOOST
        boost::uninitialized_move(nonConstLast, end(), nonConstFirst);
#else
        std::uninitialized_move(nonConstLast, end(), nonConstFirst);
#endif
        destroy(end() - count, end());
        size_ -= count;
        return nonConstFirst;
    }

    void pushBack(const T& value) { emplaceBack(value); }

    void pushBack(T&& value) { emplaceBack(std::move(value)); }

    template <typename... Args>
    auto emplaceBack(Args&&... args) -> reference {
        if (size() == capacity()) {
            reserve(capacity() == 0 ? 1 : capacity() * 2);
        }
        ::new (static_cast<void*>(end())) T(std::forward<Args>(args)...);
        ++size_;
        return back();
    }

    void popBack() {
        --size_;
        destroy(end());
    }

    void resize(size_type count, const T& value = T()) {
        if (count < size()) {
            erase(begin() + count, end());
        } else if (count > size()) {
            insert(end(), count - size(), value);
        }
    }

    void swap(SmallVector& other) ATOM_NOEXCEPT {
        using std::swap;
        swap(data_, other.data_);
        swap(size_, other.size_);
        swap(capacity_, other.capacity_);
    }

private:
    // TODO: Here we can use std::aligned_storage to optimize the memory
    auto allocate(size_type n) -> T* {
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate() {
        if (data_ != reinterpret_cast<T*>(static_buffer_.data())) {
            ::operator delete(data_);
        }
    }

    void allocateMemory(size_type n) {
        if (n <= N) {
            data_ = reinterpret_cast<T*>(static_buffer_.data());
            capacity_ = N;
        } else {
            data_ = allocate(n);
            capacity_ = n;
        }
    }

    void moveFrom(SmallVector&& other) {
        if (other.data_ == reinterpret_cast<T*>(other.static_buffer_.data())) {
            data_ = reinterpret_cast<T*>(static_buffer_.data());
#ifdef ATOM_USE_BOOST
            boost::uninitialized_move(other.begin(), other.end(), begin());
#else
            std::uninitialized_move(other.begin(), other.end(), begin());
#endif
        } else {
            data_ = other.data_;
            other.data_ = reinterpret_cast<T*>(other.static_buffer_.data());
        }
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.size_ = 0;
    }

    void destroy(iterator first, iterator last) {
        for (; first != last; ++first) {
            destroy(first);
        }
    }

    void destroy(iterator pos) { pos->~T(); }

    size_type size_ = 0;
    size_type capacity_ = N;
    alignas(T) std::array<unsigned char, N * sizeof(T)> static_buffer_;
    T* data_ = reinterpret_cast<T*>(static_buffer_.data());
};

template <typename T, std::size_t N>
auto operator==(const SmallVector<T, N>& lhs,
                const SmallVector<T, N>& rhs) -> bool {
    return lhs.size() == rhs.size() &&
           std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename T, std::size_t N>
auto operator!=(const SmallVector<T, N>& lhs,
                const SmallVector<T, N>& rhs) -> bool {
    return !(lhs == rhs);
}

template <typename T, std::size_t N>
auto operator<(const SmallVector<T, N>& lhs,
               const SmallVector<T, N>& rhs) -> bool {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(),
                                        rhs.end());
}

template <typename T, std::size_t N>
auto operator<=(const SmallVector<T, N>& lhs,
                const SmallVector<T, N>& rhs) -> bool {
    return !(rhs < lhs);
}

template <typename T, std::size_t N>
auto operator>(const SmallVector<T, N>& lhs,
               const SmallVector<T, N>& rhs) -> bool {
    return rhs < lhs;
}

template <typename T, std::size_t N>
auto operator>=(const SmallVector<T, N>& lhs,
                const SmallVector<T, N>& rhs) -> bool {
    return !(lhs < rhs);
}

template <typename T, std::size_t N>
void swap(SmallVector<T, N>& lhs, SmallVector<T, N>& rhs) ATOM_NOEXCEPT {
    lhs.swap(rhs);
}

#endif