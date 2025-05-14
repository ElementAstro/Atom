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

#include "config.hpp"
#include "dispatch.hpp"
#include "module_macro.hpp"
#include "var.hpp"

#include "atom/log/loguru.hpp"
#include "atom/meta/concept.hpp"
#include "atom/meta/constructor.hpp"
#include "atom/meta/conversion.hpp"
#include "atom/meta/func_traits.hpp"
#include "atom/meta/type_caster.hpp"
#include "atom/meta/type_info.hpp"
#include "atom/type/pointer.hpp"

#include <chrono>
#include <concepts>
#include <memory>
#include <shared_mutex>
#include <span>

// Component Lifecycle Exception
class ObjectExpiredError final : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_OBJECT_EXPIRED(...)                                            \
    throw ObjectExpiredError(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                             __VA_ARGS__)

/**
 * @brief Component lifecycle state
 */
enum class ComponentState : uint8_t {
    Created,       // Created but not initialized
    Initializing,  // Initializing
    Active,        // Initialized and active
    Disabled,      // Disabled
    Error,         // Error occurred
    Destroying     // Destroying
};

/**
 * @brief Base class for components, providing the basic infrastructure for
 * component services.
 *
 * The Component class is the base class for all components, providing features
 * such as component registration, command dispatching, and event handling.
 *
 * @note This class uses the CRTP pattern to implement static polymorphism,
 * improving runtime performance.
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
     * @brief Performance statistics structure
     */
    struct PerformanceStats {
        std::atomic<uint64_t> commandCallCount{0};   // Command call count
        std::atomic<uint64_t> commandErrorCount{0};  // Command error count
        std::atomic<uint64_t> eventCount{0};         // Event handling count

        struct {
            std::chrono::microseconds totalExecutionTime{
                0};  // Total execution time
            std::chrono::microseconds maxExecutionTime{
                0};  // Maximum execution time
            std::chrono::microseconds minExecutionTime{
                std::chrono::microseconds::max()};  // Minimum execution time
            std::chrono::microseconds avgExecutionTime{
                0};  // Average execution time
        } timing;

        // Reset performance counters
        constexpr void reset() noexcept {
            commandCallCount = 0;
            commandErrorCount = 0;
            eventCount = 0;
            timing.totalExecutionTime = std::chrono::microseconds{0};
            timing.maxExecutionTime = std::chrono::microseconds{0};
            timing.minExecutionTime = std::chrono::microseconds::max();
            timing.avgExecutionTime = std::chrono::microseconds{0};
        }

        // Update execution time statistics
        void updateExecutionTime(
            std::chrono::microseconds executionTime) noexcept {
            timing.totalExecutionTime += executionTime;
            timing.maxExecutionTime =
                std::max(timing.maxExecutionTime, executionTime);
            timing.minExecutionTime =
                std::min(timing.minExecutionTime, executionTime);
            timing.avgExecutionTime = std::chrono::microseconds{
                timing.totalExecutionTime.count() /
                std::max(uint64_t{1},
                         commandCallCount.load(std::memory_order_relaxed))};
        }
    };

    /**
     * @brief Constructs a new Component object.
     * @param name Component name
     * @throw std::invalid_argument When the name is empty
     */
    explicit Component(std::string name);

    /**
     * @brief Destroys the Component object.
     */
    virtual ~Component() noexcept = default;

    // Disable copy and move
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;

    // -------------------------------------------------------------------
    // Inject methods
    // -------------------------------------------------------------------

    /**
     * @brief Gets a const weak reference to the component instance.
     * @return A const weak reference to the component instance.
     */
    [[nodiscard]] auto getInstance() const -> std::weak_ptr<const Component>;

    /**
     * @brief Gets a shared pointer to the component instance.
     * @return A shared pointer to the component instance.
     */
    [[nodiscard]] auto getSharedInstance() -> std::shared_ptr<Component> {
        return this->shared_from_this();
    }

    // -------------------------------------------------------------------
    // Common methods
    // -------------------------------------------------------------------

    /**
     * @brief Initializes the component.
     *
     * @return Returns true if initialization is successful, false otherwise.
     * @note Derived classes can override this method to implement custom
     * initialization logic.
     */
    virtual auto initialize() -> bool;

    /**
     * @brief Destroys the component.
     *
     * @return Returns true if destruction is successful, false otherwise.
     * @note Derived classes can override this method to implement custom
     * destruction logic.
     */
    virtual auto destroy() -> bool;

    /**
     * @brief Gets the component name.
     * @return The component name.
     */
    [[nodiscard]] auto getName() const noexcept -> std::string_view;

    /**
     * @brief Gets the component type information.
     * @return The component type information.
     */
    [[nodiscard]] auto getTypeInfo() const noexcept -> atom::meta::TypeInfo;

    /**
     * @brief Sets the component type information.
     * @param typeInfo New type information.
     */
    void setTypeInfo(atom::meta::TypeInfo typeInfo) noexcept;

    /**
     * @brief Gets the current state of the component.
     * @return The component state.
     */
    [[nodiscard]] auto getState() const noexcept -> ComponentState;

    /**
     * @brief Sets the component state.
     * @param state New component state.
     */
    void setState(ComponentState state) noexcept;

    /**
     * @brief Gets the component performance statistics.
     * @return Const reference to performance statistics.
     */
    [[nodiscard]] auto getPerformanceStats() const noexcept
        -> const PerformanceStats&;

    /**
     * @brief Resets the performance statistics.
     */
    void resetPerformanceStats() noexcept;

    // -------------------------------------------------------------------
    // Event Methods (if enabled)
    // -------------------------------------------------------------------

