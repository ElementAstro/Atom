#!/usr/bin/env python3
"""
Conan package configuration for Atom library
Provides comprehensive package management with modular component support.
"""

import json
import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, save


class AtomConan(ConanFile):
    name = "atom"
    version = "1.0.0"

    # Package metadata
    description = "Foundational library for astronomical software"
    homepage = "https://github.com/ElementAstro/Atom"
    url = "https://github.com/ElementAstro/Atom"
    license = "GPL-3.0"
    topics = ("astronomy", "c++", "library", "scientific-computing")

    # Configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        # Core components
        "with_algorithm": [True, False],
        "with_async": [True, False],
        "with_components": [True, False],
        "with_connection": [True, False],
        "with_containers": [True, False],
        "with_image": [True, False],
        "with_io": [True, False],
        "with_memory": [True, False],
        "with_meta": [True, False],
        "with_search": [True, False],
        "with_secret": [True, False],
        "with_serial": [True, False],
        "with_sysinfo": [True, False],
        "with_system": [True, False],
        "with_web": [True, False],
        # Meta-packages
        "with_networking": [True, False],
        "with_imaging": [True, False],
        "with_full": [True, False],
        # Optional features
        "with_python": [True, False],
        "with_examples": [True, False],
        "with_tests": [True, False],
        "with_docs": [True, False],
        # Boost features
        "with_boost_lockfree": [True, False],
        "with_boost_graph": [True, False],
        "with_boost_intrusive": [True, False],
        # External integrations
        "with_cfitsio": [True, False],
        "with_ssh": [True, False],
        "with_readline": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
        # Core components (minimal by default)
        "with_algorithm": False,
        "with_async": False,
        "with_components": False,
        "with_connection": False,
        "with_containers": False,
        "with_image": False,
        "with_io": False,
        "with_memory": False,
        "with_meta": False,
        "with_search": False,
        "with_secret": False,
        "with_serial": False,
        "with_sysinfo": False,
        "with_system": False,
        "with_web": False,
        # Meta-packages
        "with_networking": False,
        "with_imaging": False,
        "with_full": False,
        # Optional features
        "with_python": False,
        "with_examples": False,
        "with_tests": False,
        "with_docs": False,
        # Boost features
        "with_boost_lockfree": False,
        "with_boost_graph": False,
        "with_boost_intrusive": False,
        # External integrations
        "with_cfitsio": False,
        "with_ssh": False,
        "with_readline": False,
    }

    # Component dependencies mapping
    _component_deps = {
        "algorithm": ["error"],
        "async": ["error", "log"],
        "components": ["error", "log", "type"],
        "connection": ["error", "log", "async"],
        "containers": ["error", "type"],
        "image": ["error", "log", "io"],
        "io": ["error", "log"],
        "memory": ["error"],
        "meta": ["error", "type"],
        "search": ["error", "algorithm"],
        "secret": ["error", "log"],
        "serial": ["error", "log", "io"],
        "sysinfo": ["error", "log"],
        "system": ["error", "log", "sysinfo"],
        "web": ["error", "log", "async", "connection"],
    }

    # Meta-package definitions
    _meta_packages = {
        "networking": ["connection", "web", "async"],
        "imaging": ["image", "io", "algorithm"],
        "full": list(_component_deps.keys()),
    }

    def export(self):
        """Export additional files with the recipe."""
        copy(self, "LICENSE", src=self.recipe_folder, dst=self.export_folder)
        copy(self, "README.md", src=self.recipe_folder, dst=self.export_folder)

    def export_sources(self):
        """Export source files."""
        copy(
            self,
            "*",
            src=self.recipe_folder,
            dst=self.export_sources_folder,
            excludes=["build", "dist", "*.pyc", "__pycache__"],
        )

    def config_options(self):
        """Configure options based on platform."""
        if self.settings.os == "Windows":
            del self.options.fPIC
            # Readline not available on Windows
            self.options.with_readline = False

    def configure(self):
        """Configure the package."""
        if self.options.shared:
            self.options.rm_safe("fPIC")

        # Handle meta-packages
        if self.options.with_full:
            for component in self._component_deps.keys():
                setattr(self.options, f"with_{component}", True)

        if self.options.with_networking:
            for component in self._meta_packages["networking"]:
                setattr(self.options, f"with_{component}", True)

        if self.options.with_imaging:
            for component in self._meta_packages["imaging"]:
                setattr(self.options, f"with_{component}", True)

        # Resolve component dependencies
        self._resolve_component_dependencies()

    def _resolve_component_dependencies(self):
        """Automatically enable component dependencies."""
        changed = True
        while changed:
            changed = False
            for component, deps in self._component_deps.items():
                if getattr(self.options, f"with_{component}"):
                    for dep in deps:
                        if dep in self._component_deps:
                            dep_option = f"with_{dep}"
                            if hasattr(self.options, dep_option) and not getattr(
                                self.options, dep_option
                            ):
                                setattr(self.options, dep_option, True)
                                changed = True

    def validate(self):
        """Validate configuration."""
        if self.settings.compiler.get_safe("cppstd"):
            if self.settings.compiler.cppstd < "20":
                raise ConanInvalidConfiguration("Atom requires C++20 or higher")

    def requirements(self):
        """Define package requirements."""
        # Core dependencies (always required)
        self.requires("openssl/1.1.1t")
        self.requires("zlib/1.2.13")
        self.requires("sqlite3/3.42.0")
        self.requires("fmt/10.1.1")

        # Optional dependencies
        if self.options.with_python:
            self.requires("pybind11/2.11.1")

        if self.options.with_tests:
            self.requires("gtest/1.14.0")

        if self.options.with_web:
            self.requires("cpp-httplib/0.14.1")

        if self.options.with_cfitsio:
            self.requires("cfitsio/4.2.0")

        if self.options.with_ssh:
            self.requires("libssh2/1.11.0")

        if self.options.with_readline and self.settings.os != "Windows":
            self.requires("readline/8.2")

        # Boost dependencies
        if self.options.with_boost_lockfree:
            self.requires("boost/1.82.0")

        if self.options.with_boost_graph:
            self.requires("boost/1.82.0")

        if self.options.with_boost_intrusive:
            self.requires("boost/1.82.0")

    def build_requirements(self):
        """Define build requirements."""
        self.tool_requires("cmake/3.27.1")
        if self.settings.os != "Windows":
            self.tool_requires("ninja/1.11.1")

        if self.options.with_docs:
            self.tool_requires("doxygen/1.9.7")

    def layout(self):
        """Define package layout."""
        cmake_layout(self)

    def generate(self):
        """Generate build files."""
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)

        # Core build options
        tc.variables["ATOM_BUILD_PYTHON_BINDINGS"] = self.options.with_python
        tc.variables["ATOM_BUILD_EXAMPLES"] = self.options.with_examples
        tc.variables["ATOM_BUILD_TESTS"] = self.options.with_tests
        tc.variables["ATOM_BUILD_DOCS"] = self.options.with_docs

        # Component options
        for component in self._component_deps.keys():
            tc.variables[f"ATOM_BUILD_{component.upper()}"] = getattr(
                self.options, f"with_{component}"
            )

        # Feature options
        tc.variables["ATOM_USE_CFITSIO"] = self.options.with_cfitsio
        tc.variables["ATOM_USE_SSH"] = self.options.with_ssh
        tc.variables["ATOM_USE_READLINE"] = self.options.with_readline
        tc.variables["ATOM_USE_BOOST_LOCKFREE"] = self.options.with_boost_lockfree
        tc.variables["ATOM_USE_BOOST_GRAPH"] = self.options.with_boost_graph
        tc.variables["ATOM_USE_BOOST_INTRUSIVE"] = self.options.with_boost_intrusive

        tc.generate()

    def build(self):
        """Build the package."""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        if self.options.with_tests:
            cmake.test()

    def package(self):
        """Package the built files."""
        cmake = CMake(self)
        cmake.install()

        # Copy license
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

        # Create component manifest
        self._create_component_manifest()

    def _create_component_manifest(self):
        """Create a manifest of installed components."""
        manifest = {
            "name": self.name,
            "version": self.version,
            "components": {},
            "features": {},
        }

        # Add enabled components
        for component in self._component_deps.keys():
            if getattr(self.options, f"with_{component}"):
                manifest["components"][component] = {
                    "enabled": True,
                    "dependencies": self._component_deps[component],
                }

        # Add enabled features
        feature_options = [
            "python",
            "examples",
            "tests",
            "docs",
            "boost_lockfree",
            "boost_graph",
            "boost_intrusive",
            "cfitsio",
            "ssh",
            "readline",
        ]

        for feature in feature_options:
            if getattr(self.options, f"with_{feature}"):
                manifest["features"][feature] = True

        # Save manifest
        manifest_path = os.path.join(
            self.package_folder, "share", "atom", "manifest.json"
        )
        os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
        save(self, manifest_path, json.dumps(manifest, indent=2))

    def package_info(self):
        """Define package information for consumers."""
        # Core library
        self.cpp_info.libs = ["atom-error", "atom-log", "atom-type", "atom-utils"]

        # Add component libraries
        for component in self._component_deps.keys():
            if getattr(self.options, f"with_{component}"):
                self.cpp_info.libs.append(f"atom-{component}")

        # Include directories
        self.cpp_info.includedirs = ["include"]

        # System libraries
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread", "dl"]
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = ["ws2_32", "wsock32"]

        # CMake configuration
        self.cpp_info.set_property("cmake_file_name", "atom")
        self.cpp_info.set_property("cmake_target_name", "atom::atom")

        # Component targets
        for component in self._component_deps.keys():
            if getattr(self.options, f"with_{component}"):
                self.cpp_info.components[component].libs = [f"atom-{component}"]
                self.cpp_info.components[component].set_property(
                    "cmake_target_name", f"atom::{component}"
                )

        # pkg-config
        self.cpp_info.set_property("pkg_config_name", "atom")

        # Environment variables
        self.buildenv_info.define_path("ATOM_ROOT", self.package_folder)

    def package_id(self):
        """Define package ID for binary compatibility."""
        # Remove build-only options from package ID
        del self.info.options.with_examples
        del self.info.options.with_tests
        del self.info.options.with_docs
