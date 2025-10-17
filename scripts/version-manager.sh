#!/bin/bash
# Atom Project Version Management Script
# Handles semantic versioning, tagging, and changelog generation
# Author: Max Qian

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VERSION_FILE="$PROJECT_ROOT/VERSION"
CHANGELOG_FILE="$PROJECT_ROOT/CHANGELOG.md"

# Logging functions
log_info() { echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') $*"; }
log_warn() { echo "[WARN] $(date '+%Y-%m-%d %H:%M:%S') $*"; }
log_error() { echo "[ERROR] $(date '+%Y-%m-%d %H:%M:%S') $*" >&2; }

# Version utilities
get_current_version() {
    if [[ -f "$VERSION_FILE" ]]; then
        cat "$VERSION_FILE"
    elif git tag --list | grep -E '^v?[0-9]+\.[0-9]+\.[0-9]+' | sort -V | tail -n1 | sed 's/^v//'; then
        return 0
    else
        echo "0.1.0"
    fi
}

get_git_version() {
    if command -v git &> /dev/null && git rev-parse --git-dir > /dev/null 2>&1; then
        local version=$(git describe --tags --always --dirty 2>/dev/null || echo "unknown")
        local commit=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
        local branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
        echo "${version}-${commit}-${branch}"
    else
        echo "$(get_current_version)-unknown-unknown"
    fi
}

validate_version() {
    local version=$1
    if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        log_error "Invalid version format: $version (expected: X.Y.Z)"
        return 1
    fi
    return 0
}

increment_version() {
    local current_version=$1
    local increment_type=$2

    IFS='.' read -ra VERSION_PARTS <<< "$current_version"
    local major=${VERSION_PARTS[0]}
    local minor=${VERSION_PARTS[1]}
    local patch=${VERSION_PARTS[2]}

    case "$increment_type" in
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        patch)
            patch=$((patch + 1))
            ;;
        *)
            log_error "Invalid increment type: $increment_type (expected: major, minor, patch)"
            return 1
            ;;
    esac

    echo "$major.$minor.$patch"
}

