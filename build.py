#!/usr/bin/env python3
"""
Enhanced build system for Atom project
Supports both CMake and XMake with advanced configuration management
Author: Max Qian
"""

import os
import sys
import subprocess
import argparse
import json
import yaml
import shutil
import multiprocessing
import platform
import time
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from concurrent.futures import ThreadPoolExecutor, as_completed
import psutil
from loguru import logger

# Configure loguru logging
logger.remove()  # Remove default handler
logger.add(
    sys.stderr,
    format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
    level="INFO",
    colorize=True
)
logger.add(
    "build.log",
    format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {name}:{function}:{line} - {message}",
    level="DEBUG",
    rotation="10 MB",
    retention="7 days",
    compression="gz"
)


class SystemCapabilities:
    """Cached system capabilities for performance"""
    _instance = None
    _capabilities = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    @property
    def capabilities(self) -> Dict[str, Any]:
        if self._capabilities is None:
            self._capabilities = self._detect_capabilities()
        return self._capabilities

    def _detect_capabilities(self) -> Dict[str, Any]:
        """Detect system capabilities once and cache"""
        logger.debug("Detecting system capabilities...")

        capabilities = {
            'cpu_cores': multiprocessing.cpu_count(),
            'platform': platform.system().lower(),
            'architecture': platform.machine(),
            'python_version': platform.python_version(),
        }

        # Use psutil for better memory detection
        try:
            memory = psutil.virtual_memory()
            capabilities['memory_gb'] = memory.total / (1024 ** 3)
            capabilities['memory_available_gb'] = memory.available / \
                (1024 ** 3)
        except Exception as e:
            logger.warning(f"Could not detect memory with psutil: {e}")
            capabilities['memory_gb'] = 8  # Default
            capabilities['memory_available_gb'] = 6

        # Detect available tools in parallel
        tools = ['cmake', 'ninja', 'xmake', 'ccache',
                 'doxygen', 'git', 'clang', 'gcc']
        with ThreadPoolExecutor(max_workers=4) as executor:
            futures = {executor.submit(
                shutil.which, tool): tool for tool in tools}
            for future in as_completed(futures):
                tool = futures[future]
                capabilities[f'has_{tool}'] = future.result() is not None

        logger.debug(f"Detected capabilities: {capabilities}")
        return capabilities


class ConfigManager:
    """Optimized configuration management with caching"""

    def __init__(self, config_file: Path):
        self.config_file = config_file
        self._config_cache = None
        self._config_mtime = None

    def get_config(self) -> Dict[str, Any]:
        """Get configuration with caching and file modification detection"""
        try:
            current_mtime = self.config_file.stat().st_mtime
            if self._config_cache is None or self._config_mtime != current_mtime:
                logger.debug(f"Loading configuration from {self.config_file}")
                with open(self.config_file, 'r') as f:
                    self._config_cache = yaml.safe_load(f) or {}
                self._config_mtime = current_mtime
            return self._config_cache
        except FileNotFoundError:
            logger.warning(
                f"Config file {self.config_file} not found, using defaults")
            return {}
        except Exception as e:
            logger.error(f"Error loading config: {e}")
            return {}


