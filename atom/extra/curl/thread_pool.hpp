#ifndef ATOM_EXTRA_CURL_THREAD_POOL_HPP
#define ATOM_EXTRA_CURL_THREAD_POOL_HPP

#include "atom/async/pool.hpp"

namespace atom::extra::curl {

// Use atom::async::ThreadPool directly
using ThreadPool = atom::async::ThreadPool;

// Provide compatibility aliases for configuration
namespace ThreadPoolConfig {
    inline atom::async::ThreadPool::Options createDefault() {
        return atom::async::ThreadPool::Options::createDefault();
    }

    inline atom::async::ThreadPool::Options createHighThroughput() {
        return atom::async::ThreadPool::Options::createHighPerformance();
    }

    inline atom::async::ThreadPool::Options createLowLatency() {
        return atom::async::ThreadPool::Options::createLowLatency();
    }
}

}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_THREAD_POOL_HPP
