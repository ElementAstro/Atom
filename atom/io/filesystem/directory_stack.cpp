#include "directory_stack_impl.hpp"

namespace atom::io {

DirectoryStack::DirectoryStack(
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    asio::io_context& io_context
#else
    void* io_context
#endif
    )
    : impl_(std::make_unique<DirectoryStackImpl>(
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
          io_context
#else
          io_context
#endif
          )) {
}

DirectoryStack::~DirectoryStack() noexcept = default;
DirectoryStack::DirectoryStack(DirectoryStack&& other) noexcept = default;
auto DirectoryStack::operator=(DirectoryStack&& other) noexcept
    -> DirectoryStack& = default;

auto DirectoryStack::peek() const -> fs::path {
    std::shared_lock lock(impl_->stackMutex_);
    if (impl_->dirStack_.empty()) {
        throw std::runtime_error("Directory stack is empty");
    }
    return impl_->dirStack_.top();
}

auto DirectoryStack::dirs() const noexcept -> Vector<fs::path> {
    Vector<fs::path> contents = impl_->getStackContents();
    std::reverse(contents.begin(), contents.end());
    return contents;
}

void DirectoryStack::clear() noexcept {
    std::unique_lock lock(impl_->stackMutex_);
    std::stack<fs::path>().swap(impl_->dirStack_);
}

void DirectoryStack::swap(size_t index1, size_t index2) {
    std::unique_lock lock(impl_->stackMutex_);
    std::stack<fs::path> tempStack = impl_->dirStack_;
    Vector<fs::path> contents;
    contents.reserve(tempStack.size());
    while (!tempStack.empty()) {
        contents.push_back(tempStack.top());
        tempStack.pop();
    }
    std::reverse(contents.begin(), contents.end());

    size_t size = contents.size();
    if (index1 >= size || index2 >= size) {
        spdlog::warn("swap: Index out of bounds");
        throw std::out_of_range("Index out of bounds for directory stack swap");
    }

    size_t vec_index1 = size - 1 - index1;
    size_t vec_index2 = size - 1 - index2;

    std::swap(contents[vec_index1], contents[vec_index2]);

    std::stack<fs::path> newStack;
    for (const auto& path : contents) {
        newStack.push(path);
    }
    impl_->dirStack_ = std::move(newStack);
}

void DirectoryStack::remove(size_t index) {
    std::unique_lock lock(impl_->stackMutex_);
    std::stack<fs::path> tempStack = impl_->dirStack_;
    Vector<fs::path> contents;
    contents.reserve(tempStack.size());
    while (!tempStack.empty()) {
        contents.push_back(tempStack.top());
        tempStack.pop();
    }
    std::reverse(contents.begin(), contents.end());

    size_t size = contents.size();
    if (index >= size) {
        spdlog::warn("remove: Index out of bounds");
        throw std::out_of_range(
            "Index out of bounds for directory stack remove");
    }

    size_t vec_index = size - 1 - index;
    contents.erase(contents.begin() +
                   static_cast<Vector<fs::path>::difference_type>(vec_index));

    std::stack<fs::path> newStack;
    for (const auto& path : contents) {
        newStack.push(path);
    }
    impl_->dirStack_ = std::move(newStack);
}

auto DirectoryStack::size() const noexcept -> size_t {
    std::shared_lock lock(impl_->stackMutex_);
    return impl_->dirStack_.size();
}

auto DirectoryStack::isEmpty() const noexcept -> bool {
    std::shared_lock lock(impl_->stackMutex_);
    return impl_->dirStack_.empty();
}

}  // namespace atom::io
