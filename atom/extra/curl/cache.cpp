#include "cache.hpp"

namespace atom::extra::curl {
Cache::Cache(std::chrono::seconds default_ttl) : default_ttl_(default_ttl) {}

void Cache::set(const std::string& url, const Response& response,
                std::optional<std::chrono::seconds> ttl) {
    std::lock_guard lock(mutex_);

    CacheEntry entry{
        response,
        std::chrono::system_clock::now() + (ttl ? *ttl : default_ttl_),
        "",  // empty etag
        ""   // empty last_modified
    };

    // 从响应中提取 ETag 和 Last-Modified
    auto it_etag = response.headers().find("ETag");
    if (it_etag != response.headers().end()) {
        entry.etag = it_etag->second;
    }

    auto it_last_modified = response.headers().find("Last-Modified");
    if (it_last_modified != response.headers().end()) {
        entry.last_modified = it_last_modified->second;
    }

    cache_[url] = std::move(entry);
}

std::optional<Response> Cache::get(const std::string& url) {
    std::lock_guard lock(mutex_);

    auto it = cache_.find(url);
    if (it != cache_.end()) {
        if (std::chrono::system_clock::now() < it->second.expires) {
            return it->second.response;
        } else {
            // 过期但保留条件验证所需的字段
            stale_[url] = std::move(it->second);
            cache_.erase(it);
        }
    }

    return std::nullopt;
}

void Cache::invalidate(const std::string& url) {
    std::lock_guard lock(mutex_);
    cache_.erase(url);
    stale_.erase(url);
}

void Cache::clear() {
    std::lock_guard lock(mutex_);
    cache_.clear();
    stale_.clear();
}

std::map<std::string, std::string> Cache::get_validation_headers(
    const std::string& url) {
    std::lock_guard lock(mutex_);
    std::map<std::string, std::string> headers;

    auto it = stale_.find(url);
    if (it != stale_.end()) {
        if (!it->second.etag.empty()) {
            headers["If-None-Match"] = it->second.etag;
        }

        if (!it->second.last_modified.empty()) {
            headers["If-Modified-Since"] = it->second.last_modified;
        }
    }

    return headers;
}

void Cache::handle_not_modified(const std::string& url) {
    std::lock_guard lock(mutex_);

    auto it = stale_.find(url);
    if (it != stale_.end()) {
        it->second.expires = std::chrono::system_clock::now() + default_ttl_;
        cache_[url] = std::move(it->second);
        stale_.erase(it);
    }
}
}  // namespace atom::extra::curl
