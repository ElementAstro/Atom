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

#ifdef ATOM_USE_BOOST
/**
 * @brief Exception class for PodVector when using Boost
 */
struct PodVectorException : virtual boost::exception, virtual std::exception {
    const char* what() const noexcept override { return "PodVector exception"; }
};
#endif

/**
 * @brief Concept to check if a type is POD (Plain Old Data)
 * @tparam T The type to check
 */
template <typename T>
concept PodType = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

/**
 * @brief Concept to check if a type is a valid value type
 * @tparam T The type to check
 */
template <typename T>
concept ValueType = requires(T t) {
    { std::is_copy_constructible_v<T> };
    { std::is_move_constructible_v<T> };
};

/**
 * @brief A high-performance vector implementation optimized for POD types
 *
 * This class provides a vector-like container specifically optimized for Plain
 * Old Data types. It uses memory-efficient operations like memcpy for better
 * performance with trivial types and supports both standard and Boost-based
 * implementations.
 *
 * @tparam T The POD type to store
 * @tparam Growth The growth factor for capacity expansion (default 2)
 */
template <PodType T, int Growth = 2>
class PodVector {
#ifdef ATOM_USE_BOOST
    using AllocatorType = boost::container::allocator<T>;
    using BoostVector = boost::container::vector<T, AllocatorType>;
#endif

    static constexpr int SIZE_T = sizeof(T);
    static constexpr int N = std::max(1, 64 / SIZE_T);

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
    /**
     * @brief Iterator implementation using Boost's iterator_facade
     */
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

    /**
     * @brief Const iterator implementation using Boost's iterator_facade
     */
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
    /**
     * @brief Random access iterator for PodVector
     */
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

    /**
     * @brief Const random access iterator for PodVector
     */
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

    /**
     * @brief Default constructor
     */
    constexpr PodVector() noexcept = default;

    /**
     * @brief Constructs PodVector from initializer list
     * @param il Initializer list containing elements
     */
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

    /**
     * @brief Constructs PodVector with specified size
     * @param size Initial size of the vector
     */
    explicit constexpr PodVector(int size)
        : size_(size),
          capacity_(std::max(N, size)),
          data_(allocator_.allocate(capacity_)) {}

    /**
     * @brief Copy constructor
     * @param other PodVector to copy from
     */
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

    /**
     * @brief Move constructor
     * @param other PodVector to move from
     */
    PodVector(PodVector&& other) noexcept
        : size_(other.size_),
          capacity_(other.capacity_),
          data_(std::exchange(other.data_, nullptr)) {}

    /**
     * @brief Move assignment operator
     * @param other PodVector to move from
     * @return Reference to this object
     */
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

    /**
     * @brief Copy assignment operator (deleted for performance reasons)
     */
    auto operator=(const PodVector& other) -> PodVector& = delete;

    /**
     * @brief Adds an element to the end of the vector
     * @tparam ValueT Type of the value to add
     * @param t Value to add
     */
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

    /**
     * @brief Constructs an element in-place at the end of the vector
     * @tparam Args Types of constructor arguments
     * @param args Constructor arguments
     */
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

    /**
     * @brief Reserves storage for at least the specified number of elements
     * @param cap New capacity
     */
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

    /**
     * @brief Removes the last element
     */
    constexpr void popBack() noexcept { size_--; }

    /**
     * @brief Removes and returns the last element
     * @return The last element
     */
    constexpr auto popxBack() -> T { return std::move(data_[--size_]); }

    /**
     * @brief Extends the vector with elements from another vector
     * @param other Source vector to copy elements from
     */
    void extend(const PodVector& other) {
        for (const auto& elem : other) {
            pushBack(elem);
        }
    }

    /**
     * @brief Extends the vector with elements from a range
     * @param begin_ptr Pointer to the beginning of the range
     * @param end_ptr Pointer to the end of the range
     */
    void extend(const T* begin_ptr, const T* end_ptr) {
        for (auto it = begin_ptr; it != end_ptr; ++it) {
            pushBack(*it);
        }
    }

