#ifndef ATOM_ASYNC_SYNC_LOCKFREE_HASHTABLE_HPP
#define ATOM_ASYNC_SYNC_LOCKFREE_HASHTABLE_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::async {

/**
 * @brief Concept for hash table key-value pairs.
 */
template <typename T, typename U>
concept HashTableKeyValue = requires(T t, U u) {
    { std::hash<T>{}(t) } -> std::convertible_to<size_t>;
    { t == t } -> std::convertible_to<bool>;
    requires std::default_initializable<U>;
};

/**
 * @brief A lock-free hash table implementation.
 *
 * Uses separate chaining with atomic operations for thread-safe access.
 *
 * @tparam Key The key type.
 * @tparam Value The value type.
 */
template <typename Key, typename Value>
    requires HashTableKeyValue<Key, Value>
class LockFreeHashTable {
private:
    struct Node {
        Key key;
        Value value;
        std::atomic<std::shared_ptr<Node>> next;

        Node(Key k,
             Value v) noexcept(std::is_nothrow_move_constructible_v<Key> &&
                               std::is_nothrow_move_constructible_v<Value>)
            : key(std::move(k)), value(std::move(v)), next(nullptr) {}
    };

    struct Bucket {
        std::atomic<std::shared_ptr<Node>> head;

        Bucket() noexcept : head(nullptr) {}

        auto find(const Key& key) const noexcept
            -> std::optional<std::reference_wrapper<Value>> {
            auto node = head.load(std::memory_order_acquire);
            while (node) {
                if (node->key == key) {
                    return std::ref(node->value);
                }
                node = node->next.load(std::memory_order_acquire);
            }
            return std::nullopt;
        }

        void insert(const Key& key, const Value& value) {
            try {
                auto newNode = std::make_shared<Node>(key, value);
                // Load current head into expected
                std::shared_ptr<Node> expected =
                    head.load(std::memory_order_acquire);

                // Initialize newNode->next to current head
                newNode->next.store(expected, std::memory_order_relaxed);

                // Try to update head atomically
                while (!head.compare_exchange_weak(expected, newNode,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
                    // On failure, expected is updated; update newNode->next
                    newNode->next.store(expected, std::memory_order_relaxed);
                }
            } catch (const std::exception& e) {
                // Handle allocation failure
            }
        }

        bool erase(const Key& key) noexcept {
            auto currentNode = head.load(std::memory_order_acquire);
            std::shared_ptr<Node> prevNode = nullptr;

            while (currentNode) {
                auto nextNode =
                    currentNode->next.load(std::memory_order_acquire);

                if (currentNode->key == key) {
                    if (!prevNode) {
                        if (head.compare_exchange_strong(
                                currentNode, nextNode,
                                std::memory_order_acq_rel,
                                std::memory_order_relaxed)) {
                            return true;
                        }
                    } else {
                        if (prevNode->next.compare_exchange_strong(
                                currentNode, nextNode,
                                std::memory_order_acq_rel,
                                std::memory_order_relaxed)) {
                            return true;
                        }
                    }
                    currentNode = head.load(std::memory_order_acquire);
                    prevNode = nullptr;
                    continue;
                }

                prevNode = currentNode;
                currentNode = nextNode;
            }
            return false;
        }
    };

    std::vector<std::unique_ptr<Bucket>> buckets_;
    std::hash<Key> hasher_;
    std::atomic<size_t> size_{0};

    auto getBucket(const Key& key) const noexcept -> Bucket& {
        auto bucketIndex = hasher_(key) % buckets_.size();
        return *buckets_[bucketIndex];
    }

public:
    /**
     * @brief Construct a new hash table with the specified number of buckets.
     * @param num_buckets Number of buckets (default: 16).
     */
    explicit LockFreeHashTable(size_t num_buckets = 16)
        : buckets_(std::max(num_buckets, size_t(1))) {
        for (size_t i = 0; i < buckets_.size(); ++i) {
            buckets_[i] = std::make_unique<Bucket>();
        }
    }

