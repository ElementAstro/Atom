// filepath: /home/max/Atom-1/atom/extra/injection/test_all.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "all.hpp"

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

template<typename... Dependencies>
struct Inject : Injectable<std::tuple<Dependencies...>> {};

// 修正注入声明
template<typename T>
struct InjectableA;

template<>
struct InjectableA<LoggerService> {
    using dependencies = std::tuple<LoggerSymbol>;
};

template<>
struct InjectableA<DataService> {
    using dependencies = std::tuple<LoggerSymbol, DatabaseSymbol>;
};

// Create a class with a static test method to run the tests
class InjectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create and set up the container for each test
        container = std::make_unique<Container<
            LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol, 
            LoggerServiceSymbol, DataServiceSymbol>>();
    }

    void TearDown() override {
        container.reset();
    }

    std::unique_ptr<Container<
        LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol, 
        LoggerServiceSymbol, DataServiceSymbol>> container;
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
    // Bind a singleton
    container->bind<LoggerSymbol>().to<ConsoleLogger>().inSingletonScope();

    // Resolve the binding multiple times and check it's the same instance
    auto logger1 = container->get<LoggerSymbol>();
    auto logger2 = container->get<LoggerSymbol>();
    
    ASSERT_TRUE(logger1 != nullptr);
    ASSERT_TRUE(logger2 != nullptr);
    
    // The same instance should be returned (reference equality)
    EXPECT_EQ(logger1, logger2);
}

// Test transient scope
TEST_F(InjectionTest, TransientScope) {
    // Bind a transient
    container->bind<LoggerSymbol>().to<ConsoleLogger>().inTransientScope();

    // Resolve the binding multiple times and check they are different instances
    auto logger1 = container->get<LoggerSymbol>();
    auto logger2 = container->get<LoggerSymbol>();
    
    ASSERT_TRUE(logger1 != nullptr);
    ASSERT_TRUE(logger2 != nullptr);
    
    // Different instances should be returned (reference inequality)
    EXPECT_NE(logger1, logger2);
}

// Test constant value binding
TEST_F(InjectionTest, ConstantValueBinding) {
    // Create a logger
    auto logger = std::make_shared<ConsoleLogger>();
    
    // Bind to the constant value
    container->bind<LoggerSymbol>().toConstantValue(logger);

    // Resolve the binding multiple times and check it's the same instance
    auto logger1 = container->get<LoggerSymbol>();
    auto logger2 = container->get<LoggerSymbol>();
    
    ASSERT_TRUE(logger1 != nullptr);
    ASSERT_TRUE(logger2 != nullptr);
    
    // The same instance should be returned (reference equality)
    EXPECT_EQ(logger1, logger);
    EXPECT_EQ(logger1, logger2);
}

// Test dynamic value binding
TEST_F(InjectionTest, DynamicValueBinding) {
    // Counter to ensure the factory is called each time
    int counter = 0;
    
    // Bind to a dynamic value factory
    container->bind<LoggerSymbol>().toDynamicValue(
        [&counter](const Context<LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol, 
                          LoggerServiceSymbol, DataServiceSymbol>&) {
            counter++;
            return std::make_shared<ConsoleLogger>();
        }
    );

    // Resolve the binding multiple times
    auto logger1 = container->get<LoggerSymbol>();
    auto logger2 = container->get<LoggerSymbol>();
    
    ASSERT_TRUE(logger1 != nullptr);
    ASSERT_TRUE(logger2 != nullptr);
    
    // Each resolution should call the factory
    EXPECT_EQ(counter, 2);
    
    // Different instances should be returned
    EXPECT_NE(logger1, logger2);
}

// Test dependency injection with a single dependency
TEST_F(InjectionTest, SingleDependencyInjection) {
    // Bind the logger
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    
    // Bind the service that depends on the logger
    container->bind<LoggerServiceSymbol>().to<LoggerService>();
    
    // Resolve the service
    auto service = container->get<LoggerServiceSymbol>();
    
    // Check that the logger was properly injected
    EXPECT_EQ(service.log("Test"), "ConsoleLogger: Test");
}

// Test dependency injection with multiple dependencies
TEST_F(InjectionTest, MultipleDependenciesInjection) {
    // Bind the dependencies
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    container->bind<DatabaseSymbol>().to<SQLiteDatabase>();
    
    // Bind the service with multiple dependencies
    container->bind<DataServiceSymbol>().to<DataService>();
    
    // Resolve the service
    auto service = container->get<DataServiceSymbol>();
    
    // Check that dependencies were properly injected
    EXPECT_EQ(service.executeQuery("SELECT * FROM users"), "SQLite: SELECT * FROM users");
}