#if ENABLE_EVENT_SYSTEM
    /**
     * @brief Emits an event.
     *
     * @param eventName Event name.
     * @param eventData Event data, empty by default.
     */
    void emitEvent(std::string_view eventName, std::any eventData = {});

    /**
     * @brief Registers an event handler.
     *
     * @param eventName Event name.
     * @param callback Callback function.
     * @return Callback ID.
     */
    [[nodiscard]] atom::components::EventCallbackId on(
        std::string_view eventName, atom::components::EventCallback callback);

    /**
     * @brief Registers a one-time event handler.
     *
     * @param eventName Event name.
     * @param callback Callback function.
     * @return Callback ID.
     */
    [[nodiscard]] atom::components::EventCallbackId once(
        std::string_view eventName, atom::components::EventCallback callback);

    /**
     * @brief Unregisters an event handler.
     *
     * @param eventName Event name.
     * @param callbackId Callback ID.
     * @return Whether unregistration was successful.
     */
    bool off(std::string_view eventName,
             atom::components::EventCallbackId callbackId);

    /**
     * @brief Handles an event.
     *
     * @param event Event object.
     */
    virtual void handleEvent(const atom::components::Event& event);
#endif

    // -------------------------------------------------------------------
    // Variable methods
    // -------------------------------------------------------------------

    /**
     * @brief Adds a variable to the component.
     * @tparam T Variable type.
     * @param name Variable name.
     * @param initialValue Initial value.
     * @param description Variable description, empty by default.
     * @param alias Variable alias, empty by default.
     * @param group Variable group, empty by default.
     * @throws std::invalid_argument If the name is empty.
     */
    template <typename T>
        requires std::is_copy_constructible_v<T>
    void addVariable(std::string_view name, T initialValue,
                     std::string_view description = "",
                     std::string_view alias = "", std::string_view group = "") {
        m_VariableManager_->addVariable(
            std::string(name), std::move(initialValue),
            std::string(description), std::string(alias), std::string(group));
    }

    /**
     * @brief Sets the range for a variable.
     * @tparam T Variable type.
     * @param name Variable name.
     * @param min Minimum value.
     * @param max Maximum value.
     * @throws std::out_of_range If the variable does not exist.
     */
    template <Arithmetic T>
    void setRange(std::string_view name, T min, T max) {
        m_VariableManager_->setRange(std::string(name), min, max);
    }

    /**
     * @brief Sets the allowed options for a string variable.
     * @param name Variable name.
     * @param options List of allowed options.
     * @throws std::out_of_range If the variable does not exist.
     */
    void setStringOptions(std::string_view name,
                          std::span<const std::string> options) {
        m_VariableManager_->setStringOptions(std::string(name), options);
    }

    /**
     * @brief Gets a variable.
     * @tparam T Variable type.
     * @param name Variable name.
     * @return Shared pointer to the variable.
     * @throws std::out_of_range If the variable does not exist.
     * @throws VariableTypeError If the variable type does not match.
     */
    template <typename T>
    [[nodiscard]] auto getVariable(std::string_view name)
        -> std::shared_ptr<Trackable<T>> {
        return m_VariableManager_->getVariable<T>(std::string(name));
    }

    /**
     * @brief Checks if a variable exists.
     * @param name Variable name.
     * @return Returns true if the variable exists, false otherwise.
     */
    [[nodiscard]] auto hasVariable(std::string_view name) const noexcept
        -> bool;

    /**
     * @brief Sets the value of a variable.
     * @tparam T Variable type.
     * @param name Variable name.
     * @param newValue New value.
     * @throws std::out_of_range If the variable does not exist.
     * @throws VariableTypeError If the variable type does not match.
     */
    template <typename T>
    void setValue(std::string_view name, T newValue) {
        m_VariableManager_->setValue(std::string(name), std::move(newValue));
    }

    /**
     * @brief Gets all variable names.
     * @return List of variable names.
     */
    [[nodiscard]] auto getVariableNames() const -> std::vector<std::string>;

    /**
     * @brief Gets the variable description.
     * @param name Variable name.
     * @return The variable description.
     * @throws std::out_of_range If the variable does not exist.
     */
    [[nodiscard]] auto getVariableDescription(std::string_view name) const
        -> std::string;

    /**
     * @brief Gets the variable alias.
     * @param name Variable name.
     * @return The variable alias.
     * @throws std::out_of_range If the variable does not exist.
     */
    [[nodiscard]] auto getVariableAlias(std::string_view name) const
        -> std::string;

    /**
     * @brief Gets the variable group.
     * @param name Variable name.
     * @return The variable group.
     * @throws std::out_of_range If the variable does not exist.
     */
    [[nodiscard]] auto getVariableGroup(std::string_view name) const
        -> std::string;

    // -------------------------------------------------------------------
    // Function methods
    // -------------------------------------------------------------------

    /**
     * @brief Sets the component documentation.
     * @param description Documentation description.
     */
    void doc(std::string_view description);

    /**
     * @brief Gets the component documentation.
     * @return The component documentation.
     */
    [[nodiscard]] auto getDoc() const noexcept -> std::string_view;

    // -------------------------------------------------------------------
    // No Class
    // -------------------------------------------------------------------

    /**
     * @brief Registers a callable object.
     * @tparam Callable Callable object type.
     * @param name Command name.
     * @param func Callable object.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename Callable>
    void def(std::string_view name, Callable&& func,
             std::string_view group = "", std::string_view description = "");

    /**
     * @brief Registers a function with no arguments.
     * @tparam Ret Return type.
     * @param name Command name.
     * @param func Function pointer.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename Ret>
    void def(std::string_view name, Ret (*func)(), std::string_view group = "",
             std::string_view description = "");

    /**
     * @brief Registers a function with arguments.
     * @tparam Args Argument types.
     * @tparam Ret Return type.
     * @param name Command name.
     * @param func Function pointer.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename... Args, typename Ret>
    void def(std::string_view name, Ret (*func)(Args...),
             std::string_view group = "", std::string_view description = "");

    // -------------------------------------------------------------------
    // Without instance
    // -------------------------------------------------------------------

// Define member function using macro
#define DEF_MEMBER_FUNC(cv_qualifier)                                         \
    template <typename Class, typename Ret, typename... Args>                 \
    void def(std::string_view name, Ret (Class::*func)(Args...) cv_qualifier, \
             std::string_view group = "", std::string_view description = "");

    DEF_MEMBER_FUNC()                // Non-const, non-volatile
    DEF_MEMBER_FUNC(const)           // Const
    DEF_MEMBER_FUNC(volatile)        // Volatile
    DEF_MEMBER_FUNC(const volatile)  // Const volatile
    DEF_MEMBER_FUNC(noexcept)
    DEF_MEMBER_FUNC(const noexcept)
    DEF_MEMBER_FUNC(const volatile noexcept)

    /**
     * @brief Registers a member variable.
     * @tparam Class Class type.
     * @tparam VarType Variable type.
     * @param name Command name.
     * @param var Pointer to member variable.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename Class, typename VarType>
    void def(std::string_view name, VarType Class::* var,
             std::string_view group = "", std::string_view description = "");

    // -------------------------------------------------------------------
    // With instance
    // -------------------------------------------------------------------

    /**
     * @brief Registers a member function with an instance and no arguments.
     * @tparam Ret Return type.
     * @tparam Class Class type.
     * @tparam InstanceType Instance type.
     * @param name Command name.
     * @param func Member function pointer.
     * @param instance Class instance.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename Ret, typename Class, typename InstanceType>
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
                 std::is_same_v<InstanceType, PointerSentinel<Class>>
    void def(std::string_view name, Ret (Class::*func)(),
             const InstanceType& instance, std::string_view group = "",
             std::string_view description = "");

// Define member function with instance using macro
#define DEF_MEMBER_FUNC_WITH_INSTANCE(cv_qualifier)                           \
    template <typename... Args, typename Ret, typename Class,                 \
              typename InstanceType>                                          \
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||       \
                 std::is_same_v<InstanceType, PointerSentinel<Class>>         \
    void def(std::string_view name, Ret (Class::*func)(Args...) cv_qualifier, \
             const InstanceType& instance, std::string_view group = "",       \
             std::string_view description = "");

    DEF_MEMBER_FUNC_WITH_INSTANCE()
    DEF_MEMBER_FUNC_WITH_INSTANCE(const)
    DEF_MEMBER_FUNC_WITH_INSTANCE(volatile)
    DEF_MEMBER_FUNC_WITH_INSTANCE(const volatile)
    DEF_MEMBER_FUNC_WITH_INSTANCE(noexcept)
    DEF_MEMBER_FUNC_WITH_INSTANCE(const noexcept)
    DEF_MEMBER_FUNC_WITH_INSTANCE(const volatile noexcept)

    /**
     * @brief Registers a member variable with an instance.
     * @tparam MemberType Member variable type.
     * @tparam Class Class type.
     * @tparam InstanceType Instance type.
     * @param name Command name.
     * @param var Member variable pointer.
     * @param instance Class instance.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename MemberType, typename Class, typename InstanceType>
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
                 std::is_same_v<InstanceType, PointerSentinel<Class>>
    void def(std::string_view name, MemberType Class::* var,
             const InstanceType& instance, std::string_view group = "",
             std::string_view description = "");

    /**
     * @brief Registers a const member variable with an instance.
     * @tparam MemberType Member variable type.
     * @tparam Class Class type.
     * @tparam InstanceType Instance type.
     * @param name Command name.
     * @param var Pointer to const member variable.
     * @param instance Class instance.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename MemberType, typename Class, typename InstanceType>
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
                 std::is_same_v<InstanceType, PointerSentinel<Class>>
    void def(std::string_view name, const MemberType Class::* var,
             const InstanceType& instance, std::string_view group = "",
             std::string_view description = "");

    /**
     * @brief Registers a property with a getter and setter.
     * @tparam Ret Return type.
     * @tparam Class Class type.
     * @tparam InstanceType Instance type.
     * @param name Command name.
     * @param getter Getter function pointer.
     * @param setter Setter function pointer.
     * @param instance Class instance.
     * @param group Command group.
     * @param description Command description.
     */
    template <typename Ret, typename Class, typename InstanceType>
        requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
                 std::is_same_v<InstanceType, PointerSentinel<Class>>
    void def(std::string_view name, Ret (Class::*getter)() const,
             void (Class::*setter)(Ret), const InstanceType& instance,
             std::string_view group, std::string_view description);

    /**
     * @brief Registers a static member variable.
     * @tparam MemberType Member variable type.
     * @tparam Class Class type.
     * @param name Command name.
     * @param var Pointer to static member variable.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename MemberType, typename Class>
    void def(std::string_view name, MemberType* var,
             std::string_view group = "", std::string_view description = "");

    /**
     * @brief Registers a const static member variable.
     * @tparam MemberType Member variable type.
     * @tparam Class Class type.
     * @param name Command name.
     * @param var Pointer to const static member variable.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename MemberType, typename Class>
    void def(std::string_view name, const MemberType* var,
             std::string_view group = "", std::string_view description = "");

    /**
     * @brief Registers a type constructor.
     * @tparam Class Class type.
     * @param name Command name.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename Class>
    void def(std::string_view name, std::string_view group = "",
             std::string_view description = "");

    /**
     * @brief Registers a type constructor with arguments.
     * @tparam Class Class type.
     * @tparam Args Constructor argument types.
     * @param name Command name.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename Class, typename... Args>
    void def(std::string_view name, std::string_view group = "",
             std::string_view description = "");

    /**
     * @brief Registers a constructor.
     * @tparam Class Class type.
     * @tparam Args Constructor argument types.
     * @param name Command name.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename Class, typename... Args>
    void defConstructor(std::string_view name, std::string_view group = "",
                        std::string_view description = "");

    /**
     * @brief Registers a default constructor.
     * @tparam Class Class type.
     * @param name Command name.
     * @param group Command group, empty by default.
     * @param description Command description, empty by default.
     */
    template <typename Class>
    void defDefaultConstructor(std::string_view name,
                               std::string_view group = "",
                               std::string_view description = "");

    /**
     * @brief Registers a type.
     * @tparam T Type.
     * @param name Type name.
     * @param group Type group, empty by default.
     * @param description Type description, empty by default.
     */
    template <typename T>
    void defType(std::string_view name, std::string_view group = "",
                 std::string_view description = "");

    /**
     * @brief Registers an enum type.
     * @tparam EnumType Enum type.
     * @param name Enum name.
     * @param enumMap Enum value map.
     */
    template <typename EnumType>
    void defEnum(std::string_view name,
                 const std::unordered_map<std::string, EnumType>& enumMap);

    /**
     * @brief Registers a type conversion.
     * @tparam SourceType Source type.
     * @tparam DestinationType Destination type.
     * @param func Conversion function.
     */
    template <typename SourceType, typename DestinationType>
    void defConversion(std::function<std::any(const std::any&)> func);

    /**
     * @brief Registers a base class.
     * @tparam Base Base class type.
     * @tparam Derived Derived class type.
     */
    template <typename Base, typename Derived>
        requires std::derived_from<Derived, Base>
    void defBaseClass();

    /**
     * @brief Registers a type conversion.
     * @param conversion Type conversion base class.
     */
    void defClassConversion(
        const std::shared_ptr<atom::meta::TypeConversionBase>& conversion);

    /**
     * @brief Adds a command alias.
     * @param name Command name.
     * @param alias Command alias.
     */
    void addAlias(std::string_view name, std::string_view alias) const;

    /**
     * @brief Adds a command group.
     * @param name Command name.
     * @param group Command group.
     */
    void addGroup(std::string_view name, std::string_view group) const;

    /**
     * @brief Sets command timeout.
     * @param name Command name.
     * @param timeout Timeout duration.
     */
    void setTimeout(std::string_view name,
                    std::chrono::milliseconds timeout) const;

    /**
     * @brief Dispatches a command.
     * @tparam Args Argument types.
     * @param name Command name.
     * @param args Command arguments.
     * @return Command execution result.
     * @throws Exceptions during execution.
     */
    template <typename... Args>
    auto dispatch(std::string_view name, Args&&... args) -> std::any {
        auto startTime = std::chrono::high_resolution_clock::now();

        try {
            auto result = m_CommandDispatcher_->dispatch(
                std::string(name), std::forward<Args>(args)...);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime);

            // Update performance statistics
            m_PerformanceStats_.commandCallCount.fetch_add(
                1, std::memory_order_relaxed);
            m_PerformanceStats_.updateExecutionTime(duration);

            return result;
        } catch (const std::exception& e) {
            m_PerformanceStats_.commandErrorCount.fetch_add(
                1, std::memory_order_relaxed);
            throw;  // Rethrow exception
        }
    }

    /**
     * @brief Dispatches a command.
     * @param name Command name.
     * @param args List of command arguments.
     * @return Command execution result.
     * @throws Exceptions during execution.
     */
    [[nodiscard]] auto dispatch(std::string_view name,
                                std::span<const std::any> args) const
        -> std::any;

    /**
     * @brief Checks if a command exists.
     * @param name Command name.
     * @return Returns true if the command exists, false otherwise.
     */
    [[nodiscard]] auto has(std::string_view name) const noexcept -> bool;

    /**
     * @brief Checks if a type exists.
     * @param name Type name.
     * @return Returns true if the type exists, false otherwise.
     */
    [[nodiscard]] auto hasType(std::string_view name) const noexcept -> bool;

    /**
     * @brief Checks if a type conversion exists.
     * @tparam SourceType Source type.
     * @tparam DestinationType Destination type.
     * @return Returns true if the conversion exists, false otherwise.
     */
    template <typename SourceType, typename DestinationType>
    [[nodiscard]] auto hasConversion() const noexcept -> bool;

    /**
     * @brief Removes a command.
     * @param name Command name.
     */
    void removeCommand(std::string_view name) const;

    /**
     * @brief Gets all commands in a specified group.
     * @param group Command group.
     * @return List of command names.
     */
    [[nodiscard]] auto getCommandsInGroup(std::string_view group) const
        -> std::vector<std::string>;

    /**
     * @brief Gets the command description.
     * @param name Command name.
     * @return The command description.
     */
    [[nodiscard]] auto getCommandDescription(std::string_view name) const
        -> std::string;

    /**
     * @brief Gets command aliases.
     * @param name Command name.
     * @return Set of command aliases.
     */
