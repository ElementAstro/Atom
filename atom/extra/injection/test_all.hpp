#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"
#include "container.hpp"
#include "inject.hpp"

using namespace atom::extra;
using ::testing::Eq;
using ::testing::SizeIs;

// Test interfaces and implementations
struct ILogger {
    virtual ~ILogger() = default;
    virtual std::string log(const std::string& message) = 0;
};

struct ConsoleLogger : public ILogger {
    std::string log(const std::string& message) override {
        return "ConsoleLogger: " + message;
    }
};

struct FileLogger : public ILogger {
    std::string log(const std::string& message) override {
        return "FileLogger: " + message;
    }
};

struct IDatabase {
    virtual ~IDatabase() = default;
    virtual std::string query(const std::string& sql) = 0;
};

struct SQLiteDatabase : public IDatabase {
    std::string query(const std::string& sql) override {
        return "SQLite: " + sql;
    }
};

struct PostgresDatabase : public IDatabase {
    std::string query(const std::string& sql) override {
        return "PostgreSQL: " + sql;
    }
};

// Simple class with no dependencies
struct SimpleService {
    int getValue() const { return 42; }
};

// Class with a single dependency
struct LoggerService {
    explicit LoggerService(std::shared_ptr<ILogger> logger) : logger_(logger) {}
    std::string log(const std::string& message) {
        return logger_->log(message);
    }

private:
    std::shared_ptr<ILogger> logger_;
};

// Class with multiple dependencies
struct DataService {
    DataService(std::shared_ptr<ILogger> logger, std::shared_ptr<IDatabase> db)
        : logger_(logger), db_(db) {}

    std::string executeQuery(const std::string& sql) {
        logger_->log("Executing query: " + sql);
        return db_->query(sql);
    }

private:
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IDatabase> db_;
};

// Define symbols for our container
using LoggerSymbol = Symbol<std::shared_ptr<ILogger>>;
using DatabaseSymbol = Symbol<std::shared_ptr<IDatabase>>;
using SimpleServiceSymbol = Symbol<SimpleService>;
using LoggerServiceSymbol = Symbol<LoggerService>;
using DataServiceSymbol = Symbol<DataService>;

// Define specialized injection metadata at namespace scope
namespace atom::extra {
template <>
struct InjectableA<LoggerService> {
    static auto resolve(
        const Context<LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol,
                      LoggerServiceSymbol, DataServiceSymbol>& context) {
        return std::make_tuple(context.container.template get<LoggerSymbol>());
    }
};

template <>
struct InjectableA<DataService> {
    static auto resolve(
        const Context<LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol,
                      LoggerServiceSymbol, DataServiceSymbol>& context) {
        return std::make_tuple(
            context.container.template get<LoggerSymbol>(),
            context.container.template get<DatabaseSymbol>());
    }
};
}  // namespace atom::extra

// Test classes for complex dependency chains
struct ServiceA {
    explicit ServiceA(std::shared_ptr<ILogger> logger) : logger_(logger) {}
    std::shared_ptr<ILogger> logger_;
};

struct ServiceB {
    explicit ServiceB(ServiceA serviceA) : serviceA_(serviceA) {}
    ServiceA serviceA_;
};

using ServiceASymbol = Symbol<ServiceA>;
using ServiceBSymbol = Symbol<ServiceB>;

namespace atom::extra {
template <>
struct InjectableA<ServiceA> {
    static auto resolve(
        const Context<LoggerSymbol, ServiceASymbol, ServiceBSymbol>& context) {
        return std::make_tuple(context.container.template get<LoggerSymbol>());
    }
};

template <>
struct InjectableA<ServiceB> {
    static auto resolve(
        const Context<LoggerSymbol, ServiceASymbol, ServiceBSymbol>& context) {
        return std::make_tuple(
            context.container.template get<ServiceASymbol>());
    }
};
}  // namespace atom::extra

