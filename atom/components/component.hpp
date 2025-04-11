/*
 * component.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-26

Description: Basic Component Definition

**************************************************/

#ifndef ATOM_COMPONENT_HPP
#define ATOM_COMPONENT_HPP

#include "dispatch.hpp"
#include "module_macro.hpp"
#include "var.hpp"
#include "config.hpp"

#include "atom/log/loguru.hpp"
#include "atom/meta/concept.hpp"
#include "atom/meta/constructor.hpp"
#include "atom/meta/conversion.hpp"
#include "atom/meta/func_traits.hpp"
#include "atom/meta/type_caster.hpp"
#include "atom/meta/type_info.hpp"
#include "atom/type/pointer.hpp"

#include <chrono>
#include <memory>
#include <mutex>

class ObjectExpiredError : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_OBJECT_EXPIRED(...)                                            \
    throw ObjectExpiredError(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                             __VA_ARGS__)

/**
 * @brief 组件生命周期状态
 */
enum class ComponentState {
    Created,      // 创建但未初始化
    Initializing, // 正在初始化
    Active,       // 已初始化且活跃
    Disabled,     // 已禁用
    Error,        // 发生错误
    Destroying    // 正在销毁
};

/**
 * @brief 组件基类，提供组件服务的基础结构
 * 
 * Component类是所有组件的基类，提供了组件注册、命令调度、事件处理等功能。
 */
class Component : public std::enable_shared_from_this<Component> {
public:
    /**
     * @brief Type definition for initialization function.
     */
    using InitFunc = std::function<void(Component&)>;

    /**
     * @brief Type definition for cleanup function.
     */
    using CleanupFunc = std::function<void()>;

    /**
     * @brief 性能统计结构
     */
    struct PerformanceStats {
        std::atomic<uint64_t> commandCallCount{0}; // 命令调用次数
        std::atomic<uint64_t> commandErrorCount{0}; // 命令错误次数
        std::atomic<uint64_t> eventCount{0}; // 事件处理次数
        
        struct {
            std::chrono::microseconds totalExecutionTime{0}; // 总执行时间
            std::chrono::microseconds maxExecutionTime{0};   // 最长执行时间
            std::chrono::microseconds minExecutionTime{std::chrono::microseconds::max()}; // 最短执行时间
            std::chrono::microseconds avgExecutionTime{0};   // 平均执行时间
        } timing;
        
        void reset() {
            commandCallCount = 0;
            commandErrorCount = 0;
            eventCount = 0;
            timing.totalExecutionTime = std::chrono::microseconds{0};
            timing.maxExecutionTime = std::chrono::microseconds{0};
            timing.minExecutionTime = std::chrono::microseconds::max();
            timing.avgExecutionTime = std::chrono::microseconds{0};
        }
        
        void updateExecutionTime(std::chrono::microseconds executionTime) {
            timing.totalExecutionTime += executionTime;
            timing.maxExecutionTime = std::max(timing.maxExecutionTime, executionTime);
            timing.minExecutionTime = std::min(timing.minExecutionTime, executionTime);
            timing.avgExecutionTime = std::chrono::microseconds{
                timing.totalExecutionTime.count() / std::max(uint64_t{1}, commandCallCount.load())
            };
        }
    };

    /**
     * @brief Constructs a new Component object.
     */
    explicit Component(std::string name);

    /**
     * @brief Destroys the Component object.
     */
    virtual ~Component() = default;

    // -------------------------------------------------------------------
    // Inject methods
    // -------------------------------------------------------------------

    auto getInstance() const -> std::weak_ptr<const Component>;

    auto getSharedInstance() -> std::shared_ptr<Component> {
        return shared_from_this();
    }

    // -------------------------------------------------------------------
    // Common methods
    // -------------------------------------------------------------------