    /**
     * @brief Construct from a range of key-value pairs.
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>,
                                     std::pair<Key, Value>>
    explicit LockFreeHashTable(R&& range, size_t num_buckets = 16)
        : LockFreeHashTable(num_buckets) {
        for (auto&& [key, value] : range) {
            insert(key, value);
        }
    }

    /**
     * @brief Find a value by key.
     * @return Optional reference to the value if found.
     */
    auto find(const Key& key) const noexcept
        -> std::optional<std::reference_wrapper<Value>> {
        return getBucket(key).find(key);
    }

    /**
     * @brief Insert a key-value pair.
     */
    void insert(const Key& key, const Value& value) {
        getBucket(key).insert(key, value);
        size_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Erase a key-value pair.
     * @return true if the key was found and removed.
     */
    bool erase(const Key& key) noexcept {
        bool result = getBucket(key).erase(key);
        if (result) {
            size_.fetch_sub(1, std::memory_order_relaxed);
        }
        return result;
    }

    [[nodiscard]] auto empty() const noexcept -> bool { return size() == 0; }

    [[nodiscard]] auto size() const noexcept -> size_t {
        return size_.load(std::memory_order_acquire);
    }

    void clear() noexcept {
        for (const auto& bucket : buckets_) {
            bucket->head.exchange(nullptr, std::memory_order_acq_rel);
        }
        size_.store(0, std::memory_order_release);
    }

    /**
     * @brief Access or insert a value by key.
     */
    auto operator[](const Key& key) -> Value& {
        auto found = find(key);
        if (found) {
            return found->get();
        }

        insert(key, Value{});
        auto result = find(key);
        if (!result) {
            THROW_RUNTIME_ERROR("Failed to insert value into hash table");
        }
        return result->get();
    }

    /**
     * @brief Forward iterator for the hash table.
     */
    class Iterator {
    public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type;

        Iterator(typename std::vector<std::unique_ptr<Bucket>>::const_iterator
                     bucket_iter,
                 typename std::vector<std::unique_ptr<Bucket>>::const_iterator
                     bucket_end,
                 std::shared_ptr<Node> node) noexcept
            : bucket_iter_(bucket_iter),
              bucket_end_(bucket_end),
              node_(std::move(node)) {
            advancePastEmptyBuckets();
        }

        auto operator++() noexcept -> Iterator& {
            if (node_) {
                node_ = node_->next.load(std::memory_order_acquire);
                if (!node_) {
                    ++bucket_iter_;
                    advancePastEmptyBuckets();
                }
            }
            return *this;
        }

        auto operator++(int) noexcept -> Iterator {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        auto operator==(const Iterator& other) const noexcept -> bool {
            return bucket_iter_ == other.bucket_iter_ && node_ == other.node_;
        }

        auto operator!=(const Iterator& other) const noexcept -> bool {
            return !(*this == other);
        }

        auto operator*() const noexcept -> reference {
            return {node_->key, node_->value};
        }

    private:
        void advancePastEmptyBuckets() noexcept {
            while (bucket_iter_ != bucket_end_ && !node_) {
                node_ = (*bucket_iter_)->head.load(std::memory_order_acquire);
                if (!node_) {
                    ++bucket_iter_;
                }
            }
        }

        typename std::vector<std::unique_ptr<Bucket>>::const_iterator
            bucket_iter_;
        typename std::vector<std::unique_ptr<Bucket>>::const_iterator
            bucket_end_;
        std::shared_ptr<Node> node_;
    };

    auto begin() const noexcept -> Iterator {
        auto bucketIter = buckets_.begin();
        auto bucketEnd = buckets_.end();
        std::shared_ptr<Node> node;
        if (bucketIter != bucketEnd) {
            node = (*bucketIter)->head.load(std::memory_order_acquire);
        }
        return Iterator(bucketIter, bucketEnd, node);
    }

    auto end() const noexcept -> Iterator {
        return Iterator(buckets_.end(), buckets_.end(), nullptr);
    }
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_SYNC_LOCKFREE_HASHTABLE_HPP
