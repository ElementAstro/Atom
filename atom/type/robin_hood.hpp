#ifndef ATOM_UTILS_CONTAINERS_ROBIN_HOOD_HPP
#define ATOM_UTILS_CONTAINERS_ROBIN_HOOD_HPP

#include <fmt/format.h>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace atom::utils {

/**
 * @brief A high-performance hash map implementation using Robin Hood hashing
 *
 * This class provides an efficient unordered associative container that
 * implements Robin Hood hashing for optimal performance characteristics.
 * It supports different threading policies for concurrent access scenarios.
 *
 * @tparam Key The key type
 * @tparam Value The mapped value type
 * @tparam Hash The hash function type
 * @tparam KeyEqual The key equality comparison type
 * @tparam Allocator The allocator type
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<Key, Value>>>
class unordered_flat_map {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer =
        typename std::allocator_traits<Allocator>::const_pointer;

    /**
     * @brief Threading policy enumeration for concurrent access control
     */
    enum class threading_policy {
        unsafe,       ///< No thread safety
        reader_lock,  ///< Reader-writer lock for concurrent reads
        mutex         ///< Full mutex lock for exclusive access
    };

private:
    /**
     * @brief Internal entry structure for Robin Hood hashing
     */
    struct Entry {
        size_t dist;      ///< Distance from ideal position
        value_type data;  ///< The key-value pair

        /**
         * @brief Constructs an entry with given distance and key-value pair
         * @tparam K Key type (forwarding reference)
         * @tparam V Value type (forwarding reference)
         * @param d Distance from ideal position
         * @param k Key to be stored
         * @param v Value to be stored
         */
        template <typename K, typename V>
        Entry(size_t d, K&& k, V&& v)
            : dist(d), data(std::forward<K>(k), std::forward<V>(v)) {}

        /**
         * @brief Default constructor for empty entries
         */
        Entry() : dist(0), data(Key(), Value()) {}

        /**
         * @brief Swaps two entries efficiently
         * @param other The other entry to swap with
         */
        void swap(Entry& other) noexcept {
            std::swap(dist, other.dist);
            std::swap(const_cast<Key&>(data.first),
                      const_cast<Key&>(other.data.first));
            std::swap(data.second, other.data.second);
        }
    };

    using Storage =
        std::vector<Entry, typename std::allocator_traits<
                               Allocator>::template rebind_alloc<Entry>>;
    Storage table_;
    size_t size_ = 0;
    size_t max_load_ = 0;
    float max_load_factor_ = 0.9f;
    Hash hasher_;
    KeyEqual key_equal_;
    Allocator alloc_;
    threading_policy policy_ = threading_policy::unsafe;
    std::unique_ptr<void, void (*)(void*)> lock_{nullptr, [](void*) {}};

    /**
     * @brief Forward iterator for the hash map
     */
    class iterator {
        using storage_iterator = typename Storage::iterator;
        storage_iterator it_;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename unordered_flat_map::value_type;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        explicit iterator(storage_iterator it) : it_(it) {}

        reference operator*() const { return it_->data; }
        pointer operator->() const { return &it_->data; }
        iterator& operator++() {
            ++it_;
            return *this;
        }
        iterator operator++(int) {
            iterator tmp(*this);
            ++it_;
            return tmp;
        }
        bool operator==(const iterator& other) const {
            return it_ == other.it_;
        }
        bool operator!=(const iterator& other) const {
            return it_ != other.it_;
        }
    };

    /**
     * @brief Const forward iterator for the hash map
     */
    class const_iterator {
        using storage_const_iterator = typename Storage::const_iterator;
        storage_const_iterator it_;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const typename unordered_flat_map::value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        explicit const_iterator(storage_const_iterator it) : it_(it) {}

        reference operator*() const { return it_->data; }
        pointer operator->() const { return &it_->data; }
        const_iterator& operator++() {
            ++it_;
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator tmp(*this);
            ++it_;
            return tmp;
        }
        bool operator==(const const_iterator& other) const {
            return it_ == other.it_;
        }
        bool operator!=(const const_iterator& other) const {
            return it_ != other.it_;
        }
    };

    /**
     * @brief Acquires a read lock if reader-writer policy is enabled
     */
    void lock_read() const {
        if (policy_ == threading_policy::reader_lock) {
            static_cast<std::shared_mutex*>(lock_.get())->lock_shared();
        }
    }

    /**
     * @brief Releases a read lock if reader-writer policy is enabled
     */
    void unlock_read() const {
        if (policy_ == threading_policy::reader_lock) {
            static_cast<std::shared_mutex*>(lock_.get())->unlock_shared();
        }
    }

    /**
     * @brief Acquires a write lock based on the threading policy
     */
    void lock_write() {
        if (policy_ == threading_policy::reader_lock) {
            static_cast<std::shared_mutex*>(lock_.get())->lock();
        } else if (policy_ == threading_policy::mutex) {
            static_cast<std::mutex*>(lock_.get())->lock();
        }
    }

    /**
     * @brief Releases a write lock based on the threading policy
     */
    void unlock_write() {
        if (policy_ == threading_policy::reader_lock) {
            static_cast<std::shared_mutex*>(lock_.get())->unlock();
        } else if (policy_ == threading_policy::mutex) {
            static_cast<std::mutex*>(lock_.get())->unlock();
        }
    }