    /**
     * @brief Initializes the plugin.
     *
     * @return True if the plugin was initialized successfully, false otherwise.
     * @note This function is called by the server when the plugin is loaded.
     * @note This function should be overridden by the plugin.
     */
    virtual auto initialize() -> bool;

    /**
     * @brief Destroys the plugin.
     *
     * @return True if the plugin was destroyed successfully, false otherwise.
     * @note This function is called by the server when the plugin is unloaded.
     * @note This function should be overridden by the plugin.
     * @note The plugin should not be used after this function is called.
     * @note This is for the plugin to release any resources it has allocated.
     */
    virtual auto destroy() -> bool;

    /**
     * @brief Gets the name of the plugin.
     *
     * @return The name of the plugin.
     */
    auto getName() const -> std::string;

    /**
     * @brief Gets the type information of the plugin.
     *
     * @return The type information of the plugin.
     */
    auto getTypeInfo() const -> atom::meta::TypeInfo;

    /**
     * @brief Sets the type information of the plugin.
     *
     * @param typeInfo The type information of the plugin.
     */
    void setTypeInfo(atom::meta::TypeInfo typeInfo);

    /**
     * @brief 获取组件当前状态
     * 
     * @return ComponentState 组件状态
     */
    [[nodiscard]] auto getState() const -> ComponentState;
    
    /**
     * @brief 设置组件状态
     * 
     * @param state 新的组件状态
     */
    void setState(ComponentState state);

    /**
     * @brief 获取组件性能统计信息
     * 
     * @return const PerformanceStats& 性能统计信息
     */
    [[nodiscard]] auto getPerformanceStats() const -> const PerformanceStats&;
    
    /**
     * @brief 重置性能统计信息
     */
    void resetPerformanceStats();

    // -------------------------------------------------------------------
    // Event Methods (if enabled)
    // -------------------------------------------------------------------
    
    #if ENABLE_EVENT_SYSTEM
    /**
     * @brief 发送事件
     * 
     * @param eventName 事件名称
     * @param eventData 事件数据
     */
    void emitEvent(const std::string& eventName, std::any eventData = {});
    
    /**
     * @brief 注册事件处理器
     * 
     * @param eventName 事件名称
     * @param callback 回调函数
     * @return atom::components::EventCallbackId 回调ID
     */
    atom::components::EventCallbackId on(
        const std::string& eventName, 
        atom::components::EventCallback callback);
    
    /**
     * @brief 注册一次性事件处理器
     * 
     * @param eventName 事件名称
     * @param callback 回调函数
     * @return atom::components::EventCallbackId 回调ID
     */
    atom::components::EventCallbackId once(
        const std::string& eventName,
        atom::components::EventCallback callback);
    
    /**
     * @brief 取消注册事件处理器
     * 
     * @param eventName 事件名称
     * @param callbackId 回调ID
     * @return bool 是否成功取消
     */
    bool off(const std::string& eventName, atom::components::EventCallbackId callbackId);
    
    /**
     * @brief 处理事件
     * 
     * @param event 事件对象
     */
    virtual void handleEvent(const atom::components::Event& event);
    #endif

    // -------------------------------------------------------------------
    // Variable methods
    // -------------------------------------------------------------------

    /**
     * @brief Adds a variable to the component.
     * @param name The name of the variable.
     * @param initialValue The initial value of the variable.
     * @param description The description of the variable.
     * @param alias The alias of the variable.
     * @param group The group of the variable.
     */
    template <typename T>
    void addVariable(const std::string& name, T initialValue,
                     const std::string& description = "",
                     const std::string& alias = "",
                     const std::string& group = "") {
        m_VariableManager_->addVariable(name, initialValue, description, alias,
                                        group);
    }

    /**
     * @brief Sets the range of a variable.
     * @param name The name of the variable.
     * @param min The minimum value of the variable.
     * @param max The maximum value of the variable.
     */
    template <typename T>
    void setRange(const std::string& name, T min, T max) {
        m_VariableManager_->setRange(name, min, max);
    }

