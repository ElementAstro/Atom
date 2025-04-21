#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace atom::meta {

enum class constraint_level { none, nontrivial, nothrow, trivial };
enum class thread_safety { none, synchronized, lockfree };

struct proxiable_constraints {
    std::size_t max_size;
    std::size_t max_align;
    constraint_level copyability;
    constraint_level relocatability;
    constraint_level destructibility;
    thread_safety concurrency{thread_safety::none};
};

template <class T>
concept dispatcher = requires {
    typename T::dispatch_type;
    { T::is_direct } -> std::same_as<const bool&>;
};

template <class T>
concept reflector = requires {
    typename T::reflector_type;
    { T::is_direct } -> std::same_as<const bool&>;
};

template <class T>
concept facade = requires {
    typename T::convention_types;
    typename T::reflection_types;
    { T::constraints } -> std::same_as<const proxiable_constraints&>;
};

namespace detail {
template <class... Ts1, class... Ts2>
constexpr auto merge_tuples(std::tuple<Ts1...>, std::tuple<Ts2...>) {
    return std::tuple<Ts1..., Ts2...>{};
}

template <class T1, class T2>
using merged_tuple_t =
    decltype(merge_tuples(std::declval<T1>(), std::declval<T2>()));

constexpr proxiable_constraints merge_constraints(
    const proxiable_constraints& a, const proxiable_constraints& b) {
    return {
        .max_size = std::min(a.max_size, b.max_size),
        .max_align = std::min(a.max_align, b.max_align),
        .copyability = static_cast<constraint_level>(std::max(
            static_cast<int>(a.copyability), static_cast<int>(b.copyability))),
        .relocatability = static_cast<constraint_level>(
            std::max(static_cast<int>(a.relocatability),
                     static_cast<int>(b.relocatability))),
        .destructibility = static_cast<constraint_level>(
            std::max(static_cast<int>(a.destructibility),
                     static_cast<int>(b.destructibility))),
        .concurrency = static_cast<thread_safety>(std::max(
            static_cast<int>(a.concurrency), static_cast<int>(b.concurrency)))};
}

constexpr proxiable_constraints normalize_constraints(proxiable_constraints c) {
    if (c.max_size == 0)
        c.max_size = sizeof(void*) * 2;
    if (c.max_align == 0)
        c.max_align = alignof(void*);
    return c;
}

struct vtable {
    void (*destroy)(void*) noexcept;
    void (*copy)(const void*, void*);
    void (*move)(void*, void*) noexcept;

    const std::type_info& (*type)() noexcept;
};

template <class T>
constexpr vtable make_vtable() {
    return {// destroy: Handle destruction with appropriate exception handling
            [](void* obj) noexcept {
                if constexpr (std::is_nothrow_destructible_v<T>) {
                    static_cast<T*>(obj)->~T();
                } else if constexpr (std::is_destructible_v<T>) {
                    try {
                        static_cast<T*>(obj)->~T();
                    } catch (...) {
                        // Absorb exceptions - can't propagate from noexcept
                        // function In a real implementation, might want to log
                        // this
                    }
                }
                // If not destructible, do nothing (constraints should prevent
                // this case)
            },

            // copy: Handle copy construction with appropriate constraints
            [](const void* src, void* dst) {
                if constexpr (std::is_copy_constructible_v<T>) {
                    if constexpr (std::is_trivially_copy_constructible_v<T>) {
                        std::memcpy(dst, src,
                                    sizeof(T));  // Optimize for trivial types
                    } else {
                        new (dst) T(*static_cast<const T*>(src));
                    }
                } else {
                    throw std::runtime_error("Type is not copy constructible");
                }
            },

            // move: Handle move construction with noexcept guarantee
            [](void* src, void* dst) noexcept {
                if constexpr (std::is_nothrow_move_constructible_v<T>) {
                    if constexpr (std::is_trivially_move_constructible_v<T>) {
                        std::memcpy(dst, src,
                                    sizeof(T));  // Optimize for trivial types
                    } else {
                        new (dst) T(std::move(*static_cast<T*>(src)));
                        // Note: We don't destroy the source object here
                        // The caller is responsible for that if needed
                    }
                } else if constexpr (std::is_move_constructible_v<T>) {
                    // Move construction might throw, but vtable expects
                    // noexcept
                    try {
                        new (dst) T(std::move(*static_cast<T*>(src)));
                    } catch (...) {
                        // Cannot propagate exception due to noexcept. Last
                        // resort is to terminate.
                        std::terminate();  // This should be prevented by proper
                                           // constraints
                    }
                } else if constexpr (std::is_copy_constructible_v<T> &&
                                     std::is_nothrow_copy_constructible_v<T>) {
                    // Fall back to copy if move not available but copy is
                    // nothrow
                    new (dst) T(*static_cast<const T*>(src));
                } else {
                    // Neither safe move nor copy is available - terminate
                    // This state should be prevented by proper constraints
                    std::terminate();
                }
            },

            // type: Return type information
            []() noexcept -> const std::type_info& { return typeid(T); }};
}

// Small object optimization check
template <class T, std::size_t Size, std::size_t Align>
inline constexpr bool fits_small_storage =
    sizeof(T) <= Size && alignof(T) <= Align &&
    std::is_nothrow_move_constructible_v<T>;

template <class FB, template <class> class...>
struct apply_skills;

template <class FB>
struct apply_skills<FB> {
    using type = FB;
};

template <class FB, template <class> class Skill,
          template <class> class... Skills>
struct apply_skills<FB, Skill, Skills...> {
    // Apply the first skill to the current builder type (FB)
    using next_fb = typename FB::template support<Skill>;
    // Recursively apply the remaining skills to the result (next_fb)
    using type = typename apply_skills<next_fb, Skills...>::type;
};

}  // namespace detail

