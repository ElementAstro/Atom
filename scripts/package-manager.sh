#!/bin/bash
# Atom Project Package Management Script
# Handles dependency installation, package creation, and distribution
# Author: Max Qian

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PACKAGE_DIR="$PROJECT_ROOT/packages"
VCPKG_DIR="$PROJECT_ROOT/vcpkg"

# Logging functions
log_info() { echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') $*"; }
log_warn() { echo "[WARN] $(date '+%Y-%m-%d %H:%M:%S') $*"; }
log_error() { echo "[ERROR] $(date '+%Y-%m-%d %H:%M:%S') $*" >&2; }

# System detection
detect_os() {
    case "$OSTYPE" in
        linux-gnu*) echo "linux" ;;
        darwin*) echo "macos" ;;
        cygwin|msys) echo "windows" ;;
        *) echo "unknown" ;;
    esac
}

detect_package_manager() {
    if command -v apt-get &> /dev/null; then echo "apt"
    elif command -v yum &> /dev/null; then echo "yum"
    elif command -v dnf &> /dev/null; then echo "dnf"
    elif command -v pacman &> /dev/null; then echo "pacman"
    elif command -v brew &> /dev/null; then echo "brew"
    elif command -v choco &> /dev/null; then echo "choco"
    else echo "none"; fi
}

# vcpkg management
setup_vcpkg() {
    log_info "Setting up vcpkg package manager..."

    if [[ ! -d "$VCPKG_DIR" ]]; then
        log_info "Cloning vcpkg repository..."
        git clone https://github.com/Microsoft/vcpkg.git "$VCPKG_DIR"
    else
        log_info "Updating vcpkg repository..."
        cd "$VCPKG_DIR"
        git pull
        cd "$PROJECT_ROOT"
    fi

    # Bootstrap vcpkg
    if [[ "$(detect_os)" == "windows" ]]; then
        "$VCPKG_DIR/bootstrap-vcpkg.bat"
    else
        "$VCPKG_DIR/bootstrap-vcpkg.sh"
    fi

    # Install dependencies
    local vcpkg_exe="$VCPKG_DIR/vcpkg"
    [[ "$(detect_os)" == "windows" ]] && vcpkg_exe="$VCPKG_DIR/vcpkg.exe"

    log_info "Installing vcpkg dependencies..."
    "$vcpkg_exe" install --triplet x64-linux openssl zlib sqlite3 fmt readline pybind11 boost
}

# System dependency installation
install_system_dependencies() {
    local os=$(detect_os)
    local pkg_mgr=$(detect_package_manager)

    log_info "Installing system dependencies for $os using $pkg_mgr"

    case "$pkg_mgr" in
        apt)
            sudo apt-get update
            sudo apt-get install -y \
                build-essential cmake ninja-build git curl \
                libssl-dev zlib1g-dev libsqlite3-dev \
                libfmt-dev libreadline-dev \
                python3-dev python3-pip \
                doxygen graphviz \
                pkg-config
            ;;
        yum|dnf)
            sudo $pkg_mgr install -y \
                gcc-c++ cmake ninja-build git curl \
                openssl-devel zlib-devel sqlite-devel \
                fmt-devel readline-devel \
                python3-devel python3-pip \
                doxygen graphviz \
                pkgconfig
            ;;
        pacman)
            sudo pacman -S --noconfirm \
                base-devel cmake ninja git curl \
                openssl zlib sqlite \
                fmt readline \
                python python-pip \
                doxygen graphviz \
                pkgconf
            ;;
        brew)
            brew install \
                cmake ninja git curl \
                openssl zlib sqlite3 \
                fmt readline \
                python3 \
                doxygen graphviz \
                pkg-config
            ;;
        choco)
            choco install -y \
                cmake ninja git curl \
                openssl zlib sqlite \
                python3 \
                doxygen.install graphviz
            ;;
        *)
            log_warn "Unknown package manager. Please install dependencies manually."
            ;;
    esac
}