    /**
     * @brief Sets the options of a variable.
     * @param name The name of the variable.
     * @param options The options of the variable.
     */
    void setStringOptions(const std::string& name,
                          const std::vector<std::string>& options) {
        m_VariableManager_->setStringOptions(name, options);
    }

    /**
     * @brief Gets a variable by name.
     * @param name The name of the variable.
     * @return A shared pointer to the variable.
     */
    template <typename T>
    auto getVariable(const std::string& name) -> std::shared_ptr<Trackable<T>> {
        return m_VariableManager_->getVariable<T>(name);
    }

    /**
     * @brief Gets a variable by name.
     * @param name The name of the variable.
     * @return A shared pointer to the variable.
     */
    [[nodiscard]] auto hasVariable(const std::string& name) const -> bool;

    /**
     * @brief Sets the value of a variable.
     * @param name The name of the variable.
     * @param newValue The new value of the variable.
     * @note const char * is not equivalent to std::string, please use
     * std::string
     */
    template <typename T>
    void setValue(const std::string& name, T newValue) {
        m_VariableManager_->setValue(name, newValue);
    }

    /**
     * @brief Gets the value of a variable.
     * @param name The name of the variable.
     * @return The value of the variable.
     */
    auto getVariableNames() const -> std::vector<std::string>;

    /**
     * @brief Gets the description of a variable.
     * @param name The name of the variable.
     * @return The description of the variable.
     */
    auto getVariableDescription(const std::string& name) const -> std::string;

    /**
     * @brief Gets the alias of a variable.
     * @param name The name of the variable.
     * @return The alias of the variable.
     */
    auto getVariableAlias(const std::string& name) const -> std::string;

    /**
     * @brief Gets the group of a variable.
     * @param name The name of the variable.
     * @return The group of the variable.
     */
    auto getVariableGroup(const std::string& name) const -> std::string;

    // -------------------------------------------------------------------
    // Function methods
    // -------------------------------------------------------------------

    void doc(const std::string& description);

    auto getDoc() const -> std::string;

    // -------------------------------------------------------------------
    // No Class
    // -------------------------------------------------------------------

    template <typename Callable>
    void def(const std::string& name, Callable&& func,
             const std::string& group = "",
             const std::string& description = "");

    template <typename Ret>
    void def(const std::string& name, Ret (*func)(),
             const std::string& group = "",
             const std::string& description = "");

    template <typename... Args, typename Ret>
    void def(const std::string& name, Ret (*func)(Args...),
             const std::string& group = "",
             const std::string& description = "");

    // -------------------------------------------------------------------
    // Without instance
    // -------------------------------------------------------------------

#define DEF_MEMBER_FUNC(cv_qualifier)                                      \
    template <typename Class, typename Ret, typename... Args>              \
    void def(                                                              \
        const std::string& name, Ret (Class::*func)(Args...) cv_qualifier, \
        const std::string& group = "", const std::string& description = "");

    DEF_MEMBER_FUNC()                // Non-const, non-volatile
    DEF_MEMBER_FUNC(const)           // Const
    DEF_MEMBER_FUNC(volatile)        // Volatile
    DEF_MEMBER_FUNC(const volatile)  // Const volatile
    DEF_MEMBER_FUNC(noexcept)
    DEF_MEMBER_FUNC(const noexcept)
    DEF_MEMBER_FUNC(const volatile noexcept)

    template <typename Class, typename VarType>
    void def(const std::string& name, VarType Class::* var,
             const std::string& group = "",
             const std::string& description = "");

    // -------------------------------------------------------------------
    // With instance
    // -------------------------------------------------------------------

