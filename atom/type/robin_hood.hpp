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

    enum class threading_policy {
        unsafe,       // No thread safety
        reader_lock,  // Reader-writer lock
        mutex         // Full mutex lock
    };

private:
    struct Entry {
        size_t dist;  // Distance from ideal position
        value_type data;

        template <typename K, typename V>
        Entry(size_t d, K&& k, V&& v)
            : dist(d), data(std::forward<K>(k), std::forward<V>(v)) {}
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
    threading_policy policy_;
    std::unique_ptr<void, void (*)(void*)> lock_{nullptr, [](void*) {}};

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

    void lock_read() const {
        if (policy_ == threading_policy::reader_lock) {
            static_cast<std::shared_mutex*>(lock_.get())->lock_shared();
        }
    }

    void unlock_read() const {
        if (policy_ == threading_policy::reader_lock) {
            static_cast<std::shared_mutex*>(lock_.get())->unlock_shared();
        }
    }

    void lock_write() {
        if (policy_ == threading_policy::reader_lock) {
            static_cast<std::shared_mutex*>(lock_.get())->lock();
        } else if (policy_ == threading_policy::mutex) {
            static_cast<std::mutex*>(lock_.get())->lock();
        }
    }

    void unlock_write() {
        if (policy_ == threading_policy::reader_lock) {
            static_cast<std::shared_mutex*>(lock_.get())->unlock();
        } else if (policy_ == threading_policy::mutex) {
            static_cast<std::mutex*>(lock_.get())->unlock();
        }
    }

public:
    // Constructors
    unordered_flat_map() = default;

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

    template <typename Alloc>
    explicit unordered_flat_map(const Alloc& alloc)
        : table_(alloc), alloc_(alloc) {}

    template <typename Alloc>
    unordered_flat_map(size_type bucket_count, const Alloc& alloc)
        : table_(bucket_count, alloc), alloc_(alloc) {
        max_load_ = static_cast<size_t>(bucket_count * max_load_factor());
    }

    // Capacity
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept { return table_.max_size(); }

    // Iterators
    iterator begin() noexcept { return iterator(table_.begin()); }
    iterator end() noexcept { return iterator(table_.end()); }
    const_iterator begin() const noexcept {
        return const_iterator(table_.begin());
    }
    const_iterator end() const noexcept { return const_iterator(table_.end()); }
    const_iterator cbegin() const noexcept {
        return const_iterator(table_.begin());
    }
    const_iterator cend() const noexcept {
        return const_iterator(table_.end());
    }

    // Modifiers
    void clear() noexcept {
        table_.clear();
        size_ = 0;
    }

    template <typename K, typename V>
    std::pair<iterator, bool> insert(K&& key, V&& value) {
        if (size_ + 1 > max_load_)
            rehash(table_.empty() ? 16 : table_.size() * 2);

        Entry entry(0, std::forward<K>(key), std::forward<V>(value));
        size_t mask = table_.size() - 1;
        size_t idx = hasher_(entry.data.first) & mask;

        while (true) {
            if (table_[idx].dist < entry.dist) {
                std::swap(entry, table_[idx]);
                if (entry.dist == 0) {
                    ++size_;
                    return {iterator(table_.begin() + idx), true};
                }
            }
            idx = (idx + 1) & mask;
            ++entry.dist;
        }
    }

    // Lookup
    Value& at(const Key& key) {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range(
                fmt::format("Key '{}' not found in unordered_flat_map", key));
        }
        return it->second;
    }

    const Value& at(const Key& key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range(
                fmt::format("Key '{}' not found in unordered_flat_map", key));
        }
        return it->second;
    }

    // Bucket interface
    size_type bucket_count() const noexcept { return table_.size(); }
    size_type max_bucket_count() const noexcept { return table_.max_size(); }
    float load_factor() const noexcept {
        return table_.empty() ? 0.0f
                              : static_cast<float>(size_) / table_.size();
    }
    float max_load_factor() const noexcept { return max_load_factor_; }
    void max_load_factor(float ml) {
        max_load_factor_ = ml;
        max_load_ = static_cast<size_t>(table_.size() * max_load_factor_);
    }

private:
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
                        std::swap(entry, new_table[idx]);
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

#endif  // ATOM_UTILS_CONTAINERS_ROBIN_HOOD_HPP