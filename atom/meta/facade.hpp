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
constexpr auto merge_tuples(std::tuple<Ts1...>, std::tuple<Ts2...>) noexcept {
    return std::tuple<Ts1..., Ts2...>{};
}

template <class T1, class T2>
using merged_tuple_t =
    decltype(merge_tuples(std::declval<T1>(), std::declval<T2>()));

constexpr proxiable_constraints merge_constraints(
    const proxiable_constraints& a, const proxiable_constraints& b) noexcept {
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

constexpr proxiable_constraints normalize_constraints(
    proxiable_constraints c) noexcept {
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
constexpr vtable make_vtable() noexcept {
    return {[](void* obj) noexcept {
                if constexpr (std::is_nothrow_destructible_v<T>) {
                    static_cast<T*>(obj)->~T();
                } else if constexpr (std::is_destructible_v<T>) {
                    try {
                        static_cast<T*>(obj)->~T();
                    } catch (...) {
                        // Exception absorption required for noexcept guarantee
                    }
                }
            },
            [](const void* src, void* dst) {
                if constexpr (std::is_copy_constructible_v<T>) {
                    if constexpr (std::is_trivially_copy_constructible_v<T>) {
                        std::memcpy(dst, src, sizeof(T));
                    } else {
                        new (dst) T(*static_cast<const T*>(src));
                    }
                } else {
                    throw std::runtime_error("Type is not copy constructible");
                }
            },
            [](void* src, void* dst) noexcept {
                if constexpr (std::is_nothrow_move_constructible_v<T>) {
                    if constexpr (std::is_trivially_move_constructible_v<T>) {
                        std::memcpy(dst, src, sizeof(T));
                    } else {
                        new (dst) T(std::move(*static_cast<T*>(src)));
                    }
                } else if constexpr (std::is_move_constructible_v<T>) {
                    try {
                        new (dst) T(std::move(*static_cast<T*>(src)));
                    } catch (...) {
                        std::terminate();
                    }
                } else if constexpr (std::is_copy_constructible_v<T> &&
                                     std::is_nothrow_copy_constructible_v<T>) {
                    new (dst) T(*static_cast<const T*>(src));
                } else {
                    std::terminate();
                }
            },
            []() noexcept -> const std::type_info& { return typeid(T); }};
}

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
    using next_fb = typename FB::template support<Skill>;
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
    template <class D, class... Os>
    using add_indirect_convention =
        facade_builder<detail::merged_tuple_t<
                           Cs, std::tuple<convention_impl<false, D, Os...>>>,
                       Rs, C>;

    template <class D, class... Os>
    using add_direct_convention = facade_builder<
        detail::merged_tuple_t<Cs, std::tuple<convention_impl<true, D, Os...>>>,
        Rs, C>;

    template <class D, class... Os>
    using add_convention = add_indirect_convention<D, Os...>;

    template <class R>
    using add_indirect_reflection = facade_builder<
        Cs, detail::merged_tuple_t<Rs, std::tuple<reflection_impl<false, R>>>,
        C>;

    template <class R>
    using add_direct_reflection = facade_builder<
        Cs, detail::merged_tuple_t<Rs, std::tuple<reflection_impl<true, R>>>,
        C>;

    template <class R>
    using add_reflection = add_indirect_reflection<R>;

    template <facade F, bool WithUpwardConversion = false>
    using add_facade =
        facade_builder<detail::merged_tuple_t<Cs, typename F::convention_types>,
                       detail::merged_tuple_t<Rs, typename F::reflection_types>,
                       detail::merge_constraints(C, F::constraints)>;

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

    template <template <class> class... Skills>
    using with_skills = typename detail::apply_skills<facade_builder<Cs, Rs, C>,
                                                      Skills...>::type;

    template <template <class> class Skill>
    using support = Skill<facade_builder<Cs, Rs, C>>;

    using build = facade_impl<Cs, Rs, detail::normalize_constraints(C)>;
};

using default_builder =
    facade_builder<std::tuple<>, std::tuple<>,
                   proxiable_constraints{
                       .max_size = 256,
                       .max_align = alignof(std::max_align_t),
                       .copyability = constraint_level::nothrow,
                       .relocatability = constraint_level::nothrow,
                       .destructibility = constraint_level::nothrow,
                       .concurrency = thread_safety::none}>;

/**
 * @brief Print dispatcher for type-erased printing functionality
 */
struct print_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = print_dispatch;
    using print_func_t = void (*)(const void*);

    template <class T>
    static void print_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { std::cout << concrete_obj; }) {
            std::cout << concrete_obj;
        } else {
            std::cout << "[unprintable object type: " << typeid(T).name()
                      << "]";
        }
    }
};