    template <typename Ret, typename Class, typename InstanceType>
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
                 std::is_same_v<InstanceType, PointerSentinel<Class>>
    void def(const std::string& name, Ret (Class::*func)(),
             const InstanceType& instance, const std::string& group = "",
             const std::string& description = "");

#define DEF_MEMBER_FUNC_WITH_INSTANCE(cv_qualifier)                       \
    template <typename... Args, typename Ret, typename Class,             \
              typename InstanceType>                                      \
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||   \
                 std::is_same_v<InstanceType, PointerSentinel<Class>>     \
    void def(const std::string& name,                                     \
             Ret (Class::*func)(Args...) cv_qualifier,                    \
             const InstanceType& instance, const std::string& group = "", \
             const std::string& description = "");

    DEF_MEMBER_FUNC_WITH_INSTANCE()
    DEF_MEMBER_FUNC_WITH_INSTANCE(const)
    DEF_MEMBER_FUNC_WITH_INSTANCE(volatile)
    DEF_MEMBER_FUNC_WITH_INSTANCE(const volatile)
    DEF_MEMBER_FUNC_WITH_INSTANCE(noexcept)
    DEF_MEMBER_FUNC_WITH_INSTANCE(const noexcept)
    DEF_MEMBER_FUNC_WITH_INSTANCE(const volatile noexcept)

    template <typename MemberType, typename Class, typename InstanceType>
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
                 std::is_same_v<InstanceType, PointerSentinel<Class>>
    void def(const std::string& name, MemberType Class::* var,
             const InstanceType& instance, const std::string& group = "",
             const std::string& description = "");

    template <typename MemberType, typename Class, typename InstanceType>
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
                 std::is_same_v<InstanceType, PointerSentinel<Class>>
    void def(const std::string& name, const MemberType Class::* var,
             const InstanceType& instance, const std::string& group = "",
             const std::string& description = "");

    template <typename Ret, typename Class, typename InstanceType>
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
                 std::is_same_v<InstanceType, PointerSentinel<Class>>
    void def(const std::string& name, Ret (Class::*getter)() const,
             void (Class::*setter)(Ret), const InstanceType& instance,
             const std::string& group, const std::string& description);

    // Register a static member variable
    template <typename MemberType, typename Class>
    void def(const std::string& name, MemberType* var,
             const std::string& group = "",
             const std::string& description = "");

    // Register a const & static member variable
    template <typename MemberType, typename Class>
    void def(const std::string& name, const MemberType* var,
             const std::string& group = "",
             const std::string& description = "");

    template <typename Class>
    void def(const std::string& name, const std::string& group = "",
             const std::string& description = "");

    template <typename Class, typename... Args>
    void def(const std::string& name, const std::string& group = "",
             const std::string& description = "");

    template <typename Class, typename... Args>
    void defConstructor(const std::string& name, const std::string& group = "",
                        const std::string& description = "");

    template <typename Class>
    void defDefaultConstructor(const std::string& name,
                               const std::string& group = "",
                               const std::string& description = "");

    template <typename T>
    void defType(std::string_view name, const std::string& group = "",
                 const std::string& description = "");

    template <typename EnumType>
    void defEnum(const std::string& name,
                 const std::unordered_map<std::string, EnumType>& enumMap);

    template <typename SourceType, typename DestinationType>
    void defConversion(std::function<std::any(const std::any&)> func);

    template <typename Base, typename Derived>
    void defBaseClass();

    void defClassConversion(
        const std::shared_ptr<atom::meta::TypeConversionBase>& conversion);

    void addAlias(const std::string& name, const std::string& alias) const;

    void addGroup(const std::string& name, const std::string& group) const;

    void setTimeout(const std::string& name,
                    std::chrono::milliseconds timeout) const;

