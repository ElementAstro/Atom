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

    // Default initial capacity to reduce reallocations
    static constexpr size_type DEFAULT_CAPACITY = 16;

private:
    std::vector<T> m_data_;
    Compare m_comp_;

public:
    /**
     * @brief Default constructor.
     */
    FlatSet() noexcept(noexcept(Compare())) {
        try {
            m_data_.reserve(DEFAULT_CAPACITY);
        } catch (const std::bad_alloc& e) {
            // Fall back to default capacity handling if reserve fails
        }
    }

    /**
     * @brief Destructor.
     */
    ~FlatSet() = default;

    /**
     * @brief Constructs a FlatSet with a custom comparator.
     *
     * @param comp The comparator to use.
     */
    explicit FlatSet(const Compare& comp) noexcept(
        std::is_nothrow_copy_constructible_v<Compare>)
        : m_comp_(comp) {
        try {
            m_data_.reserve(DEFAULT_CAPACITY);
        } catch (const std::bad_alloc& e) {
            // Fall back to default capacity handling if reserve fails
        }
    }

    /**
     * @brief Constructs a FlatSet from a range of elements.
     *
     * @tparam InputIt The type of the input iterator.
     * @param first The beginning of the range.
     * @param last The end of the range.
     * @param comp The comparator to use (default is Compare()).
     * @throws std::invalid_argument If the input iterators are invalid.
     */
    template <std::input_iterator InputIt>
    FlatSet(InputIt first, InputIt last, const Compare& comp = Compare())
        : m_comp_(comp) {
        try {
            // Validate input iterators
            if constexpr (std::sized_sentinel_for<InputIt, InputIt>) {
                if (first > last) {
                    throw std::invalid_argument(
                        "Iterator range is invalid: first > last");
                }
                auto size = std::distance(first, last);
                m_data_.reserve(size);
            } else {
                m_data_.reserve(DEFAULT_CAPACITY);
            }

            m_data_.assign(first, last);

            // Use parallel sort for large datasets
            if (m_data_.size() > 10000) {
                std::sort(std::execution::par_unseq, m_data_.begin(),
                          m_data_.end(), m_comp_);
            } else {
                std::sort(m_data_.begin(), m_data_.end(), m_comp_);
            }

            // Remove duplicates
            auto last_unique = std::unique(
                m_data_.begin(), m_data_.end(), [this](const T& a, const T& b) {
                    return !m_comp_(a, b) && !m_comp_(b, a);
                });

            m_data_.erase(last_unique, m_data_.end());

            // Shrink to fit if we have significant excess capacity
            if (m_data_.capacity() > m_data_.size() * 1.5) {
                m_data_.shrink_to_fit();
            }
        } catch (const std::exception& e) {
            clear();  // Ensure no memory leaks
            throw;
        }
    }

    /**
     * @brief Constructs a FlatSet from an initializer list.
     *
     * @param init The initializer list.
     * @param comp The comparator to use (default is Compare()).
     */
    FlatSet(std::initializer_list<T> init, const Compare& comp = Compare())
        : FlatSet(init.begin(), init.end(), comp) {}

    /**
     * @brief Copy constructor.
     *
     * @param other The other FlatSet to copy from.
     */
    FlatSet(const FlatSet& other) = default;

    /**
     * @brief Move constructor.
     *
     * @param other The other FlatSet to move from.
     */
    FlatSet(FlatSet&& other) noexcept = default;

    /**
     * @brief Copy assignment operator.
     *
     * @param other The other FlatSet to copy from.
     * @return A reference to this FlatSet.
     */
    FlatSet& operator=(const FlatSet& other) = default;

    /**
     * @brief Move assignment operator.
     *
     * @param other The other FlatSet to move from.
     * @return A reference to this FlatSet.
     */
    FlatSet& operator=(FlatSet&& other) noexcept = default;

    /**
     * @brief Returns an iterator to the beginning.
     *
     * @return An iterator to the beginning.
     */
    iterator begin() noexcept { return m_data_.begin(); }

    /**
     * @brief Returns a const iterator to the beginning.
     *
     * @return A const iterator to the beginning.
     */
    const_iterator begin() const noexcept { return m_data_.begin(); }

    /**
     * @brief Returns a const iterator to the beginning.
     *
     * @return A const iterator to the beginning.
     */
    const_iterator cbegin() const noexcept { return m_data_.cbegin(); }

    /**
     * @brief Returns an iterator to the end.
     *
     * @return An iterator to the end.
     */
    iterator end() noexcept { return m_data_.end(); }

    /**
     * @brief Returns a const iterator to the end.
     *
     * @return A const iterator to the end.
     */
    const_iterator end() const noexcept { return m_data_.end(); }

    /**
     * @brief Returns a const iterator to the end.
     *
     * @return A const iterator to the end.
     */
    const_iterator cend() const noexcept { return m_data_.cend(); }

    /**
     * @brief Returns a reverse iterator to the beginning.
     *
     * @return A reverse iterator to the beginning.
     */
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

    /**
     * @brief Returns a const reverse iterator to the beginning.
     *
     * @return A const reverse iterator to the beginning.
     */
    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }

    /**
     * @brief Returns a const reverse iterator to the beginning.
     *
     * @return A const reverse iterator to the beginning.
     */
    const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(cend());
    }

    /**
     * @brief Returns a reverse iterator to the end.
     *
     * @return A reverse iterator to the end.
     */
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

    /**
     * @brief Returns a const reverse iterator to the end.
     *
     * @return A const reverse iterator to the end.
     */
    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }

    /**
     * @brief Returns a const reverse iterator to the end.
     *
     * @return A const reverse iterator to the end.
     */
    const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(cbegin());
    }

    /**
     * @brief Checks if the set is empty.
     *
     * @return True if the set is empty, false otherwise.
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        return m_data_.empty();
    }

    /**
     * @brief Returns the number of elements in the set.
     *
     * @return The number of elements in the set.
     */
    [[nodiscard]] auto size() const noexcept -> size_type {
        return m_data_.size();
    }

    /**
     * @brief Returns the maximum possible number of elements in the set.
     *
     * @return The maximum possible number of elements in the set.
     */
    [[nodiscard]] auto max_size() const noexcept -> size_type {
        return m_data_.max_size();
    }

    /**
     * @brief Returns the current capacity of the underlying container.
     *
     * @return The current capacity.
     */
    [[nodiscard]] auto capacity() const noexcept -> size_type {
        return m_data_.capacity();
    }

    /**
     * @brief Reserves storage for at least the specified number of elements.
     *
     * @param new_cap The new capacity.
     * @throws std::length_error If new_cap > max_size().
     */
    void reserve(size_type new_cap) {
        try {
            m_data_.reserve(new_cap);
        } catch (const std::length_error& e) {
            throw std::length_error("FlatSet::reserve: " +
                                    std::string(e.what()));
        } catch (const std::bad_alloc& e) {
            throw std::bad_alloc();
        }
    }

    /**
     * @brief Reduces memory usage by freeing unused capacity.
     */
    void shrink_to_fit() noexcept {
        try {
            m_data_.shrink_to_fit();
        } catch (...) {
            // Ignore any exceptions as this is an optimization
        }
    }

    /**
     * @brief Clears the set.
     */
    void clear() noexcept { m_data_.clear(); }

    /**
     * @brief Inserts a value into the set.
     *
     * @param value The value to insert.
     * @return A pair consisting of an iterator to the inserted element (or to
     * the element that prevented the insertion) and a bool denoting whether the
     * insertion took place.
     */
    auto insert(const T& value) -> std::pair<iterator, bool> {
        try {
            // Use binary search to find the insertion point
            auto pos = lowerBound(value);

            // Check if element already exists
            if (pos != end() && !m_comp_(value, *pos) &&
                !m_comp_(*pos, value)) {
                return {pos, false};
            }

            // Grow capacity if we're close to full to reduce reallocations
            if (m_data_.size() == m_data_.capacity()) {
                size_type new_capacity = m_data_.capacity() * 2;
                if (new_capacity == 0)
                    new_capacity = DEFAULT_CAPACITY;
                m_data_.reserve(new_capacity);
            }

            return {m_data_.insert(pos, value), true};
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Insert operation failed: ") +
                                     e.what());
        }
    }

    /**
     * @brief Inserts a value into the set.
     *
     * @param value The value to insert.
     * @return A pair consisting of an iterator to the inserted element (or to
     * the element that prevented the insertion) and a bool denoting whether the
     * insertion took place.
     */
    auto insert(T&& value) -> std::pair<iterator, bool> {
        try {
            auto pos = lowerBound(value);

            if (pos != end() && !m_comp_(value, *pos) &&
                !m_comp_(*pos, value)) {
                return {pos, false};
            }

            if (m_data_.size() == m_data_.capacity()) {
                size_type new_capacity = m_data_.capacity() * 2;
                if (new_capacity == 0)
                    new_capacity = DEFAULT_CAPACITY;
                m_data_.reserve(new_capacity);
            }

            return {m_data_.insert(pos, std::move(value)), true};
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Insert operation failed: ") +
                                     e.what());
        }
    }

    /**
     * @brief Inserts a value into the set with a hint.
     *
     * @param hint An iterator to the position before which the value will be
     * inserted.
     * @param value The value to insert.
     * @return An iterator to the inserted element.
     * @throws std::invalid_argument If hint is invalid.
     */
    auto insert(const_iterator hint, const T& value) -> iterator {
        try {
            // Validate hint is within our range
            if (hint < begin() || hint > end()) {
                throw std::invalid_argument("Invalid hint provided to insert");
            }

            // Check if the hint is valid (value belongs right at or after the
            // hint)
            if (hint == end() || m_comp_(value, *hint)) {
                // Check if value belongs right at hint
                if (hint == begin() || m_comp_(*(std::prev(hint)), value)) {
                    // Efficient insertion with validated hint
                    return m_data_.insert(hint, value);
                }
            }

            // Hint wasn't optimal, fall back to regular insert
            return insert(value).first;
        } catch (const std::invalid_argument& e) {
            throw;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Insert operation failed: ") +
                                     e.what());
        }
    }

    /**
     * @brief Inserts a value into the set with a hint.
     *
     * @param hint An iterator to the position before which the value will be
     * inserted.
     * @param value The value to insert.
     * @return An iterator to the inserted element.
     * @throws std::invalid_argument If hint is invalid.
     */
    auto insert(const_iterator hint, T&& value) -> iterator {
        try {
            if (hint < begin() || hint > end()) {
                throw std::invalid_argument("Invalid hint provided to insert");
            }

            if (hint == end() || m_comp_(value, *hint)) {
                if (hint == begin() || m_comp_(*(std::prev(hint)), value)) {
                    return m_data_.insert(hint, std::move(value));
                }
            }

            return insert(std::move(value)).first;
        } catch (const std::invalid_argument& e) {
            throw;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Insert operation failed: ") +
                                     e.what());
        }
    }

    /**
     * @brief Inserts a range of values into the set.
     *
     * @tparam InputIt The type of the input iterator.
     * @param first The beginning of the range.
     * @param last The end of the range.
     * @throws std::invalid_argument If the iterator range is invalid.
     */
    template <std::input_iterator InputIt>
    void insert(InputIt first, InputIt last) {
        try {
            // Validate iterator range
            if constexpr (std::sized_sentinel_for<InputIt, InputIt>) {
                if (first > last) {
                    throw std::invalid_argument(
                        "Invalid iterator range: first > last");
                }

                // Reserve space for potentially new elements
                if constexpr (std::random_access_iterator<InputIt>) {
                    auto range_size = std::distance(first, last);
                    if (range_size > 0) {
                        reserve(size() + range_size);
                    }
                }
            }

            // For large ranges, use a more efficient bulk insertion strategy
            if constexpr (std::random_access_iterator<InputIt>) {
                if (std::distance(first, last) > 1000) {
                    // Copy elements to a temporary vector
                    std::vector<T> temp(first, last);

                    // Add existing elements to ensure we don't lose them
                    temp.insert(temp.end(), m_data_.begin(), m_data_.end());

                    // Use parallel sort for large sets
                    if (temp.size() > 10000) {
                        std::sort(std::execution::par_unseq, temp.begin(),
                                  temp.end(), m_comp_);
                    } else {
                        std::sort(temp.begin(), temp.end(), m_comp_);
                    }

                    // Remove duplicates
                    auto last_unique =
                        std::unique(temp.begin(), temp.end(),
                                    [this](const T& a, const T& b) {
                                        return !m_comp_(a, b) && !m_comp_(b, a);
                                    });

                    temp.erase(last_unique, temp.end());

                    // Swap with our container
                    m_data_.swap(temp);
                    return;
                }
            }

            // Default implementation for smaller ranges or non-random access
            // iterators
            while (first != last) {
                insert(*first++);
            }
        } catch (const std::invalid_argument& e) {
            throw;
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Bulk insert operation failed: ") + e.what());
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
        try {
            return insert(T(std::forward<Args>(args)...));
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Emplace operation failed: ") +
                                     e.what());
        }
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
    auto emplace_hint(const_iterator hint, Args&&... args) -> iterator {
        try {
            return insert(hint, T(std::forward<Args>(args)...));
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Emplace hint operation failed: ") + e.what());
        }
    }

    /**
     * @brief Erases an element from the set.
     *
     * @param pos An iterator to the element to erase.
     * @return An iterator to the element following the erased element.
     * @throws std::invalid_argument If pos is not a valid iterator.
     */
    auto erase(const_iterator pos) -> iterator {
        try {
            if (pos < begin() || pos >= end()) {
                throw std::invalid_argument(
                    "Invalid iterator position for erase");
            }
            return m_data_.erase(pos);
        } catch (const std::invalid_argument& e) {
            throw;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Erase operation failed: ") +
                                     e.what());
        }
    }

    /**
     * @brief Erases a range of elements from the set.
     *
     * @param first An iterator to the first element to erase.
     * @param last An iterator to the element following the last element to
     * erase.
     * @return An iterator to the element following the erased elements.
     * @throws std::invalid_argument If the iterator range is invalid.
     */
    auto erase(const_iterator first, const_iterator last) -> iterator {
        try {
            if (first < begin() || last > end() || first > last) {
                throw std::invalid_argument("Invalid iterator range for erase");
            }
            return m_data_.erase(first, last);
        } catch (const std::invalid_argument& e) {
            throw;
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Range erase operation failed: ") + e.what());
        }
    }

    /**
     * @brief Erases an element from the set by value.
     *
     * @param value The value to erase.
     * @return The number of elements erased.
     */
    auto erase(const T& value) -> size_type {
        try {
            auto it = find(value);
            if (it != end()) {
                erase(it);
                return 1;
            }
            return 0;
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Value erase operation failed: ") + e.what());
        }
    }

    /**
     * @brief Swaps the contents of this set with another set.
     *
     * @param other The other set to swap with.
     */
    void swap(FlatSet& other) noexcept(
        std::is_nothrow_swappable_v<std::vector<T>> &&
        std::is_nothrow_swappable_v<Compare>) {
        std::swap(m_data_, other.m_data_);
        std::swap(m_comp_, other.m_comp_);
    }

    /**
     * @brief Returns the number of elements matching a value.
     *
     * @param value The value to match.
     * @return The number of elements matching the value.
     */
    auto count(const T& value) const -> size_type {
        return contains(value) ? 1 : 0;
    }

    /**
     * @brief Finds an element in the set.
     *
     * @param value The value to find.
     * @return An iterator to the element, or end() if not found.
     */
    auto find(const T& value) -> iterator {
        try {
            // Use binary search for finding elements
            auto pos = lowerBound(value);
            if (pos != end() && !m_comp_(value, *pos) &&
                !m_comp_(*pos, value)) {
                return pos;
            }
            return end();
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Find operation failed: ") +
                                     e.what());
        }
    }

    /**
     * @brief Finds an element in the set.
     *
     * @param value The value to find.
     * @return A const iterator to the element, or end() if not found.
     */
    auto find(const T& value) const -> const_iterator {
        try {
            auto pos = lowerBound(value);
            if (pos != end() && !m_comp_(value, *pos) &&
                !m_comp_(*pos, value)) {
                return pos;
            }
            return end();
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Find operation failed: ") +
                                     e.what());
        }
    }

    /**
     * @brief Returns a range of elements matching a value.
     *
     * @param value The value to match.
     * @return A pair of iterators to the range of elements.
     */
    auto equalRange(const T& value) -> std::pair<iterator, iterator> {
        try {
            // Use optimized implementation for large sets
            if (size() > 10000) {
                return std::equal_range(std::execution::par_unseq,
                                        m_data_.begin(), m_data_.end(), value,
                                        m_comp_);
            }
            return std::equal_range(m_data_.begin(), m_data_.end(), value,
                                    m_comp_);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Equal range operation failed: ") + e.what());
        }
    }

    /**
     * @brief Returns a range of elements matching a value.
     *
     * @param value The value to match.
     * @return A pair of const iterators to the range of elements.
     */
    auto equalRange(const T& value) const
        -> std::pair<const_iterator, const_iterator> {
        try {
            if (size() > 10000) {
                return std::equal_range(std::execution::par_unseq,
                                        m_data_.begin(), m_data_.end(), value,
                                        m_comp_);
            }
            return std::equal_range(m_data_.begin(), m_data_.end(), value,
                                    m_comp_);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Equal range operation failed: ") + e.what());
        }
    }

    /**
     * @brief Returns an iterator to the first element not less than the given
     * value.
     *
     * @param value The value to compare.
     * @return An iterator to the first element not less than the given value.
     */
    auto lowerBound(const T& value) -> iterator {
        try {
            // Use parallel algorithm for large sets
            if (size() > 10000) {
                return std::lower_bound(std::execution::par_unseq,
                                        m_data_.begin(), m_data_.end(), value,
                                        m_comp_);
            }
            return std::lower_bound(m_data_.begin(), m_data_.end(), value,
                                    m_comp_);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Lower bound operation failed: ") + e.what());
        }
    }

    /**
     * @brief Returns a const iterator to the first element not less than the
     * given value.
     *
     * @param value The value to compare.
     * @return A const iterator to the first element not less than the given
     * value.
     */
    auto lowerBound(const T& value) const -> const_iterator {
        try {
            if (size() > 10000) {
                return std::lower_bound(std::execution::par_unseq,
                                        m_data_.begin(), m_data_.end(), value,
                                        m_comp_);
            }
            return std::lower_bound(m_data_.begin(), m_data_.end(), value,
                                    m_comp_);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Lower bound operation failed: ") + e.what());
        }
    }

    /**
     * @brief Returns an iterator to the first element greater than the given
     * value.
     *
     * @param value The value to compare.
     * @return An iterator to the first element greater than the given value.
     */
    auto upperBound(const T& value) -> iterator {
        try {
            if (size() > 10000) {
                return std::upper_bound(std::execution::par_unseq,
                                        m_data_.begin(), m_data_.end(), value,
                                        m_comp_);
            }
            return std::upper_bound(m_data_.begin(), m_data_.end(), value,
                                    m_comp_);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Upper bound operation failed: ") + e.what());
        }
    }

    /**
     * @brief Returns a const iterator to the first element greater than the
     * given value.
     *
     * @param value The value to compare.
     * @return A const iterator to the first element greater than the given
     * value.
     */
    auto upperBound(const T& value) const -> const_iterator {
        try {
            if (size() > 10000) {
                return std::upper_bound(std::execution::par_unseq,
                                        m_data_.begin(), m_data_.end(), value,
                                        m_comp_);
            }
            return std::upper_bound(m_data_.begin(), m_data_.end(), value,
                                    m_comp_);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Upper bound operation failed: ") + e.what());
        }
    }

    /**
     * @brief Returns the comparison function object.
     *
     * @return The comparison function object.
     */
    auto keyComp() const -> key_compare { return m_comp_; }

    /**
     * @brief Returns the comparison function object.
     *
     * @return The comparison function object.
     */
    auto valueComp() const -> value_compare { return m_comp_; }

    /**
     * @brief Checks if the set contains a value.
     *
     * @param value The value to check.
     * @return True if the value is found, false otherwise.
     */
    auto contains(const T& value) const -> bool {
        try {
            return find(value) != end();
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Contains operation failed: ") + e.what());
        }
    }

    /**
     * @brief Returns a view of the underlying data as a range.
     *
     * @return A view of the underlying data.
     */
    auto view() const { return std::ranges::subrange(begin(), end()); }
};

