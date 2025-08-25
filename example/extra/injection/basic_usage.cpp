#include "atom/extra/injection/all.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace atom::extra;

// Define some example interfaces and implementations
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(const std::string& message) = 0;
};

class ConsoleLogger : public ILogger {
public:
    void log(const std::string& message) override {
        std::cout << "[CONSOLE] " << message << std::endl;
    }
};

class FileLogger : public ILogger {
private:
    std::string filename_;

public:
    explicit FileLogger(const std::string& filename) : filename_(filename) {}

    void log(const std::string& message) override {
        std::cout << "[FILE:" << filename_ << "] " << message << std::endl;
    }
};

class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual void connect() = 0;
    virtual void query(const std::string& sql) = 0;
};

class MySQLDatabase : public IDatabase {
private:
    std::string connection_string_;

public:
    explicit MySQLDatabase(const std::string& conn_str)
        : connection_string_(conn_str) {}

    void connect() override {
        std::cout << "Connected to MySQL: " << connection_string_ << std::endl;
    }

    void query(const std::string& sql) override {
        std::cout << "MySQL Query: " << sql << std::endl;
    }
};

class PostgreSQLDatabase : public IDatabase {
private:
    std::string connection_string_;

public:
    explicit PostgreSQLDatabase(const std::string& conn_str)
        : connection_string_(conn_str) {}

    void connect() override {
        std::cout << "Connected to PostgreSQL: " << connection_string_
                  << std::endl;
    }

    void query(const std::string& sql) override {
        std::cout << "PostgreSQL Query: " << sql << std::endl;
    }
};

// Service that depends on logger and database
class UserService {
private:
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IDatabase> database_;

public:
    UserService(std::shared_ptr<ILogger> logger,
                std::shared_ptr<IDatabase> database)
        : logger_(logger), database_(database) {}

    void createUser(const std::string& username) {
        logger_->log("Creating user: " + username);
        database_->connect();
        database_->query("INSERT INTO users (username) VALUES ('" + username +
                         "')");
        logger_->log("User created successfully: " + username);
    }

    void getUser(const std::string& username) {
        logger_->log("Retrieving user: " + username);
        database_->connect();
        database_->query("SELECT * FROM users WHERE username = '" + username +
                         "'");
        logger_->log("User retrieved: " + username);
    }
};

// Define symbols for dependency injection
DEFINE_SYMBOL(LoggerSymbol, std::shared_ptr<ILogger>);
DEFINE_SYMBOL(DatabaseSymbol, std::shared_ptr<IDatabase>);
DEFINE_SYMBOL(UserServiceSymbol, std::shared_ptr<UserService>);

