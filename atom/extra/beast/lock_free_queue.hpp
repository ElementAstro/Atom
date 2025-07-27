#ifndef ATOM_EXTRA_BEAST_LOCK_FREE_QUEUE_HPP
#define ATOM_EXTRA_BEAST_LOCK_FREE_QUEUE_HPP

#include "concurrency_primitives.hpp"
#include <atomic>
#include <memory>
#include <concepts>

namespace atom::beast::concurrency {

/**
 * @brief Lock-free MPMC (Multi-Producer Multi-Consumer) queue using hazard pointers
 */
template<typename T>
class LockFreeMPMCQueue {
private:
    struct Node {
        std::atomic<T*> data{nullptr};
        std::atomic<Node*> next{nullptr};

        Node() = default;
        explicit Node(T&& item) : data(new T(std::move(item))) {}
    };

    CacheAligned<std::atomic<Node*>> head_;
    CacheAligned<std::atomic<Node*>> tail_;

    // Thread-local hazard pointer records
    thread_local static HazardPointer::HazardRecord* head_hazard_;
    thread_local static HazardPointer::HazardRecord* tail_hazard_;

public:
    LockFreeMPMCQueue() {
        Node* dummy = new Node;
        head_.value.store(dummy, std::memory_order_relaxed);
        tail_.value.store(dummy, std::memory_order_relaxed);
    }

    ~LockFreeMPMCQueue() {
        while (Node* old_head = head_.value.load(std::memory_order_relaxed)) {
            head_.value.store(old_head->next.load(std::memory_order_relaxed), std::memory_order_relaxed);
            delete old_head;
        }
    }

    /**
     * @brief Enqueues an item to the queue
     */
    void enqueue(T&& item) {
        Node* new_node = new Node(std::move(item));

        while (true) {
            Node* last = tail_.value.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);

            // Check if tail is still the same
            if (last == tail_.value.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // Try to link new node at the end of the list
                    if (last->next.compare_exchange_weak(next, new_node,
                                                        std::memory_order_release,
                                                        std::memory_order_relaxed)) {
                        break;
                    }
                } else {
                    // Try to swing tail to the next node
                    tail_.value.compare_exchange_weak(last, next,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed);
                }
            }
        }

        // Try to swing tail to the new node
        tail_.value.compare_exchange_weak(tail_.value.load(std::memory_order_acquire), new_node,
                                         std::memory_order_release,
                                         std::memory_order_relaxed);
    }

    /**
     * @brief Attempts to dequeue an item from the queue
     */
    [[nodiscard]] bool try_dequeue(T& result) {
        if (!head_hazard_) {
            head_hazard_ = HazardPointer::acquire_hazard_pointer();
            if (!head_hazard_) {
                spdlog::warn("Failed to acquire hazard pointer for head");
                return false;
            }
        }

        while (true) {
            Node* first = head_.value.load(std::memory_order_acquire);
            head_hazard_->pointer.store(first, std::memory_order_release);

            // Check if head changed after setting hazard pointer
            if (first != head_.value.load(std::memory_order_acquire)) {
                continue;
            }

            Node* last = tail_.value.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);

            // Check if head is still the same
            if (first == head_.value.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr) {
                        // Queue is empty
                        return false;
                    }

                    // Try to advance tail
                    tail_.value.compare_exchange_weak(last, next,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed);
                } else {
                    if (next == nullptr) {
                        continue;
                    }

                    // Read data before CAS
                    T* data = next->data.load(std::memory_order_acquire);
                    if (data == nullptr) {
                        continue;
                    }

                    // Try to swing head to the next node
                    if (head_.value.compare_exchange_weak(first, next,
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed)) {
                        result = *data;
                        delete data;

                        // Safe to delete first node if not hazardous
                        if (!HazardPointer::is_hazardous(first)) {
                            delete first;
                        }

                        return true;
                    }
                }
            }
        }
    }

    /**
     * @brief Checks if the queue is empty (approximate)
     */
    [[nodiscard]] bool empty() const noexcept {
        Node* first = head_.value.load(std::memory_order_acquire);
        Node* last = tail_.value.load(std::memory_order_acquire);
        return (first == last) && (first->next.load(std::memory_order_acquire) == nullptr);
    }

    /**
     * @brief Returns approximate size of the queue
     */
    [[nodiscard]] std::size_t size() const noexcept {
        std::size_t count = 0;
        Node* current = head_.value.load(std::memory_order_acquire);

        while (current && current->next.load(std::memory_order_acquire)) {
            current = current->next.load(std::memory_order_acquire);
            ++count;
        }

        return count;
    }
};