#if ENABLE_FASTHASH
    [[nodiscard]] emhash::HashSet<std::string> getCommandAliases(
        std::string_view name) const;
#else
    [[nodiscard]] auto getCommandAliases(std::string_view name) const
        -> std::unordered_set<std::string>;
#endif

    /**
     * @brief Gets command argument and return types.
     * @param name Command name.
     * @return List of command argument and return types.
     */
    [[nodiscard]] auto getCommandArgAndReturnType(std::string_view name)
        -> std::vector<CommandDispatcher::CommandArgRet>;

    /**
     * @brief Gets all commands.
     * @return List of command names.
     */
    [[nodiscard]] auto getAllCommands() const -> std::vector<std::string>;

    /**
     * @brief Gets all registered types.
     * @return List of type names.
     */
    [[nodiscard]] auto getRegisteredTypes() const -> std::vector<std::string>;

    // -------------------------------------------------------------------
    // Other Components methods
    // -------------------------------------------------------------------

    /**
     * @brief Gets the list of needed component names.
     * @return List of needed component names.
     * @note This method will be called during component initialization.
     */
    [[nodiscard]] static auto getNeededComponents() -> std::vector<std::string>;

    /**
     * @brief Adds a reference to another component.
     * @param name Component name.
     * @param component Component instance.
     * @throws std::invalid_argument When the name is empty or the component has
     * expired.
     */
    void addOtherComponent(std::string_view name,
                           const std::weak_ptr<Component>& component);

    /**
     * @brief Removes a reference to another component.
     * @param name Component name.
     */
    void removeOtherComponent(std::string_view name) noexcept;

    /**
     * @brief Clears all component references.
     */
    void clearOtherComponents() noexcept;

    /**
     * @brief Gets another component instance.
     * @param name Component name.
     * @return Weak reference to the component instance.
     * @throws ObjectExpiredError When the component has expired.
     */
    [[nodiscard]] auto getOtherComponent(std::string_view name)
        -> std::weak_ptr<Component>;

    /**
     * @brief Executes a command.
     *
     * It first searches for the command in this component. If not found, it
     * tries to find it in dependent components.
     *
     * @param name Command name.
     * @param args Command arguments.
     * @return Command execution result.
     * @throws atom::error::Exception When the command is not found or execution
     * fails.
     */
    [[nodiscard]] auto runCommand(std::string_view name,
                                  std::span<const std::any> args) -> std::any;

    /**
     * @brief Initialization function, called by the registrar.
     */
    InitFunc initFunc;

    /**
     * @brief Cleanup function, called by the registrar.
     */
    CleanupFunc cleanupFunc;