# Update version in project files
update_version_files() {
    local new_version=$1

    log_info "Updating version to $new_version in project files..."

    # Update VERSION file
    echo "$new_version" > "$VERSION_FILE"

    # Update CMakeLists.txt
    if [[ -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
        sed -i.bak "s/VERSION [0-9]\+\.[0-9]\+\.[0-9]\+/VERSION $new_version/" "$PROJECT_ROOT/CMakeLists.txt"
        rm -f "$PROJECT_ROOT/CMakeLists.txt.bak"
    fi

    # Update xmake.lua
    if [[ -f "$PROJECT_ROOT/xmake.lua" ]]; then
        sed -i.bak "s/set_version(\"[0-9]\+\.[0-9]\+\.[0-9]\+\")/set_version(\"$new_version\")/" "$PROJECT_ROOT/xmake.lua"
        rm -f "$PROJECT_ROOT/xmake.lua.bak"
    fi

    # Update vcpkg.json
    if [[ -f "$PROJECT_ROOT/vcpkg.json" ]]; then
        sed -i.bak "s/\"version\": \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/\"version\": \"$new_version\"/" "$PROJECT_ROOT/vcpkg.json"
        rm -f "$PROJECT_ROOT/vcpkg.json.bak"
    fi

    log_info "Version files updated successfully"
}

# Generate changelog
generate_changelog() {
    local version=$1
    local previous_tag=$(git tag --list | grep -E '^v?[0-9]+\.[0-9]+\.[0-9]+' | sort -V | tail -n2 | head -n1)

    log_info "Generating changelog for version $version..."

    # Create changelog header if file doesn't exist
    if [[ ! -f "$CHANGELOG_FILE" ]]; then
        cat > "$CHANGELOG_FILE" << 'EOF'
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

EOF
    fi

    # Generate changelog entry
    local temp_changelog=$(mktemp)
    local date=$(date '+%Y-%m-%d')

    echo "## [$version] - $date" > "$temp_changelog"
    echo "" >> "$temp_changelog"

    if [[ -n "$previous_tag" ]]; then
        log_info "Generating changelog from $previous_tag to HEAD"

        # Get commits since last tag
        git log --pretty=format:"- %s" "$previous_tag..HEAD" >> "$temp_changelog" 2>/dev/null || {
            echo "- Initial release" >> "$temp_changelog"
        }
    else
        echo "- Initial release" >> "$temp_changelog"
    fi

    echo "" >> "$temp_changelog"
    echo "" >> "$temp_changelog"

    # Insert new changelog entry at the top
    local temp_full_changelog=$(mktemp)
    head -n 6 "$CHANGELOG_FILE" > "$temp_full_changelog"
    cat "$temp_changelog" >> "$temp_full_changelog"
    tail -n +7 "$CHANGELOG_FILE" >> "$temp_full_changelog"

    mv "$temp_full_changelog" "$CHANGELOG_FILE"
    rm -f "$temp_changelog"

    log_info "Changelog updated successfully"
}

# Create git tag
create_git_tag() {
    local version=$1
    local tag_name="v$version"

    log_info "Creating git tag: $tag_name"

    # Check if tag already exists
    if git tag --list | grep -q "^$tag_name$"; then
        log_error "Tag $tag_name already exists"
        return 1
    fi

    # Create annotated tag
    git tag -a "$tag_name" -m "Release version $version"

    log_info "Git tag $tag_name created successfully"
    log_info "To push the tag, run: git push origin $tag_name"
}

# Release workflow
create_release() {
    local increment_type=$1
    local current_version=$(get_current_version)
    local new_version=$(increment_version "$current_version" "$increment_type")

    log_info "Creating release: $current_version -> $new_version"

    # Validate git repository
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        log_error "Not in a git repository"
        return 1
    fi

    # Check for uncommitted changes
    if ! git diff-index --quiet HEAD --; then
        log_error "There are uncommitted changes. Please commit or stash them first."
        return 1
    fi

    # Update version files
    update_version_files "$new_version"

    # Generate changelog
    generate_changelog "$new_version"

    # Commit changes
    git add "$VERSION_FILE" "$CHANGELOG_FILE" CMakeLists.txt xmake.lua vcpkg.json 2>/dev/null || true
    git commit -m "Release version $new_version"

    # Create tag
    create_git_tag "$new_version"

    log_info "Release $new_version created successfully!"
    log_info "Next steps:"
    log_info "  1. Review the changes: git show HEAD"
    log_info "  2. Push the changes: git push origin main"
    log_info "  3. Push the tag: git push origin v$new_version"
}

# Main command handling
case "${1:-help}" in
    current)
        echo "Current version: $(get_current_version)"
        echo "Git version: $(get_git_version)"
        ;;
    set)
        if [[ -z "${2:-}" ]]; then
            log_error "Version required. Usage: $0 set <version>"
            exit 1
        fi
        if validate_version "$2"; then
            update_version_files "$2"
            log_info "Version set to $2"
        fi
        ;;
    bump)
        if [[ -z "${2:-}" ]]; then
            log_error "Increment type required. Usage: $0 bump {major|minor|patch}"
            exit 1
        fi
        current_version=$(get_current_version)
        new_version=$(increment_version "$current_version" "$2")
        update_version_files "$new_version"
        log_info "Version bumped from $current_version to $new_version"
        ;;
    changelog)
        version=${2:-$(get_current_version)}
        generate_changelog "$version"
        ;;
    tag)
        version=${2:-$(get_current_version)}
        create_git_tag "$version"
        ;;
    release)
        increment_type=${2:-patch}
        create_release "$increment_type"
        ;;
    help|*)
        echo "Usage: $0 {current|set|bump|changelog|tag|release} [options]"
        echo ""
        echo "Commands:"
        echo "  current              Show current version information"
        echo "  set <version>        Set specific version (X.Y.Z format)"
        echo "  bump {major|minor|patch}  Increment version"
        echo "  changelog [version]  Generate changelog for version"
        echo "  tag [version]        Create git tag for version"
        echo "  release [type]       Create full release (bump, changelog, tag)"
        echo "  help                 Show this help message"
        echo ""
        echo "Examples:"
        echo "  $0 current           # Show current version"
        echo "  $0 set 1.2.3         # Set version to 1.2.3"
        echo "  $0 bump minor        # Increment minor version"
        echo "  $0 release major     # Create major release"
        ;;
esac
