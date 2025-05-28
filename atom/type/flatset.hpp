#ifndef ATOM_TYPE_FLAT_SET_HPP
#define ATOM_TYPE_FLAT_SET_HPP

#include <algorithm>
#include <concepts>
#include <execution>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace atom::type {

/**
 * @brief A flat set implementation using a sorted vector.
 *
 * This container provides set semantics with contiguous memory layout for
 * better cache locality. It trades insertion/deletion performance for faster
 * lookups and iteration.
 *
 * @tparam T The type of elements.
 * @tparam Compare The comparison function object type (default is
 * std::less<T>).
 */
template <typename T, typename Compare = std::less<T>>
    requires std::predicate<Compare, T, T>
class FlatSet {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = typename std::vector<T>::const_iterator;
    using const_iterator = typename std::vector<T>::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using key_compare = Compare;
    using value_compare = Compare;

    static constexpr size_type DEFAULT_CAPACITY = 16;
    static constexpr size_type PARALLEL_THRESHOLD = 10000;
    static constexpr double GROWTH_FACTOR = 1.5;

private:
    std::vector<T> data_;
    Compare comp_;

    auto grow_capacity() -> size_type {
        auto current = data_.capacity();
        return current == 0 ? DEFAULT_CAPACITY
                            : static_cast<size_type>(current * GROWTH_FACTOR);
    }

    void ensure_capacity(size_type min_capacity) {
        if (data_.capacity() < min_capacity) {
            data_.reserve(std::max(min_capacity, grow_capacity()));
        }
    }

    auto lower_bound_impl(const T& value) const -> const_iterator {
        return size() > PARALLEL_THRESHOLD
                   ? std::lower_bound(std::execution::par_unseq, data_.begin(),
                                      data_.end(), value, comp_)
                   : std::lower_bound(data_.begin(), data_.end(), value, comp_);
    }

    auto upper_bound_impl(const T& value) const -> const_iterator {
        return size() > PARALLEL_THRESHOLD
                   ? std::upper_bound(std::execution::par_unseq, data_.begin(),
                                      data_.end(), value, comp_)
                   : std::upper_bound(data_.begin(), data_.end(), value, comp_);
    }

    void sort_and_unique() {
        if (data_.empty())
            return;

        if (data_.size() > PARALLEL_THRESHOLD) {
            std::sort(std::execution::par_unseq, data_.begin(), data_.end(),
                      comp_);
        } else {
            std::sort(data_.begin(), data_.end(), comp_);
        }

        auto last_unique = std::unique(data_.begin(), data_.end(),
                                       [this](const T& a, const T& b) {
                                           return !comp_(a, b) && !comp_(b, a);
                                       });

        data_.erase(last_unique, data_.end());

        if (data_.capacity() > data_.size() * 2) {
            data_.shrink_to_fit();
        }
    }

public:
    /**
     * @brief Default constructor.
     */
    FlatSet() noexcept(noexcept(Compare())) {
        try {
            data_.reserve(DEFAULT_CAPACITY);
        } catch (...) {
        }
    }

    /**
     * @brief Constructs a FlatSet with a custom comparator.
     *
     * @param comp The comparator to use.
     */
    explicit FlatSet(const Compare& comp) noexcept(
        std::is_nothrow_copy_constructible_v<Compare>)
        : comp_(comp) {
        try {
            data_.reserve(DEFAULT_CAPACITY);
        } catch (...) {
        }
    }

    /**
     * @brief Constructs a FlatSet from a range of elements.
     *
     * @tparam InputIt The type of the input iterator.
     * @param first The beginning of the range.
     * @param last The end of the range.
     * @param comp The comparator to use (default is Compare()).
     */
    template <std::input_iterator InputIt>
    FlatSet(InputIt first, InputIt last, const Compare& comp = Compare())
        : comp_(comp) {
        if constexpr (std::sized_sentinel_for<InputIt, InputIt>) {
            if (first > last) {
                throw std::invalid_argument(
                    "Invalid iterator range: first > last");
            }
            auto size = std::distance(first, last);
            data_.reserve(size);
        } else {
            data_.reserve(DEFAULT_CAPACITY);
        }

        data_.assign(first, last);
        sort_and_unique();
    }

    /**
     * @brief Constructs a FlatSet from an initializer list.
     *
     * @param init The initializer list.
     * @param comp The comparator to use (default is Compare()).
     */
    FlatSet(std::initializer_list<T> init, const Compare& comp = Compare())
        : FlatSet(init.begin(), init.end(), comp) {}

    FlatSet(const FlatSet& other) = default;
    FlatSet(FlatSet&& other) noexcept = default;
    FlatSet& operator=(const FlatSet& other) = default;
    FlatSet& operator=(FlatSet&& other) noexcept = default;
    ~FlatSet() = default;

