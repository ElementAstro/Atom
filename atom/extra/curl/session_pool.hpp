#ifndef ATOM_EXTRA_CURL_SESSION_POOL_HPP
#define ATOM_EXTRA_CURL_SESSION_POOL_HPP

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

/**
 * @brief Namespace for curl related utilities.
 */
namespace atom::extra::curl {

class Session;
/**
 * @brief Manages a pool of Session objects for reuse.
 *
 * This class provides a mechanism to efficiently manage and reuse Session
 * objects, reducing the overhead of creating new sessions for each request.
 * It uses a mutex to ensure thread safety.
 */
class SessionPool {
public:
    /**
     * @brief Constructor for the SessionPool class.
     *
     * @param max_sessions The maximum number of sessions to keep in the pool.
     * Defaults to 10.
     */
    SessionPool(size_t max_sessions = 10);

    /**
     * @brief Destructor for the SessionPool class.
     *
     * Clears the session pool and releases all Session objects.
     */
    ~SessionPool();

    /**
     * @brief Acquires a Session object from the pool.
     *
     * If there are available Session objects in the pool, this method returns
     * one of them. Otherwise, it creates a new Session object.
     *
     * @return A shared pointer to a Session object.
     */
    std::shared_ptr<Session> acquire();

    /**
     * @brief Releases a Session object back to the pool.
     *
     * This method returns a Session object to the pool for reuse. If the pool
     * is full, the Session object is destroyed.
     *
     * @param session A shared pointer to the Session object to release.
     */
    void release(std::shared_ptr<Session> session);

private:
    /** @brief The maximum number of sessions to keep in the pool. */
    size_t max_sessions_;
    /** @brief The vector of Session objects in the pool. */
    std::vector<std::shared_ptr<Session>> pool_;
    /** @brief Mutex to protect the session pool from concurrent access. */
    std::mutex mutex_;
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_SESSION_POOL_HPP