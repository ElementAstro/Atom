-- xmake.lua for Atom Project
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes (including minsizerel for size optimization)
add_rules("mode.debug", "mode.release", "mode.minsizerel", "mode.releasedbg")

-- Project configuration
set_project("atom")
set_version("1.0.0")
set_languages("c++20")
set_license("GPL3")

-- =============================================================================
-- Platform Detection Helper
-- =============================================================================

-- Unified platform check for Windows (including MinGW)
function is_windows_like()
    return is_plat("windows") or is_plat("mingw") or is_plat("msys")
end

-- =============================================================================
-- Build Options
-- =============================================================================

option("shared_libs")
    set_default(false)
    set_showmenu(true)
    set_description("Build shared libraries instead of static")
option_end()

option("use_system_packages")
    set_default(false)
    set_showmenu(true)
    set_description("Prefer system packages (e.g. pacman on MSYS2) over xmake packages")
option_end()

option("build_python")
    set_default(false)
    set_showmenu(true)
    set_description("Build Python bindings")
option_end()

option("build_examples")
    set_default(false)
    set_showmenu(true)
    set_description("Build examples")
option_end()

option("build_tests")
    set_default(false)
    set_showmenu(true)
    set_description("Build tests")
option_end()

-- =============================================================================
-- Build Performance Options
-- =============================================================================

option("enable_ccache")
    set_default(true)
    set_showmenu(true)
    set_description("Enable compiler caching (ccache/sccache)")
option_end()

option("enable_pch")
    set_default(true)
    set_showmenu(true)
    set_description("Enable precompiled headers")
option_end()

option("enable_unity_build")
    set_default(false)
    set_showmenu(true)
    set_description("Enable unity build (merged compilation)")
option_end()

-- =============================================================================
-- Size Optimization Options
-- =============================================================================

option("minsize")
    set_default(false)
    set_showmenu(true)
    set_description("Optimize for minimum binary size")
option_end()

option("strip_binaries")
    set_default(true)
    set_showmenu(true)
    set_description("Strip debug symbols from release binaries")
option_end()

option("use_lto")
    set_default(false)
    set_showmenu(true)
    set_description("Enable Link Time Optimization (experimental)")
option_end()

-- =============================================================================
-- Module Group Options (matching CMake Options.cmake)
-- =============================================================================

option("build_minimal")
    set_default(false)
    set_showmenu(true)
    set_description("Build only core modules (minimal footprint)")
option_end()

option("build_core")
    set_default(false)
    set_showmenu(true)
    set_description("Build core and utility modules")
option_end()

option("build_system_group")
    set_default(false)
    set_showmenu(true)
    set_description("Build system-related modules")
option_end()

option("build_network")
    set_default(false)
    set_showmenu(true)
    set_description("Build networking modules")
option_end()

option("build_application")
    set_default(false)
    set_showmenu(true)
    set_description("Build application-level modules")
option_end()

-- =============================================================================
-- Optional Feature Dependencies (matching CMake)
-- =============================================================================

option("use_opencv")
    set_default(false)
    set_showmenu(true)
    set_description("Enable OpenCV for image processing")
option_end()

option("use_tbb")
    set_default(false)
    set_showmenu(true)
    set_description("Enable Intel TBB for parallel algorithms")
option_end()

option("use_minizip")
    set_default(false)
    set_showmenu(true)
    set_description("Enable minizip-ng for advanced compression")
option_end()

option("use_libuv")
    set_default(false)
    set_showmenu(true)
    set_description("Enable libuv for async I/O")
option_end()

-- Module build options (matching CMake)
local modules = {
    "algorithm", "async", "components", "connection", "containers",
    "error", "image", "io", "log", "memory", "meta", "search", "secret",
    "serial", "sysinfo", "system", "type", "utils", "web"
}

-- Global build all option
option("build_all")
    set_default(true)
    set_showmenu(true)
    set_description("Build all Atom modules")
option_end()

for _, module in ipairs(modules) do
    option("build_" .. module)
        set_default(false)
        set_showmenu(true)
        set_description("Build " .. module .. " module")
    option_end()
end

-- =============================================================================
-- Build Performance Rules
-- =============================================================================

-- Enable ccache/sccache for faster rebuilds
if has_config("enable_ccache") then
    set_policy("build.ccache", true)
    print("Compiler caching enabled")
end

-- Enable precompiled headers
if has_config("enable_pch") then
    set_policy("build.across_targets_in_parallel", true)
    -- PCH will be configured per-target
    print("Precompiled headers enabled")
end

-- Enable unity build (merged compilation)
if has_config("enable_unity_build") then
    set_policy("build.merge_archive", true)
    print("Unity build enabled")
end

-- =============================================================================
-- Module Group Processing
-- =============================================================================

-- Define module groups (matching CMake Options.cmake)
local core_modules = {"error", "type", "containers", "meta"}
local utility_modules = {"utils", "algorithm", "memory", "log"}
local system_modules = {"system", "sysinfo", "io", "serial"}
local network_modules = {"connection", "web", "async"}
local application_modules = {"components", "image", "search", "secret"}

