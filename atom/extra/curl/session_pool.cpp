#include "session_pool.hpp"
#include "session.hpp"

namespace atom::extra::curl {
SessionPool::SessionPool(size_t max_sessions) : max_sessions_(max_sessions) {}

SessionPool::~SessionPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.clear();  // 智能指针自动清理
}

std::shared_ptr<Session> SessionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!pool_.empty()) {
        auto session = pool_.back();
        pool_.pop_back();
        return session;
    }

    // 如果池为空，创建新的会话
    return std::make_shared<Session>();
}

void SessionPool::release(std::shared_ptr<Session> session) {
    if (!session)
        return;

    std::unique_lock<std::mutex> lock(mutex_);

    if (pool_.size() < max_sessions_) {
        pool_.push_back(std::move(session));
    }
    // 如果池已满，session 会自动析构
}
}  // namespace atom::extra::curl