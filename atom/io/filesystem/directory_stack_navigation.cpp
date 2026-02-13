#include "directory_stack_impl.hpp"

namespace atom::io {

template <PathLike P>
void DirectoryStack::asyncPushd(
    const P& new_dir,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPushd<P>(new_dir, handler);
}

template <PathLike P>
auto DirectoryStack::pushd(const P& new_dir_param) -> Task<void> {
    fs::path new_dir = new_dir_param;
    co_await std::suspend_never{};

    std::error_code ec;
    try {
        if (!isValidPath(new_dir)) {
            spdlog::error("pushd: Invalid path provided - {}",
                          new_dir.string());
            throw fs::filesystem_error(
                "Invalid path provided", new_dir,
                std::make_error_code(std::errc::invalid_argument));
        }

        auto currentPath = fs::current_path(ec);
        if (ec) {
            spdlog::error("pushd: Failed to get current path. Error: {}",
                          ec.message());
            throw fs::filesystem_error("Failed to get current path", ec);
        }

        {
            std::unique_lock lock(impl_->stackMutex_);
            impl_->dirStack_.push(currentPath);
        }

        fs::current_path(new_dir, ec);
        if (ec) {
            spdlog::warn(
                "pushd: Failed to change directory to {}, "
                "rolling back stack push. Error: {}",
                new_dir.string(), ec.message());
            std::unique_lock lock(impl_->stackMutex_);
            if (!impl_->dirStack_.empty() &&
                impl_->dirStack_.top() == currentPath) {
                impl_->dirStack_.pop();
            }
            throw fs::filesystem_error("Failed to change directory", new_dir,
                                       ec);
        }
        spdlog::info("pushd successful to {}", new_dir.string());
    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in pushd: {}", e.what());
        throw;
    }

    co_return;
}

void DirectoryStack::asyncPopd(
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPopd(handler);
}

auto DirectoryStack::popd() -> Task<void> {
    co_await std::suspend_never{};

    std::error_code ec;
    try {
        fs::path prevDir;

        {
            std::unique_lock lock(impl_->stackMutex_);
            if (impl_->dirStack_.empty()) {
                spdlog::warn("popd: Directory stack is empty");
                throw fs::filesystem_error(
                    "Directory stack is empty",
                    std::make_error_code(std::errc::operation_not_permitted));
            }
            prevDir = impl_->dirStack_.top();
            impl_->dirStack_.pop();
        }

        if (!isValidPath(prevDir)) {
            spdlog::error("popd: Invalid path found in stack - {}",
                          prevDir.string());
            throw fs::filesystem_error(
                "Invalid path in stack", prevDir,
                std::make_error_code(std::errc::invalid_argument));
        }

        fs::current_path(prevDir, ec);
        if (ec) {
            spdlog::error("popd: Failed to change directory to {}. Error: {}",
                          prevDir.string(), ec.message());
            throw fs::filesystem_error("Failed to change directory", prevDir,
                                       ec);
        }
        spdlog::info("popd successful to {}", prevDir.string());
    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in popd: {}", e.what());
        throw;
    }

    co_return;
}

void DirectoryStack::asyncGotoIndex(
    size_t index, const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncGotoIndex(index, handler);
}

auto DirectoryStack::gotoIndex(size_t index) -> Task<void> {
    co_await std::suspend_never{};

    std::error_code ec;
    try {
        Vector<fs::path> contents;
        {
            std::shared_lock lock(impl_->stackMutex_);
            std::stack<fs::path> tempStack = impl_->dirStack_;
            contents.reserve(tempStack.size());
            while (!tempStack.empty()) {
                contents.push_back(tempStack.top());
                tempStack.pop();
            }
            std::reverse(contents.begin(), contents.end());
        }

        size_t size = contents.size();
        if (index >= size) {
            spdlog::warn("gotoIndex: Index out of bounds");
            throw fs::filesystem_error(
                "Index out of bounds",
                std::make_error_code(std::errc::invalid_argument));
        }

        size_t vec_index = size - 1 - index;
        const fs::path& targetPath = contents[vec_index];

        if (!isValidPath(targetPath)) {
            spdlog::error("gotoIndex: Invalid path in stack at index {}",
                          index);
            throw fs::filesystem_error(
                "Invalid path in stack", targetPath,
                std::make_error_code(std::errc::invalid_argument));
        }

        fs::current_path(targetPath, ec);
        if (ec) {
            spdlog::error("gotoIndex: Failed to change directory. Error: {}",
                          ec.message());
            throw fs::filesystem_error("Failed to change directory", targetPath,
                                       ec);
        }
    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in gotoIndex: {}", e.what());
        throw;
    }
    co_return;
}

void DirectoryStack::asyncGetCurrentDirectory(
    const std::function<void(const std::filesystem::path&,
                             const std::error_code&)>& handler) const {
    impl_->asyncGetCurrentDirectory(handler);
}

auto DirectoryStack::getCurrentDirectory() const -> Task<fs::path> {
    co_await std::suspend_never{};

    std::error_code ec;
    fs::path currentPath;
    try {
        currentPath = fs::current_path(ec);
        if (ec) {
            spdlog::error(
                "getCurrentDirectory: Failed to get current path. Error: {}",
                ec.message());
            throw fs::filesystem_error("Failed to get current path", ec);
        }
    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in getCurrentDirectory: {}", e.what());
        throw;
    }
    co_return currentPath;
}

template void DirectoryStack::asyncPushd<fs::path>(
    const fs::path&, const std::function<void(const std::error_code&)>&);
template void DirectoryStack::asyncPushd<std::string>(
    const std::string&, const std::function<void(const std::error_code&)>&);

template auto DirectoryStack::pushd<fs::path>(const fs::path&) -> Task<void>;
template auto DirectoryStack::pushd<std::string>(const std::string&)
    -> Task<void>;

}  // namespace atom::io