class BuildSystem:
    """Advanced build system for Atom project with optimizations"""

    def __init__(self):
        self.project_root = Path(__file__).parent
        self.build_dir = self.project_root / "build"
        self.config_manager = ConfigManager(
            self.project_root / "build-config.yaml")
        self.system_caps = SystemCapabilities()
        self.start_time = time.perf_counter()  # More precise timing

    @property
    def config(self) -> Dict[str, Any]:
        return self.config_manager.get_config()

    def _optimize_parallel_jobs(self, requested_jobs: Optional[int] = None) -> int:
        """Optimize number of parallel jobs based on system capabilities"""
        caps = self.system_caps.capabilities
        max_cores = caps['cpu_cores']
        available_memory = caps.get('memory_available_gb', 6)

        if requested_jobs:
            return min(requested_jobs, max_cores)

        # More sophisticated calculation
        # Consider both CPU and memory constraints
        memory_limited_jobs = max(
            1, int(available_memory / 1.5))  # 1.5GB per job

        # Consider platform-specific optimizations
        if caps['platform'] == 'linux':
            # Linux typically handles more parallel jobs better
            cpu_limited_jobs = max_cores
        else:
            # Be more conservative on other platforms
            cpu_limited_jobs = max(1, max_cores - 1)

        optimal_jobs = min(cpu_limited_jobs, memory_limited_jobs, 20)

        logger.info(
            f"System: {max_cores} cores, {available_memory:.1f}GB available memory")
        logger.info(f"Optimal parallel jobs: {optimal_jobs}")

        return optimal_jobs

    def _setup_ccache(self) -> bool:
        """Setup ccache if available with optimized configuration"""
        if not self.system_caps.capabilities.get('has_ccache', False):
            return False

        ccache_dir = Path.home() / ".ccache"
        ccache_dir.mkdir(exist_ok=True)

        # Optimized ccache configuration
        memory_gb = self.system_caps.capabilities.get('memory_gb', 8)
        # Scale with available memory
        cache_size = min(10, max(2, int(memory_gb / 2)))

        env_updates = {
            'CCACHE_DIR': str(ccache_dir),
            'CCACHE_MAXSIZE': f'{cache_size}G',
            'CCACHE_COMPRESS': '1',
            'CCACHE_COMPRESSLEVEL': '3',  # Faster compression
            'CCACHE_SLOPPINESS': 'file_macro,locale,time_macros',
            'CCACHE_MAXFILES': '50000'
        }

        for key, value in env_updates.items():
            os.environ[key] = value

        logger.success(f"ccache configured with {cache_size}G cache size")
        return True

    def _run_command(self, cmd: List[str], cwd: Optional[Path] = None,
                     env: Optional[Dict[str, str]] = None,
                     capture_output: bool = False) -> Tuple[bool, Optional[str]]:
        """Run a command with proper error handling and optional output capture"""
        logger.debug(f"Running: {' '.join(cmd)}")

        try:
            result = subprocess.run(
                cmd,
                cwd=cwd or self.project_root,
                env={**os.environ, **(env or {})},
                check=True,
                capture_output=capture_output,
                text=True if capture_output else None
            )
            return True, result.stdout if capture_output else None
        except subprocess.CalledProcessError as e:
            logger.error(f"Command failed with exit code {e.returncode}")
            if capture_output and e.stderr:
                logger.error(f"Error output: {e.stderr}")
            return False, None

    def _clean_build_directory(self):
        """Clean the build directory with progress indication"""
        if self.build_dir.exists():
            logger.info("Cleaning build directory...")
            # Use shutil.rmtree with error handling for better performance
            try:
                shutil.rmtree(self.build_dir, ignore_errors=True)
                logger.success("Build directory cleaned")
            except Exception as e:
                logger.warning(f"Some files could not be removed: {e}")

        self.build_dir.mkdir(parents=True, exist_ok=True)

    def _get_cmake_generator(self) -> List[str]:
        """Get optimal CMake generator"""
        if self.system_caps.capabilities.get('has_ninja', False):
            return ['-G', 'Ninja']
        elif platform.system() == 'Windows':
            return ['-G', 'Visual Studio 17 2022'] if shutil.which('devenv') else []
        return []  # Use default

    def _cmake_build(self, args: argparse.Namespace) -> bool:
        """Build using CMake with optimizations"""
        logger.info("Building with CMake...")

        # Prepare CMake arguments efficiently
        cmake_args = [
            'cmake',
            '-B', str(self.build_dir),
            '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
            '-DCMAKE_COLOR_DIAGNOSTICS=ON'  # Better output
        ]

        # Add generator
        cmake_args.extend(self._get_cmake_generator())

        # Build type
        build_type_map = {
            'debug': 'Debug',
            'release': 'Release',
            'relwithdebinfo': 'RelWithDebInfo',
            'minsizerel': 'MinSizeRel'
        }
        cmake_args.extend(
            ['-DCMAKE_BUILD_TYPE', build_type_map[args.build_type]])

        # Batch feature configuration
        features = [
            ('python', 'ATOM_BUILD_PYTHON_BINDINGS'),
            ('examples', 'ATOM_BUILD_EXAMPLES'),
            ('tests', 'ATOM_BUILD_TESTS'),
            ('docs', 'ATOM_BUILD_DOCS'),
            ('shared', 'BUILD_SHARED_LIBS'),
            ('cfitsio', 'ATOM_USE_CFITSIO'),
            ('ssh', 'ATOM_USE_SSH')
        ]

        feature_args = [f'-D{cmake_var}=ON'
                        for feature, cmake_var in features
                        if getattr(args, feature, False)]
        cmake_args.extend(feature_args)

        # Optimization options
        if args.lto:
            cmake_args.append('-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON')

        if args.coverage:
            cmake_args.extend([
                '-DCMAKE_CXX_FLAGS=--coverage',
                '-DCMAKE_C_FLAGS=--coverage'
            ])

        if args.sanitizers:
            sanitizer_flags = '-fsanitize=address,undefined -fno-omit-frame-pointer'
            cmake_args.extend([
                f'-DCMAKE_CXX_FLAGS={sanitizer_flags}',
                f'-DCMAKE_C_FLAGS={sanitizer_flags}'
            ])

        if args.install_prefix:
            cmake_args.extend(['-DCMAKE_INSTALL_PREFIX', args.install_prefix])

        # Configure
        configure_start = time.perf_counter()
        success, _ = self._run_command(cmake_args + [str(self.project_root)])
        if not success:
            return False

        configure_time = time.perf_counter() - configure_start
        logger.success(
            f"CMake configuration completed in {configure_time:.1f}s")

        # Build
        parallel_jobs = self._optimize_parallel_jobs(args.parallel)
        build_args = [
            'cmake',
            '--build', str(self.build_dir),
            '--config', build_type_map[args.build_type],
            '--parallel', str(parallel_jobs)
        ]

        if args.verbose:
            build_args.append('--verbose')

        build_start = time.perf_counter()
        success, _ = self._run_command(build_args)
        if success:
            build_time = time.perf_counter() - build_start
            logger.success(f"Build completed in {build_time:.1f}s")

        return success

    def _xmake_build(self, args: argparse.Namespace) -> bool:
        """Build using XMake with optimizations"""
        logger.info("Building with XMake...")

        # Configure XMake
        xmake_config_args = ['xmake', 'f', '--yes']  # Auto-confirm

        if args.build_type == 'debug':
            xmake_config_args.extend(['-m', 'debug'])
        else:
            xmake_config_args.extend(['-m', 'release'])

        # Batch feature flags
        feature_flags = {
            'python': '--python=y',
            'shared': '--shared=y',
            'examples': '--examples=y',
            'tests': '--tests=y',
            'cfitsio': '--cfitsio=y',
            'ssh': '--ssh=y'
        }

        enabled_flags = [flag for feature, flag in feature_flags.items()
                         if getattr(args, feature, False)]
        xmake_config_args.extend(enabled_flags)

        # Configure
        configure_start = time.perf_counter()
        success, _ = self._run_command(xmake_config_args)
        if not success:
            return False

        configure_time = time.perf_counter() - configure_start
        logger.success(
            f"XMake configuration completed in {configure_time:.1f}s")

        # Build
        parallel_jobs = self._optimize_parallel_jobs(args.parallel)
        build_args = ['xmake', 'build', '-j', str(parallel_jobs)]

        if args.verbose:
            build_args.append('-v')

        build_start = time.perf_counter()
        success, _ = self._run_command(build_args)
        if success:
            build_time = time.perf_counter() - build_start
            logger.success(f"Build completed in {build_time:.1f}s")

        return success

    def _run_tests_parallel(self, args: argparse.Namespace) -> bool:
        """Run tests with parallel execution when possible"""
        if args.build_system == 'cmake':
            parallel_jobs = self._optimize_parallel_jobs()
            test_args = [
                'ctest',
                '--output-on-failure',
                # Limit test parallelism
                '--parallel', str(min(parallel_jobs, 8))
            ]

            if args.verbose:
                test_args.append('--verbose')

            success, _ = self._run_command(test_args, cwd=self.build_dir)
            return success
        elif args.build_system == 'xmake':
            success, _ = self._run_command(['xmake', 'test'])
            return success
        return False

    def _post_build_actions(self, args: argparse.Namespace):
        """Perform post-build actions with timing"""
        total_build_time = time.perf_counter() - self.start_time

        logger.success("Build completed successfully!")
        logger.info(f"Total build time: {total_build_time:.1f} seconds")

        # Run tests
        if args.tests and not args.no_test:
            logger.info("Running tests...")
            test_start = time.perf_counter()
            if self._run_tests_parallel(args):
                test_time = time.perf_counter() - test_start
                logger.success(f"Tests completed in {test_time:.1f}s")
            else:
                logger.error("Some tests failed")

        # Generate documentation
        if args.docs:
            logger.info("Generating documentation...")
            if self.system_caps.capabilities.get('has_doxygen', False):
                doc_start = time.perf_counter()
                success, _ = self._run_command(['doxygen', 'Doxyfile'])
                if success:
                    doc_time = time.perf_counter() - doc_start
                    logger.success(
                        f"Documentation generated in {doc_time:.1f}s")
            else:
                logger.warning("Doxygen not found, skipping documentation")

        # Show build summary
        self._show_build_summary(args, total_build_time)

    def _calculate_build_size(self) -> float:
        """Calculate build directory size efficiently"""
        if not self.build_dir.exists():
            return 0.0

        total_size = 0
        for dirpath, dirnames, filenames in os.walk(self.build_dir):
            for filename in filenames:
                filepath = os.path.join(dirpath, filename)
                try:
                    total_size += os.path.getsize(filepath)
                except (OSError, IOError):
                    continue  # Skip files that can't be accessed

        return total_size / (1024 * 1024)  # MB

    def _show_build_summary(self, args: argparse.Namespace, build_time: float):
        """Show optimized build summary"""
        print("\n" + "=" * 60)
        print("BUILD SUMMARY")
        print("=" * 60)
        print(f"Build system:     {args.build_system}")
        print(f"Build type:       {args.build_type}")
        print(f"Total time:       {build_time:.1f} seconds")

        # Calculate build size in background if directory exists
        if self.build_dir.exists():
            build_size = self._calculate_build_size()
            print(f"Build size:       {build_size:.1f} MB")

        # Show enabled features
        enabled_features = []
        for feature in ['python', 'shared', 'examples', 'tests', 'docs', 'cfitsio', 'ssh']:
            if getattr(args, feature, False):
                enabled_features.append(feature)

        if enabled_features:
            print(f"Enabled features: {', '.join(enabled_features)}")

        print("=" * 60)

    def apply_preset(self, preset_name: str) -> Dict[str, Any]:
        """Apply a build preset with validation"""
        presets = self.config.get('presets', {})
        if preset_name not in presets:
            available = ', '.join(presets.keys()) if presets else 'none'
            raise ValueError(
                f"Unknown preset: {preset_name}. Available: {available}")

        preset = presets[preset_name]
        logger.info(
            f"Applying preset '{preset_name}': {preset.get('description', '')}")
        return preset

    def build(self, args: argparse.Namespace) -> bool:
        """Main build function with comprehensive error handling"""
        logger.info(f"Starting Atom build with {args.build_system}")

        try:
            # Setup ccache if requested
            if args.ccache and self._setup_ccache():
                logger.info("ccache enabled for faster rebuilds")

            # Clean build directory if requested
            if args.clean:
                self._clean_build_directory()
            elif not self.build_dir.exists():
                self.build_dir.mkdir(parents=True)

            # Build with selected system
            if args.build_system == 'cmake':
                success = self._cmake_build(args)
            elif args.build_system == 'xmake':
                success = self._xmake_build(args)
            else:
                logger.error(f"Unknown build system: {args.build_system}")
                return False

            if success:
                self._post_build_actions(args)
            else:
                logger.error("Build failed")

            return success

        except Exception as e:
            logger.exception(f"Build failed with exception: {e}")
            return False