template<typename T>
thread_local HazardPointer::HazardRecord* LockFreeMPMCQueue<T>::head_hazard_ = nullptr;

template<typename T>
thread_local HazardPointer::HazardRecord* LockFreeMPMCQueue<T>::tail_hazard_ = nullptr;

/**
 * @brief Work-stealing deque for efficient task distribution
 */
template<typename T>
class WorkStealingDeque {
private:
    static constexpr std::size_t INITIAL_SIZE = 1024;

    struct CircularArray {
        std::size_t log_size;
        std::unique_ptr<std::atomic<T>[]> buffer;

        explicit CircularArray(std::size_t log_sz)
            : log_size(log_sz), buffer(std::make_unique<std::atomic<T>[]>(1ULL << log_sz)) {}

        std::size_t size() const noexcept { return 1ULL << log_size; }

        T get(std::size_t index) const {
            return buffer[index & (size() - 1)].load(std::memory_order_acquire);
        }

        void put(std::size_t index, T&& item) {
            buffer[index & (size() - 1)].store(std::move(item), std::memory_order_release);
        }
    };

    CacheAligned<std::atomic<std::size_t>> top_{0};
    CacheAligned<std::atomic<std::size_t>> bottom_{0};
    std::atomic<CircularArray*> array_;

public:
    WorkStealingDeque() {
        array_.store(new CircularArray(std::bit_width(INITIAL_SIZE) - 1), std::memory_order_relaxed);
    }

    ~WorkStealingDeque() {
        delete array_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Pushes an item to the bottom (owner thread only)
     */
    void push_bottom(T&& item) {
        std::size_t b = bottom_.value.load(std::memory_order_relaxed);
        std::size_t t = top_.value.load(std::memory_order_acquire);
        CircularArray* a = array_.load(std::memory_order_relaxed);

        if (b - t > a->size() - 1) {
            // Array is full, resize
            auto new_array = new CircularArray(a->log_size + 1);
            for (std::size_t i = t; i != b; ++i) {
                new_array->put(i, std::move(a->get(i)));
            }
            array_.store(new_array, std::memory_order_release);
            delete a;
            a = new_array;
        }

        a->put(b, std::move(item));
        std::atomic_thread_fence(std::memory_order_release);
        bottom_.value.store(b + 1, std::memory_order_relaxed);
    }

    /**
     * @brief Pops an item from the bottom (owner thread only)
     */
    [[nodiscard]] bool pop_bottom(T& result) {
        std::size_t b = bottom_.value.load(std::memory_order_relaxed);
        CircularArray* a = array_.load(std::memory_order_relaxed);
        b = b - 1;
        bottom_.value.store(b, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::size_t t = top_.value.load(std::memory_order_relaxed);

        if (t <= b) {
            result = std::move(a->get(b));
            if (t == b) {
                if (!top_.value.compare_exchange_strong(t, t + 1,
                                                       std::memory_order_seq_cst,
                                                       std::memory_order_relaxed)) {
                    bottom_.value.store(b + 1, std::memory_order_relaxed);
                    return false;
                }
                bottom_.value.store(b + 1, std::memory_order_relaxed);
            }
            return true;
        } else {
            bottom_.value.store(b + 1, std::memory_order_relaxed);
            return false;
        }
    }

    /**
     * @brief Steals an item from the top (thief threads)
     */
    [[nodiscard]] bool steal(T& result) {
        std::size_t t = top_.value.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::size_t b = bottom_.value.load(std::memory_order_acquire);

        if (t < b) {
            CircularArray* a = array_.load(std::memory_order_consume);
            result = std::move(a->get(t));
            if (!top_.value.compare_exchange_strong(t, t + 1,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_relaxed)) {
                return false;
            }
            return true;
        }
        return false;
    }

    /**
     * @brief Checks if deque is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        std::size_t b = bottom_.value.load(std::memory_order_relaxed);
        std::size_t t = top_.value.load(std::memory_order_relaxed);
        return b <= t;
    }
};

} // namespace atom::beast::concurrency

#endif // ATOM_EXTRA_BEAST_LOCK_FREE_QUEUE_HPP
