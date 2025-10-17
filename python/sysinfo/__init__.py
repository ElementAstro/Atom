"""
System Information Module for the Atom Package

This module provides comprehensive system information gathering capabilities
including hardware details, operating system information, network status,
and system monitoring functionality.

The module is organized into several submodules:

Hardware Information:
    - battery: Battery status, monitoring, and power management
    - bios: BIOS information and management
    - cpu: CPU information, monitoring, and performance metrics
    - disk: Storage device information and monitoring
    - gpu: Graphics card and monitor information
    - memory: Memory information and performance monitoring

System Information:
    - os: Operating system information and utilities
    - locale: System locale and language information
    - sn: Hardware serial number access
    - virtual: Virtualization and container detection
    - wm: Window manager and desktop environment information

Network Information:
    - wifi: Network interface and WiFi information

Utilities:
    - sysinfo_printer: System information formatting and reporting

Examples:
    >>> from atom import sysinfo
    >>>
    >>> # Get CPU information
    >>> cpu_info = sysinfo.cpu.get_cpu_info()
    >>> print(f"CPU: {cpu_info.model}")
    >>>
    >>> # Get memory information
    >>> memory_info = sysinfo.memory.get_detailed_memory_stats()
    >>> print(f"Memory: {memory_info.total_physical_memory / (1024**3):.1f} GB")
    >>>
    >>> # Get battery information
    >>> battery_info = sysinfo.battery.get_battery_info()
    >>> if battery_info and battery_info.is_battery_present:
    ...     print(f"Battery: {battery_info.battery_life_percent}%")
    >>>
    >>> # Get operating system information
    >>> os_info = sysinfo.os.get_operating_system_info()
    >>> print(f"OS: {os_info.os_name} {os_info.os_version}")
"""

__version__ = "1.0.0"
__author__ = "Atom Development Team"

# Import all submodules to make them available
try:
    from . import battery
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import battery module: {e}", ImportWarning)
    battery = None

try:
    from . import bios
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import bios module: {e}", ImportWarning)
    bios = None

try:
    from . import cpu
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import cpu module: {e}", ImportWarning)
    cpu = None

try:
    from . import disk
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import disk module: {e}", ImportWarning)
    disk = None

try:
    from . import gpu
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import gpu module: {e}", ImportWarning)
    gpu = None

try:
    from . import locale
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import locale module: {e}", ImportWarning)
    locale = None

try:
    from . import memory
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import memory module: {e}", ImportWarning)
    memory = None

try:
    from . import os
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import os module: {e}", ImportWarning)
    os = None

try:
    from . import sn
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import sn module: {e}", ImportWarning)
    sn = None

try:
    from . import sysinfo_printer
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import sysinfo_printer module: {e}", ImportWarning)
    sysinfo_printer = None

try:
    from . import virtual
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import virtual module: {e}", ImportWarning)
    virtual = None

try:
    from . import wifi
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import wifi module: {e}", ImportWarning)
    wifi = None

try:
    from . import wm
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import wm module: {e}", ImportWarning)
    wm = None

# List of all available modules
__all__ = [
    'battery',
    'bios',
    'cpu',
    'disk',
    'gpu',
    'locale',
    'memory',
    'os',
    'sn',
    'sysinfo_printer',
    'virtual',
    'wifi',
    'wm',
    'get_system_summary',
    'get_available_modules',
    'check_module_availability'
]

def get_available_modules():
    """Get a list of successfully imported modules.

    Returns:
        List of module names that were successfully imported.

    Examples:
        >>> from atom import sysinfo
        >>> available = sysinfo.get_available_modules()
        >>> print(f"Available modules: {', '.join(available)}")
    """
    modules = []
    for module_name in ['battery', 'bios', 'cpu', 'disk', 'gpu', 'locale',
                       'memory', 'os', 'sn', 'sysinfo_printer', 'virtual',
                       'wifi', 'wm']:
        if globals().get(module_name) is not None:
            modules.append(module_name)
    return modules

def check_module_availability(module_name):
    """Check if a specific module is available.

    Args:
        module_name: Name of the module to check

    Returns:
        Boolean indicating whether the module is available

    Examples:
        >>> from atom import sysinfo
        >>> if sysinfo.check_module_availability('battery'):
        ...     battery_info = sysinfo.battery.get_battery_info()
    """
    return globals().get(module_name) is not None

def get_system_summary():
    """Get a comprehensive summary of system information.

    This function attempts to gather basic information from all available
    modules and returns a summary dictionary.

    Returns:
        Dictionary containing system information from available modules

    Examples:
        >>> from atom import sysinfo
        >>> summary = sysinfo.get_system_summary()
        >>> print("System Summary:")
        >>> for category, info in summary.items():
        ...     print(f"  {category}: {info}")
    """
    summary = {}

    # CPU Information
    if cpu is not None:
        try:
            cpu_info = cpu.get_cpu_info()
            summary['cpu'] = f"{cpu_info.model} ({cpu_info.num_physical_cores} cores)"
        except Exception:
            summary['cpu'] = "CPU information unavailable"

    # Memory Information
    if memory is not None:
        try:
            mem_info = memory.get_detailed_memory_stats()
            total_gb = mem_info.total_physical_memory / (1024**3)
            usage_pct = mem_info.memory_load_percentage
            summary['memory'] = f"{total_gb:.1f} GB ({usage_pct:.1f}% used)"
        except Exception:
            summary['memory'] = "Memory information unavailable"

    # Operating System Information
    if os is not None:
        try:
            os_info = os.get_operating_system_info()
            summary['operating_system'] = f"{os_info.os_name} {os_info.os_version}"
        except Exception:
            summary['operating_system'] = "OS information unavailable"

    # Battery Information
    if battery is not None:
        try:
            battery_info = battery.get_battery_info()
            if battery_info and battery_info.is_battery_present:
                status = "charging" if battery_info.is_charging else "discharging"
                summary['battery'] = f"{battery_info.battery_life_percent}% ({status})"
            else:
                summary['battery'] = "No battery detected"
        except Exception:
            summary['battery'] = "Battery information unavailable"

    # GPU Information
    if gpu is not None:
        try:
            gpu_info = gpu.get_gpu_info()
            summary['gpu'] = gpu_info[:100] + "..." if len(gpu_info) > 100 else gpu_info
        except Exception:
            summary['gpu'] = "GPU information unavailable"

    # Virtualization Information
    if virtual is not None:
        try:
            is_vm = virtual.is_virtual_machine()
            is_container = virtual.is_container()
            if is_vm:
                vm_type = virtual.get_virtualization_type()
                summary['virtualization'] = f"Virtual Machine ({vm_type})"
            elif is_container:
                container_type = virtual.get_container_type()
                summary['virtualization'] = f"Container ({container_type})"
            else:
                summary['virtualization'] = "Physical Hardware"
        except Exception:
            summary['virtualization'] = "Virtualization detection unavailable"

    # Available modules
    summary['available_modules'] = get_available_modules()

    return summary
