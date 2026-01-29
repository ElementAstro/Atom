#!/bin/bash

# ============================================
# Atom Project Version Management Script
# Handles semantic versioning, tagging, changelog generation,
# and C++ version file generation
# Author: Max Qian
# ============================================

set -euo pipefail

# ============================================
# 初始化配置
# ============================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VERSION_FILE="$PROJECT_ROOT/VERSION"
CHANGELOG_FILE="$PROJECT_ROOT/CHANGELOG.md"
DEFAULT_BRANCH="master"
OUTPUT_DIR="$PROJECT_ROOT"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ============================================
# 日志函数
# ============================================

log_info() {
    echo -e "${CYAN}[INFO]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*" >&2
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*"
}

# ============================================
# 辅助函数
# ============================================

# 安全获取值，失败返回默认值
get_safe_value() {
    local result
    result=$(eval "$1" 2>/dev/null) || true
    if [[ -z "$result" ]]; then
        echo "${2:-unknown}"
    else
        echo "$result"
    fi
}

# 转义 C++ 字符串中的特殊字符
escape_cpp_string() {
    local value="$1"
    value="${value//\\/\\\\}"
    value="${value//\"/\\\"}"
    echo "$value"
}

# 检查是否在 Git 仓库中
is_git_repo() {
    git rev-parse --git-dir > /dev/null 2>&1
}

# 检查是否有未提交的更改
has_uncommitted_changes() {
    ! git diff-index --quiet HEAD -- 2>/dev/null
}

# ============================================
# 版本管理函数
# ============================================

get_current_version() {
    # 优先从 VERSION 文件读取
    if [[ -f "$VERSION_FILE" ]]; then
        local version
        version=$(cat "$VERSION_FILE" | tr -d '[:space:]')
        if [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+ ]]; then
            echo "$version"
            return 0
        fi
    fi

    # 从 Git 标签获取
    if is_git_repo; then
        local latest_tag
        latest_tag=$(git tag --list 2>/dev/null | grep -E '^v?[0-9]+\.[0-9]+\.[0-9]+' | \
            sed 's/^v//' | sort -V | tail -n1)
        if [[ -n "$latest_tag" ]]; then
            echo "$latest_tag"
            return 0
        fi
    fi

    echo "0.1.0"
}

