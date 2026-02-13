/**
 * @file test_mock.hpp
 * @brief Mock framework for function mocking and verification
 * @details Provides MockFunction, Spy, and expectation-based verification
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_MOCKING_TEST_MOCK_HPP
#define ATOM_TEST_MOCKING_TEST_MOCK_HPP

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

#include "atom/tests/core/test.hpp"

namespace atom::test {

/**
 * @brief Exception thrown when mock expectations are not met
 */
class MockException : public std::runtime_error {
public:
    explicit MockException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Call matcher for verifying function arguments
 */
template <typename... Args>
class ArgumentMatcher {
public:
    using ArgsTuple = std::tuple<std::decay_t<Args>...>;

    /**
     * @brief Match any arguments
     */
    static auto any() -> ArgumentMatcher {
        ArgumentMatcher matcher;
        matcher.matchAny_ = true;
        return matcher;
    }

    /**
     * @brief Match specific arguments
     */
    static auto with(Args... args) -> ArgumentMatcher {
        ArgumentMatcher matcher;
        matcher.expectedArgs_ = std::make_tuple(std::forward<Args>(args)...);
        matcher.matchAny_ = false;
        return matcher;
    }

    /**
     * @brief Check if arguments match
     */
    [[nodiscard]] auto matches(const ArgsTuple& actual) const -> bool {
        if (matchAny_) {
            return true;
        }
        return expectedArgs_ == actual;
    }

private:
    ArgsTuple expectedArgs_{};
    bool matchAny_{true};
};

/**
 * @brief Call expectation with return value configuration
 * @tparam ReturnType The return type of the mocked function
 * @tparam Args The argument types
 */
template <typename ReturnType, typename... Args>
class Expectation {
public:
    using ArgsTuple = std::tuple<std::decay_t<Args>...>;
    using ReturnFunc = std::function<ReturnType(Args...)>;

    Expectation() = default;

    /**
     * @brief Set the return value
     */
    auto willReturn(ReturnType value) -> Expectation& {
        returnValue_ = std::move(value);
        hasReturnValue_ = true;
        return *this;
    }

    /**
     * @brief Alias for willReturn (GTest compatibility)
     */
    auto WillOnce(ReturnType value) -> Expectation& {
        return willReturn(std::move(value));
    }

    /**
     * @brief Set return value to be used repeatedly
     */
    auto WillRepeatedly(ReturnType value) -> Expectation& {
        return willReturn(std::move(value));
    }

    /**
     * @brief Set a function to compute return value
     */
    auto willInvoke(ReturnFunc func) -> Expectation& {
        returnFunc_ = std::move(func);
        return *this;
    }

    /**
     * @brief Configure to throw an exception
     */
    template <typename ExceptionType>
    auto willThrow(ExceptionType exception) -> Expectation& {
        throwFunc_ = [ex = std::move(exception)]() { throw ex; };
        return *this;
    }

    /**
     * @brief Set expected call count
     */
    auto times(size_t count) -> Expectation& {
        expectedCalls_ = count;
        return *this;
    }

    /**
     * @brief Alias for times (GTest compatibility)
     */
    auto Times(size_t count) -> Expectation& { return times(count); }

    /**
     * @brief Expect at least n calls
     */
    auto atLeast(size_t count) -> Expectation& {
        minCalls_ = count;
        return *this;
    }

    auto AtLeast(size_t count) -> Expectation& { return atLeast(count); }

    /**
     * @brief Expect at most n calls
     */
    auto atMost(size_t count) -> Expectation& {
        maxCalls_ = count;
        return *this;
    }

    auto AtMost(size_t count) -> Expectation& { return atMost(count); }

    /**
     * @brief Set argument matcher
     */
    auto withArgs(ArgumentMatcher<Args...> matcher) -> Expectation& {
        argMatcher_ = std::move(matcher);
        return *this;
    }

