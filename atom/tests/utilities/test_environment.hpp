/**
 * @file test_environment.hpp
 * @brief Global test environment for setup/teardown across all tests
 * @details Provides Environment class for global test resource management
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_UTILITIES_TEST_ENVIRONMENT_HPP
#define ATOM_TEST_UTILITIES_TEST_ENVIRONMENT_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "atom/tests/core/test.hpp"

namespace atom::test {

/**
 * @brief Base class for test environments
 * @details Provides global SetUp/TearDown for test resources that need to be
 *          initialized once before all tests and cleaned up after all tests.
 *
 * Usage:
 * @code
 * class DatabaseEnvironment : public atom::test::Environment {
 * public:
 *     void SetUp() override {
 *         db_ = std::make_unique<Database>("test_db");
 *         db_->connect();
 *     }
 *
 *     void TearDown() override {
 *         if (db_) {
 *             db_->disconnect();
 *             db_.reset();
 *         }
 *     }
 *
 *     static Database* GetDatabase() { return instance_->db_.get(); }
 *
 * private:
 *     std::unique_ptr<Database> db_;
 *     static DatabaseEnvironment* instance_;
 * };
 *
 * // In main:
 * int main(int argc, char** argv) {
 *     atom::test::AddGlobalTestEnvironment(new DatabaseEnvironment);
 *     return atom::test::runAllTests(argc, argv);
 * }
 * @endcode
 */
class Environment {
public:
    Environment() = default;
    virtual ~Environment() = default;

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;
    Environment(Environment&&) = delete;
    Environment& operator=(Environment&&) = delete;

    /**
     * @brief Called before the first test starts
     * @details Override this to set up global test resources
     */
    virtual void SetUp() {}

    /**
     * @brief Called after the last test finishes
     * @details Override this to clean up global test resources
     */
    virtual void TearDown() {}

    /**
     * @brief Get the environment name for logging
     * @return Human-readable name
     */
    [[nodiscard]] virtual std::string name() const {
        return "Environment";
    }
};

/**
 * @brief Global environment registry
 */
class EnvironmentRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static EnvironmentRegistry& instance() {
        static EnvironmentRegistry registry;
        return registry;
    }

    /**
     * @brief Add an environment to the registry
     * @param env The environment to add (takes ownership)
     * @return Pointer to the added environment
     */
    Environment* addEnvironment(std::unique_ptr<Environment> env) {
        std::lock_guard lock(mutex_);
        environments_.push_back(std::move(env));
        return environments_.back().get();
    }

    /**
     * @brief Set up all registered environments
     */
    void setUpAll() {
        std::lock_guard lock(mutex_);
        for (auto& env : environments_) {
            try {
                env->SetUp();
            } catch (const std::exception& e) {
                std::cerr << "Environment " << env->name()
                          << " SetUp failed: " << e.what() << "\n";
                throw;
            }
        }
        initialized_ = true;
    }

    /**
     * @brief Tear down all registered environments (in reverse order)
     */
    void tearDownAll() {
        std::lock_guard lock(mutex_);
        for (auto it = environments_.rbegin(); it != environments_.rend();
             ++it) {
            try {
                (*it)->TearDown();
            } catch (const std::exception& e) {
                std::cerr << "Environment " << (*it)->name()
                          << " TearDown failed: " << e.what() << "\n";
            }
        }
        initialized_ = false;
    }

    /**
     * @brief Check if environments have been initialized
     */
    [[nodiscard]] bool isInitialized() const { return initialized_; }

    /**
     * @brief Get the number of registered environments
     */
    [[nodiscard]] size_t count() const {
        std::lock_guard lock(mutex_);
        return environments_.size();
    }

    /**
     * @brief Clear all environments
     */
    void clear() {
        std::lock_guard lock(mutex_);
        environments_.clear();
        initialized_ = false;
    }

private:
    EnvironmentRegistry() = default;

    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Environment>> environments_;
    bool initialized_{false};
};

/**
 * @brief Add a global test environment
 * @param env The environment to add (takes ownership via raw pointer for GTest
 * compatibility)
 * @return Pointer to the added environment
 */