    template <typename... Args>
    auto dispatch(const std::string& name, Args&&... args) -> std::any {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try {
            auto result = m_CommandDispatcher_->dispatch(name, std::forward<Args>(args)...);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime);
            
            // 更新性能统计
            m_PerformanceStats_.commandCallCount++;
            m_PerformanceStats_.updateExecutionTime(duration);
            
            return result;
        } catch (const std::exception& e) {
            m_PerformanceStats_.commandErrorCount++;
            throw; // 重新抛出异常
        }
    }

    auto dispatch(const std::string& name,
                  const std::vector<std::any>& args) const -> std::any {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try {
            auto result = m_CommandDispatcher_->dispatch(name, args);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime);
            
            // 更新性能统计
            auto& stats = const_cast<PerformanceStats&>(m_PerformanceStats_);
            stats.commandCallCount++;
            stats.updateExecutionTime(duration);
            
            return result;
        } catch (const std::exception& e) {
            auto& stats = const_cast<PerformanceStats&>(m_PerformanceStats_);
            stats.commandErrorCount++;
            throw; // 重新抛出异常
        }
    }

    [[nodiscard]] auto has(const std::string& name) const -> bool;

    [[nodiscard]] auto hasType(std::string_view name) const -> bool;

    template <typename SourceType, typename DestinationType>
    [[nodiscard]] auto hasConversion() const -> bool;

    void removeCommand(const std::string& name) const;

    auto getCommandsInGroup(const std::string& group) const
        -> std::vector<std::string>;

    auto getCommandDescription(const std::string& name) const -> std::string;

    auto getCommandArgAndReturnType(const std::string& name)
        -> std::vector<CommandDispatcher::CommandArgRet>;

#if ENABLE_FASTHASH
    emhash::HashSet<std::string> getCommandAliases(
        const std::string& name) const;
#else
    auto getCommandAliases(const std::string& name) const
        -> std::unordered_set<std::string>;
#endif

    auto getAllCommands() const -> std::vector<std::string>;

    auto getRegisteredTypes() const -> std::vector<std::string>;

    // -------------------------------------------------------------------
    // Other Components methods
    // -------------------------------------------------------------------
    /**
     * @note This method is not thread-safe. And we must make sure the pointer
     * is valid. The PointerSentinel will help you to avoid this problem. We
     * will directly get the std::weak_ptr from the pointer.
     */

    /**
     * @brief 获取依赖的组件名称列表
     * 
     * @return std::vector<std::string> 依赖的组件名称
     * @note This will be called when the component is initialized.
     */
    static auto getNeededComponents() -> std::vector<std::string>;

    /**
     * @brief 添加对其它组件的引用
     * 
     * @param name 组件名称
     * @param component 组件实例
     */
    void addOtherComponent(const std::string& name,
                           const std::weak_ptr<Component>& component);

    /**
     * @brief 移除对其它组件的引用
     * 
     * @param name 组件名称
     */
    void removeOtherComponent(const std::string& name);

    /**
     * @brief 清空所有组件引用
     */
    void clearOtherComponents();

    /**
     * @brief 获取其它组件实例
     * 
     * @param name 组件名称
     * @return std::weak_ptr<Component> 组件实例的弱引用
     */
    auto getOtherComponent(const std::string& name) -> std::weak_ptr<Component>;

    /**
     * @brief 执行命令
     * 
     * 会先在本组件中查找命令，如果找不到，则尝试在依赖组件中查找
     * 
     * @param name 命令名称
     * @param args 命令参数
     * @return std::any 命令执行结果
     */
    auto runCommand(const std::string& name, const std::vector<std::any>& args)
        -> std::any;

    /**
     * @brief 初始化函数，由注册器调用
     */
    InitFunc initFunc; 
    
    /**
     * @brief 清理函数，由注册器调用
     */
    CleanupFunc cleanupFunc; 