template <class FB>
using formattable =
    typename FB::template add_convention<print_dispatch, void() const>;

/**
 * @brief String conversion dispatcher
 */
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

/**
 * @brief Comparison dispatcher for type-erased equality operations
 */
struct compare_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = compare_dispatch;
    using equals_func_t = bool (*)(const void*, const void*,
                                   const std::type_info&);

    template <class T>
    static bool equals_impl(const void* obj1, const void* obj2,
                            const std::type_info& type2_info) {
        if (typeid(T) != type2_info)
            return false;

        const T& concrete_obj1 = *static_cast<const T*>(obj1);
        const T& concrete_obj2 = *static_cast<const T*>(obj2);

        if constexpr (requires { concrete_obj1 == concrete_obj2; }) {
            return concrete_obj1 == concrete_obj2;
        } else {
            return false;
        }
    }
};

template <class FB>
using comparable = typename FB::template add_convention<
    compare_dispatch, bool(const proxy<typename FB::build>&) const>;

/**
 * @brief Serialization dispatcher for type-erased serialization operations
 */
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
            return "{}";
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
            return false;
        }
    }
};

template <class FB>
using serializable = typename FB::template add_convention<
    serialize_dispatch, std::string() const, bool(const std::string&)>;

/**
 * @brief Cloneable dispatcher for type-erased cloning operations
 */
struct cloneable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = cloneable_dispatch;
    using clone_func_t = void (*)(const void*, void*, const detail::vtable**);

    template <class T, class F_Target>
    static void clone_impl(const void* src_obj, void* target_storage,
                           const detail::vtable** target_vptr) {
        const T& concrete_src = *static_cast<const T*>(src_obj);
        static_assert(sizeof(T) <= F_Target::constraints.max_size);
        static_assert(alignof(T) <= F_Target::constraints.max_align);

        if constexpr (requires { concrete_src.clone(); }) {
            using CloneResult = decltype(concrete_src.clone());
            static_assert(std::is_same_v<CloneResult, T>);
            new (target_storage) T(concrete_src.clone());
            static constinit const auto vtbl = detail::make_vtable<T>();
            *target_vptr = &vtbl;
        } else if constexpr (std::is_copy_constructible_v<T>) {
            if constexpr (F_Target::constraints.copyability !=
                          constraint_level::none) {
                new (target_storage) T(concrete_src);
                static constinit const auto vtbl = detail::make_vtable<T>();
                *target_vptr = &vtbl;
            } else {
                throw std::runtime_error(
                    "Cloning via copy construction forbidden by target facade");
            }
        } else {
            throw std::runtime_error("Object is not cloneable");
        }
    }
};

template <class FB>
using cloneable =
    typename FB::template add_convention<cloneable_dispatch,
                                         proxy<typename FB::build>() const>;

/**
 * @brief Math operations dispatcher
 */
struct math_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = math_dispatch;
};

template <class FB>
using mathable = typename FB::template add_convention<
    math_dispatch,
    proxy<typename FB::build>(const proxy<typename FB::build>&) const,
    proxy<typename FB::build>(const proxy<typename FB::build>&) const,
    proxy<typename FB::build>(const proxy<typename FB::build>&) const,
    proxy<typename FB::build>(const proxy<typename FB::build>&) const>;

/**
 * @brief Debug support dispatcher
 */
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
                                         void(std::ostream&) const>;

/**
 * @brief Iteration capability dispatcher
 */
struct iterable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = iterable_dispatch;
};

template <class FB>
using iterable =
    typename FB::template add_convention<iterable_dispatch, void*(), void*(),
                                         std::size_t() const, bool() const>;

/**
 * @brief Type-erased proxy class providing generic object storage and
 * operations
 * @tparam F Facade type defining constraints and capabilities
 */
template <facade F>
class proxy {
private:
    alignas(
        F::constraints.max_align) std::byte storage[F::constraints.max_size]{};
    const detail::vtable* vptr = nullptr;

    struct skill_record {
        const void* func_ptr;
        const std::type_info* skill_type;
    };