template <bool IsDirect, class D, class... Os>
struct convention_impl {
    static constexpr bool is_direct = IsDirect;
    using dispatch_type = D;
    using overload_types = std::tuple<Os...>;
};

template <bool IsDirect, class R>
struct reflection_impl {
    static constexpr bool is_direct = IsDirect;
    using reflector_type = R;
};

template <class Cs, class Rs, proxiable_constraints C>
struct facade_impl {
    using convention_types = Cs;
    using reflection_types = Rs;
    static constexpr proxiable_constraints constraints = C;
};

template <facade F>
class proxy;

template <class Cs, class Rs, proxiable_constraints C>
struct facade_builder {
    // Add indirect convention
    template <class D, class... Os>
    using add_indirect_convention =
        facade_builder<detail::merged_tuple_t<
                           Cs, std::tuple<convention_impl<false, D, Os...>>>,
                       Rs, C>;

    // Add direct convention
    template <class D, class... Os>
    using add_direct_convention = facade_builder<
        detail::merged_tuple_t<Cs, std::tuple<convention_impl<true, D, Os...>>>,
        Rs, C>;

    // Add convention (indirect by default)
    template <class D, class... Os>
    using add_convention = add_indirect_convention<D, Os...>;

    // Add indirect reflection
    template <class R>
    using add_indirect_reflection = facade_builder<
        Cs, detail::merged_tuple_t<Rs, std::tuple<reflection_impl<false, R>>>,
        C>;

    // Add direct reflection
    template <class R>
    using add_direct_reflection = facade_builder<
        Cs, detail::merged_tuple_t<Rs, std::tuple<reflection_impl<true, R>>>,
        C>;

    // Add reflection (indirect by default)
    template <class R>
    using add_reflection = add_indirect_reflection<R>;

    // Add an existing facade
    template <facade F, bool WithUpwardConversion = false>
    using add_facade =
        facade_builder<detail::merged_tuple_t<Cs, typename F::convention_types>,
                       detail::merged_tuple_t<Rs, typename F::reflection_types>,
                       detail::merge_constraints(C, F::constraints)>;

    // Restrict layout
    template <std::size_t Size, std::size_t Align = alignof(std::max_align_t)>
        requires(std::has_single_bit(Align) && Size > 0 && Align > 0 &&
                 Size % Align == 0)
    using restrict_layout =
        facade_builder<Cs, Rs,
                       proxiable_constraints{
                           .max_size = std::min(C.max_size, Size),
                           .max_align = std::min(C.max_align, Align),
                           .copyability = C.copyability,
                           .relocatability = C.relocatability,
                           .destructibility = C.destructibility,
                           .concurrency = C.concurrency}>;

    // Support copy
    template <constraint_level Level>
    using support_copy =
        facade_builder<Cs, Rs,
                       proxiable_constraints{
                           .max_size = C.max_size,
                           .max_align = C.max_align,
                           .copyability = static_cast<constraint_level>(
                               std::max(static_cast<int>(C.copyability),
                                        static_cast<int>(Level))),
                           .relocatability = C.relocatability,
                           .destructibility = C.destructibility,
                           .concurrency = C.concurrency}>;

    // Support move
    template <constraint_level Level>
    using support_relocation =
        facade_builder<Cs, Rs,
                       proxiable_constraints{
                           .max_size = C.max_size,
                           .max_align = C.max_align,
                           .copyability = C.copyability,
                           .relocatability = static_cast<constraint_level>(
                               std::max(static_cast<int>(C.relocatability),
                                        static_cast<int>(Level))),
                           .destructibility = C.destructibility,
                           .concurrency = C.concurrency}>;

