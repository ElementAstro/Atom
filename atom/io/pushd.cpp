#include "pushd.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
namespace fs = boost::filesystem;
namespace asio = boost::asio;
#else
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stack>
#include "atom/log/loguru.hpp"
namespace fs = std::filesystem;
#endif

namespace atom::io {
class DirectoryStackImpl {
public:
    explicit DirectoryStackImpl(asio::io_context& io_context)
#ifdef ATOM_USE_BOOST
        : io_context_(io_context),
          strand_(io_context)
#else
        : strand_(io_context)
#endif
    {
    }

    void asyncPushd(
        const fs::path& new_dir,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncPushd called with new_dir: " << new_dir.string();
#else
        LOG_F(INFO, "asyncPushd called with new_dir: %s",
              new_dir.string().c_str());
#endif
        asio::post(strand_, [this, new_dir, handler]() {
            std::error_code errorCode;
            fs::path currentDir = fs::current_path(errorCode);
            if (!errorCode) {
                dirStack_.push(currentDir);
                fs::current_path(new_dir, errorCode);
            }
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(info) << "asyncPushd completed with error code: "
                                    << errorCode.value();
#else
            LOG_F(INFO, "asyncPushd completed with error code: %d", errorCode.value());
#endif
            handler(errorCode);
        });
    }

    void asyncPopd(const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "asyncPopd called";
#else
        LOG_F(INFO, "asyncPopd called");
#endif
        asio::post(strand_, [this, handler]() {
            std::error_code errorCode;
            if (!dirStack_.empty()) {
                fs::path prevDir = dirStack_.top();
                dirStack_.pop();
                fs::current_path(prevDir, errorCode);
            } else {
                errorCode = std::make_error_code(std::errc::invalid_argument);
            }
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(info)
                << "asyncPopd completed with error code: " << errorCode.value();
#else
            LOG_F(INFO, "asyncPopd completed with error code: %d", errorCode.value());
#endif
            handler(errorCode);
        });
    }

    [[nodiscard]] auto getStackContents() const -> std::vector<fs::path> {
        std::stack<fs::path> tempStack = dirStack_;
        std::vector<fs::path> contents;
        while (!tempStack.empty()) {
            contents.push_back(tempStack.top());
            tempStack.pop();
        }
        std::reverse(contents.begin(), contents.end());
        return contents;
    }

    void asyncGotoIndex(
        size_t index,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncGotoIndex called with index: " << index;
#else
        LOG_F(INFO, "asyncGotoIndex called with index: %zu", index);
#endif
        asio::post(strand_, [this, index, handler]() {
            std::error_code errorCode;
            auto contents = getStackContents();
            if (index < contents.size()) {
                fs::current_path(contents[index], errorCode);
            } else {
                errorCode = std::make_error_code(std::errc::invalid_argument);
            }
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(info)
                << "asyncGotoIndex completed with error code: "
                << errorCode.value();
#else
            LOG_F(INFO, "asyncGotoIndex completed with error code: %d", errorCode.value());
#endif
            handler(errorCode);
        });
    }

    void asyncSaveStackToFile(
        const std::string& filename,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncSaveStackToFile called with filename: " << filename;
#else
        LOG_F(INFO, "asyncSaveStackToFile called with filename: %s",
              filename.c_str());
#endif
        asio::post(strand_, [this, filename, handler]() {
            std::error_code errorCode;
            std::ofstream file(filename);
            if (file) {
                auto contents = getStackContents();
                for (const auto& dir : contents) {
                    file << dir.string() << '\n';
                }
            } else {
                errorCode = std::make_error_code(std::errc::io_error);
            }
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(info)
                << "asyncSaveStackToFile completed with error code: "
                << errorCode.value();
#else
            LOG_F(INFO, "asyncSaveStackToFile completed with error code: %d", errorCode.value());
#endif
            handler(errorCode);
        });
    }