// Test tagged bindings
TEST_F(InjectionTest, TaggedBindings) {
    // Create tags
    Tag consoleTag("console");
    Tag fileTag("file");
    
    // Bind multiple implementations with different tags
    container->bind<LoggerSymbol>().to<ConsoleLogger>().when(consoleTag);
    container->bind<LoggerSymbol>().to<FileLogger>().when(fileTag);
    
    // Resolve with specific tags
    auto consoleLogger = container->get<LoggerSymbol>(consoleTag);
    auto fileLogger = container->get<LoggerSymbol>(fileTag);
    
    // Check that correct implementations were resolved
    EXPECT_EQ(consoleLogger->log("Test"), "ConsoleLogger: Test");
    EXPECT_EQ(fileLogger->log("Test"), "FileLogger: Test");
}

// Test named bindings
TEST_F(InjectionTest, NamedBindings) {
    // Bind multiple implementations with different names
    container->bind<LoggerSymbol>().to<ConsoleLogger>().whenTargetNamed("console");
    container->bind<LoggerSymbol>().to<FileLogger>().whenTargetNamed("file");
    
    // Resolve with specific names
    auto consoleLogger = container->getNamed<LoggerSymbol>("console");
    auto fileLogger = container->getNamed<LoggerSymbol>("file");
    
    // Check that correct implementations were resolved
    EXPECT_EQ(consoleLogger->log("Test"), "ConsoleLogger: Test");
    EXPECT_EQ(fileLogger->log("Test"), "FileLogger: Test");
}

// Test unbinding
TEST_F(InjectionTest, Unbind) {
    // Bind and verify
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    EXPECT_TRUE(container->hasBinding<LoggerSymbol>());
    
    // Unbind and verify
    container->unbind<LoggerSymbol>();
    EXPECT_FALSE(container->hasBinding<LoggerSymbol>());
    
    // Trying to resolve should throw an exception
    EXPECT_THROW(container->get<LoggerSymbol>(), exceptions::ResolutionException);
}

// Test getting all bindings
TEST_F(InjectionTest, GetAll) {
    // Bind multiple implementations
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    
    // Get all
    auto loggers = container->getAll<LoggerSymbol>();
    
    // Check results
    EXPECT_THAT(loggers, SizeIs(1));
    EXPECT_EQ(loggers[0]->log("Test"), "ConsoleLogger: Test");
}

// Test child containers and inheritance
TEST_F(InjectionTest, ChildContainers) {
    // Bind to parent
    container->bind<LoggerSymbol>().to<ConsoleLogger>();
    
    // Create child container
    auto childContainer = container->createChildContainer();
    
    // Child should have access to parent bindings
    auto logger = childContainer->get<LoggerSymbol>();
    EXPECT_EQ(logger->log("Test"), "ConsoleLogger: Test");
    
    // Child can override parent bindings
    childContainer->bind<LoggerSymbol>().to<FileLogger>();
    logger = childContainer->get<LoggerSymbol>();
    EXPECT_EQ(logger->log("Test"), "FileLogger: Test");
    
    // Parent bindings remain unchanged
    logger = container->get<LoggerSymbol>();
    EXPECT_EQ(logger->log("Test"), "ConsoleLogger: Test");
}

// Test lazy binding
TEST_F(InjectionTest, LazyBinding) {
    // Set up a counter to track instantiation
    int instantiationCount = 0;
    
    // Bind a service that increments the counter when instantiated
    container->bind<SimpleServiceSymbol>().toDynamicValue(
        [&instantiationCount](const Context<LoggerSymbol, DatabaseSymbol, SimpleServiceSymbol, 
                                          LoggerServiceSymbol, DataServiceSymbol>&) {
            instantiationCount++;
            return SimpleService();
        }
    );
    
    // Create a lazy wrapper
    auto lazyService = Lazy<SimpleService>([&]() {
        return container->get<SimpleServiceSymbol>();
    });
    
    // Verify no instantiation has happened yet
    EXPECT_EQ(instantiationCount, 0);
    
    // Access the lazy binding
    auto value = lazyService.get();
    
    // Verify instantiation happened once
    EXPECT_EQ(instantiationCount, 1);
    EXPECT_EQ(value.getValue(), 42);
    
    // Access again - should instantiate again since we're using a dynamic value binding
    value = lazyService.get();
    EXPECT_EQ(instantiationCount, 2);
}

// Test multi binding
TEST_F(InjectionTest, MultiBinding) {
    // Define a multi-binding symbol
    using LoggerMultiSymbol = Symbol<Multi<std::shared_ptr<ILogger>>::value>;
    
    // Create a container with the multi-binding symbol
    auto multiContainer = std::make_unique<Container<LoggerMultiSymbol>>();
    
    // Bind multiple loggers
    multiContainer->bind<LoggerMultiSymbol>().toDynamicValue(
        [](const Context<LoggerMultiSymbol>&) {
            std::vector<std::shared_ptr<ILogger>> loggers;
            loggers.push_back(std::make_shared<ConsoleLogger>());
            loggers.push_back(std::make_shared<FileLogger>());
            return loggers;
        }
    );
    
    // Get all loggers
    auto loggers = multiContainer->get<LoggerMultiSymbol>();
    
    // Check results
    ASSERT_THAT(loggers, SizeIs(2));
    EXPECT_EQ(loggers[0]->log("Test"), "ConsoleLogger: Test");
    EXPECT_EQ(loggers[1]->log("Test"), "FileLogger: Test");
}