private:
    std::string m_name_;               // 组件名称
    std::string m_doc_;                // 组件文档
    std::string m_configPath_;         // 配置路径
    std::string m_infoPath_;           // 信息路径
    atom::meta::TypeInfo m_typeInfo_{atom::meta::userType<Component>()};
    std::unordered_map<std::string_view, atom::meta::TypeInfo> m_classes_;
    
    std::atomic<ComponentState> m_state_{ComponentState::Created}; // 组件状态
    PerformanceStats m_PerformanceStats_; // 性能统计

    ///< managing commands.
    std::shared_ptr<VariableManager> m_VariableManager_{
        std::make_shared<VariableManager>()};  ///< 变量管理器

    std::unordered_map<std::string, std::weak_ptr<Component>>
        m_OtherComponents_; // 其他组件引用

    std::shared_ptr<atom::meta::TypeCaster> m_TypeCaster_{
        atom::meta::TypeCaster::createShared()};
    std::shared_ptr<atom::meta::TypeConversions> m_TypeConverter_{
        atom::meta::TypeConversions::createShared()};

    std::shared_ptr<CommandDispatcher> m_CommandDispatcher_{
        std::make_shared<CommandDispatcher>(
            m_TypeCaster_)};  ///< 命令调度器
    
    #if ENABLE_EVENT_SYSTEM
    struct EventHandler {
        atom::components::EventCallbackId id; // 处理器ID
        atom::components::EventCallback callback; // 回调函数
        bool once; // 是否一次性
    };
    
    std::unordered_map<std::string, std::vector<EventHandler>> m_EventHandlers_; // 事件处理器
    std::mutex m_EventMutex_; // 事件处理互斥锁
    std::atomic<atom::components::EventCallbackId> m_NextEventId_{1}; // 下一个事件ID
    #endif
};

template <typename SourceType, typename DestinationType>
auto Component::hasConversion() const -> bool {
    if constexpr (std::is_same_v<SourceType, DestinationType>) {
        return true;
    }
    return m_TypeConverter_->canConvert(
        atom::meta::userType<SourceType>(),
        atom::meta::userType<DestinationType>());
}

template <typename T>
void Component::defType(std::string_view name,
                        [[maybe_unused]] const std::string& group,
                        [[maybe_unused]] const std::string& description) {
    m_classes_[name] = atom::meta::userType<T>();
    m_TypeCaster_->registerType<T>(std::string(name));
}

template <typename SourceType, typename DestinationType>
void Component::defConversion(std::function<std::any(const std::any&)> func) {
    static_assert(!std::is_same_v<SourceType, DestinationType>,
                  "SourceType and DestinationType must be not the same");
    m_TypeCaster_->registerConversion<SourceType, DestinationType>(func);
}

template <typename Base, typename Derived>
void Component::defBaseClass() {
    static_assert(std::is_base_of_v<Base, Derived>,
                  "Derived must be derived from Base");
    m_TypeConverter_->addBaseClass<Base, Derived>();
}

template <typename Callable>
void Component::def(const std::string& name, Callable&& func,
                    const std::string& group, const std::string& description) {
    using Traits = atom::meta::FunctionTraits<std::decay_t<Callable>>;
    using ReturnType = typename Traits::return_type;
    static_assert(Traits::arity <= 8, "Too many arguments");
    // clang-format off
    #include "component.template"
    // clang-format on
}

template <typename Ret>
void Component::def(const std::string& name, Ret (*func)(),
                    const std::string& group, const std::string& description) {
    m_CommandDispatcher_->def(name, group, description,
                              std::function<Ret()>(func));
}

template <typename... Args, typename Ret>
void Component::def(const std::string& name, Ret (*func)(Args...),
                    const std::string& group, const std::string& description) {
    m_CommandDispatcher_->def(name, group, description,
                              std::function<Ret(Args...)>([func](Args... args) {
                                  return func(std::forward<Args>(args)...);
                              }));
}