-- Process module group options
if has_config("build_minimal") then
    -- Only enable core modules
    for _, m in ipairs(core_modules) do
        set_config("build_" .. m, true)
    end
    set_config("build_all", false)
    print("Minimal build: core modules only")
elseif has_config("build_core") then
    -- Enable core + utility modules
    for _, m in ipairs(core_modules) do set_config("build_" .. m, true) end
    for _, m in ipairs(utility_modules) do set_config("build_" .. m, true) end
    set_config("build_all", false)
    print("Core build: core + utility modules")
end

if has_config("build_system_group") then
    for _, m in ipairs(system_modules) do set_config("build_" .. m, true) end
    for _, m in ipairs(core_modules) do set_config("build_" .. m, true) end
    print("System modules enabled")
end

if has_config("build_network") then
    for _, m in ipairs(network_modules) do set_config("build_" .. m, true) end
    for _, m in ipairs(core_modules) do set_config("build_" .. m, true) end
    set_config("build_utils", true)
    print("Network modules enabled")
end

if has_config("build_application") then
    for _, m in ipairs(application_modules) do set_config("build_" .. m, true) end
    for _, m in ipairs(core_modules) do set_config("build_" .. m, true) end
    print("Application modules enabled")
end

-- =============================================================================
-- Size Optimization Rules
-- =============================================================================

-- Apply size optimization settings globally
if has_config("minsize") or is_mode("minsizerel") then
    set_optimize("smallest")
    add_cxflags("-ffunction-sections", "-fdata-sections", {tools = {"gcc", "clang"}})
    add_ldflags("-Wl,--gc-sections", {tools = {"gcc", "clang"}})
    add_ldflags("/OPT:REF", "/OPT:ICF", {tools = {"cl"}})
    print("Size optimization enabled")
end

-- Strip binaries in release mode
if has_config("strip_binaries") and (is_mode("release") or is_mode("minsizerel")) then
    add_ldflags("-s", {tools = {"gcc", "clang"}})
    print("Binary stripping enabled")
end

-- LTO support
if has_config("use_lto") then
    add_cxflags("-flto=auto", {tools = {"gcc"}})
    add_cxflags("-flto=thin", {tools = {"clang"}})
    add_ldflags("-flto=auto", {tools = {"gcc"}})
    add_ldflags("-flto=thin", {tools = {"clang"}})
    add_ldflags("/LTCG", {tools = {"cl"}})
    add_cxflags("/GL", {tools = {"cl"}})
    print("Link Time Optimization enabled")
end

-- =============================================================================
-- Core Dependencies (always required)
-- =============================================================================

local use_system_packages = has_config("use_system_packages")

-- Core dependencies (matching CMake)
add_requires("openssl", {system = use_system_packages})
add_requires("asio", {system = use_system_packages})
add_requires("zlib", {system = use_system_packages})
add_requires("fmt", {system = use_system_packages})
add_requires("spdlog", {system = use_system_packages, configs = {fmt_external = true}})
add_requires("tinyxml2", {system = use_system_packages})
add_requires("pugixml", {system = use_system_packages})

-- =============================================================================
-- Optional Feature Dependencies
-- =============================================================================

-- OpenCV for image processing (optional)
if has_config("use_opencv") then
    add_requires("opencv", {system = use_system_packages, optional = true})
end

-- TBB for parallel algorithms (optional)
if has_config("use_tbb") then
    add_requires("tbb", {system = use_system_packages, optional = true})
end

-- minizip-ng for advanced compression (optional)
if has_config("use_minizip") then
    add_requires("minizip-ng", {system = use_system_packages, optional = true})
end

-- libuv for async I/O (optional)
if has_config("use_libuv") then
    add_requires("libuv", {system = use_system_packages, optional = true})
end

-- libssh for SSH support (optional)
add_requires("libssh", {optional = true, system = use_system_packages})

-- Windows-specific packages
if is_plat("windows") or is_plat("mingw") or is_plat("msys") then
    add_requires("dlfcn-win32", {system = use_system_packages, optional = true})
end

-- Python bindings dependencies
if has_config("build_python") then
    add_requires("pybind11", {system = use_system_packages})
end

-- Include atom subdirectory
includes("atom")

-- Include Python bindings if enabled
if has_config("build_python") then
    includes("python")
end

-- Include examples if enabled
if has_config("build_examples") then
    includes("example")
end

-- Include tests if enabled
if has_config("build_tests") then
    includes("tests")
end

-- Create a task for easy installation
task("install")
    on_run(function()
        import("core.project.project")
        import("core.platform.platform")

        -- Set install prefix
        local prefix = option.get("prefix") or "/usr/local"

        -- Build the project
        os.exec("xmake build")

        -- Install the project
        os.exec("xmake install -o " .. prefix)

        cprint("${bright green}Atom has been installed to " .. prefix)
    end)

    set_menu {
        usage = "xmake install",
        description = "Install Atom libraries and headers"
    }
task_end()