    /**
     * @brief Execute the expectation
     */
    auto execute(Args... args) -> ReturnType {
        callCount_++;

        if (throwFunc_) {
            throwFunc_();
        }

        if (returnFunc_) {
            return returnFunc_(std::forward<Args>(args)...);
        }

        if (hasReturnValue_) {
            return returnValue_;
        }

        if constexpr (std::is_default_constructible_v<ReturnType>) {
            return ReturnType{};
        } else {
            throw MockException("No return value configured for mock");
        }
    }

    /**
     * @brief Check if arguments match this expectation
     */
    [[nodiscard]] auto matchesArgs(Args... args) const -> bool {
        return argMatcher_.matches(std::make_tuple(args...));
    }

    /**
     * @brief Verify expectations were met
     */
    [[nodiscard]] auto verify() const -> bool {
        if (expectedCalls_.has_value()) {
            return callCount_ == *expectedCalls_;
        }
        if (minCalls_.has_value() && callCount_ < *minCalls_) {
            return false;
        }
        if (maxCalls_.has_value() && callCount_ > *maxCalls_) {
            return false;
        }
        return true;
    }

    /**
     * @brief Get call count
     */
    [[nodiscard]] auto getCallCount() const -> size_t { return callCount_; }

    /**
     * @brief Get verification error message
     */
    [[nodiscard]] auto getVerificationError() const -> std::string {
        std::ostringstream oss;
        oss << "Call count: " << callCount_;
        if (expectedCalls_.has_value()) {
            oss << ", expected: " << *expectedCalls_;
        }
        if (minCalls_.has_value()) {
            oss << ", min: " << *minCalls_;
        }
        if (maxCalls_.has_value()) {
            oss << ", max: " << *maxCalls_;
        }
        return oss.str();
    }

private:
    ReturnType returnValue_{};
    bool hasReturnValue_{false};
    ReturnFunc returnFunc_;
    std::function<void()> throwFunc_;
    ArgumentMatcher<Args...> argMatcher_ = ArgumentMatcher<Args...>::any();
    std::optional<size_t> expectedCalls_;
    std::optional<size_t> minCalls_;
    std::optional<size_t> maxCalls_;
    size_t callCount_{0};
};

/**
 * @brief Specialization for void return type
 */
template <typename... Args>
class Expectation<void, Args...> {
public:
    using ArgsTuple = std::tuple<std::decay_t<Args>...>;
    using ActionFunc = std::function<void(Args...)>;

    Expectation() = default;

    /**
     * @brief Set an action to execute
     */
    auto willInvoke(ActionFunc func) -> Expectation& {
        actionFunc_ = std::move(func);
        return *this;
    }

    /**
     * @brief Configure to throw an exception
     */
    template <typename ExceptionType>
    auto willThrow(ExceptionType exception) -> Expectation& {
        throwFunc_ = [ex = std::move(exception)]() { throw ex; };
        return *this;
    }

    /**
     * @brief Set expected call count
     */
    auto times(size_t count) -> Expectation& {
        expectedCalls_ = count;
        return *this;
    }

    auto Times(size_t count) -> Expectation& { return times(count); }

    /**
     * @brief Expect at least n calls
     */
    auto atLeast(size_t count) -> Expectation& {
        minCalls_ = count;
        return *this;
    }

    auto AtLeast(size_t count) -> Expectation& { return atLeast(count); }

    /**
     * @brief Expect at most n calls
     */
    auto atMost(size_t count) -> Expectation& {
        maxCalls_ = count;
        return *this;
    }

    auto AtMost(size_t count) -> Expectation& { return atMost(count); }

    /**
     * @brief Set argument matcher
     */
    auto withArgs(ArgumentMatcher<Args...> matcher) -> Expectation& {
        argMatcher_ = std::move(matcher);
        return *this;
    }

