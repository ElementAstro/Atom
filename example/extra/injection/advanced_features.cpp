#include "atom/extra/injection/all.hpp"

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace atom::extra;
using namespace std::chrono_literals;

// Advanced interfaces and implementations for complex scenarios
class IEventBus {
public:
    virtual ~IEventBus() = default;
    virtual void publish(const std::string& event) = 0;
    virtual void subscribe(const std::string& eventType,
                           std::function<void(const std::string&)> handler) = 0;
};

class InMemoryEventBus : public IEventBus {
private:
    std::map<std::string, std::vector<std::function<void(const std::string&)>>>
        handlers_;

public:
    void publish(const std::string& event) override {
        std::cout << "Publishing event: " << event << std::endl;
        // Simulate event publishing
        for (const auto& [eventType, handlerList] : handlers_) {
            if (event.find(eventType) != std::string::npos) {
                for (const auto& handler : handlerList) {
                    handler(event);
                }
            }
        }
    }

    void subscribe(const std::string& eventType,
                   std::function<void(const std::string&)> handler) override {
        handlers_[eventType].push_back(handler);
        std::cout << "Subscribed to event type: " << eventType << std::endl;
    }
};

class ICache {
public:
    virtual ~ICache() = default;
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual std::string get(const std::string& key) = 0;
    virtual bool exists(const std::string& key) = 0;
};

class RedisCache : public ICache {
private:
    std::string connection_string_;
    std::map<std::string, std::string> data_;  // Simulate Redis

public:
    explicit RedisCache(const std::string& conn_str)
        : connection_string_(conn_str) {
        std::cout << "Connected to Redis: " << connection_string_ << std::endl;
    }

    void set(const std::string& key, const std::string& value) override {
        data_[key] = value;
        std::cout << "Redis SET: " << key << " = " << value << std::endl;
    }

    std::string get(const std::string& key) override {
        auto it = data_.find(key);
        if (it != data_.end()) {
            std::cout << "Redis GET: " << key << " = " << it->second
                      << std::endl;
            return it->second;
        }
        std::cout << "Redis GET: " << key << " = (not found)" << std::endl;
        return "";
    }

    bool exists(const std::string& key) override {
        return data_.find(key) != data_.end();
    }
};

class MemoryCache : public ICache {
private:
    std::map<std::string, std::string> data_;

public:
    void set(const std::string& key, const std::string& value) override {
        data_[key] = value;
        std::cout << "Memory SET: " << key << " = " << value << std::endl;
    }

    std::string get(const std::string& key) override {
        auto it = data_.find(key);
        if (it != data_.end()) {
            std::cout << "Memory GET: " << key << " = " << it->second
                      << std::endl;
            return it->second;
        }
        std::cout << "Memory GET: " << key << " = (not found)" << std::endl;
        return "";
    }

    bool exists(const std::string& key) override {
        return data_.find(key) != data_.end();
    }
};

// Complex service with multiple dependencies
class OrderService {
private:
    std::shared_ptr<IEventBus> eventBus_;
    std::shared_ptr<ICache> cache_;
    std::string serviceId_;

public:
    OrderService(std::shared_ptr<IEventBus> eventBus,
                 std::shared_ptr<ICache> cache, const std::string& serviceId)
        : eventBus_(eventBus), cache_(cache), serviceId_(serviceId) {
        std::cout << "OrderService created with ID: " << serviceId_
                  << std::endl;
    }

    void createOrder(const std::string& orderId) {
        std::cout << "Creating order: " << orderId << std::endl;

        // Cache the order
        cache_->set("order:" + orderId, "created");

        // Publish event
        eventBus_->publish("order.created:" + orderId);

        std::cout << "Order created successfully: " << orderId << std::endl;
    }

    void processOrder(const std::string& orderId) {
        std::cout << "Processing order: " << orderId << std::endl;

        if (cache_->exists("order:" + orderId)) {
            cache_->set("order:" + orderId, "processing");
            eventBus_->publish("order.processing:" + orderId);
        }
    }

    std::string getServiceId() const { return serviceId_; }
};