// Create a class with a static test method to run the tests
class InjectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create and set up the container for each test
        container = std::make_unique<
            Container<LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol,
                      LoggerServiceSymbol, DataServiceSymbol>>();
    }

    void TearDown() override { container.reset(); }

    std::unique_ptr<Container<LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol,
                              LoggerServiceSymbol, DataServiceSymbol>>
        container;
};

// Test basic binding and resolution
TEST_F(InjectionTest, BasicBinding) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    auto logger = container->get<LoggerSymbol>();
    ASSERT_TRUE(logger != nullptr);
    EXPECT_EQ(logger->log("Hello"), "ConsoleLogger: Hello");
}

// Test singleton scope
TEST_F(InjectionTest, SingletonScope) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>().inSingletonScope();

    auto logger1 = container->get<LoggerSymbol>();
    auto logger2 = container->get<LoggerSymbol>();

    ASSERT_TRUE(logger1 != nullptr);
    ASSERT_TRUE(logger2 != nullptr);
    EXPECT_EQ(logger1, logger2);
}

// Test transient scope
TEST_F(InjectionTest, TransientScope) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>().inTransientScope();

    auto logger1 = container->get<LoggerSymbol>();
    auto logger2 = container->get<LoggerSymbol>();

    ASSERT_TRUE(logger1 != nullptr);
    ASSERT_TRUE(logger2 != nullptr);
    EXPECT_NE(logger1, logger2);
}

// Test constant value binding
TEST_F(InjectionTest, ConstantValueBinding) {
    auto logger = std::make_shared<ConsoleLogger>();
    container->bind<LoggerSymbol>().toConstantValue(logger);

    auto logger1 = container->get<LoggerSymbol>();
    auto logger2 = container->get<LoggerSymbol>();

    ASSERT_TRUE(logger1 != nullptr);
    ASSERT_TRUE(logger2 != nullptr);
    EXPECT_EQ(logger1, logger);
    EXPECT_EQ(logger1, logger2);
}

// Test dynamic value binding
TEST_F(InjectionTest, DynamicValueBinding) {
    int counter = 0;

    container->bind<LoggerSymbol>().toDynamicValue(
        [&counter](
            const Context<LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol,
                          LoggerServiceSymbol, DataServiceSymbol>&) {
            counter++;
            return std::make_shared<ConsoleLogger>();
        });

    auto logger1 = container->get<LoggerSymbol>();
    auto logger2 = container->get<LoggerSymbol>();

    ASSERT_TRUE(logger1 != nullptr);
    ASSERT_TRUE(logger2 != nullptr);
    EXPECT_EQ(counter, 2);
    EXPECT_NE(logger1, logger2);
}

// Test dependency injection with a single dependency
TEST_F(InjectionTest, SingleDependencyInjection) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    container->bind<LoggerServiceSymbol>().to<LoggerService>();

    auto service = container->get<LoggerServiceSymbol>();
    EXPECT_EQ(service.log("Test"), "ConsoleLogger: Test");
}

// Test dependency injection with multiple dependencies
TEST_F(InjectionTest, MultipleDependenciesInjection) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    container->bind<DatabaseSymbol>().to<SQLiteDatabase>();
    container->bind<DataServiceSymbol>().to<DataService>();

    auto service = container->get<DataServiceSymbol>();
    EXPECT_EQ(service.executeQuery("SELECT * FROM users"),
              "SQLite: SELECT * FROM users");
}

// Test unbinding
TEST_F(InjectionTest, Unbind) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    EXPECT_TRUE(container->hasBinding<LoggerSymbol>());

    container->unbind<LoggerSymbol>();
    EXPECT_FALSE(container->hasBinding<LoggerSymbol>());

    EXPECT_THROW(container->get<LoggerSymbol>(),
                 exceptions::ResolutionException);
}

// Test getting all bindings
TEST_F(InjectionTest, GetAll) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>();

    auto loggers = container->getAll<LoggerSymbol>();

    EXPECT_THAT(loggers, SizeIs(1));
    EXPECT_EQ(loggers[0]->log("Test"), "ConsoleLogger: Test");
}

