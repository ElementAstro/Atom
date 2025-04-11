/*
 * registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-10

Description: Component Registry for Managing Component Lifecycle

**************************************************/

#ifndef ATOM_COMPONENT_REGISTRY_HPP
#define ATOM_COMPONENT_REGISTRY_HPP

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <future>
#include <optional>

#include "component.hpp"
#include "config.hpp"

#include "atom/log/loguru.hpp"

class Component;

/**
 * @brief Registry for managing component lifecycle.
 *
 * The Registry is responsible for managing the lifecycle of components,
 * including initialization, dependency resolution, and cleanup.
 */
class Registry {
public:
    /**
     * @brief Component metadata structure
     */
    struct ComponentInfo {
        std::string name;                  // 组件名称
        std::string version;               // 组件版本
        std::string description;           // 组件描述
        std::string author;                // 组件作者
        std::string license;               // 组件许可证
        std::string configPath;            // 组件配置文件路径
        std::chrono::system_clock::time_point loadTime;  // 加载时间
        std::chrono::system_clock::time_point lastUsed;  // 最后使用时间
        bool isInitialized{false};         // 是否已初始化
        bool isEnabled{true};              // 是否启用
        bool isAutoLoad{false};            // 是否自动加载
        bool isLazyLoad{false};            // 是否懒加载
        bool isHotReload{false};           // 是否支持热重载
        
        std::vector<std::string> dependencies; // 依赖的组件
        std::vector<std::string> conflicts;    // 冲突的组件
        std::vector<std::string> optionalDeps; // 可选的依赖
        std::vector<std::string> provides;     // 提供的功能
        
        // 性能统计
        struct {
            std::chrono::microseconds initTime{0};   // 初始化时间
            std::chrono::microseconds loadTime{0};   // 加载时间
            uint64_t commandCount{0};                // 命令数量
            uint64_t eventCount{0};                  // 事件数量
            uint64_t callCount{0};                   // 调用次数
        } stats;
    };
    
    /**
     * @brief 组件注册异常
     */
    class RegistryException : public atom::error::Exception {
    public:
        using atom::error::Exception::Exception;
    };
    
    /**
     * @brief 获取注册表单例实例
     */
    static auto instance() -> Registry&;

    /**
     * @brief 注册模块及其初始化函数
     * 
     * @param name 模块名称
     * @param init_func 初始化函数
     */
    void registerModule(const std::string& name, Component::InitFunc init_func);

    /**
     * @brief 添加组件初始化器
     * 
     * @param name 组件名称
     * @param init_func 初始化函数
     * @param cleanup_func 清理函数
     * @param metadata 组件元数据
     */
    void addInitializer(
        const std::string& name, 
        Component::InitFunc init_func,
        Component::CleanupFunc cleanup_func = nullptr,
        std::optional<ComponentInfo> metadata = std::nullopt);

    /**
     * @brief 添加组件依赖关系
     * 
     * @param name 依赖方组件
     * @param dependency 被依赖的组件
     * @param isOptional 是否为可选依赖
     */
    void addDependency(
        const std::string& name, 
        const std::string& dependency,
        bool isOptional = false);

    /**
     * @brief 初始化所有组件
     * 
     * @param forceReload 是否强制重新加载
     */
    void initializeAll(bool forceReload = false);

    /**
     * @brief 清理所有组件资源
     * 
     * @param force 是否强制清理
     */
    void cleanupAll(bool force = false);

    /**
     * @brief 检查组件是否已初始化
     * 
     * @param name 组件名称
     * @return bool 是否已初始化
     */
    [[nodiscard]] auto isInitialized(const std::string& name) const -> bool;

    /**
     * @brief 检查组件是否已启用
     * 
     * @param name 组件名称
     * @return bool 是否已启用
     */
    [[nodiscard]] auto isEnabled(const std::string& name) const -> bool;

    /**
     * @brief 启用组件
     * 
     * @param name 组件名称
     * @param enable 是否启用
     * @return bool 操作是否成功
     */
    bool enableComponent(const std::string& name, bool enable = true);

    /**
     * @brief 重新初始化组件
     * 
     * @param name 组件名称
     * @param reloadDependencies 是否重新加载依赖
     */
    void reinitializeComponent(
        const std::string& name, 
        bool reloadDependencies = false);

    /**
     * @brief 获取组件实例
     * 
     * @param name 组件名称
     * @return std::shared_ptr<Component> 组件实例
     * @throws ObjectNotExist 组件不存在时抛出异常
     */
    [[nodiscard]] auto getComponent(const std::string& name) const
        -> std::shared_ptr<Component>;

    /**
     * @brief 获取或加载组件（支持懒加载）
     * 
     * @param name 组件名称
     * @return std::shared_ptr<Component> 组件实例
     */
    auto getOrLoadComponent(const std::string& name) -> std::shared_ptr<Component>;

    /**
     * @brief 获取所有组件实例
     * 
     * @return std::vector<std::shared_ptr<Component>> 所有组件实例
     */
    [[nodiscard]] auto getAllComponents() const
        -> std::vector<std::shared_ptr<Component>>;

    /**
     * @brief 获取所有组件名称
     * 
     * @return std::vector<std::string> 所有组件名称
     */
    [[nodiscard]] auto getAllComponentNames() const -> std::vector<std::string>;