inline Environment* AddGlobalTestEnvironment(Environment* env) {
    return EnvironmentRegistry::instance().addEnvironment(
        std::unique_ptr<Environment>(env));
}

/**
 * @brief Add a global test environment (smart pointer version)
 * @param env The environment to add
 * @return Pointer to the added environment
 */
inline Environment* AddGlobalTestEnvironment(std::unique_ptr<Environment> env) {
    return EnvironmentRegistry::instance().addEnvironment(std::move(env));
}

/**
 * @brief RAII guard for environment lifecycle
 */
class EnvironmentGuard {
public:
    EnvironmentGuard() { EnvironmentRegistry::instance().setUpAll(); }

    ~EnvironmentGuard() { EnvironmentRegistry::instance().tearDownAll(); }

    EnvironmentGuard(const EnvironmentGuard&) = delete;
    EnvironmentGuard& operator=(const EnvironmentGuard&) = delete;
};

/**
 * @brief Temporary environment for a scope
 */
class ScopedEnvironment {
public:
    template <typename Func>
    explicit ScopedEnvironment(Func setup) {
        setup();
    }

    template <typename SetupFunc, typename TeardownFunc>
    ScopedEnvironment(SetupFunc setup, TeardownFunc teardown)
        : teardown_(std::move(teardown)) {
        setup();
    }

