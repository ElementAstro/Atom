#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Atom Project Version Management Script
.DESCRIPTION
    Handles semantic versioning, tagging, changelog generation, and version info generation
.AUTHOR
    Max Qian
.EXAMPLE
    ./version.ps1 current
    ./version.ps1 set 1.2.3
    ./version.ps1 bump minor
    ./version.ps1 release major
    ./version.ps1 generate
#>

param(
    [Parameter(Position = 0)]
    [ValidateSet("current", "set", "bump", "changelog", "tag", "release", "generate", "help")]
    [string]$Command = "help",

    [Parameter(Position = 1)]
    [string]$Argument,

    [Parameter()]
    [string]$OutputPath = "version.cpp",

    [Parameter()]
    [string]$DefaultBranch = "master"
)

# ============================================
# 初始化配置
# ============================================

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$script:ProjectRoot = Split-Path -Parent (Split-Path -Parent $script:ScriptDir)
$script:VersionFile = Join-Path $script:ProjectRoot "VERSION"
$script:ChangelogFile = Join-Path $script:ProjectRoot "CHANGELOG.md"

# ============================================
# 日志函数
# ============================================

function Write-LogInfo {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[INFO] $timestamp $Message" -ForegroundColor Cyan
}

function Write-LogWarn {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[WARN] $timestamp $Message" -ForegroundColor Yellow
}

function Write-LogError {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[ERROR] $timestamp $Message" -ForegroundColor Red
}

function Write-LogSuccess {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[SUCCESS] $timestamp $Message" -ForegroundColor Green
}

# ============================================
# 辅助函数
# ============================================

function Get-SafeValue {
    param(
        [scriptblock]$Command,
        [string]$Default = "unknown"
    )
    try {
        $result = & $Command 2>$null
        if ([string]::IsNullOrWhiteSpace($result)) {
            return $Default
        }
        return $result.Trim()
    }
    catch {
        return $Default
    }
}

function ConvertTo-CppString {
    param([string]$Value)
    return $Value -replace '\\', '\\\\' -replace '"', '\"'
}

function Test-GitRepository {
    try {
        $null = git rev-parse --git-dir 2>$null
        return $LASTEXITCODE -eq 0
    }
    catch {
        return $false
    }
}

function Test-UncommittedChanges {
    if (-not (Test-GitRepository)) { return $false }
    $status = git diff-index --quiet HEAD -- 2>$null
    return $LASTEXITCODE -ne 0
}

# ============================================
# 版本管理函数
# ============================================

function Get-CurrentVersion {
    # 优先从 VERSION 文件读取
    if (Test-Path $script:VersionFile) {
        $version = (Get-Content $script:VersionFile -Raw).Trim()
        if ($version -match '^\d+\.\d+\.\d+') {
            return $version
        }
    }

    # 从 Git 标签获取
    if (Test-GitRepository) {
        $tags = git tag --list 2>$null | Where-Object { $_ -match '^v?\d+\.\d+\.\d+' }
        if ($tags) {
            $latestTag = $tags |
                ForEach-Object { $_ -replace '^v', '' } |
                Sort-Object { [version]$_ } |
                Select-Object -Last 1
            if ($latestTag) {
                return $latestTag
            }
        }
    }

    return "0.1.0"
}

function Get-GitVersion {
    if (-not (Test-GitRepository)) {
        return "$(Get-CurrentVersion)-unknown-unknown"
    }

    $version = Get-SafeValue { git describe --tags --always --dirty } -Default "unknown"
    $commit = Get-SafeValue { git rev-parse --short HEAD } -Default "unknown"
    $branch = Get-SafeValue { git rev-parse --abbrev-ref HEAD } -Default "unknown"

    return "${version}-${commit}-${branch}"
}

function Test-ValidVersion {
    param([string]$Version)
    return $Version -match '^\d+\.\d+\.\d+$'
}