    // Support destruction
    template <constraint_level Level>
    using support_destruction =
        facade_builder<Cs, Rs,
                       proxiable_constraints{
                           .max_size = C.max_size,
                           .max_align = C.max_align,
                           .copyability = C.copyability,
                           .relocatability = C.relocatability,
                           .destructibility = static_cast<constraint_level>(
                               std::max(static_cast<int>(C.destructibility),
                                        static_cast<int>(Level))),
                           .concurrency = C.concurrency}>;

    // Add thread safety constraint
    template <thread_safety Level>
    using with_thread_safety =
        facade_builder<Cs, Rs,
                       proxiable_constraints{
                           .max_size = C.max_size,
                           .max_align = C.max_align,
                           .copyability = C.copyability,
                           .relocatability = C.relocatability,
                           .destructibility = C.destructibility,
                           .concurrency = static_cast<thread_safety>(
                               std::max(static_cast<int>(C.concurrency),
                                        static_cast<int>(Level)))}>;

    // Support multiple skills - fixed version
    // This alias template applies multiple skills using detail::apply_skills
    template <template <class> class... Skills>
    using with_skills = typename detail::apply_skills<facade_builder<Cs, Rs, C>,
                                                      Skills...>::type;

    // Support a single skill (Skill is a template template parameter)
    template <template <class> class Skill>
    using support =
        Skill<facade_builder<Cs, Rs, C>>;  // Pass the current builder type

    // Build the final facade
    using build = facade_impl<Cs, Rs, detail::normalize_constraints(C)>;
};

// Default facade builder - modified default constraints
using default_builder = facade_builder<
    std::tuple<>, std::tuple<>,
    proxiable_constraints{
        .max_size = 256,
        .max_align = alignof(std::max_align_t),
        .copyability = constraint_level::nothrow,  // Copy allowed by default
        .relocatability = constraint_level::nothrow,
        .destructibility = constraint_level::nothrow,
        .concurrency = thread_safety::none}>;

// Print dispatcher
struct print_dispatch {
    static constexpr bool is_direct =
        false;  // Indirect: works via type erasure
    using dispatch_type = print_dispatch;

    // Option 1: Function pointer in vtable (preferred for type erasure)
    using print_func_t = void (*)(const void*);

    template <class T>
    static void print_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { std::cout << concrete_obj; }) {
            std::cout << concrete_obj;  // No newline here, let caller decide
        } else {
            std::cout << "[unprintable object type: " << typeid(T).name()
                      << "]";
        }
    }

    // Option 2: Static dispatch (requires caller to know type or use visitor)
    // template <class T>
    // static void print(const T& obj) { ... }
};

// Helper to add print function to vtable (conceptual)
/*
template<typename T>
void* get_print_func() {
    return reinterpret_cast<void*>(&print_dispatch::print_impl<T>);
}
*/

// Formattable skill (Printable)
// Associates print_dispatch with a function signature void() const
// The signature here is more conceptual - it defines the *interface*
// The actual implementation might use a vtable entry.
template <class FB>
using formattable = typename FB::template add_convention<
    print_dispatch, void() const>;  // Signature placeholder

// String conversion skill
struct to_string_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = to_string_dispatch;
    using to_string_func_t = std::string (*)(const void*);

    template <class T>
    static std::string to_string_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { std::to_string(concrete_obj); }) {
            return std::to_string(concrete_obj);
        } else if constexpr (requires { std::string(concrete_obj); }) {
            return std::string(concrete_obj);
        } else if constexpr (requires { concrete_obj.to_string(); }) {
            return concrete_obj.to_string();
        } else {
            return "[no string conversion for type: " +
                   std::string(typeid(T).name()) + "]";
        }
    }
};

template <class FB>
using stringable = typename FB::template add_convention<to_string_dispatch,
                                                        std::string() const>;

// Comparable skill
// Comparison needs careful thought with type erasure.
// Comparing two proxies: are they the same type? If so, compare values.
// If different types, comparison is usually false or requires specific rules.
struct compare_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = compare_dispatch;
    // Need function pointers for type-erased comparison
    using equals_func_t = bool (*)(const void*, const void*,
                                   const std::type_info&);

    template <class T>
    static bool equals_impl(const void* obj1, const void* obj2,
                            const std::type_info& type2_info) {
        if (typeid(T) != type2_info)
            return false;  // Types must match

        const T& concrete_obj1 = *static_cast<const T*>(obj1);
        const T& concrete_obj2 =
            *static_cast<const T*>(obj2);  // Safe cast due to type check

        if constexpr (requires { concrete_obj1 == concrete_obj2; }) {
            return concrete_obj1 == concrete_obj2;
        } else {
            return false;  // Cannot compare if operator== not defined
        }
    }
    // Similar logic for less_than_impl if needed
};