// Test error handling
TEST_F(InjectionTest, ErrorHandling) {
    // Trying to resolve a binding that doesn't exist should throw
    EXPECT_THROW(container->get<LoggerSymbol>(), exceptions::ResolutionException);
    
    // Bind with a malformed binding (no resolver)
    container->bind<LoggerSymbol>(); // No 'to' or 'toConstantValue' call
    
    // Trying to resolve a malformed binding should throw
    EXPECT_THROW(container->get<LoggerSymbol>(), exceptions::ResolutionException);
}

// Test resolving with an unknown tag
TEST_F(InjectionTest, UnknownTag) {
    // Bind with a specific tag
    Tag knownTag("known");
    container->bind<LoggerSymbol>().to<ConsoleLogger>().when(knownTag);
    
    // Resolving with the correct tag should work
    EXPECT_NO_THROW(container->get<LoggerSymbol>(knownTag));
    
    // Resolving with an unknown tag should throw
    Tag unknownTag("unknown");
    EXPECT_THROW(container->get<LoggerSymbol>(unknownTag), exceptions::ResolutionException);
}

// Test resolving with an unknown name
TEST_F(InjectionTest, UnknownName) {
    // Bind with a specific name
    container->bind<LoggerSymbol>().to<ConsoleLogger>().whenTargetNamed("known");
    
    // Resolving with the correct name should work
    EXPECT_NO_THROW(container->getNamed<LoggerSymbol>("known"));
    
    // Resolving with an unknown name should throw
    EXPECT_THROW(container->getNamed<LoggerSymbol>("unknown"), exceptions::ResolutionException);
}

// Test the Symbolic concept
TEST_F(InjectionTest, SymbolicConcept) {
    // Symbol<T> should satisfy Symbolic
    static_assert(Symbolic<LoggerSymbol>, "Symbol should satisfy Symbolic concept");
    
    // Non-symbolic types should not satisfy Symbolic
    struct NonSymbolic {};
    static_assert(!Symbolic<NonSymbolic>, "Non-symbolic types should not satisfy Symbolic concept");
}

// Test the Injectable concept
TEST_F(InjectionTest, InjectableConcept) {
    // Inject<> should satisfy Injectable
    static_assert(Injectable<Inject<>>, "Inject should satisfy Injectable concept");
    
    // Non-injectable types should not satisfy Injectable
    struct NonInjectable {};
    static_assert(!Injectable<NonInjectable>, "Non-injectable types should not satisfy Injectable concept");
}

// Test request scope binding
TEST_F(InjectionTest, RequestScope) {
    container->bind<LoggerSymbol>().to<ConsoleLogger>().inRequestScope();
    
    // 简单验证绑定是否工作
    auto logger = container->get<LoggerSymbol>();
    ASSERT_TRUE(logger != nullptr);
    EXPECT_EQ(logger->log("Test"), "ConsoleLogger: Test");
}

// Test complex dependency chains
TEST_F(InjectionTest, ComplexDependencyChains) {
    // Define a class with nested dependencies
    struct ServiceA {
        explicit ServiceA(std::shared_ptr<ILogger> logger) : logger_(logger) {}
        std::shared_ptr<ILogger> logger_;
    };
    
    struct ServiceB {
        explicit ServiceB(ServiceA serviceA) : serviceA_(serviceA) {}
        ServiceA serviceA_;
    };
    
    // Define symbols
    using ServiceASymbol = Symbol<ServiceA>;
    using ServiceBSymbol = Symbol<ServiceB>;
    
    // Define injectable metadata
    template<> struct InjectableA<ServiceA> : Inject<LoggerSymbol> {};
    template<> struct InjectableA<ServiceB> : Inject<ServiceASymbol> {};
    
    // Create specialized container
    auto complexContainer = std::make_unique<Container<
        LoggerSymbol, ServiceASymbol, ServiceBSymbol>>();
    
    // Set up bindings
    complexContainer->bind<LoggerSymbol>().to<ConsoleLogger>();
    complexContainer->bind<ServiceASymbol>().to<ServiceA>();
    complexContainer->bind<ServiceBSymbol>().to<ServiceB>();
    
    // Resolve the nested dependency chain
    auto serviceB = complexContainer->get<ServiceBSymbol>();
    
    // Verify the entire dependency chain
    EXPECT_EQ(serviceB.serviceA_.logger_->log("Test"), "ConsoleLogger: Test");
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}