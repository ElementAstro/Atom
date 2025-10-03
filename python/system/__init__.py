"""
System Module for Atom Package
===============================

This module provides comprehensive system-level functionality including:

Hardware Management:
    - GPIO control and monitoring
    - Device enumeration (USB, Serial, Bluetooth)
    - Hardware voltage monitoring

Network Management:
    - Network interface management
    - Virtual network adapter creation
    - DNS configuration
    - Connection monitoring

System Information:
    - User and environment information
    - Software installation and management
    - System statistics and monitoring
    - Process management

System Operations:
    - Power management (shutdown, reboot, hibernate)
    - Clipboard operations
    - Command execution
    - Signal handling
    - Storage monitoring

Security and Monitoring:
    - Keyboard shortcut detection
    - Process monitoring
    - Registry operations (Windows)
    - Crash handling and debugging

Examples:
    >>> from atom.system import gpio, power, clipboard
    >>>
    >>> # GPIO operations
    >>> led = gpio.GPIO("18", gpio.Direction.OUTPUT)
    >>> led.set_value(True)
    >>>
    >>> # Power management
    >>> # power.shutdown()  # Be careful!
    >>>
    >>> # Clipboard operations
    >>> clip = clipboard.Clipboard.instance()
    >>> clip.set_text("Hello from Python!")
    >>> text = clip.get_text()

Available Modules:
    Hardware:
        - gpio: GPIO pin control and monitoring
        - device: Hardware device enumeration
        - voltage: System voltage monitoring

    Network:
        - network_manager: Network interface management
        - virtual_network: Virtual network adapter management

    System Info:
        - user: User and environment information
        - software: Software installation and management
        - stat: System statistics
        - env: Environment variable management

    System Operations:
        - power: Power management operations
        - clipboard: Clipboard operations
        - command: Command execution
        - process: Process management
        - process_manager: Advanced process management
        - storage: Storage monitoring

    Security/Monitoring:
        - shortcut: Keyboard shortcut detection
        - signal: Signal handling
        - signal_monitor: Signal monitoring
        - signal_utils: Signal utilities
        - pidwatcher: Process ID monitoring
        - crash_quotes: Crash handling with quotes
        - crontab: Cron job management

    Registry (Windows):
        - registry: Linux registry operations
        - wregistry: Windows registry operations

    Scheduling:
        - priority: Process priority management

Note:
    Some modules may require elevated privileges or specific hardware/OS support.
    Platform-specific modules will gracefully handle unsupported platforms.
"""

__version__ = "1.0.0"
__author__ = "Atom Development Team"

# Import all available modules with error handling
__all__ = []

def _import_module(module_name, display_name=None):
    """Helper function to safely import modules."""
    if display_name is None:
        display_name = module_name
    try:
        globals()[display_name] = __import__(f'.{module_name}', package=__name__, level=1)
        __all__.append(display_name)
        return True
    except ImportError as e:
        print(f"Warning: Could not import {module_name} module: {e}")
        return False

# Hardware modules
_import_module("gpio")
_import_module("device")
_import_module("voltage")

# Network modules
_import_module("network_manager")
_import_module("virtual_network")

# System information modules
_import_module("user")
_import_module("software")
_import_module("stat")
_import_module("env")

# System operation modules
_import_module("power")
_import_module("clipboard")
_import_module("command")
_import_module("process")
_import_module("process_info")
_import_module("process_manager")
_import_module("storage")

# Security and monitoring modules
_import_module("shortcut")
_import_module("signal")
_import_module("signal_monitor")
_import_module("signal_utils")
_import_module("pidwatcher")
_import_module("crash_quotes")
_import_module("crontab")

# Registry modules (platform-specific)
_import_module("registry")
_import_module("wregistry")

# Scheduling modules
_import_module("priority")

def get_available_modules():
    """
    Get a list of successfully imported modules.

    Returns:
        List of module names that were successfully imported.

    Examples:
        >>> from atom.system import get_available_modules
        >>> modules = get_available_modules()
        >>> print(f"Available modules: {modules}")
    """
    return __all__.copy()

def module_info():
    """
    Get information about the system module.

    Returns:
        Dictionary containing module information.

    Examples:
        >>> from atom.system import module_info
        >>> info = module_info()
        >>> print(f"System module version: {info['version']}")
        >>> print(f"Available modules: {len(info['available_modules'])}")
    """
    return {
        'version': __version__,
        'author': __author__,
        'description': 'Comprehensive system-level functionality for the Atom package',
        'available_modules': get_available_modules(),
        'total_modules': len(__all__),
        'categories': {
            'hardware': ['gpio', 'device', 'voltage'],
            'network': ['network_manager', 'virtual_network'],
            'system_info': ['user', 'software', 'stat', 'env'],
            'system_ops': ['power', 'clipboard', 'command', 'process', 'process_info', 'process_manager', 'storage'],
            'security_monitoring': ['shortcut', 'signal', 'signal_monitor', 'signal_utils', 'pidwatcher', 'crash_quotes', 'crontab'],
            'registry': ['registry', 'wregistry'],
            'scheduling': ['priority']
        }
    }