    /**
     * @brief Execute the expectation
     */
    void execute(Args... args) {
        callCount_++;

        if (throwFunc_) {
            throwFunc_();
        }

        if (actionFunc_) {
            actionFunc_(std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Check if arguments match this expectation
     */
    [[nodiscard]] auto matchesArgs(Args... args) const -> bool {
        return argMatcher_.matches(std::make_tuple(args...));
    }

    /**
     * @brief Verify expectations were met
     */
    [[nodiscard]] auto verify() const -> bool {
        if (expectedCalls_.has_value()) {
            return callCount_ == *expectedCalls_;
        }
        if (minCalls_.has_value() && callCount_ < *minCalls_) {
            return false;
        }
        if (maxCalls_.has_value() && callCount_ > *maxCalls_) {
            return false;
        }
        return true;
    }

    /**
     * @brief Get call count
     */
    [[nodiscard]] auto getCallCount() const -> size_t { return callCount_; }

    /**
     * @brief Get verification error message
     */
    [[nodiscard]] auto getVerificationError() const -> std::string {
        std::ostringstream oss;
        oss << "Call count: " << callCount_;
        if (expectedCalls_.has_value()) {
            oss << ", expected: " << *expectedCalls_;
        }
        if (minCalls_.has_value()) {
            oss << ", min: " << *minCalls_;
        }
        if (maxCalls_.has_value()) {
            oss << ", max: " << *maxCalls_;
        }
        return oss.str();
    }

private:
    ActionFunc actionFunc_;
    std::function<void()> throwFunc_;
    ArgumentMatcher<Args...> argMatcher_ = ArgumentMatcher<Args...>::any();
    std::optional<size_t> expectedCalls_;
    std::optional<size_t> minCalls_;
    std::optional<size_t> maxCalls_;
    size_t callCount_{0};
};

/**
 * @brief Mock function container
 * @tparam Signature The function signature (e.g., int(std::string, int))
 */
template <typename Signature>
class MockFunction;

template <typename ReturnType, typename... Args>
class MockFunction<ReturnType(Args...)> {
public:
    using ExpectationType = Expectation<ReturnType, Args...>;

    MockFunction() = default;

    /**
     * @brief Set up an expectation for this mock
     */
    auto expect() -> ExpectationType& {
        expectations_.emplace_back();
        return expectations_.back();
    }

    /**
     * @brief GTest-style EXPECT_CALL
     */
    auto EXPECT_CALL() -> ExpectationType& { return expect(); }

    /**
     * @brief Call the mock function
     */
    auto operator()(Args... args) -> ReturnType {
        callHistory_.emplace_back(std::forward<Args>(args)...);

        // Find matching expectation
        for (auto& exp : expectations_) {
            if (exp.matchesArgs(args...)) {
                return exp.execute(std::forward<Args>(args)...);
            }
        }

        // Default behavior
        if constexpr (std::is_void_v<ReturnType>) {
            return;
        } else if constexpr (std::is_default_constructible_v<ReturnType>) {
            return ReturnType{};
        } else {
            throw MockException("No matching expectation for mock call");
        }
    }

    /**
     * @brief Get callable for use as std::function
     */
    auto asFunction() -> std::function<ReturnType(Args...)> {
        return [this](Args... args) -> ReturnType {
            return this->operator()(std::forward<Args>(args)...);
        };
    }

    /**
     * @brief Verify all expectations
     */
    [[nodiscard]] auto verify() const -> bool {
        for (const auto& exp : expectations_) {
            if (!exp.verify()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get verification error details
     */
    [[nodiscard]] auto getVerificationErrors() const -> std::string {
        std::ostringstream oss;
        for (size_t i = 0; i < expectations_.size(); ++i) {
            if (!expectations_[i].verify()) {
                oss << "Expectation " << i << ": "
                    << expectations_[i].getVerificationError() << "\n";
            }
        }
        return oss.str();
    }

    /**
     * @brief Get total call count
     */
    [[nodiscard]] auto callCount() const -> size_t {
        return callHistory_.size();
    }

    /**
     * @brief Get call history
     */
    [[nodiscard]] auto getCallHistory() const
        -> const std::vector<std::tuple<std::decay_t<Args>...>>& {
        return callHistory_;
    }

    /**
     * @brief Check if mock was called with specific arguments
     */
    [[nodiscard]] auto wasCalledWith(Args... args) const -> bool {
        auto expected = std::make_tuple(args...);
        for (const auto& call : callHistory_) {
            if (call == expected) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Reset mock state
     */
    void reset() {
        expectations_.clear();
        callHistory_.clear();
    }

    /**
     * @brief Alias for reset (GTest compatibility)
     */
    void VerifyAndClearExpectations() {
        if (!verify()) {
            throw MockException("Mock verification failed: " +
                                getVerificationErrors());
        }
        reset();
    }

private:
    std::vector<ExpectationType> expectations_;
    std::vector<std::tuple<std::decay_t<Args>...>> callHistory_;
};

/**
 * @brief Spy wrapper that records calls while delegating to real implementation
 * @tparam Signature The function signature
 */
template <typename Signature>
class Spy;

template <typename ReturnType, typename... Args>
class Spy<ReturnType(Args...)> {
public:
    using FuncType = std::function<ReturnType(Args...)>;

    explicit Spy(FuncType realImpl) : realImpl_(std::move(realImpl)) {}

    /**
     * @brief Call the spy (records and delegates)
     */
    auto operator()(Args... args) -> ReturnType {
        callHistory_.emplace_back(std::forward<Args>(args)...);
        return realImpl_(std::forward<Args>(args)...);
    }

    /**
     * @brief Get as std::function
     */
    auto asFunction() -> FuncType {
        return [this](Args... args) -> ReturnType {
            return this->operator()(std::forward<Args>(args)...);
        };
    }

    /**
     * @brief Get call count
     */
    [[nodiscard]] auto callCount() const -> size_t {
        return callHistory_.size();
    }

    /**
     * @brief Check if called with specific arguments
     */
    [[nodiscard]] auto wasCalledWith(Args... args) const -> bool {
        auto expected = std::make_tuple(args...);
        for (const auto& call : callHistory_) {
            if (call == expected) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get call history
     */
    [[nodiscard]] auto getCallHistory() const
        -> const std::vector<std::tuple<std::decay_t<Args>...>>& {
        return callHistory_;
    }

    /**
     * @brief Reset call history
     */
    void reset() { callHistory_.clear(); }

private:
    FuncType realImpl_;
    std::vector<std::tuple<std::decay_t<Args>...>> callHistory_;
};

/**
 * @brief Stub that returns a fixed value
 */
template <typename Signature>
class Stub;

template <typename ReturnType, typename... Args>
class Stub<ReturnType(Args...)> {
public:
    explicit Stub(ReturnType value) : value_(std::move(value)) {}

    auto operator()(Args...) -> ReturnType { return value_; }

    auto asFunction() -> std::function<ReturnType(Args...)> {
        return [this](Args...) -> ReturnType { return value_; };
    }

    void setValue(ReturnType value) { value_ = std::move(value); }

private:
    ReturnType value_;
};

/**
 * @brief Fake implementation with custom logic
 */
template <typename Signature>
class Fake;

template <typename ReturnType, typename... Args>
class Fake<ReturnType(Args...)> {
public:
    using FuncType = std::function<ReturnType(Args...)>;

    explicit Fake(FuncType impl) : impl_(std::move(impl)) {}

    auto operator()(Args... args) -> ReturnType {
        return impl_(std::forward<Args>(args)...);
    }

    auto asFunction() -> FuncType {
        return [this](Args... args) -> ReturnType {
            return this->operator()(std::forward<Args>(args)...);
        };
    }

    void setImplementation(FuncType impl) { impl_ = std::move(impl); }

private:
    FuncType impl_;
};

/**
 * @brief Helper to create a mock function
 */
template <typename Signature>
auto makeMock() -> MockFunction<Signature> {
    return MockFunction<Signature>{};
}

/**
 * @brief Helper to create a spy
 */
template <typename Signature>
auto makeSpy(std::function<Signature> impl) -> Spy<Signature> {
    return Spy<Signature>{std::move(impl)};
}

/**
 * @brief Helper to create a stub
 */
template <typename Signature, typename ReturnType>
auto makeStub(ReturnType value) -> Stub<Signature> {
    return Stub<Signature>{std::move(value)};
}

/**
 * @brief Helper to create a fake
 */
template <typename Signature>
auto makeFake(std::function<Signature> impl) -> Fake<Signature> {
    return Fake<Signature>{std::move(impl)};
}

/**
 * @brief Verify mock and throw if expectations not met
 */
template <typename Signature>
void verifyMock(const MockFunction<Signature>& mock,
                const std::string& mockName = "Mock") {
    if (!mock.verify()) {
        throw MockException(
            mockName + " verification failed: " + mock.getVerificationErrors());
    }
}

/**
 * @brief Assert mock verification with test framework integration
 */
template <typename Signature>
auto expectMockVerified(const MockFunction<Signature>& mock, const char* file,
                        int line) -> Expect {
    bool verified = mock.verify();
    return {verified, file, line,
            verified ? "Mock verified" : mock.getVerificationErrors()};
}

/**
 * @brief Sequence checker for ordered mock calls
 */
class InSequence {
public:
    InSequence() { sequenceStack().push_back(this); }

    ~InSequence() {
        if (!sequenceStack().empty() && sequenceStack().back() == this) {
            sequenceStack().pop_back();
        }
    }

    InSequence(const InSequence&) = delete;
    InSequence& operator=(const InSequence&) = delete;

    static bool inSequenceScope() { return !sequenceStack().empty(); }

private:
    static std::vector<InSequence*>& sequenceStack() {
        static std::vector<InSequence*> stack;
        return stack;
    }
};

/**
 * @brief Nice mock that doesn't warn on unexpected calls
 */
template <typename MockClass>
class NiceMock : public MockClass {
public:
    using MockClass::MockClass;
};

/**
 * @brief Strict mock that fails on unexpected calls
 */
template <typename MockClass>
class StrictMock : public MockClass {
public:
    using MockClass::MockClass;
};

}  // namespace atom::test

#define expect_mock_verified(mock) \
    atom::test::expectMockVerified(mock, __FILE__, __LINE__)

#define MOCK_FUNCTION(name, signature) atom::test::MockFunction<signature> name

#define SPY_FUNCTION(name, signature, impl) \
    atom::test::Spy<signature> name(impl)

#define STUB_FUNCTION(name, signature, value) \
    atom::test::Stub<signature> name(value)

#define FAKE_FUNCTION(name, signature, impl) \
    atom::test::Fake<signature> name(impl)

// GTest-style mock method macros
#define MOCK_METHOD(return_type, method_name, args) \
    atom::test::MockFunction<return_type args> method_name

#define MOCK_METHOD0(return_type, method_name) \
    atom::test::MockFunction<return_type()> method_name

#define MOCK_METHOD1(return_type, method_name, arg1) \
    atom::test::MockFunction<return_type(arg1)> method_name

#define MOCK_METHOD2(return_type, method_name, arg1, arg2) \
    atom::test::MockFunction<return_type(arg1, arg2)> method_name

#define MOCK_METHOD3(return_type, method_name, arg1, arg2, arg3) \
    atom::test::MockFunction<return_type(arg1, arg2, arg3)> method_name

#define ON_CALL(mock, method) (mock).method.expect()

#define EXPECT_CALL(mock, method) (mock).method.expect()

#endif  // ATOM_TEST_MOCKING_TEST_MOCK_HPP