// Define symbols for advanced features
DEFINE_SYMBOL(EventBusSymbol, std::shared_ptr<IEventBus>);
DEFINE_SYMBOL(CacheSymbol, std::shared_ptr<ICache>);
DEFINE_SYMBOL(OrderServiceSymbol, std::shared_ptr<OrderService>);

// Factory interface for creating services
template <typename T>
class IFactory {
public:
    virtual ~IFactory() = default;
    virtual std::shared_ptr<T> create() = 0;
    virtual std::shared_ptr<T> create(const std::string& config) = 0;
};

class OrderServiceFactory : public IFactory<OrderService> {
private:
    std::shared_ptr<IEventBus> eventBus_;
    std::shared_ptr<ICache> cache_;
    int instanceCounter_ = 0;

public:
    OrderServiceFactory(std::shared_ptr<IEventBus> eventBus,
                        std::shared_ptr<ICache> cache)
        : eventBus_(eventBus), cache_(cache) {}

    std::shared_ptr<OrderService> create() override {
        return create("default");
    }

    std::shared_ptr<OrderService> create(const std::string& config) override {
        std::string serviceId =
            config + "_" + std::to_string(++instanceCounter_);
        return std::make_shared<OrderService>(eventBus_, cache_, serviceId);
    }
};

DEFINE_SYMBOL(OrderServiceFactorySymbol,
              std::shared_ptr<IFactory<OrderService>>);

