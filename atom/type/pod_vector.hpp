#ifndef ATOM_TYPE_POD_VECTOR_HPP
#define ATOM_TYPE_POD_VECTOR_HPP

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

#ifdef ATOM_USE_BOOST
#include <boost/container/vector.hpp>
#include <boost/exception/all.hpp>
#include <boost/iterator/iterator_facade.hpp>
#endif

namespace atom::type {

// Define Boost-specific exceptions if Boost is used
#ifdef ATOM_USE_BOOST
struct PodVectorException : virtual boost::exception, virtual std::exception {
    const char* what() const noexcept override { return "PodVector exception"; }
};
#endif

template <typename T>
concept PodType = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

template <typename T>
concept ValueType = requires(T t) {
    { std::is_copy_constructible_v<T> };
    { std::is_move_constructible_v<T> };
};

template <PodType T, int Growth = 2>
class PodVector {
#ifdef ATOM_USE_BOOST
    using AllocatorType = boost::container::allocator<T>;
    using BoostVector = boost::container::vector<T, AllocatorType>;
#endif

    static constexpr int SIZE_T = sizeof(T);
    // 修改 N 的计算方式，确保至少为 1
    static constexpr int N = std::max(1, 64 / SIZE_T);

    // 移除限制元素大小的断言
    // static_assert(N >= 4, "Element size too large");

private:
    int size_ = 0;
    int capacity_ = N;
    std::allocator<T> allocator_;
    T* data_ = allocator_.allocate(capacity_);

#ifdef ATOM_USE_BOOST
    BoostVector boost_vector_{};
#endif

public:
    using size_type = int;

#ifdef ATOM_USE_BOOST
    // Iterator using Boost's iterator_facade
    class iterator
        : public boost::iterator_facade<iterator, T,
                                        boost::random_access_traversal_tag> {
    public:
        iterator() = default;
        iterator(T* ptr) : ptr_(ptr) {}

    private:
        friend class boost::iterator_core_access;

        void increment() { ++ptr_; }
        void decrement() { --ptr_; }
        void advance(difference_type n) { ptr_ += n; }
        difference_type distance_to(iterator other) const {
            return other.ptr_ - ptr_;
        }
        bool equal(iterator other) const { return ptr_ == other.ptr_; }
        reference dereference() const { return *ptr_; }

        T* ptr_ = nullptr;
    };

    class const_iterator
        : public boost::iterator_facade<const_iterator, const T,
                                        boost::random_access_traversal_tag> {
    public:
        const_iterator() = default;
        const_iterator(const T* ptr) : ptr_(ptr) {}

    private:
        friend class boost::iterator_core_access;

        void increment() { ++ptr_; }
        void decrement() { --ptr_; }
        void advance(difference_type n) { ptr_ += n; }
        difference_type distance_to(const_iterator other) const {
            return other.ptr_ - ptr_;
        }
        bool equal(const_iterator other) const { return ptr_ == other.ptr_; }
        reference dereference() const { return *ptr_; }

        const T* ptr_ = nullptr;
    };
#else
    // Existing iterator implementation
    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator(pointer ptr) : ptr_(ptr) {}

        auto operator*() const -> reference { return *ptr_; }
        auto operator->() -> pointer { return ptr_; }

        auto operator++() -> iterator& {
            ++ptr_;
            return *this;
        }

        auto operator++(int) -> iterator {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        auto operator--() -> iterator& {
            --ptr_;
            return *this;
        }

        auto operator--(int) -> iterator {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }

        auto operator+(difference_type offset) const -> iterator {
            return iterator(ptr_ + offset);
        }

        auto operator-(difference_type offset) const -> iterator {
            return iterator(ptr_ - offset);
        }

        auto operator+=(difference_type offset) -> iterator& {
            ptr_ += offset;
            return *this;
        }

        auto operator-=(difference_type offset) -> iterator& {
            ptr_ -= offset;
            return *this;
        }

        auto operator-(const iterator& other) const -> difference_type {
            return ptr_ - other.ptr_;
        }

        auto operator==(const iterator& other) const -> bool {
            return ptr_ == other.ptr_;
        }

        auto operator!=(const iterator& other) const -> bool {
            return ptr_ != other.ptr_;
        }

        auto operator<(const iterator& other) const -> bool {
            return ptr_ < other.ptr_;
        }

        auto operator>(const iterator& other) const -> bool {
            return ptr_ > other.ptr_;
        }

        auto operator<=(const iterator& other) const -> bool {
            return ptr_ <= other.ptr_;
        }

        auto operator>=(const iterator& other) const -> bool {
            return ptr_ >= other.ptr_;
        }

    private:
        pointer ptr_;
    };

