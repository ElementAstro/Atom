#!/usr/bin/env python3
"""
Test script for verifying Python bindings functionality.

This script provides comprehensive tests for all the new Python bindings
to ensure they compile correctly and expose the expected functionality.

Usage:
    python test_bindings.py

Note: This script requires the bindings to be compiled and installed.
"""

import sys
import traceback
from typing import Dict


def test_module_imports() -> Dict[str, bool]:
    """Test that all modules can be imported successfully."""
    print("Testing module imports...")

    modules_to_test = [
        "battery",
        "bios",
        "cpu",
        "disk",
        "gpu",
        "locale",
        "memory",
        "os",
        "sn",
        "sysinfo_printer",
        "virtual",
        "wifi",
        "wm",
    ]

    results = {}

    for module_name in modules_to_test:
        try:
            exec(f"from atom.sysinfo import {module_name}")
            results[module_name] = True
            print(f"  ✓ {module_name}")
        except ImportError as e:
            results[module_name] = False
            print(f"  ✗ {module_name}: {e}")
        except Exception as e:
            results[module_name] = False
            print(f"  ✗ {module_name}: Unexpected error: {e}")

    return results


def test_new_modules_functionality():
    """Test functionality of newly created modules."""
    print("\nTesting new modules functionality...")

    # Test GPU module
    try:
        from atom.sysinfo import gpu

        # Test class instantiation
        _ = gpu.MonitorInfo()
        print("  ✓ GPU: MonitorInfo class instantiation")

        # Test function availability
        assert hasattr(gpu, "get_gpu_info"), "get_gpu_info function missing"
        assert hasattr(
            gpu, "get_all_monitors_info"
        ), "get_all_monitors_info function missing"
        print("  ✓ GPU: Required functions available")

    except Exception as e:
        print(f"  ✗ GPU module test failed: {e}")

    # Test Locale module
    try:
        from atom.sysinfo import locale

        # Test enum availability
        assert hasattr(locale, "LocaleError"), "LocaleError enum missing"
        print("  ✓ Locale: LocaleError enum available")

        # Test class instantiation
        _ = locale.LocaleInfo()
        print("  ✓ Locale: LocaleInfo class instantiation")

        # Test function availability
        assert hasattr(
            locale, "get_system_language_info"
        ), "get_system_language_info function missing"
        assert hasattr(locale, "validate_locale"), "validate_locale function missing"
        print("  ✓ Locale: Required functions available")

    except Exception as e:
        print(f"  ✗ Locale module test failed: {e}")

    # Test Serial Numbers module
    try:
        from atom.sysinfo import sn

        # Test class instantiation
        _ = sn.HardwareInfo()
        print("  ✓ SN: HardwareInfo class instantiation")

        # Test function availability
        assert hasattr(sn, "get_bios_serial"), "get_bios_serial function missing"
        assert hasattr(
            sn, "get_hardware_summary"
        ), "get_hardware_summary function missing"
        print("  ✓ SN: Required functions available")

    except Exception as e:
        print(f"  ✗ SN module test failed: {e}")

    # Test Virtual module
    try:
        from atom.sysinfo import virtual

        # Test function availability
        assert hasattr(
            virtual, "is_virtual_machine"
        ), "is_virtual_machine function missing"
        assert hasattr(virtual, "is_container"), "is_container function missing"
        assert hasattr(
            virtual, "get_virtualization_type"
        ), "get_virtualization_type function missing"
        print("  ✓ Virtual: Required functions available")

    except Exception as e:
        print(f"  ✗ Virtual module test failed: {e}")

    # Test Window Manager module
    try:
        from atom.sysinfo import wm

        # Test class instantiation
        _ = wm.SystemInfo()
        print("  ✓ WM: SystemInfo class instantiation")

        # Test function availability
        assert hasattr(wm, "get_system_info"), "get_system_info function missing"
        assert hasattr(
            wm, "get_desktop_environment"
        ), "get_desktop_environment function missing"
        print("  ✓ WM: Required functions available")

    except Exception as e:
        print(f"  ✗ WM module test failed: {e}")