    std::vector<skill_record> skill_vtable;

    template <class T>
    T* as() noexcept {
        return std::launder(reinterpret_cast<T*>(storage));
    }

    template <class T>
    const T* as() const noexcept {
        return std::launder(reinterpret_cast<const T*>(storage));
    }

    template <dispatcher D, class R, class... Args>
    R dispatch(Args&&... args) const {
        if (!has_value()) {
            throw std::bad_function_call();
        }

        if constexpr (D::is_direct) {
            throw std::logic_error("Direct dispatch requires type knowledge");
        } else {
            if constexpr (std::is_same_v<typename D::dispatch_type,
                                         print_dispatch>) {
                for (const auto& record : skill_vtable) {
                    if (*record.skill_type == typeid(print_dispatch)) {
                        auto print_func = reinterpret_cast<
                            typename print_dispatch::print_func_t>(
                            record.func_ptr);
                        if (print_func) {
                            print_func(storage);
                            return R();
                        }
                    }
                }
            }
            throw std::runtime_error(
                "Skill implementation not found in vtable");
        }
    }

    template <class T, class... Args>
    void construct(Args&&... args) {
        using value_type = std::remove_cvref_t<T>;
        static_assert(sizeof(value_type) <= F::constraints.max_size);
        static_assert(alignof(value_type) <= F::constraints.max_align);

        if constexpr (F::constraints.copyability == constraint_level::none) {
            static_assert(!(std::is_copy_constructible_v<value_type> &&
                            std::is_copy_assignable_v<value_type>));
        } else if constexpr (F::constraints.copyability ==
                             constraint_level::nothrow) {
            static_assert(std::is_nothrow_copy_constructible_v<value_type> &&
                          std::is_nothrow_copy_assignable_v<value_type>);
        } else if constexpr (F::constraints.copyability ==
                             constraint_level::trivial) {
            static_assert(std::is_trivially_copy_constructible_v<value_type> &&
                          std::is_trivially_copy_assignable_v<value_type>);
        }

        if constexpr (F::constraints.relocatability == constraint_level::none) {
            static_assert(!(std::is_move_constructible_v<value_type> &&
                            std::is_move_assignable_v<value_type>));
        } else if constexpr (F::constraints.relocatability ==
                             constraint_level::nothrow) {
            static_assert(std::is_nothrow_move_constructible_v<value_type> &&
                          std::is_nothrow_move_assignable_v<value_type>);
        } else if constexpr (F::constraints.relocatability ==
                             constraint_level::trivial) {
            static_assert(std::is_trivially_move_constructible_v<value_type> &&
                          std::is_trivially_move_assignable_v<value_type>);
        }

        if constexpr (F::constraints.destructibility ==
                      constraint_level::none) {
            static_assert(!std::is_destructible_v<value_type>);
        } else if constexpr (F::constraints.destructibility ==
                             constraint_level::nothrow) {
            static_assert(std::is_nothrow_destructible_v<value_type>);
        } else if constexpr (F::constraints.destructibility ==
                             constraint_level::trivial) {
            static_assert(std::is_trivially_destructible_v<value_type>);
        }

        new (storage) value_type(std::forward<Args>(args)...);
        static constinit const auto vtbl = detail::make_vtable<value_type>();
        vptr = &vtbl;
        register_skills<value_type>();
    }

    template <class T>
    void register_skills() {
        skill_vtable.clear();

        if constexpr (requires { std::cout << std::declval<const T&>(); }) {
            skill_vtable.push_back(
                {reinterpret_cast<const void*>(&print_dispatch::print_impl<T>),
                 &typeid(print_dispatch)});
        }

        if constexpr (
            requires { std::to_string(std::declval<const T&>()); } ||
            requires { std::string(std::declval<const T&>()); } ||
            requires { std::declval<const T&>().to_string(); }) {
            skill_vtable.push_back({reinterpret_cast<const void*>(
                                        &to_string_dispatch::to_string_impl<T>),
                                    &typeid(to_string_dispatch)});
        }

        if constexpr (requires {
                          std::declval<const T&>() == std::declval<const T&>();
                      }) {
            skill_vtable.push_back({reinterpret_cast<const void*>(
                                        &compare_dispatch::equals_impl<T>),
                                    &typeid(compare_dispatch)});
        }

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

        if constexpr (requires { std::declval<const T&>().clone(); } ||
                      std::is_copy_constructible_v<T>) {
            skill_vtable.push_back({reinterpret_cast<const void*>(
                                        &cloneable_dispatch::clone_impl<T, F>),
                                    &typeid(cloneable_dispatch)});
        }
    }

public:
    using facade_type = F;
    static constexpr proxiable_constraints constraints = F::constraints;