function Get-IncrementedVersion {
    param(
        [string]$CurrentVersion,
        [ValidateSet("major", "minor", "patch")]
        [string]$IncrementType
    )

    if (-not (Test-ValidVersion $CurrentVersion)) {
        throw "Invalid version format: $CurrentVersion"
    }

    $parts = $CurrentVersion -split '\.'
    $major = [int]$parts[0]
    $minor = [int]$parts[1]
    $patch = [int]$parts[2]

    switch ($IncrementType) {
        "major" {
            $major++
            $minor = 0
            $patch = 0
        }
        "minor" {
            $minor++
            $patch = 0
        }
        "patch" {
            $patch++
        }
    }

    return "$major.$minor.$patch"
}

function Update-VersionFiles {
    param([string]$NewVersion)

    Write-LogInfo "Updating version to $NewVersion in project files..."

    # 更新 VERSION 文件
    $NewVersion | Set-Content -Path $script:VersionFile -NoNewline -Encoding UTF8

    # 更新 CMakeLists.txt
    $cmakePath = Join-Path $script:ProjectRoot "CMakeLists.txt"
    if (Test-Path $cmakePath) {
        $content = Get-Content $cmakePath -Raw
        $content = $content -replace 'VERSION \d+\.\d+\.\d+', "VERSION $NewVersion"
        $content | Set-Content $cmakePath -NoNewline -Encoding UTF8
        Write-LogInfo "Updated CMakeLists.txt"
    }

    # 更新 xmake.lua
    $xmakePath = Join-Path $script:ProjectRoot "xmake.lua"
    if (Test-Path $xmakePath) {
        $content = Get-Content $xmakePath -Raw
        $content = $content -replace 'set_version\("\d+\.\d+\.\d+"\)', "set_version(`"$NewVersion`")"
        $content | Set-Content $xmakePath -NoNewline -Encoding UTF8
        Write-LogInfo "Updated xmake.lua"
    }

    # 更新 vcpkg.json
    $vcpkgPath = Join-Path $script:ProjectRoot "vcpkg.json"
    if (Test-Path $vcpkgPath) {
        $content = Get-Content $vcpkgPath -Raw
        $content = $content -replace '"version":\s*"\d+\.\d+\.\d+"', "`"version`": `"$NewVersion`""
        $content | Set-Content $vcpkgPath -NoNewline -Encoding UTF8
        Write-LogInfo "Updated vcpkg.json"
    }

    # 更新 package.json (如果存在)
    $packagePath = Join-Path $script:ProjectRoot "package.json"
    if (Test-Path $packagePath) {
        $content = Get-Content $packagePath -Raw
        $content = $content -replace '"version":\s*"\d+\.\d+\.\d+"', "`"version`": `"$NewVersion`""
        $content | Set-Content $packagePath -NoNewline -Encoding UTF8
        Write-LogInfo "Updated package.json"
    }

    # 更新 Cargo.toml (如果存在)
    $cargoPath = Join-Path $script:ProjectRoot "Cargo.toml"
    if (Test-Path $cargoPath) {
        $content = Get-Content $cargoPath -Raw
        $content = $content -replace 'version\s*=\s*"\d+\.\d+\.\d+"', "version = `"$NewVersion`""
        $content | Set-Content $cargoPath -NoNewline -Encoding UTF8
        Write-LogInfo "Updated Cargo.toml"
    }

    Write-LogSuccess "Version files updated successfully"
}

# ============================================
# Changelog 生成
# ============================================

function New-Changelog {
    param([string]$Version)

    Write-LogInfo "Generating changelog for version $Version..."

    # 获取上一个标签
    $previousTag = $null
    if (Test-GitRepository) {
        $tags = git tag --list 2>$null | Where-Object { $_ -match '^v?\d+\.\d+\.\d+' }
        if ($tags -and $tags.Count -ge 1) {
            $sortedTags = $tags |
                ForEach-Object { [PSCustomObject]@{ Tag = $_; Version = ($_ -replace '^v', '') } } |
                Sort-Object { [version]$_.Version }

            if ($sortedTags.Count -ge 2) {
                $previousTag = ($sortedTags | Select-Object -Last 2 | Select-Object -First 1).Tag
            }
        }
    }

    # 创建 Changelog 头部
    if (-not (Test-Path $script:ChangelogFile)) {
        $header = @"
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

"@
        $header | Set-Content $script:ChangelogFile -Encoding UTF8
    }

    # 生成新的 Changelog 条目
    $date = Get-Date -Format "yyyy-MM-dd"
    $newEntry = "## [$Version] - $date`n`n"

    if ($previousTag -and (Test-GitRepository)) {
        Write-LogInfo "Generating changelog from $previousTag to HEAD"

        $commits = git log --pretty=format:"- %s (%h)" "$previousTag..HEAD" 2>$null
        if ($commits) {
            # 分类提交信息
            $features = @()
            $fixes = @()
            $docs = @()
            $refactor = @()
            $other = @()

            foreach ($commit in $commits) {
                if ($commit -match '^\s*-\s*(feat|feature|add)' ) {
                    $features += $commit
                }
                elseif ($commit -match '^\s*-\s*(fix|bug|patch)') {
                    $fixes += $commit
                }
                elseif ($commit -match '^\s*-\s*(doc|docs)') {
                    $docs += $commit
                }
                elseif ($commit -match '^\s*-\s*(refactor|refact)') {
                    $refactor += $commit
                }
                else {
                    $other += $commit
                }
            }

            if ($features.Count -gt 0) {
                $newEntry += "### Added`n"
                $newEntry += ($features -join "`n") + "`n`n"
            }
            if ($fixes.Count -gt 0) {
                $newEntry += "### Fixed`n"
                $newEntry += ($fixes -join "`n") + "`n`n"
            }
            if ($docs.Count -gt 0) {
                $newEntry += "### Documentation`n"
                $newEntry += ($docs -join "`n") + "`n`n"
            }
            if ($refactor.Count -gt 0) {
                $newEntry += "### Changed`n"
                $newEntry += ($refactor -join "`n") + "`n`n"
            }
            if ($other.Count -gt 0) {
                $newEntry += "### Other`n"
                $newEntry += ($other -join "`n") + "`n`n"
            }
        }
        else {
            $newEntry += "- No changes recorded`n`n"
        }
    }
    else {
        $newEntry += "### Added`n- Initial release`n`n"
    }

    # 插入新条目到 Changelog
    $existingContent = Get-Content $script:ChangelogFile -Raw
    $headerLines = ($existingContent -split "`n" | Select-Object -First 6) -join "`n"
    $restContent = ($existingContent -split "`n" | Select-Object -Skip 6) -join "`n"

    $newContent = $headerLines + "`n`n" + $newEntry + $restContent
    $newContent | Set-Content $script:ChangelogFile -Encoding UTF8

    Write-LogSuccess "Changelog updated successfully"
}

