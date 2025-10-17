#!/bin/bash
# Atom Project Distribution Creation Script
# Creates comprehensive distribution packages for multiple platforms and package managers
# Author: Max Qian

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$PROJECT_ROOT/dist"
BUILD_DIR="$PROJECT_ROOT/build"
TEMP_DIR="$PROJECT_ROOT/temp-dist"

# Logging functions
log_info() { echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') $*"; }
log_warn() { echo "[WARN] $(date '+%Y-%m-%d %H:%M:%S') $*"; }
log_error() { echo "[ERROR] $(date '+%Y-%m-%d %H:%M:%S') $*" >&2; }

# Configuration
VERSION=$(cd "$PROJECT_ROOT" && git describe --tags --always --dirty 2>/dev/null || echo "0.1.0")
PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Clean and setup directories
setup_directories() {
    log_info "Setting up distribution directories..."

    rm -rf "$DIST_DIR" "$TEMP_DIR"
    mkdir -p "$DIST_DIR" "$TEMP_DIR"

    log_info "Distribution directory: $DIST_DIR"
    log_info "Temporary directory: $TEMP_DIR"
}

# Get project metadata
get_project_info() {
    log_info "Gathering project information..."

    echo "Project: Atom"
    echo "Version: $VERSION"
    echo "Platform: $PLATFORM"
    echo "Architecture: $ARCH"
    echo "Build timestamp: $TIMESTAMP"
}

# Build project for distribution
build_for_distribution() {
    log_info "Building project for distribution..."

    local build_type="Release"
    local install_prefix="$TEMP_DIR/install"

    # Configure CMake for distribution
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DCMAKE_INSTALL_PREFIX="$install_prefix" \
        -DATOM_BUILD_EXAMPLES=ON \
        -DATOM_BUILD_TESTS=OFF \
        -DATOM_BUILD_PYTHON_BINDINGS=ON \
        -DATOM_BUILD_DOCS=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -G Ninja

    # Build
    cmake --build "$BUILD_DIR" --config "$build_type" --parallel

    # Install to temporary directory
    cmake --install "$BUILD_DIR" --config "$build_type"

    log_info "Build and installation completed"
}

# Create source distribution
create_source_distribution() {
    log_info "Creating source distribution..."

    local source_name="atom-${VERSION}-source"
    local source_dir="$TEMP_DIR/$source_name"

    # Copy source files
    mkdir -p "$source_dir"

    # Copy essential files and directories
    cp -r "$PROJECT_ROOT/atom" "$source_dir/"
    cp -r "$PROJECT_ROOT/cmake" "$source_dir/"
    cp -r "$PROJECT_ROOT/example" "$source_dir/"
    cp -r "$PROJECT_ROOT/python" "$source_dir/"
    cp -r "$PROJECT_ROOT/tests" "$source_dir/"
    cp -r "$PROJECT_ROOT/extra" "$source_dir/"
    cp -r "$PROJECT_ROOT/scripts" "$source_dir/"

    # Copy configuration files
    cp "$PROJECT_ROOT/CMakeLists.txt" "$source_dir/"
    cp "$PROJECT_ROOT/xmake.lua" "$source_dir/"
    cp "$PROJECT_ROOT/vcpkg.json" "$source_dir/"
    cp "$PROJECT_ROOT/pyproject.toml" "$source_dir/"
    cp "$PROJECT_ROOT/setup.py" "$source_dir/"

    # Copy documentation
    cp "$PROJECT_ROOT/README.md" "$source_dir/"
    cp "$PROJECT_ROOT/LICENSE" "$source_dir/"
    cp "$PROJECT_ROOT/CHANGELOG.md" "$source_dir/" 2>/dev/null || true
    cp "$PROJECT_ROOT/CONTRIBUTING.md" "$source_dir/" 2>/dev/null || true
    cp "$PROJECT_ROOT/Doxyfile" "$source_dir/" 2>/dev/null || true

    # Copy build scripts
    cp "$PROJECT_ROOT/build.sh" "$source_dir/"
    cp "$PROJECT_ROOT/build.bat" "$source_dir/"

    # Create archive
    cd "$TEMP_DIR"
    tar -czf "$DIST_DIR/${source_name}.tar.gz" "$source_name"
    zip -r "$DIST_DIR/${source_name}.zip" "$source_name" >/dev/null

    log_info "Source distribution created: ${source_name}.tar.gz, ${source_name}.zip"
}

# Create binary distribution
create_binary_distribution() {
    log_info "Creating binary distribution..."

    local binary_name="atom-${VERSION}-${PLATFORM}-${ARCH}"
    local binary_dir="$TEMP_DIR/$binary_name"
    local install_dir="$TEMP_DIR/install"

    if [[ ! -d "$install_dir" ]]; then
        log_error "Installation directory not found. Run build first."
        return 1
    fi

    # Create binary distribution structure
    mkdir -p "$binary_dir"

    # Copy installed files
    cp -r "$install_dir"/* "$binary_dir/"

    # Add distribution metadata
    cat > "$binary_dir/DISTRIBUTION_INFO.txt" << EOF
Atom Library Binary Distribution
================================

Version: $VERSION
Platform: $PLATFORM
Architecture: $ARCH
Build Date: $(date)
Build Type: Release

Installation Instructions:
1. Extract this archive to your desired location
2. Add the 'lib' directory to your library path
3. Add the 'include' directory to your include path
4. For CMake projects, set CMAKE_PREFIX_PATH to this directory

For more information, visit: https://github.com/ElementAstro/Atom
EOF

    # Create archive
    cd "$TEMP_DIR"
    tar -czf "$DIST_DIR/${binary_name}.tar.gz" "$binary_name"

    if command -v zip >/dev/null 2>&1; then
        zip -r "$DIST_DIR/${binary_name}.zip" "$binary_name" >/dev/null
    fi

    log_info "Binary distribution created: ${binary_name}.tar.gz"
}

# Create Python wheel
create_python_wheel() {
    log_info "Creating Python wheel..."

    cd "$PROJECT_ROOT"

    # Clean previous builds
    rm -rf build/ dist/ *.egg-info/

    # Build wheel
    python -m pip install --upgrade build
    python -m build --wheel

    # Move wheel to distribution directory
    if [[ -d "dist" ]]; then
        mv dist/*.whl "$DIST_DIR/" 2>/dev/null || true
        rm -rf dist/
    fi

    log_info "Python wheel created"
}

# Create Debian package
create_debian_package() {
    if [[ "$PLATFORM" != "linux" ]]; then
        log_warn "Debian packages can only be created on Linux"
        return 0
    fi

    log_info "Creating Debian package..."

    local package_name="libatom-dev"
    local deb_version="${VERSION}-1"
    local deb_dir="$TEMP_DIR/debian/$package_name-$deb_version"

    mkdir -p "$deb_dir/DEBIAN"
    mkdir -p "$deb_dir/usr"

    # Copy installed files
    cp -r "$TEMP_DIR/install"/* "$deb_dir/usr/"

    # Create control file
    cat > "$deb_dir/DEBIAN/control" << EOF
Package: $package_name
Version: $deb_version
Section: libdevel
Priority: optional
Architecture: $(dpkg --print-architecture 2>/dev/null || echo "amd64")
Depends: libssl-dev, zlib1g-dev, libsqlite3-dev, libfmt-dev
Maintainer: Max Qian <max@example.com>
Description: Atom foundational library for astronomical software
 A comprehensive C++20 library providing core functionality
 for astronomical software development including algorithms,
 async utilities, networking, and more.
Homepage: https://github.com/ElementAstro/Atom
EOF

    # Build package
    if command -v dpkg-deb >/dev/null 2>&1; then
        dpkg-deb --build "$deb_dir" "$DIST_DIR/"
        log_info "Debian package created"
    else
        log_warn "dpkg-deb not available, skipping Debian package creation"
    fi
}

# Create RPM package spec
create_rpm_spec() {
    if [[ "$PLATFORM" != "linux" ]]; then
        log_warn "RPM packages are typically created on Linux"
        return 0
    fi

    log_info "Creating RPM spec file..."

    local spec_file="$DIST_DIR/atom.spec"

    cat > "$spec_file" << EOF
Name:           atom
Version:        ${VERSION}
Release:        1%{?dist}
Summary:        Foundational library for astronomical software

License:        GPL-3.0
URL:            https://github.com/ElementAstro/Atom
Source0:        atom-${VERSION}-source.tar.gz

BuildRequires:  gcc-c++ >= 9
BuildRequires:  cmake >= 3.21
BuildRequires:  ninja-build
BuildRequires:  openssl-devel
BuildRequires:  zlib-devel
BuildRequires:  sqlite-devel
BuildRequires:  fmt-devel

Requires:       openssl-devel
Requires:       zlib-devel
Requires:       sqlite-devel
Requires:       fmt-devel

%description
A comprehensive C++20 library providing core functionality
for astronomical software development including algorithms,
async utilities, networking, image processing, and more.

%prep
%autosetup

%build
%cmake -DATOM_BUILD_EXAMPLES=ON -DATOM_BUILD_TESTS=OFF -DATOM_BUILD_PYTHON_BINDINGS=ON
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_includedir}/atom/
%{_libdir}/libatom*.a
%{_libdir}/cmake/atom/

%changelog
* $(date '+%a %b %d %Y') Max Qian <max@example.com> - ${VERSION}-1
- Version ${VERSION} release
EOF

    log_info "RPM spec file created: $spec_file"
}

# Create checksums
create_checksums() {
    log_info "Creating checksums..."

    cd "$DIST_DIR"

    # Create SHA256 checksums
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum * > checksums.sha256
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 * > checksums.sha256
    fi

    # Create MD5 checksums
    if command -v md5sum >/dev/null 2>&1; then
        md5sum * > checksums.md5
    elif command -v md5 >/dev/null 2>&1; then
        md5 * > checksums.md5
    fi

    log_info "Checksums created"
}

# Generate distribution report
generate_report() {
    log_info "Generating distribution report..."

    local report_file="$DIST_DIR/DISTRIBUTION_REPORT.md"

    cat > "$report_file" << EOF
# Atom Library Distribution Report

**Generated:** $(date)
**Version:** $VERSION
**Platform:** $PLATFORM
**Architecture:** $ARCH

## Distribution Contents

EOF

    cd "$DIST_DIR"
    for file in *; do
        if [[ -f "$file" && "$file" != "DISTRIBUTION_REPORT.md" ]]; then
            local size=$(du -h "$file" | cut -f1)
            echo "- **$file** ($size)" >> "$report_file"
        fi
    done

    cat >> "$report_file" << EOF

## Installation Instructions

### Source Distribution
1. Extract the source archive
2. Run \`./build.sh --release --examples --python\`
3. Run \`sudo cmake --install build\`

### Binary Distribution
1. Extract the binary archive
2. Add the installation directory to your CMAKE_PREFIX_PATH
3. Use \`find_package(atom CONFIG REQUIRED)\` in your CMakeLists.txt

### Python Package
\`\`\`bash
pip install atom-${VERSION}-*.whl
\`\`\`

### Debian Package
\`\`\`bash
sudo dpkg -i libatom-dev_${VERSION}-1_*.deb
sudo apt-get install -f  # Fix dependencies if needed
\`\`\`

## Verification

Verify checksums:
\`\`\`bash
sha256sum -c checksums.sha256
md5sum -c checksums.md5
\`\`\`

For more information, visit: https://github.com/ElementAstro/Atom
EOF

    log_info "Distribution report created: $report_file"
}

# Main execution
main() {
    log_info "Starting Atom distribution creation..."

    get_project_info
    setup_directories
    build_for_distribution
    create_source_distribution
    create_binary_distribution
    create_python_wheel
    create_debian_package
    create_rpm_spec
    create_checksums
    generate_report

    # Cleanup
    rm -rf "$TEMP_DIR"

    log_info "Distribution creation completed!"
    log_info "Distribution files created in: $DIST_DIR"

    # List created files
    echo ""
    echo "Created distribution files:"
    ls -lh "$DIST_DIR"
}

# Run main function
main "$@"