public:
    /**
     * @brief Default constructor
     */
    unordered_flat_map() = default;

    /**
     * @brief Constructs a hash map with specified threading policy
     * @param policy The threading policy to use
     */
    explicit unordered_flat_map(threading_policy policy) : policy_(policy) {
        if (policy != threading_policy::unsafe) {
            lock_ = policy == threading_policy::reader_lock
                        ? std::unique_ptr<void, void (*)(void*)>(
                              new std::shared_mutex(),
                              [](void* p) {
                                  delete static_cast<std::shared_mutex*>(p);
                              })
                        : std::unique_ptr<void, void (*)(void*)>(
                              new std::mutex(), [](void* p) {
                                  delete static_cast<std::mutex*>(p);
                              });
        }
    }

    /**
     * @brief Constructs a hash map with specified allocator
     * @tparam Alloc The allocator type
     * @param alloc The allocator instance
     */
    template <typename Alloc>
    explicit unordered_flat_map(const Alloc& alloc)
        : table_(alloc), alloc_(alloc) {}

    /**
     * @brief Constructs a hash map with specified bucket count and allocator
     * @tparam Alloc The allocator type
     * @param bucket_count Initial number of buckets
     * @param alloc The allocator instance
     */
    template <typename Alloc>
    unordered_flat_map(size_type bucket_count, const Alloc& alloc)
        : table_(bucket_count, alloc), alloc_(alloc) {
        max_load_ = static_cast<size_t>(bucket_count * max_load_factor());
    }

    /**
     * @brief Checks if the container is empty
     * @return true if the container is empty, false otherwise
     */
    bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief Returns the number of elements
     * @return The number of elements in the container
     */
    size_type size() const noexcept { return size_; }

    /**
     * @brief Returns the maximum possible number of elements
     * @return The maximum number of elements the container can hold
     */
    size_type max_size() const noexcept { return table_.max_size(); }

    /**
     * @brief Returns an iterator to the beginning
     * @return Iterator to the first element
     */
    iterator begin() noexcept { return iterator(table_.begin()); }

    /**
     * @brief Returns an iterator to the end
     * @return Iterator to one past the last element
     */
    iterator end() noexcept { return iterator(table_.end()); }

    /**
     * @brief Returns a const iterator to the beginning
     * @return Const iterator to the first element
     */
    const_iterator begin() const noexcept {
        return const_iterator(table_.begin());
    }

    /**
     * @brief Returns a const iterator to the end
     * @return Const iterator to one past the last element
     */
    const_iterator end() const noexcept { return const_iterator(table_.end()); }

    /**
     * @brief Returns a const iterator to the beginning
     * @return Const iterator to the first element
     */
    const_iterator cbegin() const noexcept {
        return const_iterator(table_.begin());
    }

    /**
     * @brief Returns a const iterator to the end
     * @return Const iterator to one past the last element
     */
    const_iterator cend() const noexcept {
        return const_iterator(table_.end());
    }

    /**
     * @brief Clears the contents
     */
    void clear() noexcept {
        table_.clear();
        size_ = 0;
    }

    /**
     * @brief Inserts an element or assigns to the current element if the key
     * already exists
     * @tparam K Key type (forwarding reference)
     * @tparam V Value type (forwarding reference)
     * @param key The key to insert or assign
     * @param value The value to insert or assign
     * @return A pair consisting of an iterator and a boolean indicating
     * insertion
     */
    template <typename K, typename V>
    std::pair<iterator, bool> insert(K&& key, V&& value) {
        if (size_ + 1 > max_load_)
            rehash(table_.empty() ? 16 : table_.size() * 2);

        Entry entry(0, std::forward<K>(key), std::forward<V>(value));
        size_t mask = table_.size() - 1;
        size_t idx = hasher_(entry.data.first) & mask;

        while (true) {
            if (table_[idx].dist < entry.dist) {
                entry.swap(table_[idx]);
                if (entry.dist == 0) {
                    ++size_;
                    return {iterator(table_.begin() + idx), true};
                }
            }
            idx = (idx + 1) & mask;
            ++entry.dist;
        }
    }

    /**
     * @brief Finds an element with specific key
     * @param key The key to search for
     * @return Iterator to the element if found, end() otherwise
     */
    iterator find(const Key& key) {
        if (table_.empty())
            return end();

        size_t mask = table_.size() - 1;
        size_t idx = hasher_(key) & mask;
        size_t dist = 0;

        while (true) {
            if (table_[idx].dist < dist) {
                return end();
            }
            if (table_[idx].dist == dist &&
                key_equal_(table_[idx].data.first, key)) {
                return iterator(table_.begin() + idx);
            }
            idx = (idx + 1) & mask;
            ++dist;
        }
    }

    /**
     * @brief Finds an element with specific key (const version)
     * @param key The key to search for
     * @return Const iterator to the element if found, end() otherwise
     */
    const_iterator find(const Key& key) const {
        if (table_.empty())
            return end();

        size_t mask = table_.size() - 1;
        size_t idx = hasher_(key) & mask;
        size_t dist = 0;

        while (true) {
            if (table_[idx].dist < dist) {
                return end();
            }
            if (table_[idx].dist == dist &&
                key_equal_(table_[idx].data.first, key)) {
                return const_iterator(table_.begin() + idx);
            }
            idx = (idx + 1) & mask;
            ++dist;
        }
    }

    /**
     * @brief Access specified element with bounds checking
     * @param key The key of the element to find
     * @return Reference to the mapped value
     * @throws std::out_of_range if the key is not found
     */
    Value& at(const Key& key) {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range(
                fmt::format("Key not found in unordered_flat_map"));
        }
        return it->second;
    }

    /**
     * @brief Access specified element with bounds checking (const version)
     * @param key The key of the element to find
     * @return Const reference to the mapped value
     * @throws std::out_of_range if the key is not found
     */
    const Value& at(const Key& key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range(
                fmt::format("Key not found in unordered_flat_map"));
        }
        return it->second;
    }

    /**
     * @brief Returns the number of buckets
     * @return The number of buckets in the container
     */
    size_type bucket_count() const noexcept { return table_.size(); }

    /**
     * @brief Returns the maximum number of buckets
     * @return The maximum number of buckets the container can have
     */
    size_type max_bucket_count() const noexcept { return table_.max_size(); }

    /**
     * @brief Returns average number of elements per bucket
     * @return The current load factor
     */
    float load_factor() const noexcept {
        return table_.empty() ? 0.0f
                              : static_cast<float>(size_) / table_.size();
    }

    /**
     * @brief Returns maximum load factor
     * @return The maximum load factor
     */
    float max_load_factor() const noexcept { return max_load_factor_; }

    /**
     * @brief Sets maximum load factor
     * @param ml The new maximum load factor
     */
    void max_load_factor(float ml) {
        max_load_factor_ = ml;
        max_load_ = static_cast<size_t>(table_.size() * max_load_factor_);
    }

private:
    /**
     * @brief Rehashes the container to have at least count buckets
     * @param count The minimum number of buckets
     * @throws std::length_error if count exceeds max_size()
     */
    void rehash(size_type count) {
        if (count > max_size()) {
            throw std::length_error(
                fmt::format("Requested capacity {} exceeds max_size() of {}",
                            count, max_size()));
        }

        Storage new_table(count, alloc_);
        for (auto& entry : table_) {
            if (entry.dist != 0) {
                size_t mask = new_table.size() - 1;
                size_t idx = hasher_(entry.data.first) & mask;
                size_t dist = 0;

                while (true) {
                    if (new_table[idx].dist < dist) {
                        entry.swap(new_table[idx]);
                        if (entry.dist == 0)
                            break;
                    }
                    idx = (idx + 1) & mask;
                    ++dist;
                }
            }
        }
        table_ = std::move(new_table);
        max_load_ = static_cast<size_t>(table_.size() * max_load_factor_);
    }
};

}  // namespace atom::utils

#endif
