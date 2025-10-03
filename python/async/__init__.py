"""
Atom Async Module
================

This module provides comprehensive asynchronous programming utilities and data structures
for high-performance concurrent applications with Python bindings for C++20 features.

Core Features:
- Asynchronous execution with futures and promises
- Thread-safe data structures and synchronization primitives
- Lock-free data structures for high-performance scenarios
- Signal-slot system for event-driven programming
- Timer and scheduling utilities with precise timing control
- C++20 coroutine-based generators for lazy evaluation
- Daemon process management for background services

Modules:
- async: Core async functionality with workers and managers
- future: Enhanced futures with C++20 coroutine support
- promise: Promise-based asynchronous programming
- lock: Thread synchronization primitives and lock-free structures
- thread_wrapper: Enhanced thread management utilities
- threadlocal: Thread-local storage with advanced features
- message_bus: Publish-subscribe messaging system
- message_queue: Priority-based message queuing
- queue: Thread-safe and lock-free queue implementations
- eventstack: Thread-safe stack for event management
- async_executor: High-performance async task execution
- pool: Thread pool management with work-stealing
- parallel: Parallel algorithms with SIMD optimizations
- packaged_task: Enhanced packaged tasks with callbacks
- trigger: Event-driven callback management
- limiter: Rate limiting and flow control
- safetype: Thread-safe type wrappers
- slot: Signal-slot implementation for observer pattern
- timer: High-precision timer and scheduling
- generator: C++20 coroutine-based generators
- daemon: Daemon process management

Example:
    >>> from atom.async import Timer, Signal, LockFreeStack
    >>>
    >>> # Timer usage
    >>> timer = Timer()
    >>> timer.set_timeout(lambda: print("Hello!"), 1000)
    >>>
    >>> # Signal-slot usage
    >>> signal = Signal()
    >>> signal.connect(lambda data: print(f"Received: {data}"))
    >>> signal.emit("Hello, World!")
    >>>
    >>> # Lock-free data structure usage
    >>> stack = LockFreeStack()
    >>> stack.push("item1")
    >>> item = stack.pop()
"""

__version__ = "1.0.0"
__author__ = "Atom Development Team"

# Import all modules to make them available
# Note: We use importlib to handle the 'async' module since it's a Python keyword
import importlib
import warnings

_modules = {}

def _import_module(name, alias=None):
    """Helper function to import modules with error handling."""
    try:
        if name == "async":
            # Special handling for async module since it's a Python keyword
            module = importlib.import_module(f".{name}", package=__name__)
        else:
            module = importlib.import_module(f".{name}", package=__name__)

        _modules[alias or name] = module
        globals()[alias or name] = module
        return module
    except ImportError as e:
        warnings.warn(f"Failed to import {name} module: {e}", ImportWarning)
        return None

# Import all modules
_import_module("async", "async_core")  # Import async as async_core to avoid keyword conflict
_import_module("future")
_import_module("promise")
_import_module("lock")
_import_module("thread_wrapper")
_import_module("threadlocal")
_import_module("message_bus")
_import_module("message_queue")
_import_module("queue")
_import_module("eventstack")
_import_module("async_executor")
_import_module("pool")
_import_module("parallel")
_import_module("packaged_task")
_import_module("trigger")
_import_module("limiter")
_import_module("safetype")
_import_module("slot")
_import_module("timer")
_import_module("generator")
_import_module("daemon")

# Re-export commonly used classes and functions for convenience
__all__ = [
    # Core modules
    "async_core", "future", "promise", "lock", "thread_wrapper", "threadlocal",

    # Messaging modules
    "message_bus", "message_queue", "queue", "eventstack",

    # Execution modules
    "async_executor", "pool", "parallel", "packaged_task",

    # Synchronization modules
    "trigger", "limiter", "safetype", "slot",

    # Utility modules
    "timer", "generator", "daemon",
]

# Add convenience functions for module access
def get_module(name):
    """Get a module by name.

    Args:
        name: Module name (use 'async' for the core async module)

    Returns:
        The requested module or None if not available

    Examples:
        >>> async_mod = get_module('async')
        >>> timer_mod = get_module('timer')
    """
    if name == "async":
        return _modules.get("async_core")
    return _modules.get(name)

def list_modules():
    """List all available modules.

    Returns:
        dict: Dictionary mapping module names to descriptions
    """
    return __modules__.copy()

def get_module_info(name):
    """Get information about a specific module.

    Args:
        name: Module name

    Returns:
        str: Module description or None if not found
    """
    if name == "async":
        return __modules__.get("async")
    return __modules__.get(name)

# Module metadata
__modules__ = {
    "async": "Core async functionality with workers and managers",
    "future": "Enhanced futures with C++20 coroutine support",
    "promise": "Promise-based asynchronous programming",
    "lock": "Thread synchronization primitives and lock-free structures",
    "thread_wrapper": "Enhanced thread management utilities",
    "threadlocal": "Thread-local storage with advanced features",
    "message_bus": "Publish-subscribe messaging system",
    "message_queue": "Priority-based message queuing",
    "queue": "Thread-safe and lock-free queue implementations",
    "eventstack": "Thread-safe stack for event management",
    "async_executor": "High-performance async task execution",
    "pool": "Thread pool management with work-stealing",
    "parallel": "Parallel algorithms with SIMD optimizations",
    "packaged_task": "Enhanced packaged tasks with callbacks",
    "trigger": "Event-driven callback management",
    "limiter": "Rate limiting and flow control",
    "safetype": "Thread-safe type wrappers",
    "slot": "Signal-slot implementation for observer pattern",
    "timer": "High-precision timer and scheduling",
    "generator": "C++20 coroutine-based generators",
    "daemon": "Daemon process management",
}
