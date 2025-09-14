#!/usr/bin/env python3
"""
Atom Library Package Validation Tool
Validates package integrity, dependencies, and installation correctness.
"""

import argparse
import os
import sys
import subprocess
import tempfile
import shutil
import json
import hashlib
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import logging
import zipfile
import tarfile

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class PackageValidator:
    """Validates Atom library packages for integrity and correctness."""

    def __init__(self):
        self.validation_results = {
            'package_info': {},
            'structure_check': {'passed': False, 'issues': []},
            'dependency_check': {'passed': False, 'issues': []},
            'integrity_check': {'passed': False, 'issues': []},
            'installation_test': {'passed': False, 'issues': []},
            'overall_status': 'UNKNOWN'
        }

        # Expected package structure
        self.expected_structure = {
            'binary': {
                'required_dirs': ['include', 'lib'],
                'optional_dirs': ['bin', 'share', 'cmake'],
                'required_files': [],
                'forbidden_files': ['.git', '.gitignore', 'CMakeCache.txt']
            },
            'source': {
                'required_dirs': ['atom', 'cmake'],
                'optional_dirs': ['example', 'tests', 'python', 'docs'],
                'required_files': ['CMakeLists.txt', 'README.md', 'LICENSE'],
                'forbidden_files': ['build/', 'dist/', '.git/']
            },
            'python': {
                'required_dirs': [],
                'optional_dirs': ['atom'],
                'required_files': [],
                'forbidden_files': []
            }
        }

    def validate_package(self, package_path: Path) -> Dict:
        """Validate a package and return results."""
        logger.info(f"Validating package: {package_path}")

        if not package_path.exists():
            self.validation_results['overall_status'] = 'FAILED'
            self.validation_results['package_info']['error'] = 'Package file not found'
            return self.validation_results

        # Extract package info
        self._extract_package_info(package_path)

        # Extract package to temporary directory
        with tempfile.TemporaryDirectory(prefix='atom-validate-') as temp_dir:
            extract_dir = Path(temp_dir) / 'extracted'

            try:
                self._extract_package(package_path, extract_dir)

                # Run validation checks
                self._validate_structure(extract_dir)
                self._validate_dependencies(extract_dir)
                self._validate_integrity(extract_dir)
                self._test_installation(extract_dir)

                # Determine overall status
                self._determine_overall_status()

            except Exception as e:
                logger.error(f"Validation failed: {e}")
                self.validation_results['overall_status'] = 'ERROR'
                self.validation_results['package_info']['error'] = str(e)

        return self.validation_results

    def _extract_package_info(self, package_path: Path):
        """Extract basic package information."""
        self.validation_results['package_info'] = {
            'name': package_path.name,
            'size': package_path.stat().st_size,
            'type': self._detect_package_type(package_path),
            'format': package_path.suffix,
            'checksum': self._calculate_checksum(package_path)
        }

    def _detect_package_type(self, package_path: Path) -> str:
        """Detect the type of package (binary, source, python)."""
        name = package_path.name.lower()

        if 'source' in name:
            return 'source'
        elif '.whl' in name or 'python' in name:
            return 'python'
        elif any(platform in name for platform in ['linux', 'windows', 'macos', 'darwin']):
            return 'binary'
        else:
            return 'unknown'

    def _calculate_checksum(self, file_path: Path) -> str:
        """Calculate SHA256 checksum of file."""
        sha256_hash = hashlib.sha256()
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha256_hash.update(chunk)
        return sha256_hash.hexdigest()

    def _extract_package(self, package_path: Path, extract_dir: Path):
        """Extract package to directory."""
        extract_dir.mkdir(parents=True, exist_ok=True)

        if package_path.suffix == '.zip' or package_path.name.endswith('.whl'):
            with zipfile.ZipFile(package_path, 'r') as zip_ref:
                zip_ref.extractall(extract_dir)
        elif package_path.suffix in ['.gz', '.bz2', '.xz'] or '.tar' in package_path.name:
            with tarfile.open(package_path, 'r:*') as tar_ref:
                tar_ref.extractall(extract_dir)
        else:
            raise ValueError(f"Unsupported package format: {package_path.suffix}")

    def _validate_structure(self, extract_dir: Path):
        """Validate package directory structure."""
        logger.info("Validating package structure")

        package_type = self.validation_results['package_info']['type']
        expected = self.expected_structure.get(package_type, self.expected_structure['binary'])

        issues = []

        # Find the actual content directory (handle nested structures)
        content_dir = self._find_content_directory(extract_dir)

        # Check required directories
        for req_dir in expected['required_dirs']:
            dir_path = content_dir / req_dir
            if not dir_path.exists():
                issues.append(f"Missing required directory: {req_dir}")

        # Check required files
        for req_file in expected['required_files']:
            file_path = content_dir / req_file
            if not file_path.exists():
                issues.append(f"Missing required file: {req_file}")

        # Check for forbidden files/directories
        for forbidden in expected['forbidden_files']:
            forbidden_path = content_dir / forbidden
            if forbidden_path.exists():
                issues.append(f"Contains forbidden file/directory: {forbidden}")

        # Validate specific content based on package type
        if package_type == 'binary':
            self._validate_binary_structure(content_dir, issues)
        elif package_type == 'source':
            self._validate_source_structure(content_dir, issues)
        elif package_type == 'python':
            self._validate_python_structure(content_dir, issues)

        self.validation_results['structure_check'] = {
            'passed': len(issues) == 0,
            'issues': issues
        }

    def _find_content_directory(self, extract_dir: Path) -> Path:
        """Find the actual content directory in extracted package."""
        # Check if there's a single top-level directory
        contents = list(extract_dir.iterdir())
        if len(contents) == 1 and contents[0].is_dir():
            return contents[0]
        return extract_dir

    def _validate_binary_structure(self, content_dir: Path, issues: List[str]):
        """Validate binary package structure."""
        # Check for libraries
        lib_dir = content_dir / 'lib'
        if lib_dir.exists():
            lib_files = list(lib_dir.glob('*atom*'))
            if not lib_files:
                issues.append("No Atom libraries found in lib directory")

        # Check for headers
        include_dir = content_dir / 'include' / 'atom'
        if include_dir.exists():
            header_files = list(include_dir.rglob('*.hpp'))
            if not header_files:
                issues.append("No Atom headers found in include directory")

        # Check for CMake config files
        cmake_dirs = [
            content_dir / 'lib' / 'cmake' / 'atom',
            content_dir / 'cmake',
            content_dir / 'share' / 'cmake' / 'atom'
        ]

        cmake_config_found = False
        for cmake_dir in cmake_dirs:
            if cmake_dir.exists() and list(cmake_dir.glob('*Config.cmake')):
                cmake_config_found = True
                break

        if not cmake_config_found:
            issues.append("No CMake configuration files found")

    def _validate_source_structure(self, content_dir: Path, issues: List[str]):
        """Validate source package structure."""
        # Check for main source directory
        atom_dir = content_dir / 'atom'
        if atom_dir.exists():
            # Check for component directories
            expected_components = ['error', 'log', 'type', 'utils']
            for component in expected_components:
                component_dir = atom_dir / component
                if not component_dir.exists():
                    issues.append(f"Missing core component directory: {component}")

        # Check for build system files
        build_files = ['CMakeLists.txt', 'xmake.lua']
        found_build_system = False
        for build_file in build_files:
            if (content_dir / build_file).exists():
                found_build_system = True
                break

        if not found_build_system:
            issues.append("No build system configuration found")

    def _validate_python_structure(self, content_dir: Path, issues: List[str]):
        """Validate Python package structure."""
        # Look for Python module files
        python_files = list(content_dir.rglob('*.py'))
        if not python_files:
            issues.append("No Python files found in package")

        # Check for __init__.py files
        init_files = list(content_dir.rglob('__init__.py'))
        if not init_files:
            issues.append("No __init__.py files found")

    def _validate_dependencies(self, extract_dir: Path):
        """Validate package dependencies."""
        logger.info("Validating dependencies")

        issues: list[str] = []
        content_dir = self._find_content_directory(extract_dir)

        # Check for dependency information
        dependency_files = [
            content_dir / 'share' / 'atom' / 'manifest.json',
            content_dir / 'METADATA',
            content_dir / 'PKG-INFO'
        ]

        dependency_info_found = False
        for dep_file in dependency_files:
            if dep_file.exists():
                dependency_info_found = True
                try:
                    self._parse_dependency_file(dep_file, issues)
                except Exception as e:
                    issues.append(f"Failed to parse dependency file {dep_file.name}: {e}")

        if not dependency_info_found:
            issues.append("No dependency information found")

        self.validation_results['dependency_check'] = {
            'passed': len(issues) == 0,
            'issues': issues
        }

    def _parse_dependency_file(self, dep_file: Path, issues: List[str]):
        """Parse dependency file and validate."""
        if dep_file.name == 'manifest.json':
            with open(dep_file, 'r') as f:
                manifest = json.load(f)
                # Validate manifest structure
                required_fields = ['name', 'version']
                for field in required_fields:
                    if field not in manifest:
                        issues.append(f"Missing required field in manifest: {field}")

    def _validate_integrity(self, extract_dir: Path):
        """Validate package integrity."""
        logger.info("Validating package integrity")

        issues: list[str] = []
        content_dir = self._find_content_directory(extract_dir)

        # Check for corrupted files
        try:
            self._check_file_integrity(content_dir, issues)
        except Exception as e:
            issues.append(f"Integrity check failed: {e}")

        # Validate checksums if available
        checksum_files = [
            content_dir / 'checksums.sha256',
            content_dir / 'checksums.md5'
        ]

        for checksum_file in checksum_files:
            if checksum_file.exists():
                try:
                    self._validate_checksums(checksum_file, content_dir, issues)
                except Exception as e:
                    issues.append(f"Checksum validation failed: {e}")

        self.validation_results['integrity_check'] = {
            'passed': len(issues) == 0,
            'issues': issues
        }

    def _check_file_integrity(self, content_dir: Path, issues: List[str]):
        """Check for corrupted or suspicious files."""
        for file_path in content_dir.rglob('*'):
            if file_path.is_file():
                # Check file size
                if file_path.stat().st_size == 0:
                    issues.append(f"Empty file found: {file_path.relative_to(content_dir)}")

                # Check for suspicious extensions
                suspicious_extensions = ['.exe', '.bat', '.sh', '.ps1']
                if file_path.suffix in suspicious_extensions:
                    # This might be expected for some packages
                    logger.warning(f"Executable file found: {file_path.relative_to(content_dir)}")

    def _validate_checksums(self, checksum_file: Path, content_dir: Path, issues: List[str]):
        """Validate file checksums."""
        with open(checksum_file, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue

                parts = line.split()
                if len(parts) >= 2:
                    expected_checksum = parts[0]
                    file_path = content_dir / parts[1]

                    if file_path.exists():
                        actual_checksum = self._calculate_checksum(file_path)
                        if actual_checksum != expected_checksum:
                            issues.append(f"Checksum mismatch for {parts[1]}")

    def _test_installation(self, extract_dir: Path):
        """Test package installation."""
        logger.info("Testing package installation")

        issues: list[str] = []
        content_dir = self._find_content_directory(extract_dir)
        package_type = self.validation_results['package_info']['type']

        try:
            if package_type == 'binary':
                self._test_binary_installation(content_dir, issues)
            elif package_type == 'python':
                self._test_python_installation(content_dir, issues)
            elif package_type == 'source':
                self._test_source_build(content_dir, issues)
        except Exception as e:
            issues.append(f"Installation test failed: {e}")

        self.validation_results['installation_test'] = {
            'passed': len(issues) == 0,
            'issues': issues
        }

    def _test_binary_installation(self, content_dir: Path, issues: List[str]):
        """Test binary package installation."""
        # Test CMake integration
        cmake_test_dir = content_dir / 'test_cmake'
        cmake_test_dir.mkdir(exist_ok=True)

        # Create a simple CMake test
        cmake_test_file = cmake_test_dir / 'CMakeLists.txt'
        with open(cmake_test_file, 'w') as f:
            f.write("""
cmake_minimum_required(VERSION 3.21)
project(AtomTest)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/..")
find_package(atom QUIET)
if(atom_FOUND)
    message(STATUS "Atom found successfully")
else()
    message(FATAL_ERROR "Atom not found")
endif()
""")

        # Run CMake test
        try:
            result = subprocess.run(
                ['cmake', '.'],
                cwd=cmake_test_dir,
                capture_output=True,
                text=True,
                timeout=30
            )
            if result.returncode != 0:
                issues.append(f"CMake integration test failed: {result.stderr}")
        except subprocess.TimeoutExpired:
            issues.append("CMake integration test timed out")
        except FileNotFoundError:
            logger.warning("CMake not available for testing")

    def _test_python_installation(self, content_dir: Path, issues: List[str]):
        """Test Python package installation."""
        # Try to import the package
        try:
            import sys
            sys.path.insert(0, str(content_dir))
            import atom
            logger.info(f"Successfully imported atom module")
        except ImportError as e:
            issues.append(f"Failed to import atom module: {e}")
        finally:
            if str(content_dir) in sys.path:
                sys.path.remove(str(content_dir))

    def _test_source_build(self, content_dir: Path, issues: List[str]):
        """Test source package build."""
        # Check if CMakeLists.txt is valid
        cmake_file = content_dir / 'CMakeLists.txt'
        if cmake_file.exists():
            try:
                # Basic syntax check
                with open(cmake_file, 'r') as f:
                    content = f.read()
                    if 'project(' not in content:
                        issues.append("CMakeLists.txt missing project() declaration")
                    if 'cmake_minimum_required(' not in content:
                        issues.append("CMakeLists.txt missing cmake_minimum_required()")
            except Exception as e:
                issues.append(f"Failed to validate CMakeLists.txt: {e}")

    def _determine_overall_status(self):
        """Determine overall validation status."""
        checks = [
            self.validation_results['structure_check']['passed'],
            self.validation_results['dependency_check']['passed'],
            self.validation_results['integrity_check']['passed'],
            self.validation_results['installation_test']['passed']
        ]

        if all(checks):
            self.validation_results['overall_status'] = 'PASSED'
        elif any(checks):
            self.validation_results['overall_status'] = 'PARTIAL'
        else:
            self.validation_results['overall_status'] = 'FAILED'

    def print_results(self):
        """Print validation results in a human-readable format."""
        results = self.validation_results

        print(f"\n{'='*60}")
        print(f"ATOM PACKAGE VALIDATION REPORT")
        print(f"{'='*60}")

        # Package info
        info = results['package_info']
        print(f"\nPackage Information:")
        print(f"  Name: {info.get('name', 'Unknown')}")
        print(f"  Type: {info.get('type', 'Unknown')}")
        print(f"  Size: {info.get('size', 0):,} bytes")
        print(f"  Format: {info.get('format', 'Unknown')}")
        print(f"  Checksum: {info.get('checksum', 'Unknown')[:16]}...")

        # Validation results
        checks = [
            ('Structure Check', results['structure_check']),
            ('Dependency Check', results['dependency_check']),
            ('Integrity Check', results['integrity_check']),
            ('Installation Test', results['installation_test'])
        ]

        print(f"\nValidation Results:")
        for check_name, check_result in checks:
            status = "✓ PASS" if check_result['passed'] else "✗ FAIL"
            print(f"  {check_name}: {status}")

            if check_result['issues']:
                for issue in check_result['issues']:
                    print(f"    - {issue}")

        # Overall status
        status_color = {
            'PASSED': '✓',
            'PARTIAL': '⚠',
            'FAILED': '✗',
            'ERROR': '✗',
            'UNKNOWN': '?'
        }

        overall = results['overall_status']
        print(f"\nOverall Status: {status_color.get(overall, '?')} {overall}")

        if overall == 'PASSED':
            print("Package validation completed successfully!")
        elif overall == 'PARTIAL':
            print("Package validation completed with warnings.")
        else:
            print("Package validation failed. Please review the issues above.")


def main():
    parser = argparse.ArgumentParser(description='Validate Atom library packages')
    parser.add_argument('package', type=Path, help='Package file to validate')
    parser.add_argument('--json', action='store_true', help='Output results in JSON format')
    parser.add_argument('--verbose', '-v', action='store_true', help='Enable verbose logging')

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    try:
        validator = PackageValidator()
        results = validator.validate_package(args.package)

        if args.json:
            print(json.dumps(results, indent=2))
        else:
            validator.print_results()

        # Exit with appropriate code
        status = results['overall_status']
        if status == 'PASSED':
            sys.exit(0)
        elif status == 'PARTIAL':
            sys.exit(1)
        else:
            sys.exit(2)

    except Exception as e:
        logger.error(f"Validation failed: {e}")
        sys.exit(3)


if __name__ == '__main__':
    main()