template <class FB>
using comparable = typename FB::template add_convention<
    compare_dispatch,
    bool(const proxy<typename FB::build>&) const  // Placeholder signature
    // bool(const proxy<typename FB::build>&) const // For less than
    >;

// Serialization skill (Conceptual - implementation needs vtable integration)
struct serialize_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = serialize_dispatch;
    using serialize_func_t = std::string (*)(const void*);
    using deserialize_func_t = bool (*)(void*, const std::string&);

    template <class T>
    static std::string serialize_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires {
                          {
                              concrete_obj.serialize()
                          } -> std::convertible_to<std::string>;
                      }) {
            return concrete_obj.serialize();
        } else {
            return "{}";  // Default empty JSON or similar
        }
    }

    template <class T>
    static bool deserialize_impl(void* obj, const std::string& data) {
        T& concrete_obj = *static_cast<T*>(obj);
        if constexpr (requires {
                          {
                              concrete_obj.deserialize(data)
                          } -> std::convertible_to<bool>;
                      }) {
            return concrete_obj.deserialize(data);
        } else {
            return false;  // Cannot deserialize
        }
    }
};

template <class FB>
using serializable = typename FB::template add_convention<
    serialize_dispatch,
    std::string() const,      // serialize signature placeholder
    bool(const std::string&)  // deserialize signature placeholder
    >;

// Cloneable skill
struct cloneable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = cloneable_dispatch;
    // The return type depends on the Facade (F), making it tricky for a simple
    // function pointer. It might need to return a raw pointer + vtable, or the
    // clone function needs the target proxy type. Let's define a function that
    // allocates and constructs into a provided proxy reference.
    using clone_func_t = void (*)(const void*, void* /* target storage */,
                                  const detail::vtable** /* target vptr*/);

    template <class T,
              class F_Target>  // F_Target is the Facade of the target proxy
    static void clone_impl(const void* src_obj, void* target_storage,
                           const detail::vtable** target_vptr) {
        const T& concrete_src = *static_cast<const T*>(src_obj);
        // Check if T is compatible with F_Target constraints
        static_assert(sizeof(T) <= F_Target::constraints.max_size,
                      "Clone target size mismatch");
        static_assert(alignof(T) <= F_Target::constraints.max_align,
                      "Clone target align mismatch");
        // Add other constraint checks as needed

        if constexpr (requires { concrete_src.clone(); }) {
            using CloneResult = decltype(concrete_src.clone());
            static_assert(std::is_same_v<CloneResult, T>,
                          "clone() must return the same type T");
            // Construct the cloned object in the target storage
            new (target_storage) T(concrete_src.clone());
            static constinit const auto vtbl = detail::make_vtable<T>();
            *target_vptr = &vtbl;
        } else if constexpr (std::is_copy_constructible_v<T>) {
            // Check if copying is allowed by the *target* facade
            if constexpr (F_Target::constraints.copyability !=
                          constraint_level::none) {
                new (target_storage) T(concrete_src);  // Copy construct
                static constinit const auto vtbl = detail::make_vtable<T>();
                *target_vptr = &vtbl;
            } else {
                throw std::runtime_error(
                    "Cloning via copy construction forbidden by target facade");
            }
        } else {
            throw std::runtime_error(
                "Object is not cloneable (no clone() method or copy "
                "constructor)");
        }
    }
};

template <class FB>
using cloneable =
    typename FB::template add_convention<cloneable_dispatch,
                                         proxy<typename FB::build>()
                                             const>;  // Placeholder signature

// Math operations skill (Conceptual - needs vtable/dispatch refinement)
struct math_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = math_dispatch;
    // Math operations between proxies are complex due to types.
    // Usually requires operands to be the same type or specific conversion
    // rules. Vtable entries would be needed for type-erased arithmetic.
};

template <class FB>
using mathable = typename FB::template add_convention<
    math_dispatch,
    // Placeholder signatures for arithmetic operations
    proxy<typename FB::build>(const proxy<typename FB::build>&) const,  // add
    proxy<typename FB::build>(const proxy<typename FB::build>&)
        const,  // subtract
    proxy<typename FB::build>(const proxy<typename FB::build>&)
        const,  // multiply
    proxy<typename FB::build>(const proxy<typename FB::build>&) const  // divide
    >;

// Debug support skill
struct debug_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = debug_dispatch;
    using dump_func_t = void (*)(const void*, std::ostream&);

    template <class T>
    static void dump_impl(const void* obj, std::ostream& os) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        os << "Object of type: " << typeid(T).name() << "\n";
        os << "  Size: " << sizeof(T) << " bytes\n";
        os << "  Alignment: " << alignof(T) << " bytes\n";

        if constexpr (requires { os << concrete_obj; }) {
            os << "  Content: " << concrete_obj << "\n";
        } else {
            os << "  Content: <not streamable>\n";
        }
    }
};