    /**
     * @brief Accesses element at specified index
     * @param index Index of the element
     * @return Reference to the element
     */
    constexpr auto operator[](int index) -> T& { return data_[index]; }

    /**
     * @brief Accesses element at specified index (const version)
     * @param index Index of the element
     * @return Const reference to the element
     */
    constexpr auto operator[](int index) const -> const T& {
        return data_[index];
    }

    /**
     * @brief Returns iterator to the beginning
     * @return Iterator to the first element
     */
    constexpr auto begin() noexcept -> iterator { return iterator(data_); }

    /**
     * @brief Returns iterator to the end
     * @return Iterator to one past the last element
     */
    constexpr auto end() noexcept -> iterator {
        return iterator(data_ + size_);
    }

    /**
     * @brief Returns const iterator to the beginning
     * @return Const iterator to the first element
     */
    constexpr auto begin() const noexcept -> const_iterator {
        return const_iterator(data_);
    }

    /**
     * @brief Returns const iterator to the end
     * @return Const iterator to one past the last element
     */
    constexpr auto end() const noexcept -> const_iterator {
        return const_iterator(data_ + size_);
    }

    /**
     * @brief Accesses the last element
     * @return Reference to the last element
     */
    constexpr auto back() -> T& { return data_[size_ - 1]; }

    /**
     * @brief Accesses the last element (const version)
     * @return Const reference to the last element
     */
    constexpr auto back() const -> const T& { return data_[size_ - 1]; }

    /**
     * @brief Checks if the vector is empty
     * @return true if empty, false otherwise
     */
    [[nodiscard]] constexpr auto empty() const noexcept -> bool {
        return size_ == 0;
    }

    /**
     * @brief Returns the number of elements
     * @return Number of elements in the vector
     */
    [[nodiscard]] constexpr auto size() const noexcept -> int { return size_; }

    /**
     * @brief Returns pointer to the underlying data
     * @return Pointer to the data array
     */
    constexpr auto data() noexcept -> T* { return data_; }

    /**
     * @brief Returns const pointer to the underlying data
     * @return Const pointer to the data array
     */
    constexpr auto data() const noexcept -> const T* { return data_; }

    /**
     * @brief Clears the vector content
     */
    constexpr void clear() noexcept { size_ = 0; }

    /**
     * @brief Inserts an element at the specified position
     * @tparam ValueT Type of the value to insert
     * @param i Index where to insert
     * @param val Value to insert
     */
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

    /**
     * @brief Erases element at the specified position
     * @param i Index of the element to erase
     */
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

    /**
     * @brief Reverses the order of elements
     */
    constexpr void reverse() { std::ranges::reverse(data_, data_ + size_); }

    /**
     * @brief Resizes the vector to specified size
     * @param new_size New size of the vector
     */
    constexpr void resize(int new_size) {
        if (new_size > capacity_) {
            reserve(new_size);
        }
        size_ = new_size;
    }

    /**
     * @brief Detaches the internal data array from the vector
     * @return Pair containing the data pointer and current size
     */
    auto detach() noexcept -> std::pair<T*, int> {
        T* p = data_;
        int current_size = size_;
        data_ = nullptr;
        size_ = 0;
        return {p, current_size};
    }

    /**
     * @brief Destructor
     */
    ~PodVector() {
        if (data_ != nullptr) {
#ifdef ATOM_USE_BOOST
            try {
                allocator_.deallocate(data_, capacity_);
            } catch (...) {
            }
#else
            allocator_.deallocate(data_, capacity_);
#endif
        }
    }

    /**
     * @brief Returns the current capacity
     * @return Current capacity of the vector
     */
    [[nodiscard]] constexpr auto capacity() const noexcept -> int {
        return capacity_;
    }
};

}  // namespace atom::type

#endif  // ATOM_TYPE_POD_VECTOR_HPP