#define DEF_MEMBER_FUNC_IMPL(cv_qualifier)                                   \
    template <typename Class, typename Ret, typename... Args>                \
    void Component::def(                                                     \
        const std::string& name, Ret (Class::*func)(Args...) cv_qualifier,   \
        const std::string& group, const std::string& description) {          \
        auto boundFunc = atom::meta::bindMemberFunction(func);               \
        m_CommandDispatcher_->def(                                           \
            name, group, description,                                        \
            std::function<Ret(std::reference_wrapper<Class>, Args...)>(      \
                [boundFunc](std::reference_wrapper<Class> instance,          \
                            Args... args) -> Ret {                           \
                    return boundFunc(instance, std::forward<Args>(args)...); \
                }));                                                         \
    }

DEF_MEMBER_FUNC_IMPL()
DEF_MEMBER_FUNC_IMPL(const)
DEF_MEMBER_FUNC_IMPL(volatile)
DEF_MEMBER_FUNC_IMPL(const volatile)
DEF_MEMBER_FUNC_IMPL(noexcept)
DEF_MEMBER_FUNC_IMPL(const noexcept)
DEF_MEMBER_FUNC_IMPL(const volatile noexcept)

template <typename Ret, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(const std::string& name, Ret (Class::*func)(),
                    const InstanceType& instance, const std::string& group,
                    const std::string& description) {
    m_CommandDispatcher_->def(name, group, description,
                              std::function<Ret()>([instance, func]() {
                                  return std::invoke(func, instance.get());
                              }));
}

template <typename... Args, typename Ret, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(const std::string& name, Ret (Class::*func)(Args...),
                    const InstanceType& instance, const std::string& group,
                    const std::string& description) {
    m_CommandDispatcher_->def(
        name, group, description,
        std::function<Ret(Args...)>([instance, func](Args... args) {
            return std::invoke(func, instance.get(),
                               std::forward<Args>(args)...);
        }));
}

template <typename... Args, typename Ret, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(const std::string& name, Ret (Class::*func)(Args...) const,
                    const InstanceType& instance, const std::string& group,
                    const std::string& description) {
    if constexpr (std::is_same_v<InstanceType, std::unique_ptr<Class>>) {
        m_CommandDispatcher_->def(
            name, group, description,
            std::function<Ret(Args...)>([&instance, func](Args... args) {
                return std::invoke(func, instance.get(),
                                   std::forward<Args>(args)...);
            }));

    } else if constexpr (SmartPointer<InstanceType> ||
                         std::is_same_v<InstanceType, PointerSentinel<Class>>) {
        m_CommandDispatcher_->def(
            name, group, description,
            std::function<Ret(Args...)>([instance, func](Args... args) {
                return std::invoke(func, instance.get(),
                                   std::forward<Args>(args)...);
            }));
    } else {
        m_CommandDispatcher_->def(
            name, group, description,
            std::function<Ret(Args...)>([instance, func](Args... args) {
                return std::invoke(func, instance, std::forward<Args>(args)...);
            }));
    }
}

template <typename... Args, typename Ret, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(const std::string& name,
                    Ret (Class::*func)(Args...) noexcept,
                    const InstanceType& instance, const std::string& group,
                    const std::string& description) {
    m_CommandDispatcher_->def(
        name, group, description,
        std::function<Ret(Args...)>([instance, func](Args... args) {
            return std::invoke(func, instance.get(),
                               std::forward<Args>(args)...);
        }));
}

template <typename... Args, typename Ret, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(const std::string& name,
                    Ret (Class::*func)(Args...) const noexcept,
                    const InstanceType& instance, const std::string& group,
                    const std::string& description) {
    m_CommandDispatcher_->def(
        name, group, description,
        std::function<Ret(Args...)>([instance, func](Args... args) {
            return std::invoke(func, instance.get(),
                               std::forward<Args>(args)...);
        }));
}

template <typename MemberType, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(const std::string& name, MemberType Class::* member_var,
                    const InstanceType& instance, const std::string& group,
                    const std::string& description) {
    m_CommandDispatcher_->def(
        "get_" + name, group, "Get " + description,
        std::function<MemberType()>([instance, member_var]() {
            return atom::meta::bindMemberVariable(member_var)(*instance);
        }));
    m_CommandDispatcher_->def(
        "set_" + name, group, "Set " + description,
        std::function<void(MemberType)>(
            [instance, member_var](MemberType value) {
                atom::meta::bindMemberVariable(member_var)(*instance) = value;
            }));
}