private:
    std::string m_name_;        // Component name
    std::string m_doc_;         // Component documentation
    std::string m_configPath_;  // Configuration path
    std::string m_infoPath_;    // Information path
    atom::meta::TypeInfo m_typeInfo_{atom::meta::userType<Component>()};
    std::unordered_map<std::string_view, atom::meta::TypeInfo> m_classes_;

    std::atomic<ComponentState> m_state_{
        ComponentState::Created};          // Component state
    PerformanceStats m_PerformanceStats_;  // Performance statistics

    ///< managing commands.
    std::shared_ptr<VariableManager> m_VariableManager_{
        std::make_shared<VariableManager>()};  ///< Variable manager

    std::unordered_map<std::string, std::weak_ptr<Component>>
        m_OtherComponents_;                        // Other component references
    mutable std::shared_mutex m_ComponentsMutex_;  // Component reference mutex

    std::shared_ptr<atom::meta::TypeCaster> m_TypeCaster_{
        atom::meta::TypeCaster::createShared()};
    std::shared_ptr<atom::meta::TypeConversions> m_TypeConverter_{
        atom::meta::TypeConversions::createShared()};

    std::shared_ptr<CommandDispatcher> m_CommandDispatcher_{
        std::make_shared<CommandDispatcher>(
            m_TypeCaster_)};  ///< Command dispatcher