    /**
     * @brief Returns an iterator to the beginning.
     */
    iterator begin() noexcept { return data_.begin(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }

    /**
     * @brief Returns an iterator to the end.
     */
    iterator end() noexcept { return data_.end(); }
    const_iterator end() const noexcept { return data_.end(); }
    const_iterator cend() const noexcept { return data_.cend(); }

    /**
     * @brief Returns a reverse iterator to the beginning.
     */
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }
    const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(cend());
    }

    /**
     * @brief Returns a reverse iterator to the end.
     */
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }
    const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(cbegin());
    }

    /**
     * @brief Checks if the set is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    /**
     * @brief Returns the number of elements in the set.
     */
    [[nodiscard]] size_type size() const noexcept { return data_.size(); }

    /**
     * @brief Returns the maximum possible number of elements in the set.
     */
    [[nodiscard]] size_type max_size() const noexcept {
        return data_.max_size();
    }

    /**
     * @brief Returns the current capacity of the underlying container.
     */
    [[nodiscard]] size_type capacity() const noexcept {
        return data_.capacity();
    }

    /**
     * @brief Reserves storage for at least the specified number of elements.
     *
     * @param new_cap The new capacity.
     */
    void reserve(size_type new_cap) { data_.reserve(new_cap); }

    /**
     * @brief Reduces memory usage by freeing unused capacity.
     */
    void shrink_to_fit() noexcept {
        try {
            data_.shrink_to_fit();
        } catch (...) {
        }
    }

    /**
     * @brief Clears the set.
     */
    void clear() noexcept { data_.clear(); }

    /**
     * @brief Inserts a value into the set.
     *
     * @param value The value to insert.
     * @return A pair consisting of an iterator to the inserted element (or to
     * the element that prevented the insertion) and a bool denoting whether the
     * insertion took place.
     */
    std::pair<iterator, bool> insert(const T& value) {
        auto pos = lower_bound_impl(value);

        if (pos != end() && !comp_(value, *pos) && !comp_(*pos, value)) {
            return {pos, false};
        }

        ensure_capacity(size() + 1);
        return {data_.insert(pos, value), true};
    }

    /**
     * @brief Inserts a value into the set.
     *
     * @param value The value to insert.
     * @return A pair consisting of an iterator to the inserted element (or to
     * the element that prevented the insertion) and a bool denoting whether the
     * insertion took place.
     */
    std::pair<iterator, bool> insert(T&& value) {
        auto pos = lower_bound_impl(value);

        if (pos != end() && !comp_(value, *pos) && !comp_(*pos, value)) {
            return {pos, false};
        }

        ensure_capacity(size() + 1);
        return {data_.insert(pos, std::move(value)), true};
    }

    /**
     * @brief Inserts a value into the set with a hint.
     *
     * @param hint An iterator to the position before which the value will be
     * inserted.
     * @param value The value to insert.
     * @return An iterator to the inserted element.
     */
    iterator insert(const_iterator hint, const T& value) {
        if (hint < begin() || hint > end()) {
            throw std::invalid_argument("Invalid hint provided to insert");
        }

        if (hint != end() && !comp_(value, *hint) && !comp_(*hint, value)) {
            return data_.insert(hint, value);
        }

        if (hint != begin() && comp_(*(std::prev(hint)), value) &&
            comp_(value, *hint)) {
            return data_.insert(hint, value);
        }

        return insert(value).first;
    }

    /**
     * @brief Inserts a value into the set with a hint.
     *
     * @param hint An iterator to the position before which the value will be
     * inserted.
     * @param value The value to insert.
     * @return An iterator to the inserted element.
     */
    iterator insert(const_iterator hint, T&& value) {
        if (hint < begin() || hint > end()) {
            throw std::invalid_argument("Invalid hint provided to insert");
        }

        if (hint == end() || comp_(value, *hint)) {
            if (hint == begin() || comp_(*(std::prev(hint)), value)) {
                return data_.insert(hint, std::move(value));
            }
        }

        return insert(std::move(value)).first;
    }

    /**
     * @brief Inserts a range of values into the set.
     *
     * @tparam InputIt The type of the input iterator.
     * @param first The beginning of the range.
     * @param last The end of the range.
     */
    template <std::input_iterator InputIt>
    void insert(InputIt first, InputIt last) {
        if constexpr (std::sized_sentinel_for<InputIt, InputIt>) {
            if (first > last) {
                throw std::invalid_argument(
                    "Invalid iterator range: first > last");
            }
        }

        if constexpr (std::random_access_iterator<InputIt>) {
            auto range_size = std::distance(first, last);
            if (range_size > 1000) {
                std::vector<T> temp(first, last);
                temp.insert(temp.end(), data_.begin(), data_.end());
                data_.swap(temp);
                sort_and_unique();
                return;
            }
        }

        while (first != last) {
            insert(*first++);
        }
    }

    /**
     * @brief Inserts values from an initializer list into the set.
     *
     * @param ilist The initializer list.
     */
    void insert(std::initializer_list<T> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    /**
     * @brief Constructs and inserts a value into the set.
     *
     * @tparam Args The types of the arguments.
     * @param args The arguments to construct the value.
     * @return A pair consisting of an iterator to the inserted element (or to
     * the element that prevented the insertion) and a bool denoting whether the
     * insertion took place.
     */
    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return insert(T(std::forward<Args>(args)...));
    }

    /**
     * @brief Constructs and inserts a value into the set with a hint.
     *
     * @tparam Args The types of the arguments.
     * @param hint An iterator to the position before which the value will be
     * inserted.
     * @param args The arguments to construct the value.
     * @return An iterator to the inserted element.
     */
    template <typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return insert(hint, T(std::forward<Args>(args)...));
    }

    /**
     * @brief Erases an element from the set.
     *
     * @param pos An iterator to the element to erase.
     * @return An iterator to the element following the erased element.
     */
    iterator erase(const_iterator pos) {
        if (pos < begin() || pos >= end()) {
            throw std::invalid_argument("Invalid iterator position for erase");
        }
        return data_.erase(pos);
    }

    /**
     * @brief Erases a range of elements from the set.
     *
     * @param first An iterator to the first element to erase.
     * @param last An iterator to the element following the last element to
     * erase.
     * @return An iterator to the element following the erased elements.
     */
    iterator erase(const_iterator first, const_iterator last) {
        if (first < begin() || last > end() || first > last) {
            throw std::invalid_argument("Invalid iterator range for erase");
        }
        return data_.erase(first, last);
    }

    /**
     * @brief Erases an element from the set by value.
     *
     * @param value The value to erase.
     * @return The number of elements erased.
     */
    size_type erase(const T& value) {
        auto it = find(value);
        if (it != end()) {
            erase(it);
            return 1;
        }
        return 0;
    }

    /**
     * @brief Swaps the contents of this set with another set.
     *
     * @param other The other set to swap with.
     */
    void swap(FlatSet& other) noexcept(
        std::is_nothrow_swappable_v<std::vector<T>> &&
        std::is_nothrow_swappable_v<Compare>) {
        std::swap(data_, other.data_);
        std::swap(comp_, other.comp_);
    }

    /**
     * @brief Returns the number of elements matching a value.
     *
     * @param value The value to match.
     * @return The number of elements matching the value.
     */
    size_type count(const T& value) const { return contains(value) ? 1 : 0; }

    /**
     * @brief Finds an element in the set.
     *
     * @param value The value to find.
     * @return An iterator to the element, or end() if not found.
     */
    iterator find(const T& value) const {
        auto pos = lower_bound_impl(value);
        if (pos != end() && !comp_(value, *pos) && !comp_(*pos, value)) {
            return pos;
        }
        return end();
    }

    /**
     * @brief Returns a range of elements matching a value.
     *
     * @param value The value to match.
     * @return A pair of iterators to the range of elements.
     */
    std::pair<iterator, iterator> equal_range(const T& value) const {
        return size() > PARALLEL_THRESHOLD
                   ? std::equal_range(std::execution::par_unseq, data_.begin(),
                                      data_.end(), value, comp_)
                   : std::equal_range(data_.begin(), data_.end(), value, comp_);
    }

    /**
     * @brief Returns an iterator to the first element not less than the given
     * value.
     *
     * @param value The value to compare.
     * @return An iterator to the first element not less than the given value.
     */
    iterator lower_bound(const T& value) const {
        return lower_bound_impl(value);
    }

    /**
     * @brief Returns an iterator to the first element greater than the given
     * value.
     *
     * @param value The value to compare.
     * @return An iterator to the first element greater than the given value.
     */
    iterator upper_bound(const T& value) const {
        return upper_bound_impl(value);
    }

    /**
     * @brief Returns the comparison function object.
     */
    key_compare key_comp() const { return comp_; }

    /**
     * @brief Returns the comparison function object.
     */
    value_compare value_comp() const { return comp_; }

    /**
     * @brief Checks if the set contains a value.
     *
     * @param value The value to check.
     * @return True if the value is found, false otherwise.
     */
    bool contains(const T& value) const { return find(value) != end(); }

    /**
     * @brief Returns a view of the underlying data as a range.
     *
     * @return A view of the underlying data.
     */
    auto view() const { return std::ranges::subrange(begin(), end()); }
};

/**
 * @brief Equality comparison operator for FlatSets.
 */
template <typename T, typename Compare>
bool operator==(const FlatSet<T, Compare>& lhs,
                const FlatSet<T, Compare>& rhs) {
    return lhs.size() == rhs.size() &&
           std::ranges::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Lexicographical comparison operator for FlatSets.
 */
template <typename T, typename Compare>
auto operator<=>(const FlatSet<T, Compare>& lhs,
                 const FlatSet<T, Compare>& rhs) {
    return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(),
                                                  rhs.begin(), rhs.end());
}

/**
 * @brief Swaps the contents of two FlatSets.
 */
template <typename T, typename Compare>
void swap(FlatSet<T, Compare>& lhs,
          FlatSet<T, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

}  // namespace atom::type

#endif  // ATOM_TYPE_FLAT_SET_HPP