int main() {
    try {
        std::cout << "=== Dependency Injection Basic Usage Example ==="
                  << std::endl;

        // 1. Basic container setup and binding
        std::cout << "\n1. Basic Container Setup and Binding:" << std::endl;
        {
            Container<LoggerSymbol, DatabaseSymbol, UserServiceSymbol>
                container;

            // Bind logger to console logger
            container.bind<LoggerSymbol>().to(
                []() { return std::make_shared<ConsoleLogger>(); });

            // Bind database to MySQL implementation
            container.bind<DatabaseSymbol>().to([]() {
                return std::make_shared<MySQLDatabase>(
                    "mysql://localhost:3306/myapp");
            });

            // Bind user service with dependencies
            container.bind<UserServiceSymbol>().to([&container]() {
                auto logger = container.get<LoggerSymbol>();
                auto database = container.get<DatabaseSymbol>();
                return std::make_shared<UserService>(logger, database);
            });

            // Resolve and use the service
            auto userService = container.get<UserServiceSymbol>();
            userService->createUser("john_doe");
            userService->getUser("john_doe");
        }

        // 2. Named bindings
        std::cout << "\n2. Named Bindings:" << std::endl;
        {
            Container<LoggerSymbol, DatabaseSymbol> container;

            // Bind multiple logger implementations with names
            container.bind<LoggerSymbol>().toNamed(
                "console", []() { return std::make_shared<ConsoleLogger>(); });

            container.bind<LoggerSymbol>().toNamed("file", []() {
                return std::make_shared<FileLogger>("app.log");
            });

            // Bind multiple database implementations with names
            container.bind<DatabaseSymbol>().toNamed("mysql", []() {
                return std::make_shared<MySQLDatabase>(
                    "mysql://localhost:3306/myapp");
            });

            container.bind<DatabaseSymbol>().toNamed("postgresql", []() {
                return std::make_shared<PostgreSQLDatabase>(
                    "postgresql://localhost:5432/myapp");
            });

            // Resolve by name
            auto consoleLogger = container.getNamed<LoggerSymbol>("console");
            auto fileLogger = container.getNamed<LoggerSymbol>("file");
            auto mysqlDb = container.getNamed<DatabaseSymbol>("mysql");
            auto postgresDb = container.getNamed<DatabaseSymbol>("postgresql");

            consoleLogger->log("Message from console logger");
            fileLogger->log("Message from file logger");

            mysqlDb->connect();
            mysqlDb->query("SELECT 1");

            postgresDb->connect();
            postgresDb->query("SELECT version()");
        }

        // 3. Singleton vs Transient lifetimes
        std::cout << "\n3. Singleton vs Transient Lifetimes:" << std::endl;
        {
            Container<LoggerSymbol> container;

            // Singleton binding - same instance returned every time
            container.bind<LoggerSymbol>().toSingleton([]() {
                std::cout << "Creating singleton logger instance" << std::endl;
                return std::make_shared<ConsoleLogger>();
            });

            std::cout << "Getting singleton instances:" << std::endl;
            auto logger1 = container.get<LoggerSymbol>();
            auto logger2 = container.get<LoggerSymbol>();
            auto logger3 = container.get<LoggerSymbol>();

            std::cout << "Logger1 address: " << logger1.get() << std::endl;
            std::cout << "Logger2 address: " << logger2.get() << std::endl;
            std::cout << "Logger3 address: " << logger3.get() << std::endl;
            std::cout << "All instances are the same: "
                      << (logger1 == logger2 && logger2 == logger3)
                      << std::endl;

            // Transient binding - new instance every time
            container.bind<LoggerSymbol>().toTransient([]() {
                std::cout << "Creating transient logger instance" << std::endl;
                return std::make_shared<ConsoleLogger>();
            });

            std::cout << "\nGetting transient instances:" << std::endl;
            auto transient1 = container.get<LoggerSymbol>();
            auto transient2 = container.get<LoggerSymbol>();
            auto transient3 = container.get<LoggerSymbol>();

            std::cout << "Transient1 address: " << transient1.get()
                      << std::endl;
            std::cout << "Transient2 address: " << transient2.get()
                      << std::endl;
            std::cout << "Transient3 address: " << transient3.get()
                      << std::endl;
            std::cout << "All instances are different: "
                      << (transient1 != transient2 && transient2 != transient3)
                      << std::endl;
        }

        // 4. Tagged bindings
        std::cout << "\n4. Tagged Bindings:" << std::endl;
        {
            Container<LoggerSymbol, DatabaseSymbol> container;

            // Bind with tags
            container.bind<LoggerSymbol>().withTag(Tag("development")).to([]() {
                return std::make_shared<ConsoleLogger>();
            });

            container.bind<LoggerSymbol>().withTag(Tag("production")).to([]() {
                return std::make_shared<FileLogger>("production.log");
            });

            container.bind<DatabaseSymbol>()
                .withTag(Tag("development"))
                .to([]() {
                    return std::make_shared<MySQLDatabase>(
                        "mysql://localhost:3306/dev_db");
                });

            container.bind<DatabaseSymbol>()
                .withTag(Tag("production"))
                .to([]() {
                    return std::make_shared<PostgreSQLDatabase>(
                        "postgresql://prod-server:5432/prod_db");
                });

            // Resolve by tag
            std::cout << "Development environment:" << std::endl;
            auto devLogger = container.get<LoggerSymbol>(Tag("development"));
            auto devDb = container.get<DatabaseSymbol>(Tag("development"));
            devLogger->log("Development message");
            devDb->connect();

            std::cout << "\nProduction environment:" << std::endl;
            auto prodLogger = container.get<LoggerSymbol>(Tag("production"));
            auto prodDb = container.get<DatabaseSymbol>(Tag("production"));
            prodLogger->log("Production message");
            prodDb->connect();
        }

        // 5. Conditional bindings
        std::cout << "\n5. Conditional Bindings:" << std::endl;
        {
            Container<LoggerSymbol> container;

            bool isDebugMode = true;

            // Conditional binding based on runtime condition
            if (isDebugMode) {
                container.bind<LoggerSymbol>().to([]() {
                    std::cout << "Binding debug logger" << std::endl;
                    return std::make_shared<ConsoleLogger>();
                });
            } else {
                container.bind<LoggerSymbol>().to([]() {
                    std::cout << "Binding production logger" << std::endl;
                    return std::make_shared<FileLogger>("production.log");
                });
            }

            auto logger = container.get<LoggerSymbol>();
            logger->log("Conditional binding message");
        }

        // 6. Factory bindings
        std::cout << "\n6. Factory Bindings:" << std::endl;
        {
            Container<DatabaseSymbol> container;

            // Factory that creates different database types based on
            // configuration
            container.bind<DatabaseSymbol>().toFactory(
                [](const std::string& dbType) {
                    if (dbType == "mysql") {
                        return std::make_shared<MySQLDatabase>(
                            "mysql://localhost:3306/factory_db");
                    } else if (dbType == "postgresql") {
                        return std::make_shared<PostgreSQLDatabase>(
                            "postgresql://localhost:5432/factory_db");
                    } else {
                        throw std::runtime_error("Unknown database type: " +
                                                 dbType);
                    }
                });

            // Use factory to create different database instances
            auto mysqlDb =
                container.getFactory<DatabaseSymbol>()->create("mysql");
            auto postgresDb =
                container.getFactory<DatabaseSymbol>()->create("postgresql");

            mysqlDb->connect();
            postgresDb->connect();
        }

        // 7. Circular dependency handling
        std::cout << "\n7. Circular Dependency Handling:" << std::endl;
        {
            // Example of how to handle circular dependencies using lazy
            // initialization
            class ServiceA;
            class ServiceB;

            class ServiceA {
            private:
                std::function<std::shared_ptr<ServiceB>()> serviceBFactory_;

            public:
                ServiceA(
                    std::function<std::shared_ptr<ServiceB>()> serviceBFactory)
                    : serviceBFactory_(serviceBFactory) {}

                void doSomething() {
                    std::cout << "ServiceA doing something" << std::endl;
                    auto serviceB = serviceBFactory_();
                    serviceB->doSomethingElse();
                }
            };

            class ServiceB {
            private:
                std::shared_ptr<ServiceA> serviceA_;

            public:
                ServiceB(std::shared_ptr<ServiceA> serviceA)
                    : serviceA_(serviceA) {}

                void doSomethingElse() {
                    std::cout << "ServiceB doing something else" << std::endl;
                }
            };

            DEFINE_SYMBOL(ServiceASymbol, std::shared_ptr<ServiceA>);
            DEFINE_SYMBOL(ServiceBSymbol, std::shared_ptr<ServiceB>);

            Container<ServiceASymbol, ServiceBSymbol> container;

            // Break circular dependency using lazy factory
            container.bind<ServiceASymbol>().to([&container]() {
                return std::make_shared<ServiceA>(
                    [&container]() { return container.get<ServiceBSymbol>(); });
            });

            container.bind<ServiceBSymbol>().to([&container]() {
                return std::make_shared<ServiceB>(
                    container.get<ServiceASymbol>());
            });

            auto serviceA = container.get<ServiceASymbol>();
            serviceA->doSomething();
        }

        // 8. Container hierarchies and scoping
        std::cout << "\n8. Container Hierarchies and Scoping:" << std::endl;
        {
            // Parent container with global services
            Container<LoggerSymbol> parentContainer;
            parentContainer.bind<LoggerSymbol>().toSingleton(
                []() { return std::make_shared<ConsoleLogger>(); });

            // Child container with additional services
            Container<LoggerSymbol, DatabaseSymbol> childContainer;

            // Child can inherit from parent (conceptual - would need
            // implementation)
            childContainer.bind<LoggerSymbol>().to([&parentContainer]() {
                return parentContainer.get<LoggerSymbol>();
            });

            childContainer.bind<DatabaseSymbol>().to([]() {
                return std::make_shared<MySQLDatabase>(
                    "mysql://localhost:3306/child_db");
            });

            auto logger = childContainer.get<LoggerSymbol>();
            auto database = childContainer.get<DatabaseSymbol>();

            logger->log("Message from child container");
            database->connect();
        }

        // 9. Configuration-driven binding
        std::cout << "\n9. Configuration-Driven Binding:" << std::endl;
        {
            Container<LoggerSymbol, DatabaseSymbol> container;

            // Simulate configuration
            struct Config {
                std::string loggerType = "console";
                std::string databaseType = "mysql";
                std::string databaseUrl = "mysql://localhost:3306/config_db";
            } config;

            // Bind based on configuration
            if (config.loggerType == "console") {
                container.bind<LoggerSymbol>().to(
                    []() { return std::make_shared<ConsoleLogger>(); });
            } else if (config.loggerType == "file") {
                container.bind<LoggerSymbol>().to([]() {
                    return std::make_shared<FileLogger>("config.log");
                });
            }

            if (config.databaseType == "mysql") {
                container.bind<DatabaseSymbol>().to([&config]() {
                    return std::make_shared<MySQLDatabase>(config.databaseUrl);
                });
            } else if (config.databaseType == "postgresql") {
                container.bind<DatabaseSymbol>().to([&config]() {
                    return std::make_shared<PostgreSQLDatabase>(
                        config.databaseUrl);
                });
            }

            auto logger = container.get<LoggerSymbol>();
            auto database = container.get<DatabaseSymbol>();

            logger->log("Configuration-driven binding");
            database->connect();
        }

        std::cout
            << "\n=== Dependency Injection Basic Usage Example Completed ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