def create_parser() -> argparse.ArgumentParser:
    """Create command line argument parser"""
    parser = argparse.ArgumentParser(
        description="Enhanced build system for Atom project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build.py --preset dev                    # Use development preset
  python build.py --release --python --tests     # Release build with Python and tests
  python build.py --debug --sanitizers           # Debug build with sanitizers
  python build.py --cmake --lto --parallel 8     # CMake build with LTO using 8 jobs
        """
    )

    # Build system selection
    parser.add_argument('--cmake', dest='build_system', action='store_const', const='cmake',
                        help='Use CMake build system (default)')
    parser.add_argument('--xmake', dest='build_system', action='store_const', const='xmake',
                        help='Use XMake build system')
    parser.set_defaults(build_system='cmake')

    # Build type
    build_group = parser.add_mutually_exclusive_group()
    build_group.add_argument('--debug', dest='build_type', action='store_const', const='debug',
                             help='Debug build')
    build_group.add_argument('--release', dest='build_type', action='store_const', const='release',
                             help='Release build')
    build_group.add_argument('--relwithdebinfo', dest='build_type', action='store_const',
                             const='relwithdebinfo', help='Release with debug info')
    build_group.add_argument('--minsizerel', dest='build_type', action='store_const',
                             const='minsizerel', help='Minimum size release')
    parser.set_defaults(build_type='release')

    # Features
    parser.add_argument('--python', action='store_true',
                        help='Build Python bindings')
    parser.add_argument('--shared', action='store_true',
                        help='Build shared libraries')
    parser.add_argument('--examples', action='store_true',
                        help='Build examples')
    parser.add_argument('--tests', action='store_true', help='Build tests')
    parser.add_argument('--docs', action='store_true',
                        help='Build documentation')
    parser.add_argument('--benchmarks', action='store_true',
                        help='Build benchmarks')
    parser.add_argument('--cfitsio', action='store_true',
                        help='Enable CFITSIO support')
    parser.add_argument('--ssh', action='store_true',
                        help='Enable SSH support')

    # Optimization
    parser.add_argument('--lto', action='store_true',
                        help='Enable Link Time Optimization')
    parser.add_argument('--coverage', action='store_true',
                        help='Enable code coverage')
    parser.add_argument('--sanitizers', action='store_true',
                        help='Enable sanitizers')

    # Build options
    parser.add_argument('--clean', action='store_true',
                        help='Clean build directory')
    parser.add_argument('--ccache', action='store_true', help='Enable ccache')
    parser.add_argument('--verbose', action='store_true',
                        help='Verbose build output')
    parser.add_argument('--parallel', '-j', type=int,
                        help='Number of parallel jobs')
    parser.add_argument('--install-prefix', help='Installation prefix')
    parser.add_argument('--no-test', action='store_true',
                        help='Skip running tests after build')

    # Presets
    parser.add_argument(
        '--preset', help='Use a build preset (debug, release, dev, python, minimal, full)')
    parser.add_argument('--list-presets', action='store_true',
                        help='List available presets')

    return parser


def main():
    """Main entry point with improved error handling"""
    parser = create_parser()
    args = parser.parse_args()

    try:
        build_system = BuildSystem()

        # List presets if requested
        if args.list_presets:
            presets = build_system.config.get('presets', {})
            if presets:
                logger.info("Available build presets:")
                for name, preset in presets.items():
                    description = preset.get('description', 'No description')
                    logger.info(f"  {name:<12} - {description}")
            else:
                logger.warning("No presets defined in configuration")
            return 0

        # Apply preset if specified
        if args.preset:
            try:
                preset = build_system.apply_preset(args.preset)
                # Override args with preset values
                args.build_type = preset.get('build_type', args.build_type)

                # Apply preset options
                preset_options = preset.get('options', [])
                for option in preset_options:
                    option_name = option.lstrip('-').replace('-', '_')
                    if hasattr(args, option_name):
                        setattr(args, option_name, True)

            except ValueError as e:
                logger.error(str(e))
                return 1

        # Validate build system availability
        if args.build_system == 'cmake' and not shutil.which('cmake'):
            logger.error("CMake not found. Please install CMake.")
            return 1
        elif args.build_system == 'xmake' and not shutil.which('xmake'):
            logger.error("XMake not found. Please install XMake.")
            return 1

        # Run the build
        success = build_system.build(args)
        return 0 if success else 1

    except KeyboardInterrupt:
        logger.warning("Build interrupted by user")
        return 130
    except Exception as e:
        logger.exception(f"Unexpected error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
