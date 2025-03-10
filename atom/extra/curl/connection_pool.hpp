#ifndef ATOM_EXTRA_CURL_CONNECTION_POOL_HPP
#define ATOM_EXTRA_CURL_CONNECTION_POOL_HPP

#include <curl/curl.h>
#include <mutex>
#include <vector>

namespace atom::extra::curl {
class ConnectionPool {
public:
    ConnectionPool(size_t max_connections = 10);
    ~ConnectionPool();
    CURL* acquire();
    void release(CURL* handle);

private:
    size_t max_connections_;
    std::vector<CURL*> pool_;
    std::mutex mutex_;
};
}  // namespace atom::extra::curl

#endif