    /**
     * @brief Default constructor creating an empty proxy
     */
    proxy() noexcept = default;

    /**
     * @brief Construct empty proxy from nullptr
     */
    proxy(std::nullptr_t) noexcept : proxy() {}

    /**
     * @brief Destructor
     */
    ~proxy() { reset(); }

    /**
     * @brief Copy constructor
     */
    proxy(const proxy& other) {
        if constexpr (F::constraints.copyability != constraint_level::none) {
            if (other.vptr) {
                other.vptr->copy(other.storage, storage);
                vptr = other.vptr;
                skill_vtable = other.skill_vtable;
            }
        } else {
            static_assert(F::constraints.copyability != constraint_level::none);
        }
    }

    /**
     * @brief Move constructor
     */
    proxy(proxy&& other) noexcept {
        if constexpr (F::constraints.relocatability != constraint_level::none) {
            if (other.vptr) {
                other.vptr->move(other.storage, storage);
                vptr = other.vptr;
                skill_vtable = std::move(other.skill_vtable);
                other.vptr = nullptr;
            }
        } else {
            static_assert(F::constraints.relocatability !=
                          constraint_level::none);
        }
    }

    /**
     * @brief Copy assignment operator
     */
    proxy& operator=(const proxy& other) {
        if constexpr (F::constraints.copyability != constraint_level::none) {
            if (this != &other) {
                reset();
                if (other.vptr) {
                    other.vptr->copy(other.storage, storage);
                    vptr = other.vptr;
                    skill_vtable = other.skill_vtable;
                }
            }
            return *this;
        } else {
            static_assert(F::constraints.copyability != constraint_level::none);
            return *this;
        }
    }

    /**
     * @brief Move assignment operator
     */
    proxy& operator=(proxy&& other) noexcept {
        if constexpr (F::constraints.relocatability != constraint_level::none) {
            if (this != &other) {
                reset();
                if (other.vptr) {
                    other.vptr->move(other.storage, storage);
                    vptr = other.vptr;
                    skill_vtable = std::move(other.skill_vtable);
                    other.vptr = nullptr;
                }
            }
            return *this;
        } else {
            static_assert(F::constraints.relocatability !=
                          constraint_level::none);
            return *this;
        }
    }

    /**
     * @brief Construct from a compatible type T
     * @tparam T Type to construct from
     * @param value Value to construct with
     */
    template <class T>
        requires(
            !std::same_as<std::remove_cvref_t<T>, proxy> &&
            !std::is_same_v<std::remove_cvref_t<T>, std::in_place_type_t<T>>)
    explicit proxy(T&& value) {
        construct<std::remove_cvref_t<T>>(std::forward<T>(value));
    }

