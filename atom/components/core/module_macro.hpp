// Helper macros for registering initializers, dependencies, and modules
#ifndef REGISTER_INITIALIZER
#define REGISTER_INITIALIZER(name, init_func, cleanup_func)       \
    namespace {                                                   \
    struct Initializer_##name {                                   \
        Initializer_##name() {                                    \
            LOG_F(INFO, "Registering initializer: {}", #name);    \
            Registry::instance().addInitializer(#name, init_func, \
                                                cleanup_func);    \
        }                                                         \
    };                                                            \
    static Initializer_##name initializer_##name;                 \
    }
#endif

#ifndef REGISTER_DEPENDENCY
#define REGISTER_DEPENDENCY(name, dependency)                                 \
    namespace {                                                               \
    struct Dependency_##name##_##dependency {                                 \
        Dependency_##name##_##dependency() {                                  \
            LOG_F(INFO, "Registering dependency: {} -> {}", #name,            \
                  #dependency);                                               \
            Registry::instance().addDependency(#name, #dependency);           \
        }                                                                     \
    };                                                                        \
    static Dependency_##name##_##dependency dependency_##name##_##dependency; \
    }
#endif

#ifndef REGISTER_COMPONENT_DEPENDENCIES
#define REGISTER_COMPONENT_DEPENDENCIES(name, ...)                           \
    namespace {                                                              \
    template <typename... Deps>                                              \
    struct DependencyRegistrar_##name {                                      \
        template <typename T>                                                \
        static void register_one() {                                         \
            LOG_F(INFO, "Registering component dependency: {} -> {}", #name, \
                  typeid(T).name());                                         \
            Registry::instance().addDependency(#name, typeid(T).name());     \
        }                                                                    \
                                                                             \
        DependencyRegistrar_##name() { (register_one<Deps>(), ...); }        \
    };                                                                       \
    static DependencyRegistrar_##name<__VA_ARGS__>                           \
        dependency_registrar_##name;                                         \
    }
#endif