int main() {
    try {
        std::cout << "=== Dependency Injection Advanced Features Example ==="
                  << std::endl;

        // 1. Multi-level dependency resolution
        std::cout << "\n1. Multi-level Dependency Resolution:" << std::endl;
        {
            Container<EventBusSymbol, CacheSymbol, OrderServiceSymbol>
                container;

            // Bind dependencies in order
            container.bind<EventBusSymbol>().toSingleton(
                []() { return std::make_shared<InMemoryEventBus>(); });

            container.bind<CacheSymbol>().toSingleton([]() {
                return std::make_shared<RedisCache>("redis://localhost:6379");
            });

            container.bind<OrderServiceSymbol>().to([&container]() {
                auto eventBus = container.get<EventBusSymbol>();
                auto cache = container.get<CacheSymbol>();
                return std::make_shared<OrderService>(eventBus, cache,
                                                      "main_service");
            });

            // Resolve the complex service
            auto orderService = container.get<OrderServiceSymbol>();
            orderService->createOrder("ORDER_001");
            orderService->processOrder("ORDER_001");
        }

        // 2. Factory pattern with dependency injection
        std::cout << "\n2. Factory Pattern with Dependency Injection:"
                  << std::endl;
        {
            Container<EventBusSymbol, CacheSymbol, OrderServiceFactorySymbol>
                container;

            container.bind<EventBusSymbol>().toSingleton(
                []() { return std::make_shared<InMemoryEventBus>(); });

            container.bind<CacheSymbol>().toSingleton(
                []() { return std::make_shared<MemoryCache>(); });

            container.bind<OrderServiceFactorySymbol>().toSingleton(
                [&container]() {
                    auto eventBus = container.get<EventBusSymbol>();
                    auto cache = container.get<CacheSymbol>();
                    return std::make_shared<OrderServiceFactory>(eventBus,
                                                                 cache);
                });

            // Use factory to create multiple service instances
            auto factory = container.get<OrderServiceFactorySymbol>();

            auto service1 = factory->create("service_a");
            auto service2 = factory->create("service_b");
            auto service3 = factory->create("service_c");

            service1->createOrder("ORDER_A1");
            service2->createOrder("ORDER_B1");
            service3->createOrder("ORDER_C1");

            std::cout << "Service IDs: " << service1->getServiceId() << ", "
                      << service2->getServiceId() << ", "
                      << service3->getServiceId() << std::endl;
        }

        // 3. Decorator pattern with dependency injection
        std::cout << "\n3. Decorator Pattern with Dependency Injection:"
                  << std::endl;
        {
            // Decorator for cache with logging
            class LoggingCacheDecorator : public ICache {
            private:
                std::shared_ptr<ICache> wrapped_;

            public:
                explicit LoggingCacheDecorator(std::shared_ptr<ICache> wrapped)
                    : wrapped_(wrapped) {}

                void set(const std::string& key,
                         const std::string& value) override {
                    std::cout << "[CACHE LOG] Setting key: " << key
                              << std::endl;
                    wrapped_->set(key, value);
                }

                std::string get(const std::string& key) override {
                    std::cout << "[CACHE LOG] Getting key: " << key
                              << std::endl;
                    return wrapped_->get(key);
                }

                bool exists(const std::string& key) override {
                    std::cout
                        << "[CACHE LOG] Checking existence of key: " << key
                        << std::endl;
                    return wrapped_->exists(key);
                }
            };

            Container<CacheSymbol> container;

            // Bind with decorator
            container.bind<CacheSymbol>().to([]() {
                auto baseCache = std::make_shared<MemoryCache>();
                return std::make_shared<LoggingCacheDecorator>(baseCache);
            });

            auto cache = container.get<CacheSymbol>();
            cache->set("decorated_key", "decorated_value");
            auto value = cache->get("decorated_key");
            bool exists = cache->exists("decorated_key");

            std::cout << "Decorated cache operations completed" << std::endl;
        }

        // 4. Conditional binding with runtime configuration
        std::cout << "\n4. Conditional Binding with Runtime Configuration:"
                  << std::endl;
        {
            struct RuntimeConfig {
                std::string environment = "development";
                bool enableCaching = true;
                std::string cacheType = "memory";
            };

            RuntimeConfig config;
            config.environment = "production";
            config.cacheType = "redis";

            Container<CacheSymbol, EventBusSymbol> container;

            // Conditional binding based on environment
            if (config.environment == "development") {
                container.bind<CacheSymbol>().to(
                    []() { return std::make_shared<MemoryCache>(); });
            } else if (config.environment == "production") {
                if (config.cacheType == "redis") {
                    container.bind<CacheSymbol>().to([]() {
                        return std::make_shared<RedisCache>(
                            "redis://prod-server:6379");
                    });
                } else {
                    container.bind<CacheSymbol>().to(
                        []() { return std::make_shared<MemoryCache>(); });
                }
            }

            container.bind<EventBusSymbol>().to(
                []() { return std::make_shared<InMemoryEventBus>(); });

            auto cache = container.get<CacheSymbol>();
            auto eventBus = container.get<EventBusSymbol>();

            cache->set("config_test", "production_value");
            eventBus->publish("config.loaded");
        }

        // 5. Thread-safe container usage
        std::cout << "\n5. Thread-Safe Container Usage:" << std::endl;
        {
            Container<CacheSymbol> container;

            container.bind<CacheSymbol>().toSingleton(
                []() { return std::make_shared<MemoryCache>(); });

            std::vector<std::future<void>> futures;
            const int num_threads = 4;

            // Launch multiple threads that use the container
            for (int i = 0; i < num_threads; ++i) {
                futures.push_back(
                    std::async(std::launch::async, [&container, i]() {
                        for (int j = 0; j < 5; ++j) {
                            auto cache = container.get<CacheSymbol>();
                            std::string key = "thread_" + std::to_string(i) +
                                              "_key_" + std::to_string(j);
                            std::string value = "thread_" + std::to_string(i) +
                                                "_value_" + std::to_string(j);

                            cache->set(key, value);
                            auto retrieved = cache->get(key);

                            std::this_thread::sleep_for(10ms);
                        }
                    }));
            }

            // Wait for all threads to complete
            for (auto& future : futures) {
                future.wait();
            }

            std::cout << "Thread-safe container test completed" << std::endl;
        }

        // 6. Lazy initialization and circular dependency resolution
        std::cout << "\n6. Lazy Initialization and Circular Dependencies:"
                  << std::endl;
        {
            class ServiceX;
            class ServiceY;

            class ServiceX {
            private:
                std::function<std::shared_ptr<ServiceY>()> serviceYProvider_;

            public:
                ServiceX(
                    std::function<std::shared_ptr<ServiceY>()> serviceYProvider)
                    : serviceYProvider_(serviceYProvider) {
                    std::cout << "ServiceX created" << std::endl;
                }

                void useServiceY() {
                    std::cout << "ServiceX using ServiceY" << std::endl;
                    auto serviceY = serviceYProvider_();
                    serviceY->doWork();
                }
            };

            class ServiceY {
            private:
                std::weak_ptr<ServiceX> serviceX_;

            public:
                ServiceY(std::weak_ptr<ServiceX> serviceX)
                    : serviceX_(serviceX) {
                    std::cout << "ServiceY created" << std::endl;
                }

                void doWork() {
                    std::cout << "ServiceY doing work" << std::endl;
                    if (auto serviceX = serviceX_.lock()) {
                        std::cout << "ServiceY has reference to ServiceX"
                                  << std::endl;
                    }
                }
            };

            DEFINE_SYMBOL(ServiceXSymbol, std::shared_ptr<ServiceX>);
            DEFINE_SYMBOL(ServiceYSymbol, std::shared_ptr<ServiceY>);

            Container<ServiceXSymbol, ServiceYSymbol> container;

            std::shared_ptr<ServiceX> serviceXInstance;

            container.bind<ServiceXSymbol>().toSingleton([&container,
                                                          &serviceXInstance]() {
                serviceXInstance = std::make_shared<ServiceX>(
                    [&container]() { return container.get<ServiceYSymbol>(); });
                return serviceXInstance;
            });

            container.bind<ServiceYSymbol>().toSingleton([&serviceXInstance]() {
                return std::make_shared<ServiceY>(
                    std::weak_ptr<ServiceX>(serviceXInstance));
            });

            auto serviceX = container.get<ServiceXSymbol>();
            serviceX->useServiceY();
        }

        // 7. Container composition and modules
        std::cout << "\n7. Container Composition and Modules:" << std::endl;
        {
            // Core module
            Container<EventBusSymbol> coreContainer;
            coreContainer.bind<EventBusSymbol>().toSingleton(
                []() { return std::make_shared<InMemoryEventBus>(); });

            // Cache module
            Container<CacheSymbol> cacheContainer;
            cacheContainer.bind<CacheSymbol>().toSingleton(
                []() { return std::make_shared<MemoryCache>(); });

            // Application module that composes other modules
            Container<EventBusSymbol, CacheSymbol, OrderServiceSymbol>
                appContainer;

            // Compose dependencies from other containers
            appContainer.bind<EventBusSymbol>().to([&coreContainer]() {
                return coreContainer.get<EventBusSymbol>();
            });

            appContainer.bind<CacheSymbol>().to([&cacheContainer]() {
                return cacheContainer.get<CacheSymbol>();
            });

            appContainer.bind<OrderServiceSymbol>().to([&appContainer]() {
                auto eventBus = appContainer.get<EventBusSymbol>();
                auto cache = appContainer.get<CacheSymbol>();
                return std::make_shared<OrderService>(eventBus, cache,
                                                      "composed_service");
            });

            auto orderService = appContainer.get<OrderServiceSymbol>();
            orderService->createOrder("COMPOSED_ORDER");
        }

        // 8. Performance testing with large number of dependencies
        std::cout << "\n8. Performance Testing:" << std::endl;
        {
            Container<CacheSymbol> container;

            container.bind<CacheSymbol>().toSingleton(
                []() { return std::make_shared<MemoryCache>(); });

            const int num_resolutions = 10000;

            auto start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_resolutions; ++i) {
                auto cache = container.get<CacheSymbol>();
                // Use the cache to ensure it's not optimized away
                cache->exists("perf_test_key");
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "Performance test results:" << std::endl;
            std::cout << "  Resolutions: " << num_resolutions << std::endl;
            std::cout << "  Total time: " << duration.count() << " μs"
                      << std::endl;
            std::cout << "  Average time per resolution: "
                      << (duration.count() / num_resolutions) << " μs"
                      << std::endl;
            std::cout << "  Resolutions per second: "
                      << (num_resolutions * 1000000.0 / duration.count())
                      << std::endl;
        }

        std::cout << "\n=== Dependency Injection Advanced Features Example "
                     "Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