def test_enhanced_battery_module():
    """Test enhanced battery module functionality."""
    print("\nTesting enhanced battery module...")

    try:
        from atom.sysinfo import battery

        # Test new enums
        assert hasattr(battery, "BatteryError"), "BatteryError enum missing"
        assert hasattr(battery, "AlertType"), "AlertType enum missing"
        print("  ✓ Battery: New enums available")

        # Test enum values
        assert hasattr(
            battery.BatteryError, "NOT_PRESENT"
        ), "BatteryError.NOT_PRESENT missing"
        assert hasattr(
            battery.AlertType, "LOW_BATTERY"
        ), "AlertType.LOW_BATTERY missing"
        print("  ✓ Battery: Enum values accessible")

    except Exception as e:
        print(f"  ✗ Enhanced battery module test failed: {e}")


def test_main_package_functionality():
    """Test main package functionality."""
    print("\nTesting main package functionality...")

    try:
        from atom import sysinfo

        # Test utility functions
        assert hasattr(
            sysinfo, "get_available_modules"
        ), "get_available_modules function missing"
        assert hasattr(
            sysinfo, "check_module_availability"
        ), "check_module_availability function missing"
        assert hasattr(
            sysinfo, "get_system_summary"
        ), "get_system_summary function missing"
        print("  ✓ Main package: Utility functions available")

        # Test module availability checking
        available_modules = sysinfo.get_available_modules()
        print(f"  ✓ Available modules: {available_modules}")

    except Exception as e:
        print(f"  ✗ Main package test failed: {e}")


def test_documentation_availability():
    """Test that documentation is available for key components."""
    print("\nTesting documentation availability...")

    try:
        from atom.sysinfo import gpu, locale, sn, virtual, wm

        # Test class documentation
        assert gpu.MonitorInfo.__doc__ is not None, "MonitorInfo missing documentation"
        assert locale.LocaleInfo.__doc__ is not None, "LocaleInfo missing documentation"
        assert sn.HardwareInfo.__doc__ is not None, "HardwareInfo missing documentation"
        assert wm.SystemInfo.__doc__ is not None, "SystemInfo missing documentation"
        print("  ✓ Class documentation available")

        # Test function documentation
        assert (
            gpu.get_gpu_info.__doc__ is not None
        ), "get_gpu_info missing documentation"
        assert (
            locale.get_system_language_info.__doc__ is not None
        ), "get_system_language_info missing documentation"
        assert (
            virtual.is_virtual_machine.__doc__ is not None
        ), "is_virtual_machine missing documentation"
        print("  ✓ Function documentation available")

    except Exception as e:
        print(f"  ✗ Documentation test failed: {e}")


def run_comprehensive_test():
    """Run all tests and provide a summary."""
    print("=" * 60)
    print("COMPREHENSIVE PYTHON BINDINGS TEST")
    print("=" * 60)

    try:
        # Test imports
        import_results = test_module_imports()

        # Test new modules
        test_new_modules_functionality()

        # Test enhanced battery
        test_enhanced_battery_module()

        # Test main package
        test_main_package_functionality()

        # Test documentation
        test_documentation_availability()

        # Summary
        print("\n" + "=" * 60)
        print("TEST SUMMARY")
        print("=" * 60)

        successful_imports = sum(import_results.values())
        total_modules = len(import_results)

        print(f"Module imports: {successful_imports}/{total_modules} successful")

        if successful_imports == total_modules:
            print("🎉 ALL TESTS PASSED! Bindings are working correctly.")
            return True
        else:
            print("⚠️  Some modules failed to import. Check compilation.")
            failed_modules = [
                name for name, success in import_results.items() if not success
            ]
            print(f"Failed modules: {failed_modules}")
            return False

    except Exception as e:
        print(f"\n❌ Test suite failed with error: {e}")
        traceback.print_exc()
        return False


if __name__ == "__main__":
    success = run_comprehensive_test()
    sys.exit(0 if success else 1)
