#ifndef ATOM_EXTRA_CURL_CACHE_HPP
#define ATOM_EXTRA_CURL_CACHE_HPP

#include "response.hpp"

#include <chrono>
#include <mutex>
#include <unordered_map>

class Cache {
public:
    struct CacheEntry {
        Response response;
        std::chrono::system_clock::time_point expires;
        std::string etag;
        std::string last_modified;
    };

    Cache(std::chrono::seconds default_ttl = std::chrono::minutes(5))
        : default_ttl_(default_ttl) {}

    void set(const std::string& url, const Response& response,
             std::optional<std::chrono::seconds> ttl = std::nullopt) {
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

    std::optional<Response> get(const std::string& url) {
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

    void invalidate(const std::string& url) {
        std::lock_guard lock(mutex_);
        cache_.erase(url);
        stale_.erase(url);
    }

    void clear() {
        std::lock_guard lock(mutex_);
        cache_.clear();
        stale_.clear();
    }

    // 获取条件验证请求的头信息
    std::map<std::string, std::string> get_validation_headers(
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

    // 处理304响应，恢复缓存
    void handle_not_modified(const std::string& url) {
        std::lock_guard lock(mutex_);

        auto it = stale_.find(url);
        if (it != stale_.end()) {
            it->second.expires =
                std::chrono::system_clock::now() + default_ttl_;
            cache_[url] = std::move(it->second);
            stale_.erase(it);
        }
    }

private:
    std::chrono::seconds default_ttl_;
    std::unordered_map<std::string, CacheEntry> cache_;
    std::unordered_map<std::string, CacheEntry>
        stale_;  // 过期但可能仍有效的条目
    std::mutex mutex_;
};

#endif  // ATOM_EXTRA_CURL_CACHE_HPP