get_git_version() {
    if is_git_repo; then
        local version commit branch
        version=$(get_safe_value "git describe --tags --always --dirty" "unknown")
        commit=$(get_safe_value "git rev-parse --short HEAD" "unknown")
        branch=$(get_safe_value "git rev-parse --abbrev-ref HEAD" "unknown")
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

# ============================================
# 更新版本文件
# ============================================

update_version_files() {
    local new_version=$1

    log_info "Updating version to $new_version in project files..."

    # 更新 VERSION 文件
    echo -n "$new_version" > "$VERSION_FILE"
    log_info "Updated VERSION"

    # 更新 CMakeLists.txt
    if [[ -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
        sed -i.bak "s/VERSION [0-9]\+\.[0-9]\+\.[0-9]\+/VERSION $new_version/" \
            "$PROJECT_ROOT/CMakeLists.txt"
        rm -f "$PROJECT_ROOT/CMakeLists.txt.bak"
        log_info "Updated CMakeLists.txt"
    fi

    # 更新 xmake.lua
    if [[ -f "$PROJECT_ROOT/xmake.lua" ]]; then
        sed -i.bak "s/set_version(\"[0-9]\+\.[0-9]\+\.[0-9]\+\")/set_version(\"$new_version\")/" \
            "$PROJECT_ROOT/xmake.lua"
        rm -f "$PROJECT_ROOT/xmake.lua.bak"
        log_info "Updated xmake.lua"
    fi

    # 更新 vcpkg.json
    if [[ -f "$PROJECT_ROOT/vcpkg.json" ]]; then
        sed -i.bak "s/\"version\": \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/\"version\": \"$new_version\"/" \
            "$PROJECT_ROOT/vcpkg.json"
        rm -f "$PROJECT_ROOT/vcpkg.json.bak"
        log_info "Updated vcpkg.json"
    fi

    # 更新 package.json
    if [[ -f "$PROJECT_ROOT/package.json" ]]; then
        sed -i.bak "s/\"version\": \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/\"version\": \"$new_version\"/" \
            "$PROJECT_ROOT/package.json"
        rm -f "$PROJECT_ROOT/package.json.bak"
        log_info "Updated package.json"
    fi

    # 更新 Cargo.toml
    if [[ -f "$PROJECT_ROOT/Cargo.toml" ]]; then
        sed -i.bak "s/version = \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version = \"$new_version\"/" \
            "$PROJECT_ROOT/Cargo.toml"
        rm -f "$PROJECT_ROOT/Cargo.toml.bak"
        log_info "Updated Cargo.toml"
    fi

    # 更新 meson.build
    if [[ -f "$PROJECT_ROOT/meson.build" ]]; then
        sed -i.bak "s/version: '[0-9]\+\.[0-9]\+\.[0-9]\+'/version: '$new_version'/" \
            "$PROJECT_ROOT/meson.build"
        rm -f "$PROJECT_ROOT/meson.build.bak"
        log_info "Updated meson.build"
    fi

    log_success "Version files updated successfully"
}

# ============================================
# Changelog 生成
# ============================================

generate_changelog() {
    local version=$1

    log_info "Generating changelog for version $version..."

    # 获取上一个标签
    local previous_tag=""
    if is_git_repo; then
        previous_tag=$(git tag --list 2>/dev/null | grep -E '^v?[0-9]+\.[0-9]+\.[0-9]+' | \
            sort -V | tail -n2 | head -n1)
    fi

    # 创建 Changelog 头部
    if [[ ! -f "$CHANGELOG_FILE" ]]; then
        cat > "$CHANGELOG_FILE" << 'EOF'
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

EOF
    fi

    # 生成新的 Changelog 条目
    local temp_changelog
    temp_changelog=$(mktemp)
    local date
    date=$(date '+%Y-%m-%d')

    echo "## [$version] - $date" > "$temp_changelog"
    echo "" >> "$temp_changelog"

    if [[ -n "$previous_tag" ]] && is_git_repo; then
        log_info "Generating changelog from $previous_tag to HEAD"

        # 分类提交信息
        local features="" fixes="" docs="" refactor="" other=""

        while IFS= read -r commit; do
            [[ -z "$commit" ]] && continue

            if [[ "$commit" =~ ^-[[:space:]]*(feat|feature|add) ]]; then
                features+="$commit"$'\n'
            elif [[ "$commit" =~ ^-[[:space:]]*(fix|bug|patch) ]]; then
                fixes+="$commit"$'\n'
            elif [[ "$commit" =~ ^-[[:space:]]*(doc|docs) ]]; then
                docs+="$commit"$'\n'
            elif [[ "$commit" =~ ^-[[:space:]]*(refactor|refact|perf) ]]; then
                refactor+="$commit"$'\n'
            else
                other+="$commit"$'\n'
            fi
        done < <(git log --pretty=format:"- %s (%h)" "$previous_tag..HEAD" 2>/dev/null)

        if [[ -n "$features" ]]; then
            echo "### Added" >> "$temp_changelog"
            echo "$features" >> "$temp_changelog"
        fi
        if [[ -n "$fixes" ]]; then
            echo "### Fixed" >> "$temp_changelog"
            echo "$fixes" >> "$temp_changelog"
        fi
        if [[ -n "$docs" ]]; then
            echo "### Documentation" >> "$temp_changelog"
            echo "$docs" >> "$temp_changelog"
        fi
        if [[ -n "$refactor" ]]; then
            echo "### Changed" >> "$temp_changelog"
            echo "$refactor" >> "$temp_changelog"
        fi
        if [[ -n "$other" ]]; then
            echo "### Other" >> "$temp_changelog"
            echo "$other" >> "$temp_changelog"
        fi

        # 如果没有任何分类的提交
        if [[ -z "$features" && -z "$fixes" && -z "$docs" && -z "$refactor" && -z "$other" ]]; then
            echo "- No changes recorded" >> "$temp_changelog"
            echo "" >> "$temp_changelog"
        fi
    else
        echo "### Added" >> "$temp_changelog"
        echo "- Initial release" >> "$temp_changelog"
        echo "" >> "$temp_changelog"
    fi

    echo "" >> "$temp_changelog"

    # 插入新条目到 Changelog
    local temp_full_changelog
    temp_full_changelog=$(mktemp)
    head -n 6 "$CHANGELOG_FILE" > "$temp_full_changelog"
    cat "$temp_changelog" >> "$temp_full_changelog"
    tail -n +7 "$CHANGELOG_FILE" >> "$temp_full_changelog"

    mv "$temp_full_changelog" "$CHANGELOG_FILE"
    rm -f "$temp_changelog"

    log_success "Changelog updated successfully"
}

# ============================================
# Git 标签管理
# ============================================

create_git_tag() {
    local version=$1
    local tag_name="v$version"

    log_info "Creating git tag: $tag_name"

    if ! is_git_repo; then
        log_error "Not in a git repository"
        return 1
    fi

    # 检查标签是否已存在
    if git tag --list | grep -q "^$tag_name$"; then
        log_error "Tag $tag_name already exists"
        return 1
    fi

    # 创建带注释的标签
    git tag -a "$tag_name" -m "Release version $version"

    log_success "Git tag $tag_name created successfully"
    log_info "To push the tag, run: git push origin $tag_name"
}

# ============================================
# C++ 版本文件生成
# ============================================

generate_version_cpp() {
    local version="${1:-$(get_current_version)}"
    local output_dir="${2:-$PROJECT_ROOT}"

    log_info "Generating C++ version files..."

    # ============================================
    # 收集 Git 信息
    # ============================================

    local git_hash git_tag git_branch git_commit_time git_dirty git_commit_count

    git_hash=$(get_safe_value "git rev-parse --short HEAD" "unknown")
    git_tag=$(get_safe_value "git describe --tags --abbrev=0" "v$version")
    git_branch=$(get_safe_value "git rev-parse --abbrev-ref HEAD" "$DEFAULT_BRANCH")

    # CI/CD 环境变量支持
    if [[ "$git_branch" == "unknown" || "$git_branch" == "HEAD" ]]; then
        git_branch="${CI_COMMIT_BRANCH:-${GITHUB_REF_NAME:-${GIT_BRANCH:-${BRANCH_NAME:-$DEFAULT_BRANCH}}}}"
    fi

    git_commit_time=$(get_safe_value "git log -1 --format=%cd --date=format:'%Y-%m-%d %H:%M:%S'" "unknown")

    # 检测 dirty 状态
    git_dirty=""
    if is_git_repo; then
        local git_status
        git_status=$(git status --porcelain 2>/dev/null)
        if [[ -n "$git_status" ]]; then
            git_dirty="-dirty"
        fi
    fi

    # 提交数量
    git_commit_count=$(get_safe_value "git rev-list --count HEAD" "0")

    # ============================================
    # 构建时间（中国时区 UTC+8）
    # ============================================

    local build_time
    if command -v date &> /dev/null; then
        # 尝试使用 TZ 环境变量
        build_time=$(TZ='Asia/Shanghai' date '+%Y-%m-%d %H:%M:%S' 2>/dev/null) || \
        build_time=$(date -u '+%Y-%m-%d %H:%M:%S' 2>/dev/null) || \
        build_time="unknown"
    else
        build_time="unknown"
    fi

    # ============================================
    # 系统信息获取
    # ============================================

    local system_version kernel_version architecture compiler_info build_type

    # 系统版本
    if [[ -f /etc/os-release ]]; then
        system_version=$(source /etc/os-release && echo "$PRETTY_NAME")
    elif command -v uname &> /dev/null; then
        system_version=$(uname -s 2>/dev/null || echo "unknown")
    else
        system_version="unknown"
    fi

    # 内核版本
    kernel_version=$(get_safe_value "uname -r" "unknown")

    # 架构
    architecture=$(get_safe_value "uname -m" "unknown")

    # 编译器信息
    compiler_info="unknown"
    if [[ -n "${CXX:-}" ]] && command -v "$CXX" &> /dev/null; then
        compiler_info=$("$CXX" --version 2>/dev/null | head -n1 || echo "unknown")
    elif command -v clang++ &> /dev/null; then
        compiler_info=$(clang++ --version 2>/dev/null | head -n1 || echo "unknown")
    elif command -v g++ &> /dev/null; then
        compiler_info=$(g++ --version 2>/dev/null | head -n1 || echo "unknown")
    fi

    # 构建类型
    build_type="${CMAKE_BUILD_TYPE:-${BUILD_TYPE:-Release}}"

    # ============================================
    # 转义特殊字符
    # ============================================

    system_version=$(escape_cpp_string "$system_version")
    compiler_info=$(escape_cpp_string "$compiler_info")

    # ============================================
    # 解析版本号
    # ============================================

    IFS='.' read -ra VERSION_PARTS <<< "$version"
    local major=${VERSION_PARTS[0]:-0}
    local minor=${VERSION_PARTS[1]:-0}
    local patch=${VERSION_PARTS[2]:-0}

    # ============================================
    # 生成 version.h 头文件
    # ============================================

    local header_file="$output_dir/version.h"

    cat > "$header_file" << EOF
/**
 * @file version.h
 * @brief Auto-generated version information header
 * @note This file is auto-generated by version.sh, do not modify manually
 */

#ifndef ATOM_VERSION_H
#define ATOM_VERSION_H

#include <string_view>
#include <cstdint>

namespace atom::version {

// Version components
constexpr uint32_t MAJOR = $major;
constexpr uint32_t MINOR = $minor;
constexpr uint32_t PATCH = $patch;

// Git information
extern const std::string_view GIT_HASH;
extern const std::string_view GIT_TAG;
extern const std::string_view GIT_BRANCH;
extern const std::string_view GIT_COMMIT_TIME;
extern const std::string_view GIT_DIRTY;
extern const uint32_t GIT_COMMIT_COUNT;

// Build information
extern const std::string_view BUILD_TIME;
extern const std::string_view BUILD_TYPE;

// System information
extern const std::string_view SYSTEM_VERSION;
extern const std::string_view KERNEL_VERSION;
extern const std::string_view ARCHITECTURE;
extern const std::string_view COMPILER_INFO;

// Convenience functions
constexpr std::string_view VERSION_STRING = "$version";

inline constexpr uint32_t version_number() {
    return (MAJOR << 16) | (MINOR << 8) | PATCH;
}

inline constexpr bool is_dirty() {
    return sizeof("$git_dirty") > 1;
}

// Runtime version string with git info
const std::string& full_version_string();

} // namespace atom::version

#endif // ATOM_VERSION_H
EOF

    log_info "Generated $header_file"

    # ============================================
    # 生成 version.cpp 源文件
    # ============================================

    local source_file="$output_dir/version.cpp"

    cat > "$source_file" << EOF
/**
 * @file version.cpp
 * @brief Auto-generated version information implementation
 * @note This file is auto-generated by version.sh, do not modify manually
 * @date $build_time
 */

#include "version.h"
#include <string>

namespace atom::version {

// Git information
const std::string_view GIT_HASH = "$git_hash";
const std::string_view GIT_TAG = "$git_tag";
const std::string_view GIT_BRANCH = "$git_branch";
const std::string_view GIT_COMMIT_TIME = "$git_commit_time";
const std::string_view GIT_DIRTY = "$git_dirty";
const uint32_t GIT_COMMIT_COUNT = $git_commit_count;

// Build information
const std::string_view BUILD_TIME = "$build_time";
const std::string_view BUILD_TYPE = "$build_type";

// System information
const std::string_view SYSTEM_VERSION = "$system_version";
const std::string_view KERNEL_VERSION = "$kernel_version";
const std::string_view ARCHITECTURE = "$architecture";
const std::string_view COMPILER_INFO = "$compiler_info";

// Runtime version string
const std::string& full_version_string() {
    static const std::string version =
        std::string(VERSION_STRING) +
        std::string(GIT_DIRTY) +
        " (" + std::string(GIT_HASH) +
        " " + std::string(GIT_BRANCH) + ")";
    return version;
}

} // namespace atom::version
EOF

    log_info "Generated $source_file"

    # ============================================
    # 输出结果
    # ============================================

    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN} Version Information Generated${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""

    printf "%-15s : %s\n" "Version" "$version"
    printf "%-15s : %s\n" "Git Hash" "$git_hash$git_dirty"
    printf "%-15s : %s\n" "Git Tag" "$git_tag"
    printf "%-15s : %s\n" "Git Branch" "$git_branch"
    printf "%-15s : %s\n" "Commit Time" "$git_commit_time"
    printf "%-15s : %s\n" "Commit Count" "$git_commit_count"
    printf "%-15s : %s\n" "Build Time" "$build_time"
    printf "%-15s : %s\n" "Build Type" "$build_type"
    printf "%-15s : %s\n" "System" "$system_version"
    printf "%-15s : %s\n" "Kernel" "$kernel_version"
    printf "%-15s : %s\n" "Architecture" "$architecture"
    printf "%-15s : %s\n" "Compiler" "$compiler_info"

    echo ""
    log_success "Generated files:"
    echo "  - $source_file"
    echo "  - $header_file"
    echo ""
}

# ============================================
# 发布流程
# ============================================

create_release() {
    local increment_type="${1:-patch}"
    local current_version
    current_version=$(get_current_version)
    local new_version
    new_version=$(increment_version "$current_version" "$increment_type")

    log_info "Creating release: $current_version -> $new_version"

    # 验证 Git 仓库
    if ! is_git_repo; then
        log_error "Not in a git repository"
        return 1
    fi

    # 检查未提交的更改
    if has_uncommitted_changes; then
        log_error "There are uncommitted changes. Please commit or stash them first."
        return 1
    fi

    # 更新版本文件
    update_version_files "$new_version"

    # 生成 Changelog
    generate_changelog "$new_version"

    # 生成版本信息 C++ 文件
    generate_version_cpp "$new_version"

    # 提交更改
    local files_to_add=("$VERSION_FILE" "$CHANGELOG_FILE")
    local optional_files=(
        "CMakeLists.txt" "xmake.lua" "vcpkg.json"
        "package.json" "Cargo.toml" "meson.build"
        "version.cpp" "version.h"
    )

    for file in "${optional_files[@]}"; do
        local full_path="$PROJECT_ROOT/$file"
        if [[ -f "$full_path" ]]; then
            files_to_add+=("$full_path")
        fi
    done

    for file in "${files_to_add[@]}"; do
        if [[ -f "$file" ]]; then
            git add "$file" 2>/dev/null || true
        fi
    done

    git commit -m "Release version $new_version"

    # 创建标签
    create_git_tag "$new_version"

    echo ""
    log_success "Release $new_version created successfully!"
    echo ""
    log_info "Next steps:"
    log_info "  1. Review the changes: git show HEAD"
    log_info "  2. Push the changes: git push origin main"
    log_info "  3. Push the tag: git push origin v$new_version"
}

# ============================================
# 显示帮助信息
# ============================================

show_help() {
    cat << EOF

Atom Project Version Management Script
======================================

Usage: $0 <command> [options]

Commands:
  current              Show current version information
  set <version>        Set specific version (X.Y.Z format)
  bump <type>          Increment version (major|minor|patch)
  changelog [version]  Generate changelog for version
  tag [version]        Create git tag for version
  release [type]       Create full release (bump, changelog, tag, generate)
  generate [version]   Generate C++ version files only
  help                 Show this help message

Options:
  -o, --output <dir>   Output directory for generated files (default: project root)
  -b, --branch <name>  Default branch name (default: master)

Examples:
  $0 current                    # Show current version
  $0 set 1.2.3                  # Set version to 1.2.3
  $0 bump minor                 # Increment minor version
  $0 release major              # Create major release
  $0 generate                   # Generate C++ version files
  $0 generate 1.0.0 -o src/     # Generate to src/ directory

Environment Variables:
  CXX                  C++ compiler for version detection
  CMAKE_BUILD_TYPE     Build type (Debug/Release/etc.)
  BUILD_TYPE           Alternative build type variable
  CI_COMMIT_BRANCH     GitLab CI branch name
  GITHUB_REF_NAME      GitHub Actions branch name
  GIT_BRANCH           Jenkins branch name

EOF
}

# ============================================
# 参数解析
# ============================================

parse_args() {
    local positional_args=()

    while [[ $# -gt 0 ]]; do
        case $1 in
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -b|--branch)
                DEFAULT_BRANCH="$2"
                shift 2
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            -*)
                log_error "Unknown option: $1"
                show_help
                exit 1
                ;;
            *)
                positional_args+=("$1")
                shift
                ;;
        esac
    done

    # 恢复位置参数
    set -- "${positional_args[@]}"

    COMMAND="${1:-help}"
    ARGUMENT="${2:-}"
}

# ============================================
# 主命令处理
# ============================================

main() {
    parse_args "$@"

    case "$COMMAND" in
        current)
            echo ""
            echo -e "${CYAN}Current version:${NC} $(get_current_version)"
            echo -e "${CYAN}Git version:${NC} $(get_git_version)"
            echo ""
            ;;

        set)
            if [[ -z "$ARGUMENT" ]]; then
                log_error "Version required. Usage: $0 set <version>"
                exit 1
            fi
            if validate_version "$ARGUMENT"; then
                update_version_files "$ARGUMENT"
                log_success "Version set to $ARGUMENT"
            fi
            ;;

        bump)
            if [[ -z "$ARGUMENT" ]]; then
                log_error "Increment type required. Usage: $0 bump {major|minor|patch}"
                exit 1
            fi
            if [[ ! "$ARGUMENT" =~ ^(major|minor|patch)$ ]]; then
                log_error "Invalid increment type: $ARGUMENT (expected: major, minor, patch)"
                exit 1
            fi
            local current_version new_version
            current_version=$(get_current_version)
            new_version=$(increment_version "$current_version" "$ARGUMENT")
            update_version_files "$new_version"
            log_success "Version bumped from $current_version to $new_version"
            ;;

        changelog)
            local version="${ARGUMENT:-$(get_current_version)}"
            generate_changelog "$version"
            ;;

        tag)
            local version="${ARGUMENT:-$(get_current_version)}"
            create_git_tag "$version"
            ;;

        release)
            local increment_type="${ARGUMENT:-patch}"
            if [[ ! "$increment_type" =~ ^(major|minor|patch)$ ]]; then
                log_error "Invalid increment type: $increment_type (expected: major, minor, patch)"
                exit 1
            fi
            create_release "$increment_type"
            ;;

        generate)
            local version="${ARGUMENT:-$(get_current_version)}"
            generate_version_cpp "$version" "$OUTPUT_DIR"
            ;;

        help|*)
            show_help
            ;;
    esac
}

# 执行主函数
main "$@"