template <typename MemberType, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(const std::string& name,
                    const MemberType Class::* member_var,
                    const InstanceType& instance, const std::string& group,
                    const std::string& description) {
    m_CommandDispatcher_->def(
        "get_" + name, group, "Get " + description,
        std::function<MemberType()>([instance, member_var]() {
            return atom::meta::bindMemberVariable(member_var)(*instance);
        }));
}

template <typename Ret, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(const std::string& name, Ret (Class::*getter)() const,
                    void (Class::*setter)(Ret), const InstanceType& instance,
                    const std::string& group, const std::string& description) {
    m_CommandDispatcher_->def("get_" + name, group, "Get " + description,
                              std::function<Ret()>([instance, getter]() {
                                  return std::invoke(getter, instance.get());
                              }));
    m_CommandDispatcher_->def(
        "set_" + name, group, "Set " + description,
        std::function<void(Ret)>([instance, setter](Ret value) {
            std::invoke(setter, instance.get(), value);
        }));
}

template <typename MemberType, typename ClassType>
void Component::def(const std::string& name, MemberType* member_var,
                    const std::string& group, const std::string& description) {
    m_CommandDispatcher_->def(
        name, group, description,
        std::function<MemberType&()>(
            [member_var]() -> MemberType& { return *member_var; }));
}

template <typename MemberType, typename ClassType>
void Component::def(const std::string& name, const MemberType* member_var,
                    const std::string& group, const std::string& description) {
    m_CommandDispatcher_->def(
        name, group, description,
        std::function<const MemberType&()>(
            [member_var]() -> const MemberType& { return *member_var; }));
}

template <typename Class, typename... Args>
void Component::defConstructor(const std::string& name,
                               const std::string& group,
                               const std::string& description) {
    m_CommandDispatcher_->def(name, group, description,
                              std::function<std::shared_ptr<Class>(Args...)>(
                                  atom::meta::constructor<Class, Args...>()));
}

template <typename EnumType>
void Component::defEnum(
    const std::string& name,
    const std::unordered_map<std::string, EnumType>& enumMap) {
    m_TypeCaster_->registerType<EnumType>(std::string(name));

    for (const auto& [key, value] : enumMap) {
        m_TypeCaster_->registerEnumValue<EnumType>(name, key, value);
    }

    defConversion<EnumType, std::string>(
        [this, name](const std::any& enumValue) -> std::any {
            const EnumType& value = std::any_cast<EnumType>(enumValue);
            return m_TypeCaster_->enumToString<EnumType>(value, name);
        });

    defConversion<std::string, EnumType>(
        [this, name](const std::any& strValue) -> std::any {
            const std::string& value = std::any_cast<std::string>(strValue);
            return m_TypeCaster_->stringToEnum<EnumType>(value, name);
        });
}

template <typename Class>
void Component::defDefaultConstructor(const std::string& name,
                                      const std::string& group,
                                      const std::string& description) {
    m_CommandDispatcher_->def(
        name, group, description,
        std::function<std::shared_ptr<Class>()>([]() -> std::shared_ptr<Class> {
            return std::make_shared<Class>();
        }));
}

template <typename Class>
void Component::def(const std::string& name, const std::string& group,
                    const std::string& description) {
    auto constructor = atom::meta::defaultConstructor<Class>();
    def(name, constructor, group, description);
}

template <typename Class, typename... Args>
void Component::def(const std::string& name, const std::string& group,
                    const std::string& description) {
    auto constructor_ = atom::meta::constructor<Class, Args...>();
    def(name, constructor_, group, description);
}

#endif // ATOM_COMPONENT_HPP