# Python package management
setup_python_environment() {
    log_info "Setting up Python environment..."

    # Create virtual environment if it doesn't exist
    if [[ ! -d "$PROJECT_ROOT/.venv" ]]; then
        python3 -m venv "$PROJECT_ROOT/.venv"
    fi

    # Activate virtual environment
    source "$PROJECT_ROOT/.venv/bin/activate"

    # Upgrade pip and install build tools
    pip install --upgrade pip setuptools wheel
    pip install pybind11 numpy pytest sphinx

    log_info "Python environment ready"
}

# Package creation functions
create_deb_package() {
    log_info "Creating Debian package..."

    local package_name="libatom-dev"
    local version=$(git describe --tags --always --dirty 2>/dev/null || echo "0.1.0")
    local arch=$(dpkg --print-architecture 2>/dev/null || echo "amd64")

    local deb_dir="$PACKAGE_DIR/deb/$package_name-$version"
    mkdir -p "$deb_dir/DEBIAN"
    mkdir -p "$deb_dir/usr/include"
    mkdir -p "$deb_dir/usr/lib"

    # Create control file
    cat > "$deb_dir/DEBIAN/control" << EOF
Package: $package_name
Version: $version
Section: libdevel
Priority: optional
Architecture: $arch
Depends: libssl-dev, zlib1g-dev, libsqlite3-dev
Maintainer: Max Qian <max@example.com>
Description: Atom foundational library for astronomical software
 A comprehensive C++20 library providing core functionality
 for astronomical software development.
EOF

    # Copy files
    cp -r "$PROJECT_ROOT/atom" "$deb_dir/usr/include/"

    # Build package
    dpkg-deb --build "$deb_dir"
    log_info "Debian package created: $deb_dir.deb"
}

create_rpm_package() {
    log_info "Creating RPM package..."

    local spec_file="$PACKAGE_DIR/rpm/atom.spec"
    mkdir -p "$(dirname "$spec_file")"

    cat > "$spec_file" << 'EOF'
Name:           atom
Version:        0.1.0
Release:        1%{?dist}
Summary:        Foundational library for astronomical software

License:        GPL-3.0
URL:            https://github.com/ElementAstro/Atom
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++ cmake ninja-build
BuildRequires:  openssl-devel zlib-devel sqlite-devel
Requires:       openssl-devel zlib-devel sqlite-devel

%description
A comprehensive C++20 library providing core functionality
for astronomical software development.

%prep
%autosetup

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_includedir}/atom/
%{_libdir}/libatom*.so*

%changelog
* $(date '+%a %b %d %Y') Max Qian <max@example.com> - 0.1.0-1
- Initial package
EOF

    log_info "RPM spec file created: $spec_file"
}

# Conan package management
create_conan_package() {
    log_info "Creating Conan package..."

    local conanfile="$PACKAGE_DIR/conan/conanfile.py"
    mkdir -p "$(dirname "$conanfile")"

    cat > "$conanfile" << 'EOF'
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy, save
import os

class AtomConan(ConanFile):
    name = "atom"
    version = "1.0.0"

    # Package metadata
    description = "Foundational library for astronomical software"
    homepage = "https://github.com/ElementAstro/Atom"
    url = "https://github.com/ElementAstro/Atom"
    license = "GPL-3.0"
    topics = ("astronomy", "c++", "library")

    # Configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_python": [True, False],
        "with_examples": [True, False],
        "with_tests": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_python": False,
        "with_examples": False,
        "with_tests": False
    }

    # Requirements
    def requirements(self):
        self.requires("openssl/1.1.1t")
        self.requires("zlib/1.2.13")
        self.requires("sqlite3/3.42.0")
        self.requires("fmt/10.1.1")
        if self.options.with_python:
            self.requires("pybind11/2.11.1")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["ATOM_BUILD_PYTHON_BINDINGS"] = self.options.with_python
        tc.variables["ATOM_BUILD_EXAMPLES"] = self.options.with_examples
        tc.variables["ATOM_BUILD_TESTS"] = self.options.with_tests
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["atom"]
        self.cpp_info.includedirs = ["include"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread", "dl"]
EOF

    log_info "Conan package file created: $conanfile"
}

