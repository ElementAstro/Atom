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
    all)
        install_system_dependencies
        setup_vcpkg
        setup_python_environment
        ;;
    help|*)
        echo "Usage: $0 {install-deps|setup-vcpkg|setup-python|create-deb|create-rpm|all}"
        echo ""
        echo "Commands:"
        echo "  install-deps   Install system dependencies"
        echo "  setup-vcpkg    Setup and configure vcpkg"
        echo "  setup-python   Setup Python virtual environment"
        echo "  create-deb     Create Debian package"
        echo "  create-rpm     Create RPM package"
        echo "  all            Run install-deps, setup-vcpkg, and setup-python"
        echo "  help           Show this help message"
        ;;
esac