// Nested macro for module initialization
#ifndef ATOM_MODULE_INIT
#define ATOM_MODULE_INIT(module_name, init_func)                               \
    namespace module_name {                                                    \
    struct ModuleManager {                                                     \
        static void init() {                                                   \
            LOG_F(INFO, "Initializing module: {}", #module_name);              \
            std::shared_ptr<Component> instance = init_func();                 \
            Registry::instance().registerModule(                               \
                #module_name, [instance]() { return instance; });              \
            Registry::instance().addInitializer(                               \
                #module_name, [instance]() { instance->initialize(); });       \
            auto neededComponents = instance->getNeededComponents();           \
            for (const auto& comp : neededComponents) {                        \
                Registry::instance().addDependency(#module_name, comp);        \
                try {                                                          \
                    auto dependency = Registry::instance().getComponent(comp); \
                    if (dependency) {                                          \
                        instance->addOtherComponent(comp, dependency);         \
                    }                                                          \
                } catch (const std::exception& e) {                            \
                    LOG_F(WARNING, "Could not load dependency {} for {}: {}",  \
                          comp, #module_name, e.what());                       \
                }                                                              \
            }                                                                  \
        }                                                                      \
        static void cleanup() {                                                \
            static std::once_flag flag;                                        \
            std::call_once(flag, []() {                                        \
                LOG_F(INFO, "Cleaning up module: {}", #module_name);           \
                auto component =                                               \
                    Registry::instance().getComponent(#module_name);           \
                if (component) {                                               \
                    component->clearOtherComponents();                         \
                    component->destroy();                                      \
                }                                                              \
            });                                                                \
        }                                                                      \
    };                                                                         \
    }
#endif

// Macro for dynamic library module
#ifndef ATOM_MODULE
#define ATOM_MODULE(module_name, init_func)                                   \
    ATOM_MODULE_INIT(module_name, init_func)                                  \
    extern "C" void module_name##_initialize_registry() {                     \
        LOG_F(INFO, "Initializing registry for module: {}", #module_name);    \
        try {                                                                 \
            module_name::ModuleManager::init();                               \
            Registry::instance().initializeAll();                             \
            LOG_F(INFO, "Initialized registry for module: {}", #module_name); \
        } catch (const std::exception& e) {                                   \
            LOG_F(ERROR, "Failed to initialize module {}: {}", #module_name,  \
                  e.what());                                                  \
        }                                                                     \
    }                                                                         \
    extern "C" void module_name##_cleanup_registry() {                        \
        LOG_F(INFO, "Cleaning up registry for module: {}", #module_name);     \
        try {                                                                 \
            module_name::ModuleManager::cleanup();                            \
            Registry::instance().cleanupAll();                                \
            LOG_F(INFO, "Cleaned up registry for module: {}", #module_name);  \
        } catch (const std::exception& e) {                                   \
            LOG_F(ERROR, "Error during cleanup of module {}: {}",             \
                  #module_name, e.what());                                    \
        }                                                                     \
    }                                                                         \
    extern "C" auto module_name##_getInstance()->std::shared_ptr<Component> { \
        LOG_F(INFO, "Getting instance of module: {}", #module_name);          \
        return Registry::instance().getComponent(#module_name);               \
    }                                                                         \
    extern "C" auto module_name##_getVersion()->const char* {                 \
        return ATOM_VERSION;                                                  \
    }
#endif

// Macro for embedded module
#ifndef ATOM_EMBED_MODULE
#define ATOM_EMBED_MODULE(module_name, init_func)                             \
    ATOM_MODULE_INIT(module_name, init_func)                                  \
    namespace module_name {                                                   \
    inline std::optional<std::once_flag> init_flag;                           \
    struct ModuleInitializer {                                                \
        ModuleInitializer() {                                                 \
            if (!init_flag.has_value()) {                                     \
                LOG_F(INFO, "Embedding module: {}", #module_name);            \
                init_flag.emplace();                                          \
                try {                                                         \
                    ModuleManager::init();                                    \
                } catch (const std::exception& e) {                           \
                    LOG_F(ERROR,                                              \
                          "Failed to initialize embedded module {}: {}",      \
                          #module_name, e.what());                            \
                }                                                             \
            }                                                                 \
        }                                                                     \
        ~ModuleInitializer() {                                                \
            if (init_flag.has_value()) {                                      \
                LOG_F(INFO, "Cleaning up embedded module: {}", #module_name); \
                try {                                                         \
                    ModuleManager::cleanup();                                 \
                } catch (const std::exception& e) {                           \
                    LOG_F(ERROR,                                              \
                          "Error during cleanup of embedded module {}: {}",   \
                          #module_name, e.what());                            \
                }                                                             \
                init_flag.reset();                                            \
            }                                                                 \
        }                                                                     \
    };                                                                        \
    inline ModuleInitializer module_initializer;                              \
    }                                                                         \
    auto module_name##_getInstance()->std::shared_ptr<Component> {            \
        return Registry::instance().getComponent(#module_name);               \
    }
#endif

// Macro for dynamic library module with test support
#ifndef ATOM_MODULE_TEST
#define ATOM_MODULE_TEST(module_name, init_func, test_func)                  \
    ATOM_MODULE(module_name, init_func)                                      \
    extern "C" void module_name##_test() {                                   \
        LOG_F(INFO, "Running tests for module: {}", #module_name);           \
        try {                                                                \
            auto instance = Registry::instance().getComponent(#module_name); \
            test_func(instance);                                             \
            LOG_F(INFO, "Tests completed successfully for module: {}",       \
                  #module_name);                                             \
        } catch (const std::exception& e) {                                  \
            LOG_F(ERROR, "Test failed for module {}: {}", #module_name,      \
                  e.what());                                                 \
        }                                                                    \
    }
#endif

// Macro for component implementation
#ifndef ATOM_COMPONENT
#define ATOM_COMPONENT(component_name, component_type)                     \
    class component_name : public component_type {                         \
    public:                                                                \
        explicit component_name(const std::string& name = #component_name) \
            : component_type(name) {                                       \
            LOG_F(INFO, "Component {} created", name);                     \
        }                                                                  \
        ~component_name() override {                                       \
            LOG_F(INFO, "Component {} destroyed", getName());              \
        }                                                                  \
        static auto create() -> std::shared_ptr<component_name> {          \
            return std::make_shared<component_name>();                     \
        }
#endif

#ifndef ATOM_COMPONENT_END
#define ATOM_COMPONENT_END \
    }                      \
    ;
#endif

// Macro for hot-reloadable component
#ifndef ATOM_HOT_COMPONENT
#define ATOM_HOT_COMPONENT(component_name, component_type)               \
    ATOM_COMPONENT(component_name, component_type)                       \
    bool initialize() override {                                         \
        if (!component_type::initialize())                               \
            return false;                                                \
        Registry::instance().registerModule(                             \
            #component_name, []() { return component_name::create(); }); \
        return true;                                                     \
    }                                                                    \
    bool reload() {                                                      \
        LOG_F(INFO, "Reloading component: {}", getName());               \
        return destroy() && initialize();                                \
    }
#endif