# Homebrew formula creation
create_homebrew_formula() {
    log_info "Creating Homebrew formula..."

    local formula_file="$PACKAGE_DIR/homebrew/atom.rb"
    mkdir -p "$(dirname "$formula_file")"

    local version=$(git describe --tags --always --dirty 2>/dev/null || echo "1.0.0")
    local tarball_url="https://github.com/ElementAstro/Atom/archive/v${version}.tar.gz"

    cat > "$formula_file" << EOF
class Atom < Formula
  desc "Foundational library for astronomical software"
  homepage "https://github.com/ElementAstro/Atom"
  url "$tarball_url"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  license "GPL-3.0"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "openssl@3"
  depends_on "sqlite"
  depends_on "fmt"
  depends_on "python@3.11" => :optional

  def install
    args = %W[
      -DCMAKE_BUILD_TYPE=Release
      -DATOM_BUILD_EXAMPLES=OFF
      -DATOM_BUILD_TESTS=OFF
      -DATOM_BUILD_PYTHON_BINDINGS=#{build.with?("python@3.11") ? "ON" : "OFF"}
    ]

    system "cmake", "-B", "build", "-S", ".", *args, *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include <atom/error/error.hpp>
      int main() {
        return 0;
      }
    EOS

    system ENV.cxx, "test.cpp", "-I#{include}", "-L#{lib}", "-latom", "-o", "test"
    system "./test"
  end
end
EOF

    log_info "Homebrew formula created: $formula_file"
}

# vcpkg port creation
create_vcpkg_port() {
    log_info "Creating vcpkg port..."

    local port_dir="$PACKAGE_DIR/vcpkg/ports/atom"
    mkdir -p "$port_dir"

    # Update existing portfile with latest features
    cp "$PROJECT_ROOT/ports/atom/portfile.cmake" "$port_dir/"
    cp "$PROJECT_ROOT/ports/atom/vcpkg.json" "$port_dir/"

    log_info "vcpkg port created: $port_dir"
}

# Arch Linux PKGBUILD creation
create_arch_package() {
    log_info "Creating Arch Linux PKGBUILD..."

    local pkgbuild_file="$PACKAGE_DIR/arch/PKGBUILD"
    mkdir -p "$(dirname "$pkgbuild_file")"

    local version=$(git describe --tags --always --dirty 2>/dev/null || echo "1.0.0")

    cat > "$pkgbuild_file" << EOF
# Maintainer: Max Qian <max@example.com>
pkgname=atom
pkgver=${version}
pkgrel=1
pkgdesc="Foundational library for astronomical software"
arch=('x86_64' 'aarch64')
url="https://github.com/ElementAstro/Atom"
license=('GPL3')
depends=('openssl' 'zlib' 'sqlite' 'fmt')
makedepends=('cmake' 'ninja' 'git')
optdepends=('python: for Python bindings')
source=("atom-\${pkgver}.tar.gz::https://github.com/ElementAstro/Atom/archive/v\${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cd "Atom-\${pkgver}"

    cmake -B build -S . \\
        -DCMAKE_BUILD_TYPE=Release \\
        -DCMAKE_INSTALL_PREFIX=/usr \\
        -DATOM_BUILD_EXAMPLES=OFF \\
        -DATOM_BUILD_TESTS=OFF \\
        -DATOM_BUILD_PYTHON_BINDINGS=ON \\
        -G Ninja

    cmake --build build
}

check() {
    cd "Atom-\${pkgver}"
    cmake --build build --target test
}

package() {
    cd "Atom-\${pkgver}"
    DESTDIR="\${pkgdir}" cmake --install build
}
EOF

    log_info "Arch Linux PKGBUILD created: $pkgbuild_file"
}