template <class FB>
using debuggable =
    typename FB::template add_convention<debug_dispatch,
                                         void(std::ostream&)
                                             const>;  // Placeholder

// Iteration capability (Highly complex with type erasure)
// Would require type-erased iterators.
struct iterable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = iterable_dispatch;
    // Vtable entries for begin, end, size, empty needed.
    // Iterator types would need to be type-erased as well.
};

template <class FB>
using iterable = typename FB::template add_convention<
    iterable_dispatch,
    // Placeholder signatures
    void*(),              // begin() - returns opaque iterator pointer?
    void*(),              // end() - returns opaque iterator pointer?
    std::size_t() const,  // size()
    bool() const          // empty()
    >;

template <facade F>
class proxy {
private:
    // Storage management
    alignas(
        F::constraints.max_align) std::byte storage[F::constraints.max_size]{};
    const detail::vtable* vptr = nullptr;

    // Extended vtable for skills (conceptual - actual implementation could
    // vary)
    struct skill_record {
        const void* func_ptr;
        const std::type_info* skill_type;
    };

    // This could be a map of skill type IDs to function pointers
    // In a real implementation, this might be part of the vtable or a separate
    // structure
    std::vector<skill_record> skill_vtable;

    // Get storage pointer with proper alignment
    template <class T>
    T* as() noexcept {
        return std::launder(reinterpret_cast<T*>(storage));
    }

    template <class T>
    const T* as() const noexcept {
        return std::launder(reinterpret_cast<const T*>(storage));
    }

    // Dispatcher helper function
    template <dispatcher D, class R, class... Args>
    R dispatch([[maybe_unused]] Args&&... args)
        const {  // Mark args as potentially unused
        if (!has_value()) {
            throw std::bad_function_call();
        }

        if constexpr (D::is_direct) {
            // Direct dispatch - requires concrete type knowledge
            // This would typically be used when the caller knows the exact type
            // and can directly call methods on it

            // For a complete implementation, we'd need a way to:
            // 1. Get the concrete type T from the stored object
            // 2. Call the appropriate method on T with args

            // Since direct dispatch requires compile-time type knowledge,
            // it's typically handled by external template code that knows T
            throw std::logic_error("Direct dispatch requires type knowledge");
        } else {
            // Indirect dispatch - uses type erasure via function pointers

            // Find the appropriate function pointer in the vtable extension
            // This needs to locate the correct skill implementation

            // Example for print_dispatch skill:
            if constexpr (std::is_same_v<typename D::dispatch_type,
                                         print_dispatch>) {
                // Find print function in skill vtable
                for (const auto& record : skill_vtable) {
                    if (*record.skill_type == typeid(print_dispatch)) {
                        // Cast to appropriate function type and call
                        auto print_func = reinterpret_cast<
                            typename print_dispatch::print_func_t>(
                            record.func_ptr);
                        if (print_func) {
                            print_func(storage);
                            return R();  // Return void or default-constructed R
                        }
                    }
                }
            }

            // Similar implementations for other dispatch types

            throw std::runtime_error(
                "Skill implementation not found in vtable");
        }
    }

    // Internal construction helper
    template <class T, class... Args>
    void construct(Args&&... args) {
        using value_type = std::remove_cvref_t<T>;
        // Check size constraints
        static_assert(sizeof(value_type) <= F::constraints.max_size,
                      "Type exceeds maximum size constraint");
        static_assert(alignof(value_type) <= F::constraints.max_align,
                      "Type exceeds maximum alignment constraint");

        // Check lifecycle constraints - only when explicitly restricted
        if constexpr (F::constraints.copyability == constraint_level::none) {
            static_assert(!(std::is_copy_constructible_v<value_type> &&
                            std::is_copy_assignable_v<value_type>),
                          "Type is copyable but facade forbids it.");
        } else if constexpr (F::constraints.copyability ==
                             constraint_level::nothrow) {
            static_assert(std::is_nothrow_copy_constructible_v<value_type> &&
                              std::is_nothrow_copy_assignable_v<value_type>,
                          "Type must support nothrow copy operations");
        } else if constexpr (F::constraints.copyability ==
                             constraint_level::trivial) {
            static_assert(std::is_trivially_copy_constructible_v<value_type> &&
                              std::is_trivially_copy_assignable_v<value_type>,
                          "Type must support trivial copy operations");
        }

        // Check relocation constraints
        if constexpr (F::constraints.relocatability == constraint_level::none) {
            static_assert(!(std::is_move_constructible_v<value_type> &&
                            std::is_move_assignable_v<value_type>),
                          "Type is movable but facade forbids it.");
        } else if constexpr (F::constraints.relocatability ==
                             constraint_level::nothrow) {
            static_assert(std::is_nothrow_move_constructible_v<value_type> &&
                              std::is_nothrow_move_assignable_v<value_type>,
                          "Type must support nothrow move operations");
        } else if constexpr (F::constraints.relocatability ==
                             constraint_level::trivial) {
            static_assert(std::is_trivially_move_constructible_v<value_type> &&
                              std::is_trivially_move_assignable_v<value_type>,
                          "Type must support trivial move operations");
        }

        // Check destruction constraints
        if constexpr (F::constraints.destructibility ==
                      constraint_level::none) {
            static_assert(!std::is_destructible_v<value_type>,
                          "Type is destructible but facade forbids it.");
        } else if constexpr (F::constraints.destructibility ==
                             constraint_level::nothrow) {
            static_assert(std::is_nothrow_destructible_v<value_type>,
                          "Type must support nothrow destruction");
        } else if constexpr (F::constraints.destructibility ==
                             constraint_level::trivial) {
            static_assert(std::is_trivially_destructible_v<value_type>,
                          "Type must support trivial destruction");
        }

        // Construct in storage
        new (storage) value_type(std::forward<Args>(args)...);
        static constinit const auto vtbl = detail::make_vtable<value_type>();
        vptr = &vtbl;

        // Register skill implementations based on T's capabilities
        register_skills<value_type>();
    }