# ============================================
# Git 标签管理
# ============================================

function New-GitTag {
    param([string]$Version)

    $tagName = "v$Version"

    Write-LogInfo "Creating git tag: $tagName"

    if (-not (Test-GitRepository)) {
        throw "Not in a git repository"
    }

    # 检查标签是否已存在
    $existingTags = git tag --list 2>$null
    if ($existingTags -contains $tagName) {
        throw "Tag $tagName already exists"
    }

    # 创建带注释的标签
    git tag -a $tagName -m "Release version $Version"

    Write-LogSuccess "Git tag $tagName created successfully"
    Write-LogInfo "To push the tag, run: git push origin $tagName"
}

# ============================================
# 发布流程
# ============================================

function New-Release {
    param(
        [ValidateSet("major", "minor", "patch")]
        [string]$IncrementType = "patch"
    )

    $currentVersion = Get-CurrentVersion
    $newVersion = Get-IncrementedVersion -CurrentVersion $currentVersion -IncrementType $IncrementType

    Write-LogInfo "Creating release: $currentVersion -> $newVersion"

    # 验证 Git 仓库
    if (-not (Test-GitRepository)) {
        throw "Not in a git repository"
    }

    # 检查未提交的更改
    if (Test-UncommittedChanges) {
        throw "There are uncommitted changes. Please commit or stash them first."
    }

    # 更新版本文件
    Update-VersionFiles -NewVersion $newVersion

    # 生成 Changelog
    New-Changelog -Version $newVersion

    # 生成版本信息 C++ 文件
    New-VersionCppFiles -Version $newVersion

    # 提交更改
    $filesToAdd = @($script:VersionFile, $script:ChangelogFile)
    $optionalFiles = @("CMakeLists.txt", "xmake.lua", "vcpkg.json", "package.json", "Cargo.toml", "version.cpp", "version.h")

    foreach ($file in $optionalFiles) {
        $fullPath = Join-Path $script:ProjectRoot $file
        if (Test-Path $fullPath) {
            $filesToAdd += $fullPath
        }
    }

    foreach ($file in $filesToAdd) {
        if (Test-Path $file) {
            git add $file 2>$null
        }
    }

    git commit -m "Release version $newVersion"

    # 创建标签
    New-GitTag -Version $newVersion

    Write-Host ""
    Write-LogSuccess "Release $newVersion created successfully!"
    Write-Host ""
    Write-LogInfo "Next steps:"
    Write-LogInfo "  1. Review the changes: git show HEAD"
    Write-LogInfo "  2. Push the changes: git push origin main"
    Write-LogInfo "  3. Push the tag: git push origin v$newVersion"
}