// Test child containers and inheritance
TEST_F(InjectionTest, ChildContainers) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>();

    auto childContainer = container->createChildContainer();

    auto logger = childContainer->get<LoggerSymbol>();
    EXPECT_EQ(logger->log("Test"), "ConsoleLogger: Test");

    childContainer->bind<LoggerSymbol>().to<FileLogger>();
    logger = childContainer->get<LoggerSymbol>();
    EXPECT_EQ(logger->log("Test"), "FileLogger: Test");

    logger = container->get<LoggerSymbol>();
    EXPECT_EQ(logger->log("Test"), "ConsoleLogger: Test");
}

// Test lazy binding
TEST_F(InjectionTest, LazyBinding) {
    int instantiationCount = 0;

    container->bind<SimpleServiceSymbol>().toDynamicValue(
        [&instantiationCount](
            const Context<LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol,
                          LoggerServiceSymbol, DataServiceSymbol>&) {
            instantiationCount++;
            return SimpleService();
        });

    auto lazyService = Lazy<SimpleService>(
        [&]() { return container->get<SimpleServiceSymbol>(); });

    EXPECT_EQ(instantiationCount, 0);

    auto value = lazyService.get();
    EXPECT_EQ(instantiationCount, 1);
    EXPECT_EQ(value.getValue(), 42);

    value = lazyService.get();
    EXPECT_EQ(instantiationCount, 2);
}

// Test multi binding
TEST_F(InjectionTest, MultiBinding) {
    using LoggerMultiSymbol = Symbol<Multi<std::shared_ptr<ILogger>>::value>;

    auto multiContainer = std::make_unique<Container<LoggerMultiSymbol>>();

    multiContainer->bind<LoggerMultiSymbol>().toDynamicValue(
        [](const Context<LoggerMultiSymbol>&) {
            std::vector<std::shared_ptr<ILogger>> loggers;
            loggers.push_back(std::make_shared<ConsoleLogger>());
            loggers.push_back(std::make_shared<FileLogger>());
            return loggers;
        });

    auto loggers = multiContainer->get<LoggerMultiSymbol>();

    ASSERT_THAT(loggers, SizeIs(2));
    EXPECT_EQ(loggers[0]->log("Test"), "ConsoleLogger: Test");
    EXPECT_EQ(loggers[1]->log("Test"), "FileLogger: Test");
}

// Test error handling
TEST_F(InjectionTest, ErrorHandling) {
    EXPECT_THROW(container->get<LoggerSymbol>(),
                 exceptions::ResolutionException);

    container->bind<LoggerSymbol>();
    EXPECT_THROW(container->get<LoggerSymbol>(),
                 exceptions::ResolutionException);
}

// Test the Symbolic concept
TEST_F(InjectionTest, SymbolicConcept) {
    static_assert(Symbolic<LoggerSymbol>,
                  "Symbol should satisfy Symbolic concept");

    struct NonSymbolic {};
    static_assert(!Symbolic<NonSymbolic>,
                  "Non-symbolic types should not satisfy Symbolic concept");
}

// Test request scope binding
TEST_F(InjectionTest, RequestScope) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>().inRequestScope();

    auto logger = container->get<LoggerSymbol>();
    ASSERT_TRUE(logger != nullptr);
    EXPECT_EQ(logger->log("Test"), "ConsoleLogger: Test");
}

// Test complex dependency chains
TEST_F(InjectionTest, ComplexDependencyChains) {
    auto complexContainer = std::make_unique<
        Container<LoggerSymbol, ServiceASymbol, ServiceBSymbol>>();

    complexContainer->bind<LoggerSymbol>().to<ConsoleLogger>();
    complexContainer->bind<ServiceASymbol>().to<ServiceA>();
    complexContainer->bind<ServiceBSymbol>().to<ServiceB>();

    auto serviceB = complexContainer->get<ServiceBSymbol>();
    EXPECT_EQ(serviceB.serviceA_.logger_->log("Test"), "ConsoleLogger: Test");
}