    // Register skill implementations for type T
    template <class T>
    void register_skills() {
        skill_vtable.clear();

        // Register print skill if T is printable
        if constexpr (requires { std::cout << std::declval<const T&>(); }) {
            skill_vtable.push_back(
                {reinterpret_cast<const void*>(&print_dispatch::print_impl<T>),
                 &typeid(print_dispatch)});
        }

        // Register to_string skill if T supports conversion to string
        if constexpr (
            requires { std::to_string(std::declval<const T&>()); } ||
            requires { std::string(std::declval<const T&>()); } ||
            requires { std::declval<const T&>().to_string(); }) {
            skill_vtable.push_back({reinterpret_cast<const void*>(
                                        &to_string_dispatch::to_string_impl<T>),
                                    &typeid(to_string_dispatch)});
        }

        // Register comparison skill if T is comparable
        if constexpr (requires {
                          std::declval<const T&>() == std::declval<const T&>();
                      }) {
            skill_vtable.push_back({reinterpret_cast<const void*>(
                                        &compare_dispatch::equals_impl<T>),
                                    &typeid(compare_dispatch)});
        }

        // Register serialization skill if T supports it
        if constexpr (requires {
                          {
                              std::declval<const T&>().serialize()
                          } -> std::convertible_to<std::string>;
                      }) {
            skill_vtable.push_back({reinterpret_cast<const void*>(
                                        &serialize_dispatch::serialize_impl<T>),
                                    &typeid(serialize_dispatch)});

            if constexpr (requires {
                              {
                                  std::declval<T&>().deserialize(std::string{})
                              } -> std::convertible_to<bool>;
                          }) {
                skill_vtable.push_back(
                    {reinterpret_cast<const void*>(
                         &serialize_dispatch::deserialize_impl<T>),
                     &typeid(serialize_dispatch)});
            }
        }

        // Register clone skill if T is cloneable
        if constexpr (requires { std::declval<const T&>().clone(); } ||
                      std::is_copy_constructible_v<T>) {
            // Note: Actual implementation would need to handle the target
            // facade type This is a simplified version
            skill_vtable.push_back({reinterpret_cast<const void*>(
                                        &cloneable_dispatch::clone_impl<T, F>),
                                    &typeid(cloneable_dispatch)});
        }

        // Additional skills could be registered similarly
    }

public:
    using facade_type = F;
    static constexpr proxiable_constraints constraints = F::constraints;

    // Default constructor (empty proxy)
    proxy() noexcept = default;

    // Construct from nullptr (empty proxy)
    proxy(std::nullptr_t) noexcept : proxy() {}

    // Destructor
    ~proxy() { reset(); }

    // Copy constructor
    proxy(const proxy& other) {
        if constexpr (F::constraints.copyability != constraint_level::none) {
            if (other.vptr) {
                other.vptr->copy(other.storage, storage);
                vptr = other.vptr;

                // Copy skill vtable
                skill_vtable = other.skill_vtable;
            }
        } else {
            static_assert(F::constraints.copyability != constraint_level::none,
                          "Facade does not support copying");
        }
    }

    // Move constructor
    proxy(proxy&& other) noexcept {
        if constexpr (F::constraints.relocatability != constraint_level::none) {
            if (other.vptr) {
                other.vptr->move(other.storage, storage);
                vptr = other.vptr;

                // Move skill vtable
                skill_vtable = std::move(other.skill_vtable);

                other.vptr = nullptr;
            }
        } else {
            static_assert(
                F::constraints.relocatability != constraint_level::none,
                "Facade does not support moving");
        }
    }