    class const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator(pointer ptr) : ptr_(ptr) {}

        auto operator*() const -> reference { return *ptr_; }
        auto operator->() const -> pointer { return ptr_; }

        auto operator++() -> const_iterator& {
            ++ptr_;
            return *this;
        }

        auto operator++(int) -> const_iterator {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        auto operator--() -> const_iterator& {
            --ptr_;
            return *this;
        }

        auto operator--(int) -> const_iterator {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        auto operator+(difference_type offset) const -> const_iterator {
            return const_iterator(ptr_ + offset);
        }

        auto operator-(difference_type offset) const -> const_iterator {
            return const_iterator(ptr_ - offset);
        }

        auto operator+=(difference_type offset) -> const_iterator& {
            ptr_ += offset;
            return *this;
        }

        auto operator-=(difference_type offset) -> const_iterator& {
            ptr_ -= offset;
            return *this;
        }

        auto operator-(const const_iterator& other) const -> difference_type {
            return ptr_ - other.ptr_;
        }

        auto operator==(const const_iterator& other) const -> bool {
            return ptr_ == other.ptr_;
        }

        auto operator!=(const const_iterator& other) const -> bool {
            return ptr_ != other.ptr_;
        }

        auto operator<(const const_iterator& other) const -> bool {
            return ptr_ < other.ptr_;
        }

        auto operator>(const const_iterator& other) const -> bool {
            return ptr_ > other.ptr_;
        }

        auto operator<=(const const_iterator& other) const -> bool {
            return ptr_ <= other.ptr_;
        }

        auto operator>=(const const_iterator& other) const -> bool {
            return ptr_ >= other.ptr_;
        }

    private:
        pointer ptr_;
    };
#endif

    constexpr PodVector() noexcept = default;

    constexpr PodVector(std::initializer_list<T> il)
        : size_(static_cast<int>(il.size())),
          capacity_(std::max(N, static_cast<int>(il.size()))),
          data_(allocator_.allocate(capacity_)) {
#ifdef ATOM_USE_BOOST
        try {
            std::ranges::copy(il, data_);
        } catch (...) {
            throw PodVectorException();
        }
#else
        std::ranges::copy(il, data_);
#endif
    }

    explicit constexpr PodVector(int size)
        : size_(size),
          capacity_(std::max(N, size)),
          data_(allocator_.allocate(capacity_)) {}

    PodVector(const PodVector& other)
        : size_(other.size_),
          capacity_(other.capacity_),
          data_(allocator_.allocate(capacity_)) {
#ifdef ATOM_USE_BOOST
        try {
            std::memcpy(data_, other.data_, SIZE_T * size_);
        } catch (...) {
            throw PodVectorException();
        }
#else
        std::memcpy(data_, other.data_, SIZE_T * size_);
#endif
    }

    PodVector(PodVector&& other) noexcept
        : size_(other.size_),
          capacity_(other.capacity_),
          data_(std::exchange(other.data_, nullptr)) {}

    auto operator=(PodVector&& other) noexcept -> PodVector& {
        if (this != &other) {
            if (data_ != nullptr) {
                allocator_.deallocate(data_, capacity_);
            }
            size_ = other.size_;
            capacity_ = other.capacity_;
            data_ = std::exchange(other.data_, nullptr);
        }
        return *this;
    }

    auto operator=(const PodVector& other) -> PodVector& = delete;

    template <typename ValueT>
    void pushBack(ValueT&& t) {
#ifdef ATOM_USE_BOOST
        try {
            if (size_ == capacity_) [[unlikely]] {
                reserve(capacity_ * Growth);
            }
            data_[size_++] = std::forward<ValueT>(t);
        } catch (...) {
            throw PodVectorException();
        }
#else
        if (size_ == capacity_) [[unlikely]] {
            reserve(capacity_ * Growth);
        }
        data_[size_++] = std::forward<ValueT>(t);
#endif
    }

    template <typename... Args>
    void emplaceBack(Args&&... args) {
#ifdef ATOM_USE_BOOST
        try {
            if (size_ == capacity_) [[unlikely]] {
                reserve(capacity_ * Growth);
            }
            new (&data_[size_++]) T(std::forward<Args>(args)...);
        } catch (...) {
            throw PodVectorException();
        }
#else
        if (size_ == capacity_) [[unlikely]] {
            reserve(capacity_ * Growth);
        }
        new (&data_[size_++]) T(std::forward<Args>(args)...);
#endif
    }

    constexpr void reserve(int cap) {
        if (cap <= capacity_) [[likely]] {
            return;
        }
#ifdef ATOM_USE_BOOST
        try {
            T* newData = allocator_.allocate(cap);
            if (data_ != nullptr) {
                std::memcpy(newData, data_, SIZE_T * size_);
                allocator_.deallocate(data_, capacity_);
            }
            data_ = newData;
            capacity_ = cap;
        } catch (...) {
            throw PodVectorException();
        }
#else
        T* newData = allocator_.allocate(cap);
        if (data_ != nullptr) {
            std::memcpy(newData, data_, SIZE_T * size_);
            allocator_.deallocate(data_, capacity_);
        }
        data_ = newData;
        capacity_ = cap;
#endif
    }

    constexpr void popBack() noexcept { size_--; }

    constexpr auto popxBack() -> T { return std::move(data_[--size_]); }

    void extend(const PodVector& other) {
        for (const auto& elem : other) {
            pushBack(elem);
        }
    }

    void extend(const T* begin_ptr, const T* end_ptr) {
        for (auto it = begin_ptr; it != end_ptr; ++it) {
            pushBack(*it);
        }
    }

    constexpr auto operator[](int index) -> T& { return data_[index]; }
    constexpr auto operator[](int index) const -> const T& {
        return data_[index];
    }

#ifdef ATOM_USE_BOOST
    constexpr auto begin() noexcept -> iterator { return iterator(data_); }
    constexpr auto end() noexcept -> iterator {
        return iterator(data_ + size_);
    }
    constexpr auto begin() const noexcept -> const_iterator {
        return const_iterator(data_);
    }
    constexpr auto end() const noexcept -> const_iterator {
        return const_iterator(data_ + size_);
    }
#else
    constexpr auto begin() noexcept -> iterator { return iterator(data_); }
    constexpr auto end() noexcept -> iterator {
        return iterator(data_ + size_);
    }
    constexpr auto begin() const noexcept -> const_iterator {
        return const_iterator(data_);
    }
    constexpr auto end() const noexcept -> const_iterator {
        return const_iterator(data_ + size_);
    }
#endif

    constexpr auto back() -> T& { return data_[size_ - 1]; }
    constexpr auto back() const -> const T& { return data_[size_ - 1]; }

    [[nodiscard]] constexpr auto empty() const noexcept -> bool {
        return size_ == 0;
    }

    [[nodiscard]] constexpr auto size() const noexcept -> int { return size_; }

    constexpr auto data() noexcept -> T* { return data_; }
    constexpr auto data() const noexcept -> const T* { return data_; }

    constexpr void clear() noexcept { size_ = 0; }

    template <typename ValueT>
    void insert(int i, ValueT&& val) {
#ifdef ATOM_USE_BOOST
        try {
            if (size_ == capacity_) {
                reserve(capacity_ * Growth);
            }
            for (int j = size_; j > i; j--) {
                data_[j] = data_[j - 1];
            }
            data_[i] = std::forward<ValueT>(val);
            size_++;
        } catch (...) {
            throw PodVectorException();
        }
#else
        if (size_ == capacity_) {
            reserve(capacity_ * Growth);
        }
        for (int j = size_; j > i; j--) {
            data_[j] = data_[j - 1];
        }
        data_[i] = std::forward<ValueT>(val);
        size_++;
#endif
    }

    constexpr void erase(int i) {
#ifdef ATOM_USE_BOOST
        try {
            std::ranges::copy(data_ + i + 1, data_ + size_, data_ + i);
            size_--;
        } catch (...) {
            throw PodVectorException();
        }
#else
        std::ranges::copy(data_ + i + 1, data_ + size_, data_ + i);
        size_--;
#endif
    }

    constexpr void reverse() { std::ranges::reverse(data_, data_ + size_); }

    constexpr void resize(int new_size) {
        if (new_size > capacity_) {
            reserve(new_size);
        }
        size_ = new_size;
    }

    auto detach() noexcept -> std::pair<T*, int> {
        T* p = data_;
        int current_size = size_;
        data_ = nullptr;
        size_ = 0;
        return {p, current_size};
    }

    ~PodVector() {
        if (data_ != nullptr) {
#ifdef ATOM_USE_BOOST
            try {
                allocator_.deallocate(data_, capacity_);
            } catch (...) {
                // Suppress all exceptions in destructor
            }
#else
            allocator_.deallocate(data_, capacity_);
#endif
        }
    }

    [[nodiscard]] constexpr auto capacity() const noexcept -> int {
        return capacity_;
    }
};

}  // namespace atom::type

#endif  // ATOM_TYPE_POD_VECTOR_HPP