    /**
     * @brief 获取组件元数据
     * 
     * @param name 组件名称
     * @return const ComponentInfo& 组件元数据
     * @throws ObjectNotExist 组件不存在时抛出异常
     */
    [[nodiscard]] auto getComponentInfo(const std::string& name) const 
        -> const ComponentInfo&;

    /**
     * @brief 更新组件元数据
     * 
     * @param name 组件名称
     * @param info 新的元数据
     * @return bool 更新是否成功
     */
    bool updateComponentInfo(const std::string& name, const ComponentInfo& info);

    /**
     * @brief 从文件系统加载组件
     * 
     * @param path 组件路径
     * @return bool 加载是否成功
     */
    bool loadComponentFromFile(const std::string& path);

    /**
     * @brief 监控组件文件变化，实现热重载
     * 
     * @param enable 是否启用监控
     * @return bool 操作是否成功
     */
    bool watchComponentChanges(bool enable = true);

    /**
     * @brief 卸载组件
     * 
     * @param name 组件名称
     * @return bool 卸载是否成功
     */
    bool removeComponent(const std::string& name);

    #if ENABLE_EVENT_SYSTEM
    /**
     * @brief 订阅组件事件
     * 
     * @param eventName 事件名称
     * @param callback 回调函数
     * @return EventCallbackId 事件回调ID
     */
    atom::components::EventCallbackId subscribeToEvent(
        const std::string& eventName, 
        atom::components::EventCallback callback);

    /**
     * @brief 取消订阅组件事件
     * 
     * @param eventName 事件名称
     * @param callbackId 事件回调ID
     * @return bool 操作是否成功
     */
    bool unsubscribeFromEvent(
        const std::string& eventName, 
        atom::components::EventCallbackId callbackId);

    /**
     * @brief 触发组件事件
     * 
     * @param event 事件对象
     */
    void triggerEvent(const atom::components::Event& event);
    #endif

private:
    Registry() = default;
    ~Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    /**
     * @brief 检查循环依赖
     * 
     * @param name 组件名称
     * @param dependency 依赖的组件名称
     * @return bool 是否存在循环依赖
     */
    bool hasCircularDependency(const std::string& name,
                               const std::string& dependency);

    /**
     * @brief 初始化组件
     * 
     * @param name 组件名称
     * @param init_stack 初始化栈
     */
    void initializeComponent(
        const std::string& name, std::unordered_set<std::string>& init_stack);

    /**
     * @brief 确定组件初始化顺序
     */
    void determineInitializationOrder();

    /**
     * @brief 检查组件依赖是否满足
     * 
     * @param name 组件名称
     * @return std::tuple<bool, std::vector<std::string>> 是否满足及未满足的依赖列表
     */
    std::tuple<bool, std::vector<std::string>> checkDependenciesSatisfied(
        const std::string& name);

    /**
     * @brief 检查组件冲突
     * 
     * @param name 组件名称
     * @return std::tuple<bool, std::vector<std::string>> 是否冲突及冲突组件列表
     */
    std::tuple<bool, std::vector<std::string>> checkConflicts(
        const std::string& name);

    mutable std::shared_mutex mutex_; // 用于线程安全的互斥锁

    // 使用配置所选容器类型
    #if ENABLE_FASTHASH
    emhash::HashMap<std::string, std::shared_ptr<Component>> initializers_; // 组件实例映射
    emhash::HashMap<std::string, ComponentInfo> componentInfos_; // 组件元数据
    emhash::HashMap<std::string, Component::InitFunc> module_initializers_; // 模块初始化函数
    emhash::HashMap<std::string, emhash::HashSet<std::string>> dependencies_; // 组件依赖关系
    emhash::HashMap<std::string, emhash::HashSet<std::string>> optionalDependencies_; // 可选依赖
    #else
    std::unordered_map<std::string, std::shared_ptr<Component>> initializers_; // 组件实例映射
    std::unordered_map<std::string, ComponentInfo> componentInfos_; // 组件元数据
    std::unordered_map<std::string, Component::InitFunc> module_initializers_; // 模块初始化函数
    std::unordered_map<std::string, std::unordered_set<std::string>> dependencies_; // 组件依赖关系
    std::unordered_map<std::string, std::unordered_set<std::string>> optionalDependencies_; // 可选依赖
    #endif

    std::vector<std::string> initializationOrder_; // 组件初始化顺序

    #if ENABLE_EVENT_SYSTEM
    struct EventSubscription {
        atom::components::EventCallbackId id;
        atom::components::EventCallback callback;
    };
    
    std::unordered_map<std::string, std::vector<EventSubscription>> eventSubscriptions_;
    std::atomic<atom::components::EventCallbackId> nextEventId_{1};
    #endif

    #if ENABLE_HOT_RELOAD
    std::unordered_map<std::string, std::filesystem::file_time_type> componentFileTimestamps_;
    std::atomic<bool> watchingForChanges_{false};
    std::future<void> fileWatcherFuture_;
    #endif
};

// 方便组件注册的宏
#define REGISTER_COMPONENT(name, func) \
    static bool registered_##name = (Registry::instance().registerModule(#name, func), true)

// 辅助宏，用于定义组件依赖
#define DECLARE_COMPONENT_DEPENDENCIES(name, ...) \
    std::vector<std::string> name::getNeededComponents() { \
        return {__VA_ARGS__}; \
    }

#endif // ATOM_COMPONENT_REGISTRY_HPP