    // Copy assignment
    proxy& operator=(const proxy& other) {
        if constexpr (F::constraints.copyability != constraint_level::none) {
            if (this != &other) {
                reset();
                if (other.vptr) {
                    other.vptr->copy(other.storage, storage);
                    vptr = other.vptr;

                    // Copy skill vtable
                    skill_vtable = other.skill_vtable;
                }
            }
            return *this;
        } else {
            static_assert(F::constraints.copyability != constraint_level::none,
                          "Facade does not support copy assignment");
            return *this;
        }
    }

    // Move assignment
    proxy& operator=(proxy&& other) noexcept {
        if constexpr (F::constraints.relocatability != constraint_level::none) {
            if (this != &other) {
                reset();
                if (other.vptr) {
                    other.vptr->move(other.storage, storage);
                    vptr = other.vptr;

                    // Move skill vtable
                    skill_vtable = std::move(other.skill_vtable);

                    other.vptr = nullptr;
                }
            }
            return *this;
        } else {
            static_assert(
                F::constraints.relocatability != constraint_level::none,
                "Facade does not support move assignment");
            return *this;
        }
    }

    // Construct from a compatible type T
    template <class T>
        requires(
            !std::same_as<std::remove_cvref_t<T>, proxy> &&
            !std::is_same_v<std::remove_cvref_t<T>, std::in_place_type_t<T>>)
    explicit proxy(T&& value) {
        construct<std::remove_cvref_t<T>>(std::forward<T>(value));
    }

    // In-place construction
    template <class T, class... Args>
    explicit proxy(std::in_place_type_t<T>, Args&&... args) {
        construct<T>(std::forward<Args>(args)...);
    }

    // Static factory function for in-place construction
    template <class T, class... Args>
    static proxy make(Args&&... args) {
        proxy result;
        result.template construct<T>(std::forward<Args>(args)...);
        return result;
    }

    // Reset the proxy to an empty state
    void reset() noexcept {
        if (vptr) {
            if constexpr (F::constraints.destructibility !=
                          constraint_level::none) {
                vptr->destroy(storage);
            }
            vptr = nullptr;
            skill_vtable.clear();
        }
    }

    // Check if the proxy holds a value
    bool has_value() const noexcept { return vptr != nullptr; }

    // Boolean conversion
    explicit operator bool() const noexcept { return has_value(); }

    // Get dynamic type information
    const std::type_info& type() const noexcept {
        if (vptr) {
            return vptr->type();
        }
        return typeid(void);
    }

    // Swap two proxies
    void swap(proxy& other) noexcept {
        if constexpr (F::constraints.relocatability != constraint_level::none) {
            if (this == &other)
                return;

            if (vptr && other.vptr) {
                alignas(F::constraints.max_align)
                    std::byte temp_storage[F::constraints.max_size];
                const detail::vtable* temp_vptr = nullptr;

                // Move this to temp
                vptr->move(storage, temp_storage);
                temp_vptr = vptr;

                // Move other to this
                other.vptr->move(other.storage, storage);
                vptr = other.vptr;

                // Move temp to other
                temp_vptr->move(temp_storage, other.storage);
                other.vptr = temp_vptr;

                // Swap skill vtables
                skill_vtable.swap(other.skill_vtable);
            } else if (vptr) {
                other.vptr = vptr;
                vptr->move(storage, other.storage);
                vptr = nullptr;

                skill_vtable.swap(other.skill_vtable);
            } else if (other.vptr) {
                vptr = other.vptr;
                other.vptr->move(other.storage, storage);
                other.vptr = nullptr;

                skill_vtable.swap(other.skill_vtable);
            }
        } else {
            static_assert(
                F::constraints.relocatability != constraint_level::none,
                "Facade does not support relocation (needed for swap)");
        }
    }

    // Access the contained object (unsafe)
    template <typename T>
    T* target() noexcept {
        if (has_value() && type() == typeid(T)) {
            return as<T>();
        }
        return nullptr;
    }

    template <typename T>
    const T* target() const noexcept {
        if (has_value() && type() == typeid(T)) {
            return as<T>();
        }
        return nullptr;
    }