    ~ScopedEnvironment() {
        if (teardown_) {
            try {
                teardown_();
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

private:
    std::function<void()> teardown_;
};

/**
 * @brief Environment variable setter for tests
 */
class EnvironmentVariable {
public:
    /**
     * @brief Set an environment variable for the duration of this object's
     * lifetime
     * @param name Variable name
     * @param value Variable value
     */
    EnvironmentVariable(const std::string& name, const std::string& value)
        : name_(name) {
        // Save old value if exists
        const char* oldValue = std::getenv(name.c_str());
        if (oldValue) {
            hadOldValue_ = true;
            oldValue_ = oldValue;
        }

        // Set new value
#ifdef _WIN32
        _putenv_s(name_.c_str(), value.c_str());
#else
        setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    ~EnvironmentVariable() {
        // Restore old value or unset
        if (hadOldValue_) {
#ifdef _WIN32
            _putenv_s(name_.c_str(), oldValue_.c_str());
#else
            setenv(name_.c_str(), oldValue_.c_str(), 1);
#endif
        } else {
#ifdef _WIN32
            _putenv_s(name_.c_str(), "");
#else
            unsetenv(name_.c_str());
#endif
        }
    }

    EnvironmentVariable(const EnvironmentVariable&) = delete;
    EnvironmentVariable& operator=(const EnvironmentVariable&) = delete;

private:
    std::string name_;
    std::string oldValue_;
    bool hadOldValue_{false};
};

/**
 * @brief Test configuration holder
 */
class TestConfiguration {
public:
    static TestConfiguration& instance() {
        static TestConfiguration config;
        return config;
    }

    /**
     * @brief Set a configuration value
     */
    void set(const std::string& key, const std::string& value) {
        std::lock_guard lock(mutex_);
        config_[key] = value;
    }

    /**
     * @brief Get a configuration value
     */
    [[nodiscard]] std::string get(const std::string& key,
                                  const std::string& defaultValue = "") const {
        std::lock_guard lock(mutex_);
        auto it = config_.find(key);
        return it != config_.end() ? it->second : defaultValue;
    }

    /**
     * @brief Check if a configuration key exists
     */
    [[nodiscard]] bool has(const std::string& key) const {
        std::lock_guard lock(mutex_);
        return config_.find(key) != config_.end();
    }

    /**
     * @brief Get as integer
     */
    [[nodiscard]] int getInt(const std::string& key, int defaultValue = 0) const {
        std::string value = get(key);
        if (value.empty()) return defaultValue;
        try {
            return std::stoi(value);
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief Get as boolean
     */
    [[nodiscard]] bool getBool(const std::string& key,
                               bool defaultValue = false) const {
        std::string value = get(key);
        if (value.empty()) return defaultValue;
        return value == "true" || value == "1" || value == "yes";
    }

    /**
     * @brief Clear all configuration
     */
    void clear() {
        std::lock_guard lock(mutex_);
        config_.clear();
    }

private:
    TestConfiguration() = default;

    mutable std::mutex mutex_;
    std::map<std::string, std::string> config_;
};

/**
 * @brief Test resource pool for expensive shared resources
 */
template <typename Resource>
class ResourcePool {
public:
    using CreateFunc = std::function<std::unique_ptr<Resource>()>;
    using ResetFunc = std::function<void(Resource&)>;

    ResourcePool(CreateFunc create, ResetFunc reset = nullptr,
                 size_t maxSize = 10)
        : create_(std::move(create)),
          reset_(std::move(reset)),
          maxSize_(maxSize) {}

    /**
     * @brief Acquire a resource from the pool
     */
    std::unique_ptr<Resource, std::function<void(Resource*)>> acquire() {
        std::lock_guard lock(mutex_);

        std::unique_ptr<Resource> resource;
        if (!available_.empty()) {
            resource = std::move(available_.back());
            available_.pop_back();
        } else {
            resource = create_();
        }

        auto* rawPtr = resource.release();
        return {rawPtr, [this](Resource* ptr) { this->release(ptr); }};
    }

    /**
     * @brief Get pool statistics
     */
    [[nodiscard]] size_t availableCount() const {
        std::lock_guard lock(mutex_);
        return available_.size();
    }

private:
    void release(Resource* ptr) {
        if (!ptr) return;

        std::lock_guard lock(mutex_);
        if (available_.size() < maxSize_) {
            if (reset_) {
                reset_(*ptr);
            }
            available_.push_back(std::unique_ptr<Resource>(ptr));
        } else {
            delete ptr;
        }
    }

    mutable std::mutex mutex_;
    CreateFunc create_;
    ResetFunc reset_;
    size_t maxSize_;
    std::vector<std::unique_ptr<Resource>> available_;
};

/**
 * @brief Test event listener interface
 */
class TestEventListener {
public:
    virtual ~TestEventListener() = default;

    virtual void onTestProgramStart() {}
    virtual void onTestProgramEnd() {}
    virtual void onEnvironmentSetUp() {}
    virtual void onEnvironmentTearDown() {}
    virtual void onTestSuiteStart(const std::string& suiteName) {
        (void)suiteName;
    }
    virtual void onTestSuiteEnd(const std::string& suiteName) {
        (void)suiteName;
    }
    virtual void onTestStart(const TestCase& testCase) { (void)testCase; }
    virtual void onTestEnd(const TestResult& result) { (void)result; }
    virtual void onTestPartResult(bool success, const std::string& message) {
        (void)success;
        (void)message;
    }
};

/**
 * @brief Event listener registry
 */
class EventListenerRegistry {
public:
    static EventListenerRegistry& instance() {
        static EventListenerRegistry registry;
        return registry;
    }

    void addListener(std::unique_ptr<TestEventListener> listener) {
        std::lock_guard lock(mutex_);
        listeners_.push_back(std::move(listener));
    }

    void notifyTestProgramStart() {
        std::lock_guard lock(mutex_);
        for (auto& l : listeners_) {
            l->onTestProgramStart();
        }
    }

    void notifyTestProgramEnd() {
        std::lock_guard lock(mutex_);
        for (auto& l : listeners_) {
            l->onTestProgramEnd();
        }
    }

    void notifyTestStart(const TestCase& testCase) {
        std::lock_guard lock(mutex_);
        for (auto& l : listeners_) {
            l->onTestStart(testCase);
        }
    }

    void notifyTestEnd(const TestResult& result) {
        std::lock_guard lock(mutex_);
        for (auto& l : listeners_) {
            l->onTestEnd(result);
        }
    }

private:
    EventListenerRegistry() = default;

    std::mutex mutex_;
    std::vector<std::unique_ptr<TestEventListener>> listeners_;
};

}  // namespace atom::test

/**
 * @brief Define a test environment class
 */
#define TEST_ENVIRONMENT(class_name)                                          \
    class class_name : public atom::test::Environment

/**
 * @brief Register environment in main
 */
#define REGISTER_TEST_ENVIRONMENT(class_name)                                 \
    atom::test::AddGlobalTestEnvironment(new class_name())

#endif  // ATOM_TEST_UTILITIES_TEST_ENVIRONMENT_HPP