    void asyncLoadStackFromFile(
        const std::string& filename,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncLoadStackFromFile called with filename: " << filename;
#else
        LOG_F(INFO, "asyncLoadStackFromFile called with filename: %s",
              filename.c_str());
#endif
        asio::post(strand_, [this, filename, handler]() {
            std::error_code errorCode;
            std::ifstream file(filename);
            if (file) {
                std::stack<fs::path> newStack;
                std::string line;
                while (std::getline(file, line)) {
                    newStack.emplace(line);
                }
                dirStack_ = std::move(newStack);
            } else {
                errorCode = std::make_error_code(std::errc::io_error);
            }
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(info)
                << "asyncLoadStackFromFile completed with error code: "
                << errorCode.value();
#else
            LOG_F(INFO, "asyncLoadStackFromFile completed with error code: %d", errorCode.value());
#endif
            handler(errorCode);
        });
    }

    void asyncGetCurrentDirectory(
        const std::function<void(const fs::path&)>& handler) const {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "asyncGetCurrentDirectory called";
#else
        LOG_F(INFO, "asyncGetCurrentDirectory called");
#endif
        asio::post(strand_, [handler]() {
            auto currentPath = fs::current_path();
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(info)
                << "asyncGetCurrentDirectory completed with current path: "
                << currentPath.string();
#else
            LOG_F(INFO, "asyncGetCurrentDirectory completed with current path: %s", currentPath.string().c_str());
#endif
            handler(currentPath);
        });
    }

    std::stack<fs::path> dirStack_;
    asio::io_context::strand strand_;
#ifdef ATOM_USE_BOOST
    asio::io_context& io_context_;
#endif
};

// DirectoryStack public interface methods implementation

DirectoryStack::DirectoryStack(asio::io_context& io_context)
#ifdef ATOM_USE_BOOST
    : impl_(std::make_unique<DirectoryStackImpl>(io_context))
#else
    : impl_(std::make_unique<DirectoryStackImpl>(io_context))
#endif
{
}

DirectoryStack::~DirectoryStack() = default;

DirectoryStack::DirectoryStack(DirectoryStack&& other) noexcept = default;

auto DirectoryStack::operator=(DirectoryStack&& other) noexcept
    -> DirectoryStack& = default;

void DirectoryStack::asyncPushd(
    const fs::path& new_dir,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPushd(new_dir, handler);
}

void DirectoryStack::asyncPopd(
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPopd(handler);
}

auto DirectoryStack::peek() const -> fs::path {
    return impl_->dirStack_.empty() ? fs::path() : impl_->dirStack_.top();
}

auto DirectoryStack::dirs() const -> std::vector<fs::path> {
    return impl_->getStackContents();
}

void DirectoryStack::clear() { impl_->dirStack_ = std::stack<fs::path>(); }

void DirectoryStack::swap(size_t index1, size_t index2) {
    auto contents = impl_->getStackContents();
    if (index1 < contents.size() && index2 < contents.size()) {
        std::swap(contents[index1], contents[index2]);
        impl_->dirStack_ = std::stack<fs::path>(
            std::deque<fs::path>(contents.begin(), contents.end()));
    }
}

void DirectoryStack::remove(size_t index) {
    auto contents = impl_->getStackContents();
    if (index < contents.size()) {
        contents.erase(contents.begin() + static_cast<long>(index));
        impl_->dirStack_ = std::stack<fs::path>(
            std::deque<fs::path>(contents.begin(), contents.end()));
    }
}

void DirectoryStack::asyncGotoIndex(
    size_t index, const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncGotoIndex(index, handler);
}

void DirectoryStack::asyncSaveStackToFile(
    const std::string& filename,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncSaveStackToFile(filename, handler);
}

void DirectoryStack::asyncLoadStackFromFile(
    const std::string& filename,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncLoadStackFromFile(filename, handler);
}

auto DirectoryStack::size() const -> size_t { return impl_->dirStack_.size(); }

auto DirectoryStack::isEmpty() const -> bool {
    return impl_->dirStack_.empty();
}

void DirectoryStack::asyncGetCurrentDirectory(
    const std::function<void(const fs::path&)>& handler) const {
    impl_->asyncGetCurrentDirectory(handler);
}

}  // namespace atom::io