    // Call a function associated with a skill/convention
    template <typename Convention, typename R = void, typename... Args>
    R call(Args&&... args) const {
        if (!has_value()) {
            throw std::bad_function_call();
        }

        // Check if skill is supported
        for (const auto& record : skill_vtable) {
            if (*record.skill_type == typeid(Convention)) {
                // Found the skill implementation
                if constexpr (std::is_same_v<Convention, print_dispatch>) {
                    auto func =
                        reinterpret_cast<typename print_dispatch::print_func_t>(
                            record.func_ptr);
                    func(storage);
                    return R();
                } else if constexpr (std::is_same_v<Convention,
                                                    to_string_dispatch>) {
                    auto func = reinterpret_cast<
                        typename to_string_dispatch::to_string_func_t>(
                        record.func_ptr);
                    if constexpr (std::is_same_v<R, std::string>) {
                        return func(storage);
                    } else {
                        func(storage);
                        return R();
                    }
                } else if constexpr (std::is_same_v<Convention,
                                                    compare_dispatch>) {
                    // Comparison requires two objects - args should contain the
                    // other proxy
                    if constexpr (sizeof...(Args) == 1 &&
                                  std::is_same_v<R, bool>) {
                        // Extract the other proxy from args
                        const proxy& other = std::get<0>(
                            std::forward_as_tuple(std::forward<Args>(args)...));
                        if (!other.has_value())
                            return false;

                        auto func = reinterpret_cast<
                            typename compare_dispatch::equals_func_t>(
                            record.func_ptr);
                        return func(storage, other.storage, other.type());
                    }
                } else if constexpr (std::is_same_v<Convention,
                                                    serialize_dispatch>) {
                    if constexpr (sizeof...(Args) == 0 &&
                                  std::is_same_v<R, std::string>) {
                        // Serialize
                        auto func = reinterpret_cast<
                            typename serialize_dispatch::serialize_func_t>(
                            record.func_ptr);
                        return func(storage);
                    } else if constexpr (sizeof...(Args) == 1 &&
                                         std::is_same_v<R, bool>) {
                        // Deserialize - requires mutable object
                        const std::string& data = std::get<0>(
                            std::forward_as_tuple(std::forward<Args>(args)...));
                        auto func = reinterpret_cast<
                            typename serialize_dispatch::deserialize_func_t>(
                            record.func_ptr);
                        return func(const_cast<void*>(
                                        static_cast<const void*>(storage)),
                                    data);
                    }
                }
                // Add similar handlers for other skills

                // If we reach here, the skill is found but the signature
                // doesn't match
                throw std::bad_function_call();
            }
        }

        // Skill not found
        throw std::runtime_error("Skill not supported by this object");
    }

    // Convenience methods for common skills

    // Print the object to stream
    void print(std::ostream& os = std::cout) const {
        try {
            call<print_dispatch>();
        } catch (const std::exception&) {
            os << "[unprintable object]";
        }
    }

    // Convert to string
    std::string to_string() const {
        try {
            return call<to_string_dispatch, std::string>();
        } catch (const std::exception&) {
            return "[unconvertible object]";
        }
    }

    // Compare with another proxy
    bool equals(const proxy& other) const {
        try {
            return call<compare_dispatch, bool>(other);
        } catch (const std::exception&) {
            return false;  // Default to not equal if comparison not supported
        }
    }

    // Create a clone of this proxy
    proxy clone() const {
        if (!has_value()) {
            return proxy();
        }

        // Implementation depends on cloneable_dispatch being properly set up
        // For now, try to use copy construction as a fallback
        return proxy(*this);
    }
};

// Equality comparison operator
template <facade F>
bool operator==(const proxy<F>& a, const proxy<F>& b) {
    if (!a.has_value() && !b.has_value())
        return true;  // Both empty
    if (a.has_value() != b.has_value())
        return false;  // One empty, one not

    // Both have values, check if comparable skill exists and types match
    // This requires accessing the vtable and the comparison function pointer.
    // Simplified check: only compare if types are identical.
    if (a.type() != b.type()) {
        return false;
    }

    // TODO: Integrate with compare_dispatch skill via vtable lookup if
    // available. Placeholder: Assume not comparable for now if skill dispatch
    // isn't implemented. If types are the same, ideally we'd call the
    // equals_impl via vtable. Example (conceptual): if (auto* equals_fp =
    // find_vtable_entry<compare_dispatch::equals_func_t>(a.vptr)) {
    //     return equals_fp(a.storage, b.storage, b.type());
    // }
    return false;  // Placeholder: Cannot compare without skill implementation
}

// Inequality comparison operator
template <facade F>
bool operator!=(const proxy<F>& a, const proxy<F>& b) {
    return !(a == b);
}

// Stream output operator
template <facade F>
std::ostream& operator<<(std::ostream& os, const proxy<F>& p) {
    if (p.has_value()) {
        // TODO: Integrate with print_dispatch or debug_dispatch skill via
        // vtable. Example (conceptual): if (auto* print_fp =
        // find_vtable_entry<print_dispatch::print_func_t>(p.vptr)) {
        //     print_fp(p.storage);
        // } else {
        //     os << "[proxy object type: " << p.type().name() << "]";
        // }
        os << "[proxy object type: " << p.type().name() << "]";  // Placeholder
    } else {
        os << "[empty proxy]";
    }
    return os;
}

}  // namespace atom::meta