#if ENABLE_EVENT_SYSTEM
    struct EventHandler {
        atom::components::EventCallbackId id;      // Handler ID
        atom::components::EventCallback callback;  // Callback function
        bool once;  // Whether it's a one-time handler
    };

    std::unordered_map<std::string, std::vector<EventHandler>>
        m_EventHandlers_;                     // Event handlers
    mutable std::shared_mutex m_EventMutex_;  // Event handling mutex
    std::atomic<atom::components::EventCallbackId> m_NextEventId_{
        1};  // Next event ID
#endif
};

// Optimized template method implementation
template <typename SourceType, typename DestinationType>
void Component::defConversion(std::function<std::any(const std::any&)> func) {
    static_assert(!std::is_same_v<SourceType, DestinationType>,
                  "SourceType and DestinationType must be not the same");
    if (!func) {
        throw std::invalid_argument("Conversion function cannot be null");
    }
    if (!m_TypeCaster_) {
        throw std::runtime_error("Type caster not initialized");
    }
    m_TypeCaster_->registerConversion<SourceType, DestinationType>(
        std::move(func));
}

template <typename Base, typename Derived>
    requires std::derived_from<Derived, Base>
void Component::defBaseClass() {
    if (!m_TypeConverter_) {
        throw std::runtime_error("Type converter not initialized");
    }
    m_TypeConverter_->addBaseClass<Base, Derived>();
}

