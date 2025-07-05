#ifndef ATOM_EXTRA_CURL_CACHE_HPP
#define ATOM_EXTRA_CURL_CACHE_HPP

#include "response.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace atom::extra::curl {
/**
 * @brief Class for caching HTTP responses.
 *
 * This class provides a simple caching mechanism for HTTP responses,
 * allowing you to store and retrieve responses based on their URL.
 * It supports expiration and validation headers for efficient caching.
 */
class Cache {
public:
    /**
     * @brief Structure representing a cache entry.
     *
     * This structure holds the cached response, its expiration time,
     * ETag, and Last-Modified header for validation.
     */
    struct CacheEntry {
        /** @brief The cached HTTP response. */
        Response response;
        /** @brief The expiration time of the cache entry. */
        std::chrono::system_clock::time_point expires;
        /** @brief The ETag header associated with the response. */
        std::string etag;
        /** @brief The Last-Modified header associated with the response. */
        std::string last_modified;
    };

    /**
     * @brief Constructor for the Cache class.
     *
     * @param default_ttl The default time-to-live for cache entries, in
     * seconds. Defaults to 5 minutes.
     */
    Cache(std::chrono::seconds default_ttl = std::chrono::minutes(5));

    /**
     * @brief Sets a cache entry for the given URL.
     *
     * @param url The URL to cache the response for.
     * @param response The HTTP response to cache.
     * @param ttl An optional time-to-live for the cache entry, in seconds.
     *            If not provided, the default TTL is used.
     */
    void set(const std::string& url, const Response& response,
             std::optional<std::chrono::seconds> ttl = std::nullopt);

    /**
     * @brief Retrieves a cached response for the given URL.
     *
     * @param url The URL to retrieve the cached response for.
     * @return An optional Response object if a valid cache entry exists,
     *         std::nullopt otherwise.
     */
    std::optional<Response> get(const std::string& url);

    /**
     * @brief Invalidates the cache entry for the given URL.
     *
     * @param url The URL to invalidate the cache entry for.
     */
    void invalidate(const std::string& url);

    /**
     * @brief Clears the entire cache.
     */
    void clear();

    /**
     * @brief Gets the validation headers for the given URL.
     *
     * These headers can be used to perform conditional requests to
     * validate the cached response with the server.
     *
     * @param url The URL to get the validation headers for.
     * @return A map of header names to header values.
     */
    std::map<std::string, std::string> get_validation_headers(
        const std::string& url);

    /**
     * @brief Handles a "Not Modified" response from the server.
     *
     * This method updates the expiration time of the cache entry
     * when the server returns a "304 Not Modified" response,
     * indicating that the cached response is still valid.
     *
     * @param url The URL that received the "Not Modified" response.
     */
    void handle_not_modified(const std::string& url);

private:
    /** @brief The default time-to-live for cache entries, in seconds. */
    std::chrono::seconds default_ttl_;
    /** @brief The cache map, storing URL-to-CacheEntry mappings. */
    std::unordered_map<std::string, CacheEntry> cache_;
    /** @brief The stale cache map, storing expired entries for validation. */
    std::unordered_map<std::string, CacheEntry> stale_;
    /** @brief Mutex to protect the cache from concurrent access. */
    std::mutex mutex_;
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_CACHE_HPP
