#include "cache.hpp"
#include <algorithm>
#include <cstring>

namespace atom::extra::curl {

// Thread-local storage for epoch manager
thread_local size_t Cache::EpochManager::thread_index_ = SIZE_MAX;

Cache::Cache(const Config& config)
    : config_(config), bucket_count_(config.initial_bucket_count),
      epoch_manager_(std::make_unique<EpochManager>()) {

    buckets_ = std::make_unique<Bucket[]>(bucket_count_.load());
    stale_buckets_ = std::make_unique<Bucket[]>(bucket_count_.load());

    spdlog::info("Initialized lock-free cache with {} buckets, max_entries: {}",
                 bucket_count_.load(), config_.max_entries);
}

Cache::Cache(std::chrono::seconds default_ttl)
    : Cache(Config{.default_ttl = default_ttl}) {}

Cache::~Cache() {
    spdlog::info("Destroying cache. Stats - Gets: {}, Sets: {}, Hits: {}, Hit ratio: {:.2f}%",
                 stats_.get_count.load(), stats_.set_count.load(),
                 stats_.hit_count.load(), stats_.getHitRatio() * 100.0);

    clear();
}

void Cache::set(const std::string& url, const Response& response,
                std::optional<std::chrono::seconds> ttl) {
    stats_.set_count.fetch_add(1, std::memory_order_relaxed);

    epoch_manager_->enter();

    auto entry = std::make_shared<CacheEntry>(
        response,
        std::chrono::system_clock::now() + (ttl ? *ttl : config_.default_ttl),
        "",  // Will be filled from headers
        ""   // Will be filled from headers
    );

    // Extract ETag and Last-Modified headers
    auto it_etag = response.headers().find("ETag");
    if (it_etag != response.headers().end()) {
        entry->etag = it_etag->second;
    }

    auto it_last_modified = response.headers().find("Last-Modified");
    if (it_last_modified != response.headers().end()) {
        entry->last_modified = it_last_modified->second;
    }

    Bucket& bucket = getBucket(url);

    if (insertOrUpdate(bucket, url, entry)) {
        entry_count_.fetch_add(1, std::memory_order_relaxed);

        // Check if we need to resize
        if (entry_count_.load(std::memory_order_relaxed) >
            bucket_count_.load(std::memory_order_relaxed) * config_.load_factor_threshold) {
            tryResize();
        }
    }

    epoch_manager_->exit();
}

std::optional<Response> Cache::get(const std::string& url) {
    stats_.get_count.fetch_add(1, std::memory_order_relaxed);

    epoch_manager_->enter();

    Bucket& bucket = getBucket(url);
    Bucket::Node* node = findNode(bucket, url);

    if (node) {
        auto entry = node->entry.load(std::memory_order_acquire);
        if (entry && !entry->marked_for_deletion.load(std::memory_order_acquire)) {
            if (!isExpired(*entry)) {
                stats_.hit_count.fetch_add(1, std::memory_order_relaxed);
                epoch_manager_->exit();
                return entry->response;
            } else {
                // Move to stale for validation
                auto stale_entry = std::make_shared<CacheEntry>();
                stale_entry->etag = entry->etag;
                stale_entry->last_modified = entry->last_modified;
                stale_entry->expires = entry->expires;

                Bucket& stale_bucket = getStaleBucket(url);
                insertOrUpdate(stale_bucket, url, stale_entry);

                // Mark original as deleted
                entry->marked_for_deletion.store(true, std::memory_order_release);
                removeNode(bucket, url);
                entry_count_.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    }

    stats_.miss_count.fetch_add(1, std::memory_order_relaxed);
    epoch_manager_->exit();
    return std::nullopt;
}

void Cache::invalidate(const std::string& url) {
    epoch_manager_->enter();

    Bucket& bucket = getBucket(url);
    if (removeNode(bucket, url)) {
        entry_count_.fetch_sub(1, std::memory_order_relaxed);
    }

    Bucket& stale_bucket = getStaleBucket(url);
    removeNode(stale_bucket, url);

    epoch_manager_->exit();
}

void Cache::clear() {
    epoch_manager_->enter();

    size_t bucket_count = bucket_count_.load(std::memory_order_acquire);

    // Clear main buckets
    for (size_t i = 0; i < bucket_count; ++i) {
        Bucket& bucket = buckets_[i];
        Bucket::Node* head = bucket.head.exchange(nullptr, std::memory_order_acq_rel);

        while (head) {
            Bucket::Node* next = head->next.load(std::memory_order_acquire);
            epoch_manager_->retire(head);
            head = next;
        }
    }

    // Clear stale buckets
    for (size_t i = 0; i < bucket_count; ++i) {
        Bucket& bucket = stale_buckets_[i];
        Bucket::Node* head = bucket.head.exchange(nullptr, std::memory_order_acq_rel);

        while (head) {
            Bucket::Node* next = head->next.load(std::memory_order_acquire);
            epoch_manager_->retire(head);
            head = next;
        }
    }

    entry_count_.store(0, std::memory_order_release);
    epoch_manager_->exit();
}

std::map<std::string, std::string> Cache::get_validation_headers(const std::string& url) {
    std::map<std::string, std::string> headers;

    epoch_manager_->enter();

    Bucket& stale_bucket = getStaleBucket(url);
    Bucket::Node* node = findNode(stale_bucket, url);

    if (node) {
        auto entry = node->entry.load(std::memory_order_acquire);
        if (entry && !entry->marked_for_deletion.load(std::memory_order_acquire)) {
            if (!entry->etag.empty()) {
                headers["If-None-Match"] = entry->etag;
            }
            if (!entry->last_modified.empty()) {
                headers["If-Modified-Since"] = entry->last_modified;
            }
        }
    }

    epoch_manager_->exit();
    return headers;
}

void Cache::handle_not_modified(const std::string& url) {
    epoch_manager_->enter();

    Bucket& stale_bucket = getStaleBucket(url);
    Bucket::Node* stale_node = findNode(stale_bucket, url);

    if (stale_node) {
        auto stale_entry = stale_node->entry.load(std::memory_order_acquire);
        if (stale_entry && !stale_entry->marked_for_deletion.load(std::memory_order_acquire)) {
            // Create new entry with updated expiration
            auto new_entry = std::make_shared<CacheEntry>(
                stale_entry->response,
                std::chrono::system_clock::now() + config_.default_ttl,
                stale_entry->etag,
                stale_entry->last_modified
            );

            // Insert back into main cache
            Bucket& bucket = getBucket(url);
            if (insertOrUpdate(bucket, url, new_entry)) {
                entry_count_.fetch_add(1, std::memory_order_relaxed);
            }

            // Remove from stale
            removeNode(stale_bucket, url);
        }
    }

    epoch_manager_->exit();
}

size_t Cache::size() const noexcept {
    return entry_count_.load(std::memory_order_relaxed);
}

size_t Cache::hash(const std::string& url) const noexcept {
    // Simple FNV-1a hash
    size_t hash = 14695981039346656037ULL;
    for (char c : url) {
        hash ^= static_cast<size_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

Cache::Bucket& Cache::getBucket(const std::string& url) const noexcept {
    size_t h = hash(url);
    size_t bucket_count = bucket_count_.load(std::memory_order_acquire);
    return buckets_[h % bucket_count];
}

Cache::Bucket& Cache::getStaleBucket(const std::string& url) const noexcept {
    size_t h = hash(url);
    size_t bucket_count = bucket_count_.load(std::memory_order_acquire);
    return stale_buckets_[h % bucket_count];
}

Cache::Bucket::Node* Cache::findNode(Bucket& bucket, const std::string& url) const noexcept {
    Bucket::Node* current = bucket.head.load(std::memory_order_acquire);

    while (current) {
        if (current->key == url) {
            return current;
        }
        current = current->next.load(std::memory_order_acquire);
    }

    return nullptr;
}

bool Cache::insertOrUpdate(Bucket& bucket, const std::string& url,
                          std::shared_ptr<CacheEntry> entry) noexcept {
    // Try to find existing node first
    Bucket::Node* current = bucket.head.load(std::memory_order_acquire);

    while (current) {
        if (current->key == url) {
            // Update existing entry
            current->entry.store(entry, std::memory_order_release);
            current->version.fetch_add(1, std::memory_order_relaxed);
            return false;  // Updated, not inserted
        }
        current = current->next.load(std::memory_order_acquire);
    }

    // Create new node
    auto new_node = new(std::nothrow) Bucket::Node(url);
    if (!new_node) {
        return false;
    }

    new_node->entry.store(entry, std::memory_order_release);

    // Insert at head using CAS
    Bucket::Node* head = bucket.head.load(std::memory_order_relaxed);
    do {
        new_node->next.store(head, std::memory_order_relaxed);
    } while (!bucket.head.compare_exchange_weak(head, new_node,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));

    return true;  // Inserted new node
}

bool Cache::removeNode(Bucket& bucket, const std::string& url) noexcept {
    Bucket::Node* prev = nullptr;
    Bucket::Node* current = bucket.head.load(std::memory_order_acquire);

    while (current) {
        if (current->key == url) {
            // Mark entry for deletion
            auto entry = current->entry.load(std::memory_order_acquire);
            if (entry) {
                entry->marked_for_deletion.store(true, std::memory_order_release);
            }

            // Remove from list
            Bucket::Node* next = current->next.load(std::memory_order_acquire);

            if (prev) {
                prev->next.store(next, std::memory_order_release);
            } else {
                bucket.head.store(next, std::memory_order_release);
            }

            // Retire node for safe deletion
            epoch_manager_->retire(current);
            return true;
        }

        prev = current;
        current = current->next.load(std::memory_order_acquire);
    }

    return false;
}

bool Cache::isExpired(const CacheEntry& entry) const noexcept {
    return std::chrono::system_clock::now() >= entry.expires;
}

void Cache::tryResize() noexcept {
    // Simple resize strategy - double the bucket count
    size_t current_bucket_count = bucket_count_.load(std::memory_order_acquire);

    // For now, skip resizing to keep implementation simple
    // In a production system, you'd implement rehashing here
    spdlog::debug("Cache resize triggered but skipped (current buckets: {})", current_bucket_count);
}

// EpochManager implementation
void Cache::EpochManager::enter() noexcept {
    size_t index = getThreadIndex();
    if (index < MAX_THREADS) {
        auto& thread_epoch = thread_epochs_[index];
        thread_epoch.thread_id.store(std::this_thread::get_id(), std::memory_order_relaxed);
        thread_epoch.active.store(true, std::memory_order_release);
        thread_epoch.epoch.store(global_epoch_.load(std::memory_order_acquire),
                                std::memory_order_release);
    }
}

void Cache::EpochManager::exit() noexcept {
    size_t index = getThreadIndex();
    if (index < MAX_THREADS) {
        thread_epochs_[index].active.store(false, std::memory_order_release);

        // Periodically try to advance epoch
        static thread_local size_t counter = 0;
        if (++counter % 64 == 0) {
            tryAdvanceEpoch();
        }
    }
}

void Cache::EpochManager::retire(Bucket::Node* node) noexcept {
    if (!node) return;

    uint64_t current_epoch = global_epoch_.load(std::memory_order_acquire);
    size_t epoch_index = current_epoch % EPOCHS;

    auto& retired_list = retired_lists_[epoch_index];

    // Add to retired list
    Bucket::Node* head = retired_list.head.load(std::memory_order_relaxed);
    do {
        node->next.store(head, std::memory_order_relaxed);
    } while (!retired_list.head.compare_exchange_weak(head, node,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed));

    retired_list.count.fetch_add(1, std::memory_order_relaxed);
}

void Cache::EpochManager::tryAdvanceEpoch() noexcept {
    uint64_t current_epoch = global_epoch_.load(std::memory_order_acquire);
    uint64_t min_epoch = getMinEpoch();

    // Can advance if all active threads are at current epoch
    if (min_epoch >= current_epoch) {
        uint64_t new_epoch = current_epoch + 1;
        if (global_epoch_.compare_exchange_strong(current_epoch, new_epoch,
                                                  std::memory_order_acq_rel)) {
            // Successfully advanced, reclaim old epoch
            size_t reclaim_epoch = (new_epoch - EPOCHS) % EPOCHS;
            reclaimEpoch(reclaim_epoch);
        }
    }
}

size_t Cache::EpochManager::getThreadIndex() noexcept {
    if (thread_index_ == SIZE_MAX) {
        std::thread::id tid = std::this_thread::get_id();

        // Find available slot
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            std::thread::id expected{};
            if (thread_epochs_[i].thread_id.compare_exchange_strong(expected, tid,
                                                                   std::memory_order_acq_rel)) {
                thread_index_ = i;
                break;
            }
            if (thread_epochs_[i].thread_id.load(std::memory_order_acquire) == tid) {
                thread_index_ = i;
                break;
            }
        }
    }

    return thread_index_;
}

uint64_t Cache::EpochManager::getMinEpoch() const noexcept {
    uint64_t min_epoch = global_epoch_.load(std::memory_order_acquire);

    for (const auto& thread_epoch : thread_epochs_) {
        if (thread_epoch.active.load(std::memory_order_acquire)) {
            uint64_t epoch = thread_epoch.epoch.load(std::memory_order_acquire);
            min_epoch = std::min(min_epoch, epoch);
        }
    }

    return min_epoch;
}

void Cache::EpochManager::reclaimEpoch(size_t epoch_index) noexcept {
    auto& retired_list = retired_lists_[epoch_index];

    Bucket::Node* head = retired_list.head.exchange(nullptr, std::memory_order_acq_rel);
    retired_list.count.store(0, std::memory_order_relaxed);

    // Delete all retired nodes from this epoch
    while (head) {
        Bucket::Node* next = head->next.load(std::memory_order_relaxed);
        delete head;
        head = next;
    }
}

}  // namespace atom::extra::curl