template <typename Callable>
void Component::def(std::string_view name, Callable&& func,
                    std::string_view group, std::string_view description) {
    using Traits = atom::meta::FunctionTraits<std::decay_t<Callable>>;
    using ReturnType = typename Traits::return_type;

    if (name.empty()) {
        throw std::invalid_argument("Command name cannot be empty");
    }

    static_assert(Traits::arity <= 8,
                  "Too many arguments in function (maximum is 8)");

    // clang-format off
    #include "component.template"
    // clang-format on
}

template <typename Ret>
void Component::def(std::string_view name, Ret (*func)(),
                    std::string_view group, std::string_view description) {
    if (!func) {
        throw std::invalid_argument("Function pointer cannot be null");
    }

    m_CommandDispatcher_->def(
        std::string(name), std::string(group), std::string(description),
        std::function<Ret()>([func]() -> Ret { return func(); }));
}

template <typename... Args, typename Ret>
void Component::def(std::string_view name, Ret (*func)(Args...),
                    std::string_view group, std::string_view description) {
    if (!func) {
        throw std::invalid_argument("Function pointer cannot be null");
    }

    m_CommandDispatcher_->def(
        std::string(name), std::string(group), std::string(description),
        std::function<Ret(Args...)>([func](Args... args) -> Ret {
            return func(std::forward<Args>(args)...);
        }));
}

