#include "atom/async/threading/lock.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <memory>

namespace py = pybind11;
using namespace atom::async;

void init_lock(py::module_& m) {
    // LockError exception
    py::register_exception<LockError>(m, "LockError");

    // ScopedLock template for different lock types
    py::class_<ScopedLock<Spinlock>>(m, "ScopedSpinlock",
                                     R"pbdoc(
        RAII scoped lock guard for Spinlock.

        Automatically acquires the lock on construction and releases it on destruction.
        This provides exception-safe lock management.
        )pbdoc")
        .def(py::init<Spinlock&>(), py::arg("mutex"),
             R"pbdoc(
             Constructs the scoped lock and acquires the provided spinlock.

             Args:
                 mutex: The Spinlock to guard.
             )pbdoc")
        .def("unlock", &ScopedLock<Spinlock>::unlock,
             R"pbdoc(
             Manually release the lock before destruction.

             After calling this method, the lock will not be automatically
             released in the destructor.
             )pbdoc");

    py::class_<ScopedLock<TicketSpinlock>>(m, "ScopedTicketSpinlock",
                                           R"pbdoc(
        RAII scoped lock guard for TicketSpinlock.

        Automatically acquires the lock on construction and releases it on destruction.
        This provides exception-safe lock management with fair ordering.
        )pbdoc")
        .def(py::init<TicketSpinlock&>(), py::arg("mutex"),
             R"pbdoc(
             Constructs the scoped lock and acquires the provided ticket spinlock.

             Args:
                 mutex: The TicketSpinlock to guard.
             )pbdoc")
        .def("unlock", &ScopedLock<TicketSpinlock>::unlock,
             R"pbdoc(
             Manually release the lock before destruction.
             )pbdoc");

    py::class_<ScopedLock<UnfairSpinlock>>(m, "ScopedUnfairSpinlock",
                                           R"pbdoc(
        RAII scoped lock guard for UnfairSpinlock.

        Automatically acquires the lock on construction and releases it on destruction.
        This provides exception-safe lock management with maximum performance.
        )pbdoc")
        .def(py::init<UnfairSpinlock&>(), py::arg("mutex"),
             R"pbdoc(
             Constructs the scoped lock and acquires the provided unfair spinlock.

             Args:
                 mutex: The UnfairSpinlock to guard.
             )pbdoc")
        .def("unlock", &ScopedLock<UnfairSpinlock>::unlock,
             R"pbdoc(
             Manually release the lock before destruction.
             )pbdoc");

    py::class_<ScopedLock<AdaptiveSpinlock>>(m, "ScopedAdaptiveSpinlock",
                                             R"pbdoc(
        RAII scoped lock guard for AdaptiveSpinlock.

        Automatically acquires the lock on construction and releases it on destruction.
        This provides exception-safe lock management with adaptive behavior.
        )pbdoc")
        .def(py::init<AdaptiveSpinlock&>(), py::arg("mutex"),
             R"pbdoc(
             Constructs the scoped lock and acquires the provided adaptive spinlock.

             Args:
                 mutex: The AdaptiveSpinlock to guard.
             )pbdoc")
        .def("unlock", &ScopedLock<AdaptiveSpinlock>::unlock,
             R"pbdoc(
             Manually release the lock before destruction.
             )pbdoc");

    // Spinlock class
    py::class_<Spinlock>(m, "Spinlock",
                         R"pbdoc(
        Simple spinlock implementation using atomic_flag with C++20 features.

        A spinlock is a lock that causes a thread trying to acquire it to simply wait
        in a loop ("spin") while repeatedly checking if the lock is available.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &Spinlock::lock,
             R"pbdoc(
             Acquire the lock.

             This method will block until the lock is acquired.
             )pbdoc")
        .def("unlock", &Spinlock::unlock,
             R"pbdoc(
             Release the lock.
             )pbdoc")
        .def("try_lock", &Spinlock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc")
        .def(
            "try_lock_for",
            [](Spinlock& self, std::chrono::milliseconds timeout) {
                return self.tryLockFor(timeout);
            },
            py::arg("timeout"),
            R"pbdoc(
            Try to acquire the lock with a timeout.

            Args:
                timeout: Maximum time to wait for the lock.

            Returns:
                bool: True if the lock was acquired within the timeout, False otherwise.
            )pbdoc");

    // TicketSpinlock class
    py::class_<TicketSpinlock>(m, "TicketSpinlock",
                               R"pbdoc(
        Fair spinlock implementation using ticket-based ordering.

        Provides fair locking in first-come, first-served order.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &TicketSpinlock::lock,
             R"pbdoc(
             Acquire the lock in FIFO order.
             )pbdoc")
        .def("unlock", &TicketSpinlock::unlock,
             R"pbdoc(
             Release the lock.
             )pbdoc")
        .def("try_lock", &TicketSpinlock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");

    // TicketSpinlock::LockGuard class
    py::class_<TicketSpinlock::LockGuard>(m, "TicketSpinlockGuard",
                                          R"pbdoc(
        RAII lock guard for TicketSpinlock.

        Automatically acquires the lock on construction and releases it on destruction.
        )pbdoc")
        .def(py::init<TicketSpinlock&>(), py::arg("spinlock"),
             R"pbdoc(
             Construct the lock guard and acquire the lock.

             Args:
                 spinlock: The TicketSpinlock to guard.
             )pbdoc")
        .def("unlock", &TicketSpinlock::LockGuard::unlock,
             R"pbdoc(
             Manually release the lock before destruction.
             )pbdoc")
        .def("is_locked", &TicketSpinlock::LockGuard::isLocked,
             R"pbdoc(
             Check if the lock is currently held.

             Returns:
                 bool: True if the lock is held, False otherwise.
             )pbdoc");

    // UnfairSpinlock class
    py::class_<UnfairSpinlock>(m, "UnfairSpinlock",
                               R"pbdoc(
        Unfair spinlock implementation for maximum performance.

        May cause starvation but has lower overhead than fair locks.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &UnfairSpinlock::lock,
             R"pbdoc(
             Acquire the lock.
             )pbdoc")
        .def("unlock", &UnfairSpinlock::unlock,
             R"pbdoc(
             Release the lock.
             )pbdoc")
        .def("try_lock", &UnfairSpinlock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");

    // AdaptiveSpinlock class
    py::class_<AdaptiveSpinlock>(m, "AdaptiveSpinlock",
                                 R"pbdoc(
        Adaptive spinlock that switches between spinning and yielding.

        Spins for a short time, then yields to reduce CPU usage.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &AdaptiveSpinlock::lock,
             R"pbdoc(
             Acquire the lock with adaptive spinning.
             )pbdoc")
        .def("unlock", &AdaptiveSpinlock::unlock,
             R"pbdoc(
             Release the lock.
             )pbdoc")
        .def("try_lock", &AdaptiveSpinlock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");

    // Platform-specific lock implementations
#ifdef ATOM_PLATFORM_WINDOWS
    // WindowsSpinlock class
    py::class_<WindowsSpinlock>(m, "WindowsSpinlock",
                                R"pbdoc(
        Windows platform-specific spinlock implementation.

        Uses Windows critical sections with spin count optimization for
        better performance on Windows systems.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor with optimized spin count.
             )pbdoc")
        .def("lock", &WindowsSpinlock::lock,
             R"pbdoc(
             Acquire the lock using Windows critical section.
             )pbdoc")
        .def("unlock", &WindowsSpinlock::unlock,
             R"pbdoc(
             Release the lock.
             )pbdoc")
        .def("try_lock", &WindowsSpinlock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");

    // ScopedLock for WindowsSpinlock
    py::class_<ScopedLock<WindowsSpinlock>>(m, "ScopedWindowsSpinlock",
                                            R"pbdoc(
        RAII scoped lock guard for WindowsSpinlock.
        )pbdoc")
        .def(py::init<WindowsSpinlock&>(), py::arg("mutex"),
             R"pbdoc(
             Constructs the scoped lock and acquires the Windows spinlock.

             Args:
                 mutex: The WindowsSpinlock to guard.
             )pbdoc")
        .def("unlock", &ScopedLock<WindowsSpinlock>::unlock,
             R"pbdoc(
             Manually release the lock before destruction.
             )pbdoc");
#endif

#ifdef ATOM_PLATFORM_MACOS
    // DarwinSpinlock class
    py::class_<DarwinSpinlock>(m, "DarwinSpinlock",
                               R"pbdoc(
        macOS platform-specific spinlock implementation.

        Uses optimized OSSpinLock (before 10.12) or os_unfair_lock (10.12+)
        for optimal performance on macOS systems.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &DarwinSpinlock::lock,
             R"pbdoc(
             Acquire the lock using macOS-specific primitives.
             )pbdoc")
        .def("unlock", &DarwinSpinlock::unlock,
             R"pbdoc(
             Release the lock.
             )pbdoc")
        .def("try_lock", &DarwinSpinlock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");

    // ScopedLock for DarwinSpinlock
    py::class_<ScopedLock<DarwinSpinlock>>(m, "ScopedDarwinSpinlock",
                                           R"pbdoc(
        RAII scoped lock guard for DarwinSpinlock.
        )pbdoc")
        .def(py::init<DarwinSpinlock&>(), py::arg("mutex"),
             R"pbdoc(
             Constructs the scoped lock and acquires the Darwin spinlock.

             Args:
                 mutex: The DarwinSpinlock to guard.
             )pbdoc")
        .def("unlock", &ScopedLock<DarwinSpinlock>::unlock,
             R"pbdoc(
             Manually release the lock before destruction.
             )pbdoc");
#endif

#ifdef ATOM_PLATFORM_LINUX
    // LinuxFutexLock class
    py::class_<LinuxFutexLock>(m, "LinuxFutexLock",
                               R"pbdoc(
        Linux platform-specific spinlock implementation.

        Uses futex system call for optimized long waits, providing
        better performance on Linux systems.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &LinuxFutexLock::lock,
             R"pbdoc(
             Acquire the lock using Linux futex system call.
             )pbdoc")
        .def("unlock", &LinuxFutexLock::unlock,
             R"pbdoc(
             Release the lock and notify waiting threads.
             )pbdoc")
        .def("try_lock", &LinuxFutexLock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");

    // ScopedLock for LinuxFutexLock
    py::class_<ScopedLock<LinuxFutexLock>>(m, "ScopedLinuxFutexLock",
                                           R"pbdoc(
        RAII scoped lock guard for LinuxFutexLock.
        )pbdoc")
        .def(py::init<LinuxFutexLock&>(), py::arg("mutex"),
             R"pbdoc(
             Constructs the scoped lock and acquires the Linux futex lock.

             Args:
                 mutex: The LinuxFutexLock to guard.
             )pbdoc")
        .def("unlock", &ScopedLock<LinuxFutexLock>::unlock,
             R"pbdoc(
             Manually release the lock before destruction.
             )pbdoc");
#endif

#ifdef ATOM_HAS_ATOMIC_WAIT
    // AtomicWaitLock class
    py::class_<AtomicWaitLock>(m, "AtomicWaitLock",
                               R"pbdoc(
        C++20 atomic wait/notify spinlock implementation.

        More efficient than plain spinlocks if supported by hardware.
        Uses atomic wait/notify operations for better performance.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &AtomicWaitLock::lock,
             R"pbdoc(
             Acquire the lock using atomic wait/notify.
             )pbdoc")
        .def("unlock", &AtomicWaitLock::unlock,
             R"pbdoc(
             Release the lock and notify waiting threads.
             )pbdoc")
        .def("try_lock", &AtomicWaitLock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");

    // ScopedLock for AtomicWaitLock
    py::class_<ScopedLock<AtomicWaitLock>>(m, "ScopedAtomicWaitLock",
                                           R"pbdoc(
        RAII scoped lock guard for AtomicWaitLock.
        )pbdoc")
        .def(py::init<AtomicWaitLock&>(), py::arg("mutex"),
             R"pbdoc(
             Constructs the scoped lock and acquires the atomic wait lock.

             Args:
                 mutex: The AtomicWaitLock to guard.
             )pbdoc")
        .def("unlock", &ScopedLock<AtomicWaitLock>::unlock,
             R"pbdoc(
             Manually release the lock before destruction.
             )pbdoc");
#endif

    // Platform-specific and optional lock types will be conditionally available
    // based on compile-time configuration

#ifdef ATOM_USE_BOOST_LOCKS
    // BoostSpinlock class
    py::class_<BoostSpinlock>(m, "BoostSpinlock",
                              R"pbdoc(
        Boost-based spinlock implementation.

        Alternative spinlock implementation using Boost.Atomic for specialized scenarios.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &BoostSpinlock::lock,
             R"pbdoc(
             Acquire the lock.
             )pbdoc")
        .def("unlock", &BoostSpinlock::unlock,
             R"pbdoc(
             Release the lock.
             )pbdoc")
        .def("try_lock", &BoostSpinlock::tryLock,
             R"pbdoc(
             Try to acquire the lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");

    // BoostSharedMutex class
    py::class_<BoostSharedMutex>(m, "BoostSharedMutex",
                                 R"pbdoc(
        Boost-based shared mutex implementation.

        Alternative to std::shared_mutex using Boost.Thread implementation.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &BoostSharedMutex::lock,
             R"pbdoc(
             Acquire exclusive lock.
             )pbdoc")
        .def("unlock", &BoostSharedMutex::unlock,
             R"pbdoc(
             Release exclusive lock.
             )pbdoc")
        .def("try_lock", &BoostSharedMutex::tryLock,
             R"pbdoc(
             Try to acquire exclusive lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc")
        .def("lock_shared", &BoostSharedMutex::lockShared,
             R"pbdoc(
             Acquire shared (read) lock.
             )pbdoc")
        .def("unlock_shared", &BoostSharedMutex::unlockShared,
             R"pbdoc(
             Release shared (read) lock.
             )pbdoc")
        .def("try_lock_shared", &BoostSharedMutex::tryLockShared,
             R"pbdoc(
             Try to acquire shared lock without blocking.

             Returns:
                 bool: True if the shared lock was acquired, False otherwise.
             )pbdoc");

    // BoostSharedMutex::SharedLock class
    py::class_<BoostSharedMutex::SharedLock>(m, "BoostSharedLock",
                                             R"pbdoc(
        RAII shared lock guard for BoostSharedMutex.

        Automatically acquires a shared lock on construction and releases it on destruction.
        )pbdoc")
        .def(py::init<BoostSharedMutex&>(), py::arg("mutex"),
             R"pbdoc(
             Construct the shared lock guard and acquire the shared lock.

             Args:
                 mutex: The BoostSharedMutex to guard.
             )pbdoc")
        .def("unlock", &BoostSharedMutex::SharedLock::unlock,
             R"pbdoc(
             Manually release the shared lock before destruction.
             )pbdoc")
        .def("is_locked", &BoostSharedMutex::SharedLock::isLocked,
             R"pbdoc(
             Check if the shared lock is currently held.

             Returns:
                 bool: True if the shared lock is held, False otherwise.
             )pbdoc");

    // BoostRecursiveMutex class
    py::class_<BoostRecursiveMutex>(m, "BoostRecursiveMutex",
                                    R"pbdoc(
        Boost-based recursive mutex implementation.

        A mutex that can be locked multiple times by the same thread without deadlocking.
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Default constructor.
             )pbdoc")
        .def("lock", &BoostRecursiveMutex::lock,
             R"pbdoc(
             Acquire the recursive lock.
             )pbdoc")
        .def("unlock", &BoostRecursiveMutex::unlock,
             R"pbdoc(
             Release the recursive lock.
             )pbdoc")
        .def("try_lock", &BoostRecursiveMutex::tryLock,
             R"pbdoc(
             Try to acquire the recursive lock without blocking.

             Returns:
                 bool: True if the lock was acquired, False otherwise.
             )pbdoc");
#endif

    // LockFactory enum and factory methods
    py::enum_<LockFactory::LockType>(m, "LockType",
                                     R"pbdoc(
        Enumeration of available lock types.

        Different lock types provide different performance characteristics
        and are optimized for different use cases and platforms.
        )pbdoc")
        .value("SPINLOCK", LockFactory::LockType::SPINLOCK,
               "Basic spinlock implementation")
        .value("TICKET_SPINLOCK", LockFactory::LockType::TICKET_SPINLOCK,
               "Fair ticket-based spinlock")
        .value("UNFAIR_SPINLOCK", LockFactory::LockType::UNFAIR_SPINLOCK,
               "High-performance unfair spinlock")
        .value("ADAPTIVE_SPINLOCK", LockFactory::LockType::ADAPTIVE_SPINLOCK,
               "Adaptive spinlock that yields after spinning")
#ifdef ATOM_HAS_ATOMIC_WAIT
        .value("ATOMIC_WAIT_LOCK", LockFactory::LockType::ATOMIC_WAIT_LOCK,
               "C++20 atomic wait/notify lock")
#endif
#ifdef ATOM_PLATFORM_WINDOWS
        .value("WINDOWS_SPINLOCK", LockFactory::LockType::WINDOWS_SPINLOCK,
               "Windows-specific critical section spinlock")
#endif
#ifdef ATOM_PLATFORM_MACOS
        .value("DARWIN_SPINLOCK", LockFactory::LockType::DARWIN_SPINLOCK,
               "macOS-specific os_unfair_lock spinlock")
#endif
#ifdef ATOM_PLATFORM_LINUX
        .value("LINUX_FUTEX_LOCK", LockFactory::LockType::LINUX_FUTEX_LOCK,
               "Linux-specific futex-based lock")
#endif
        .value("STD_MUTEX", LockFactory::LockType::STD_MUTEX,
               "Standard library mutex")
        .value("STD_RECURSIVE_MUTEX",
               LockFactory::LockType::STD_RECURSIVE_MUTEX,
               "Standard library recursive mutex")
        .value("STD_SHARED_MUTEX", LockFactory::LockType::STD_SHARED_MUTEX,
               "Standard library shared mutex")
        .value("AUTO_OPTIMIZED", LockFactory::LockType::AUTO_OPTIMIZED,
               "Automatically select the best lock for the platform")
        .export_values();

    // LockFactory static methods
    m.def("create_lock", &LockFactory::createLock, py::arg("lock_type"),
          R"pbdoc(
          Create a lock of the specified type.

          Args:
              lock_type: The type of lock to create.

          Returns:
              A unique pointer to the created lock.

          Raises:
              ValueError: If the lock type is invalid or not available.
          )pbdoc");

    m.def("create_optimized_lock", &LockFactory::createOptimizedLock,
          R"pbdoc(
          Create the most optimal lock implementation for the current platform.

          Returns:
              A unique pointer to the lock optimized for the current platform.
          )pbdoc");

    // Utility functions for lock performance optimization
    m.def("cpu_relax", []() { cpu_relax(); }, R"pbdoc(
    Execute a CPU relax instruction to improve spinlock performance.

    This is a platform-specific instruction that hints to the CPU that
    we are in a spin-wait loop, allowing for better power management
    and performance. Should be called in tight spin loops.

    Example:
        >>> import time
        >>> from atom.async.lock import Spinlock, cpu_relax
        >>> lock = Spinlock()
        >>> # In a spin loop waiting for a condition
        >>> while not some_condition():
        ...     cpu_relax()  # Hint to CPU we're spinning
        ...     time.sleep(0.001)
    )pbdoc");

    // Lock performance and debugging utilities

    m.def(
        "get_hardware_concurrency",
        []() { return std::thread::hardware_concurrency(); }, R"pbdoc(
    Get the number of concurrent threads supported by the implementation.

    Returns:
        int: Number of concurrent threads supported, or 0 if not determinable.

    This can be useful for determining optimal spin counts or deciding
    between different lock implementations.

    Example:
        >>> from atom.async.lock import get_hardware_concurrency
        >>> cores = get_hardware_concurrency()
        >>> print(f"System has {cores} hardware threads")
    )pbdoc");

    // Module provides comprehensive lock implementations for high-performance
    // scenarios
}