/**
 * @brief Equality comparison operator for FlatSets.
 *
 * @tparam T The type of elements.
 * @tparam Compare The comparison function object type.
 * @param lhs The first FlatSet.
 * @param rhs The second FlatSet.
 * @return True if the sets are equal, false otherwise.
 */
template <typename T, typename Compare>
auto operator==(const FlatSet<T, Compare>& lhs,
                const FlatSet<T, Compare>& rhs) -> bool {
    return lhs.size() == rhs.size() &&
           std::ranges::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Inequality comparison operator for FlatSets.
 *
 * @tparam T The type of elements.
 * @tparam Compare The comparison function object type.
 * @param lhs The first FlatSet.
 * @param rhs The second FlatSet.
 * @return True if the sets are not equal, false otherwise.
 */
template <typename T, typename Compare>
auto operator!=(const FlatSet<T, Compare>& lhs,
                const FlatSet<T, Compare>& rhs) -> bool {
    return !(lhs == rhs);
}

/**
 * @brief Less than comparison operator for FlatSets.
 *
 * @tparam T The type of elements.
 * @tparam Compare The comparison function object type.
 * @param lhs The first FlatSet.
 * @param rhs The second FlatSet.
 * @return True if lhs is lexicographically less than rhs, false otherwise.
 */
template <typename T, typename Compare>
auto operator<(const FlatSet<T, Compare>& lhs,
               const FlatSet<T, Compare>& rhs) -> bool {
    return std::ranges::lexicographical_compare(lhs.begin(), lhs.end(),
                                                rhs.begin(), rhs.end());
}

/**
 * @brief Less than or equal comparison operator for FlatSets.
 *
 * @tparam T The type of elements.
 * @tparam Compare The comparison function object type.
 * @param lhs The first FlatSet.
 * @param rhs The second FlatSet.
 * @return True if lhs is lexicographically less than or equal to rhs, false
 * otherwise.
 */
template <typename T, typename Compare>
auto operator<=(const FlatSet<T, Compare>& lhs,
                const FlatSet<T, Compare>& rhs) -> bool {
    return !(rhs < lhs);
}

/**
 * @brief Greater than comparison operator for FlatSets.
 *
 * @tparam T The type of elements.
 * @tparam Compare The comparison function object type.
 * @param lhs The first FlatSet.
 * @param rhs The second FlatSet.
 * @return True if lhs is lexicographically greater than rhs, false otherwise.
 */
template <typename T, typename Compare>
auto operator>(const FlatSet<T, Compare>& lhs,
               const FlatSet<T, Compare>& rhs) -> bool {
    return rhs < lhs;
}

/**
 * @brief Greater than or equal comparison operator for FlatSets.
 *
 * @tparam T The type of elements.
 * @tparam Compare The comparison function object type.
 * @param lhs The first FlatSet.
 * @param rhs The second FlatSet.
 * @return True if lhs is lexicographically greater than or equal to rhs, false
 * otherwise.
 */
template <typename T, typename Compare>
auto operator>=(const FlatSet<T, Compare>& lhs,
                const FlatSet<T, Compare>& rhs) -> bool {
    return !(lhs < rhs);
}

/**
 * @brief Swaps the contents of two FlatSets.
 *
 * @tparam T The type of elements.
 * @tparam Compare The comparison function object type.
 * @param lhs The first FlatSet.
 * @param rhs The second FlatSet.
 */
template <typename T, typename Compare>
void swap(FlatSet<T, Compare>& lhs,
          FlatSet<T, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

}  // namespace atom::type

#endif  // ATOM_TYPE_FLAT_SET_HPP