    /**
     * @brief In-place construction
     * @tparam T Type to construct
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     */
    template <class T, class... Args>
    explicit proxy(std::in_place_type_t<T>, Args&&... args) {
        construct<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief Static factory function for in-place construction
     * @tparam T Type to construct
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Constructed proxy
     */
    template <class T, class... Args>
    static proxy make(Args&&... args) {
        proxy result;
        result.template construct<T>(std::forward<Args>(args)...);
        return result;
    }

    /**
     * @brief Reset the proxy to an empty state
     */
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

    /**
     * @brief Check if the proxy holds a value
     * @return true if proxy contains an object, false otherwise
     */
    [[nodiscard]] bool has_value() const noexcept { return vptr != nullptr; }

    /**
     * @brief Boolean conversion operator
     * @return true if proxy contains an object, false otherwise
     */
    explicit operator bool() const noexcept { return has_value(); }

    /**
     * @brief Get dynamic type information
     * @return Type information of stored object
     */
    [[nodiscard]] const std::type_info& type() const noexcept {
        if (vptr) {
            return vptr->type();
        }
        return typeid(void);
    }

    /**
     * @brief Swap two proxies
     * @param other Other proxy to swap with
     */
    void swap(proxy& other) noexcept {
        if constexpr (F::constraints.relocatability != constraint_level::none) {
            if (this == &other)
                return;

            if (vptr && other.vptr) {
                alignas(F::constraints.max_align)
                    std::byte temp_storage[F::constraints.max_size];
                const detail::vtable* temp_vptr = nullptr;

                vptr->move(storage, temp_storage);
                temp_vptr = vptr;

                other.vptr->move(other.storage, storage);
                vptr = other.vptr;

                temp_vptr->move(temp_storage, other.storage);
                other.vptr = temp_vptr;

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
            static_assert(F::constraints.relocatability !=
                          constraint_level::none);
        }
    }

    /**
     * @brief Access the contained object (unsafe)
     * @tparam T Expected type
     * @return Pointer to object if type matches, nullptr otherwise
     */
    template <typename T>
    [[nodiscard]] T* target() noexcept {
        if (has_value() && type() == typeid(T)) {
            return as<T>();
        }
        return nullptr;
    }

    template <typename T>
    [[nodiscard]] const T* target() const noexcept {
        if (has_value() && type() == typeid(T)) {
            return as<T>();
        }
        return nullptr;
    }

    /**
     * @brief Call a function associated with a skill/convention
     * @tparam Convention Convention type
     * @tparam R Return type
     * @tparam Args Argument types
     * @param args Function arguments
     * @return Result of the function call
     */
    template <typename Convention, typename R = void, typename... Args>
    R call(Args&&... args) const {
        if (!has_value()) {
            throw std::bad_function_call();
        }

        for (const auto& record : skill_vtable) {
            if (*record.skill_type == typeid(Convention)) {
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
                    if constexpr (sizeof...(Args) == 1 &&
                                  std::is_same_v<R, bool>) {
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
                        auto func = reinterpret_cast<
                            typename serialize_dispatch::serialize_func_t>(
                            record.func_ptr);
                        return func(storage);
                    } else if constexpr (sizeof...(Args) == 1 &&
                                         std::is_same_v<R, bool>) {
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
                throw std::bad_function_call();
            }
        }
        throw std::runtime_error("Skill not supported by this object");
    }

    /**
     * @brief Print the object to stream
     * @param os Output stream
     */
    void print(std::ostream& os = std::cout) const {
        try {
            call<print_dispatch>();
        } catch (const std::exception&) {
            os << "[unprintable object]";
        }
    }

    /**
     * @brief Convert to string
     * @return String representation of the object
     */
    [[nodiscard]] std::string to_string() const {
        try {
            return call<to_string_dispatch, std::string>();
        } catch (const std::exception&) {
            return "[unconvertible object]";
        }
    }

    /**
     * @brief Compare with another proxy
     * @param other Other proxy to compare with
     * @return true if objects are equal, false otherwise
     */
    [[nodiscard]] bool equals(const proxy& other) const {
        try {
            return call<compare_dispatch, bool>(other);
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief Create a clone of this proxy
     * @return Cloned proxy
     */
    [[nodiscard]] proxy clone() const {
        if (!has_value()) {
            return proxy();
        }
        return proxy(*this);
    }
};

/**
 * @brief Equality comparison operator
 * @tparam F Facade type
 * @param a First proxy
 * @param b Second proxy
 * @return true if proxies are equal, false otherwise
 */
template <facade F>
[[nodiscard]] bool operator==(const proxy<F>& a, const proxy<F>& b) {
    if (!a.has_value() && !b.has_value())
        return true;
    if (a.has_value() != b.has_value())
        return false;

    if (a.type() != b.type()) {
        return false;
    }

    return false;
}

/**
 * @brief Inequality comparison operator
 * @tparam F Facade type
 * @param a First proxy
 * @param b Second proxy
 * @return true if proxies are not equal, false otherwise
 */
template <facade F>
[[nodiscard]] bool operator!=(const proxy<F>& a, const proxy<F>& b) {
    return !(a == b);
}

/**
 * @brief Stream output operator
 * @tparam F Facade type
 * @param os Output stream
 * @param p Proxy to output
 * @return Reference to output stream
 */
template <facade F>
std::ostream& operator<<(std::ostream& os, const proxy<F>& p) {
    if (p.has_value()) {
        os << "[proxy object type: " << p.type().name() << "]";
    } else {
        os << "[empty proxy]";
    }
    return os;
}

}  // namespace atom::meta