# Docker image creation
create_docker_images() {
    log_info "Creating Docker images..."

    local docker_dir="$PACKAGE_DIR/docker"
    mkdir -p "$docker_dir"

    # Development image
    cat > "$docker_dir/Dockerfile.dev" << 'EOF'
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git curl \
    libssl-dev zlib1g-dev libsqlite3-dev \
    libfmt-dev libreadline-dev \
    python3-dev python3-pip \
    doxygen graphviz \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=$VCPKG_ROOT:$PATH

# Create workspace
WORKDIR /workspace

# Copy source code
COPY . .

# Build Atom
RUN cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DATOM_BUILD_EXAMPLES=ON \
    -DATOM_BUILD_TESTS=ON \
    -DATOM_BUILD_PYTHON_BINDINGS=ON \
    -G Ninja \
    && cmake --build build

# Install
RUN cmake --install build --prefix /usr/local

CMD ["/bin/bash"]
EOF

    # Runtime image
    cat > "$docker_dir/Dockerfile.runtime" << 'EOF'
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 zlib1g libsqlite3-0 \
    libfmt9 libreadline8 \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Copy installed libraries from build stage
COPY --from=atom-dev:latest /usr/local /usr/local

# Update library cache
RUN ldconfig

CMD ["/bin/bash"]
EOF

    # Docker Compose for development
    cat > "$docker_dir/docker-compose.yml" << 'EOF'
version: '3.8'

services:
  atom-dev:
    build:
      context: ..
      dockerfile: docker/Dockerfile.dev
    volumes:
      - ..:/workspace
      - atom-build:/workspace/build
    environment:
      - CMAKE_BUILD_TYPE=Debug
    working_dir: /workspace

  atom-runtime:
    build:
      context: ..
      dockerfile: docker/Dockerfile.runtime
    depends_on:
      - atom-dev
    volumes:
      - atom-data:/data

volumes:
  atom-build:
  atom-data:
EOF

    log_info "Docker images created in: $docker_dir"
}

# Main command handling
case "${1:-help}" in
    install-deps)
        install_system_dependencies
        ;;
    setup-vcpkg)
        setup_vcpkg
        ;;
    setup-python)
        setup_python_environment
        ;;
    create-deb)
        create_deb_package
        ;;
    create-rpm)
        create_rpm_package
        ;;
    create-conan)
        create_conan_package
        ;;
    create-homebrew)
        create_homebrew_formula
        ;;
    create-vcpkg)
        create_vcpkg_port
        ;;
    create-arch)
        create_arch_package
        ;;
    create-docker)
        create_docker_images
        ;;
    create-all-packages)
        create_deb_package
        create_rpm_package
        create_conan_package
        create_homebrew_formula
        create_vcpkg_port
        create_arch_package
        create_docker_images
        ;;
    all)
        install_system_dependencies
        setup_vcpkg
        setup_python_environment
        ;;
    help|*)
        echo "Usage: $0 {install-deps|setup-vcpkg|setup-python|create-deb|create-rpm|create-conan|create-homebrew|create-vcpkg|create-arch|create-docker|create-all-packages|all}"
        echo ""
        echo "Commands:"
        echo "  install-deps        Install system dependencies"
        echo "  setup-vcpkg         Setup and configure vcpkg"
        echo "  setup-python        Setup Python virtual environment"
        echo "  create-deb          Create Debian package"
        echo "  create-rpm          Create RPM package"
        echo "  create-conan        Create Conan package"
        echo "  create-homebrew     Create Homebrew formula"
        echo "  create-vcpkg        Create vcpkg port"
        echo "  create-arch         Create Arch Linux PKGBUILD"
        echo "  create-docker       Create Docker images"
        echo "  create-all-packages Create all package formats"
        echo "  all                 Run install-deps, setup-vcpkg, and setup-python"
        echo "  help                Show this help message"
        ;;
esac