# ============================================
# C++ 版本文件生成
# ============================================

function New-VersionCppFiles {
    param(
        [string]$Version = $null,
        [string]$OutputDir = $null
    )

    Write-LogInfo "Generating C++ version files..."

    if (-not $OutputDir) {
        $OutputDir = $script:ProjectRoot
    }

    if (-not $Version) {
        $Version = Get-CurrentVersion
    }

    # ============================================
    # 收集 Git 信息
    # ============================================

    $git_hash = Get-SafeValue { git rev-parse --short HEAD }
    $git_tag = Get-SafeValue { git describe --tags --abbrev=0 } -Default "v$Version"
    $git_branch = Get-SafeValue { git branch --show-current } -Default $DefaultBranch

    # CI/CD 环境变量支持
    if ($git_branch -eq "unknown" -or [string]::IsNullOrWhiteSpace($git_branch)) {
        $ci_branch = $env:CI_COMMIT_BRANCH
        if ([string]::IsNullOrWhiteSpace($ci_branch)) {
            $ci_branch = $env:GITHUB_REF_NAME
        }
        if ([string]::IsNullOrWhiteSpace($ci_branch)) {
            $ci_branch = $env:GIT_BRANCH
        }
        if ([string]::IsNullOrWhiteSpace($ci_branch)) {
            $ci_branch = $env:BRANCH_NAME
        }
        if (-not [string]::IsNullOrWhiteSpace($ci_branch)) {
            $git_branch = $ci_branch
        }
        else {
            $git_branch = $DefaultBranch
        }
    }

    $git_commit_time = Get-SafeValue {
        git log -1 --format=%cd --date=format:'%Y-%m-%d %H:%M:%S'
    }

    # 检测 dirty 状态
    $git_dirty = ""
    if (Test-GitRepository) {
        $git_status = git status --porcelain 2>$null
        if (-not [string]::IsNullOrWhiteSpace($git_status)) {
            $git_dirty = "-dirty"
        }
    }

    # 提交数量
    $git_commit_count = Get-SafeValue { git rev-list --count HEAD } -Default "0"

    # ============================================
    # 构建时间（中国时区）
    # ============================================

    try {
        $build_time = [System.TimeZoneInfo]::ConvertTimeBySystemTimeZoneId(
            [DateTime]::UtcNow,
            'China Standard Time'
        ).ToString('yyyy-MM-dd HH:mm:ss')
    }
    catch {
        $build_time = [DateTime]::UtcNow.AddHours(8).ToString('yyyy-MM-dd HH:mm:ss')
    }

    # ============================================
    # 系统信息获取
    # ============================================

    $system_version = Get-SafeValue {
        [System.Runtime.InteropServices.RuntimeInformation]::OSDescription.ToString()
    }
    $kernel_version = Get-SafeValue {
        [System.Environment]::OSVersion.Version.ToString()
    }
    $architecture = Get-SafeValue {
        [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
    }

    # 编译器信息
    $compiler_info = Get-SafeValue {
        $cxx = $env:CXX
        if (-not $cxx) {
            if (Get-Command "clang++" -ErrorAction SilentlyContinue) {
                $cxx = "clang++"
            }
            elseif (Get-Command "g++" -ErrorAction SilentlyContinue) {
                $cxx = "g++"
            }
            elseif (Get-Command "cl" -ErrorAction SilentlyContinue) {
                $cxx = "cl"
            }
        }
        if ($cxx -and (Get-Command $cxx -ErrorAction SilentlyContinue)) {
            & $cxx --version 2>$null | Select-Object -First 1
        }
        else {
            "unknown"
        }
    }

    # 构建类型
    $build_type = $env:CMAKE_BUILD_TYPE
    if ([string]::IsNullOrWhiteSpace($build_type)) {
        $build_type = $env:BUILD_TYPE
    }
    if ([string]::IsNullOrWhiteSpace($build_type)) {
        $build_type = "Release"
    }

    # ============================================
    # 转义特殊字符
    # ============================================

    $system_version = ConvertTo-CppString $system_version
    $compiler_info = ConvertTo-CppString $compiler_info

    # ============================================
    # 生成 version.h 头文件
    # ============================================

    $header_content = @"
/**
 * @file version.h
 * @brief Auto-generated version information header
 * @note This file is auto-generated by version.ps1, do not modify manually
 */

#ifndef ATOM_VERSION_H
#define ATOM_VERSION_H

#include <string_view>
#include <cstdint>

namespace atom::version {

// Version components
constexpr uint32_t MAJOR = $($Version.Split('.')[0]);
constexpr uint32_t MINOR = $($Version.Split('.')[1]);
constexpr uint32_t PATCH = $($Version.Split('.')[2]);

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
constexpr std::string_view VERSION_STRING = "$Version";

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
"@

    # ============================================
    # 生成 version.cpp 源文件
    # ============================================

    $source_content = @"
/**
 * @file version.cpp
 * @brief Auto-generated version information implementation
 * @note This file is auto-generated by version.ps1, do not modify manually
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
"@

    # ============================================
    # 写入文件
    # ============================================

    $cppPath = Join-Path $OutputDir "version.cpp"
    $hPath = Join-Path $OutputDir "version.h"

    # 确保目录存在
    $outDir = Split-Path -Parent $cppPath
    if ($outDir -and -not (Test-Path $outDir)) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }

    # 写入文件（UTF-8 无 BOM）
    if ($PSVersionTable.PSVersion.Major -ge 6) {
        $source_content | Set-Content -Path $cppPath -Encoding UTF8NoBOM -NoNewline
        $header_content | Set-Content -Path $hPath -Encoding UTF8NoBOM -NoNewline
    }
    else {
        $utf8NoBom = New-Object System.Text.UTF8Encoding $false
        [System.IO.File]::WriteAllText($cppPath, $source_content, $utf8NoBom)
        [System.IO.File]::WriteAllText($hPath, $header_content, $utf8NoBom)
    }

    # ============================================
    # 输出结果
    # ============================================

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host " Version Information Generated" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""

    $info = [ordered]@{
        "Version"       = $Version
        "Git Hash"      = "$git_hash$git_dirty"
        "Git Tag"       = $git_tag
        "Git Branch"    = $git_branch
        "Commit Time"   = $git_commit_time
        "Commit Count"  = $git_commit_count
        "Build Time"    = $build_time
        "Build Type"    = $build_type
        "System"        = $system_version
        "Kernel"        = $kernel_version
        "Architecture"  = $architecture
        "Compiler"      = $compiler_info
    }

    foreach ($item in $info.GetEnumerator()) {
        Write-Host ("{0,-15} : {1}" -f $item.Key, $item.Value)
    }

    Write-Host ""
    Write-LogSuccess "Generated files:"
    Write-Host "  - $cppPath"
    Write-Host "  - $hPath"
    Write-Host ""
}

# ============================================
# 显示帮助信息
# ============================================

function Show-Help {
    $helpText = @"

Atom Project Version Management Script
======================================

Usage: $($MyInvocation.MyCommand.Name) <command> [options]

Commands:
  current              Show current version information
  set <version>        Set specific version (X.Y.Z format)
  bump <type>          Increment version (major|minor|patch)
  changelog [version]  Generate changelog for version
  tag [version]        Create git tag for version
  release [type]       Create full release (bump, changelog, tag, generate)
  generate             Generate C++ version files only
  help                 Show this help message

Options:
  -OutputPath <path>   Output path for generated files (default: version.cpp)
  -DefaultBranch <name> Default branch name (default: master)

Examples:
  ./version.ps1 current                    # Show current version
  ./version.ps1 set 1.2.3                  # Set version to 1.2.3
  ./version.ps1 bump minor                 # Increment minor version
  ./version.ps1 release major              # Create major release
  ./version.ps1 generate                   # Generate C++ version files
  ./version.ps1 generate -OutputPath src/  # Generate to src/ directory

"@
    Write-Host $helpText
}

# ============================================
# 主命令处理
# ============================================

try {
    switch ($Command) {
        "current" {
            Write-Host ""
            Write-Host "Current version: $(Get-CurrentVersion)" -ForegroundColor Cyan
            Write-Host "Git version: $(Get-GitVersion)" -ForegroundColor Cyan
            Write-Host ""
        }

        "set" {
            if ([string]::IsNullOrWhiteSpace($Argument)) {
                throw "Version required. Usage: $($MyInvocation.MyCommand.Name) set <version>"
            }
            if (-not (Test-ValidVersion $Argument)) {
                throw "Invalid version format: $Argument (expected: X.Y.Z)"
            }
            Update-VersionFiles -NewVersion $Argument
            Write-LogSuccess "Version set to $Argument"
        }

        "bump" {
            if ([string]::IsNullOrWhiteSpace($Argument)) {
                throw "Increment type required. Usage: $($MyInvocation.MyCommand.Name) bump {major|minor|patch}"
            }
            if ($Argument -notin @("major", "minor", "patch")) {
                throw "Invalid increment type: $Argument (expected: major, minor, patch)"
            }
            $currentVersion = Get-CurrentVersion
            $newVersion = Get-IncrementedVersion -CurrentVersion $currentVersion -IncrementType $Argument
            Update-VersionFiles -NewVersion $newVersion
            Write-LogSuccess "Version bumped from $currentVersion to $newVersion"
        }

        "changelog" {
            $version = if ([string]::IsNullOrWhiteSpace($Argument)) { Get-CurrentVersion } else { $Argument }
            New-Changelog -Version $version
        }

        "tag" {
            $version = if ([string]::IsNullOrWhiteSpace($Argument)) { Get-CurrentVersion } else { $Argument }
            New-GitTag -Version $version
        }

        "release" {
            $incrementType = if ([string]::IsNullOrWhiteSpace($Argument)) { "patch" } else { $Argument }
            if ($incrementType -notin @("major", "minor", "patch")) {
                throw "Invalid increment type: $incrementType (expected: major, minor, patch)"
            }
            New-Release -IncrementType $incrementType
        }

        "generate" {
            $outputDir = if ([string]::IsNullOrWhiteSpace($OutputPath) -or $OutputPath -eq "version.cpp") {
                $script:ProjectRoot
            }
            else {
                $OutputPath
            }
            New-VersionCppFiles -OutputDir $outputDir
        }

        "help" {
            Show-Help
        }

        default {
            Show-Help
        }
    }
}
catch {
    Write-LogError $_.Exception.Message
    exit 1
}