#define DEF_MEMBER_FUNC_IMPL(cv_qualifier)                                   \
    template <typename Class, typename Ret, typename... Args>                \
    void Component::def(                                                     \
        std::string_view name, Ret (Class::*func)(Args...) cv_qualifier,     \
        std::string_view group, std::string_view description) {              \
        if (!func) {                                                         \
            throw std::invalid_argument(                                     \
                "Member function pointer cannot be null");                   \
        }                                                                    \
                                                                             \
        auto boundFunc = atom::meta::bindMemberFunction(func);               \
        m_CommandDispatcher_->def(                                           \
            std::string(name), std::string(group), std::string(description), \
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
void Component::def(std::string_view name, Ret (Class::*func)(),
                    const InstanceType& instance, std::string_view group,
                    std::string_view description) {
    if (!func) {
        throw std::invalid_argument("Member function pointer cannot be null");
    }

    if constexpr (SmartPointer<InstanceType>) {
        if (!instance) {
            throw std::invalid_argument("Instance pointer cannot be null");
        }
    }

    m_CommandDispatcher_->def(std::string(name), std::string(group),
                              std::string(description),
                              std::function<Ret()>([instance, func]() -> Ret {
                                  return std::invoke(func, instance.get());
                              }));
}

template <typename... Args, typename Ret, typename Class, typename InstanceType>
    requires Pointer<InstanceType> || SmartPointer<InstanceType> ||
             std::is_same_v<InstanceType, PointerSentinel<Class>>
void Component::def(std::string_view name, Ret (Class::*func)(Args...),
                    const InstanceType& instance, std::string_view group,
                    std::string_view description) {
    if (!func) {
        throw std::invalid_argument("Member function pointer cannot be null");
    }

    if constexpr (SmartPointer<InstanceType>) {
        if (!instance) {
            throw std::invalid_argument("Instance pointer cannot be null");
        }
    }

    m_CommandDispatcher_->def(
        std::string(name), std::string(group), std::string(description),
        std::function<Ret(Args...)>([instance, func](Args... args) -> Ret {
            return std::invoke(func, instance.get(),
                               std::forward<Args>(args)...);
        }));
}

template <typename MemberType, typename Class>
void Component::def(std::string_view name, MemberType* var,
                    std::string_view group, std::string_view description) {
    if (!var) {
        throw std::invalid_argument("Member variable pointer cannot be null");
    }

    m_CommandDispatcher_->def(
        std::string(name), std::string(group), std::string(description),
        std::function<MemberType&()>([var]() -> MemberType& { return *var; }));
}

template <typename EnumType>
void Component::defEnum(
    std::string_view name,
    const std::unordered_map<std::string, EnumType>& enumMap) {
    if (name.empty()) {
        throw std::invalid_argument("Enum name cannot be empty");
    }

    if (enumMap.empty()) {
        throw std::invalid_argument("Enum map cannot be empty");
    }

    std::string nameStr(name);
    m_TypeCaster_->registerType<EnumType>(nameStr);

    for (const auto& [key, value] : enumMap) {
        m_TypeCaster_->registerEnumValue<EnumType>(nameStr, key, value);
    }

    // Register conversion from enum to string
    defConversion<EnumType, std::string>(
        [this, nameStr](const std::any& enumValue) -> std::any {
            try {
                const EnumType& value = std::any_cast<EnumType>(enumValue);
                return m_TypeCaster_->enumToString<EnumType>(value, nameStr);
            } catch (const std::bad_any_cast& e) {
                throw VariableTypeError(
                    ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                    "Failed to cast enum value: {}", e.what());
            }
        });

    // Register conversion from string to enum
    defConversion<std::string, EnumType>(
        [this, nameStr](const std::any& strValue) -> std::any {
            try {
                const std::string& value = std::any_cast<std::string>(strValue);
                return m_TypeCaster_->stringToEnum<EnumType>(value, nameStr);
            } catch (const std::bad_any_cast& e) {
                throw VariableTypeError(
                    ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                    "Failed to cast string value: {}", e.what());
            }
        });
}

template <typename SourceType, typename DestinationType>
auto Component::hasConversion() const noexcept -> bool {
    if constexpr (std::is_same_v<SourceType, DestinationType>) {
        return true;
    }
    return m_TypeConverter_->canConvert(
        atom::meta::userType<SourceType>(),
        atom::meta::userType<DestinationType>());
}

template <typename T>
void Component::defType(std::string_view name,
                        [[maybe_unused]] std::string_view group,
                        [[maybe_unused]] std::string_view description) {
    m_classes_[name] = atom::meta::userType<T>();
    m_TypeCaster_->registerType<T>(std::string(name));
}

// Implement template method definitions...

#endif  // ATOM_COMPONENT_HPP
