-- xmake script for Atom-Component
-- This project adheres to the GPL3 license.
--
-- Project Details:
--   Name: Atom-Component
--   Description: Central component library for the Atom framework
--   Author: Max Qian
--   License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-component")
set_version("1.0.0", {build = "%Y%m%d%H%M"})
set_license("GPL-3.0")

-- Set languages
set_languages("c11", "cxx17")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Add required packages
local use_system_packages = has_config("use_system_packages")
add_requires("spdlog", {system = use_system_packages, configs = {fmt_external = true}})
add_requires("fmt", {system = use_system_packages})

-- Define sources and headers from new structure
local sources = {
    -- Core components
    "core/component.cpp",
    "core/component_pool.cpp",
    "core/registry.cpp",

    -- Scripting components
    "scripting/bindings.cpp",
    "scripting/script_engine.cpp",
    "scripting/script_sandbox.cpp",
    "scripting/scripting_api.cpp",

    -- Lifecycle components
    "lifecycle/dispatch.cpp",
    "lifecycle/iteration.cpp",
    "lifecycle/lifecycle.cpp",

    -- Data components
    "data/serialization.cpp",
    "data/var.cpp"
}

local headers = {
    -- Backwards compatibility headers
    "component.hpp",
    "component_pool.hpp",
    "registry.hpp",
    "types.hpp",
    "module_macro.hpp",
    "package.hpp",
    "dispatch.hpp",
    "iteration.hpp",
    "lifecycle.hpp",
    "script_engine.hpp",
    "script_sandbox.hpp",
    "scripting_api.hpp",
    "serialization.hpp",
    "var.hpp"
}

-- Optional scripting engine support
option("lua")
    set_default(false)
    set_showmenu(true)
    set_description("Enable Lua scripting support")
option_end()

option("python")
    set_default(false)
    set_showmenu(true)
    set_description("Enable Python scripting support")
option_end()

option("hot_reload")
    set_default(false)
    set_showmenu(true)
    set_description("Enable runtime loading of components from shared libraries")
option_end()

-- Main shared library target
target("atom-component")
    -- Set target kind to shared library
    set_kind("shared")

    -- Add source files from new structure
    add_files("core/*.cpp")
    add_files("scripting/*.cpp")
    add_files("lifecycle/*.cpp")
    add_files("data/*.cpp")

    -- Add header files from new structure
    add_headerfiles("*.hpp")  -- Backwards compatibility headers
    add_headerfiles("core/*.hpp")
    add_headerfiles("scripting/*.hpp")
    add_headerfiles("lifecycle/*.hpp")
    add_headerfiles("data/*.hpp")

    -- Conditional scripting engines
    if has_config("lua") then
        add_files("scripting/lua_engine.cpp")
        add_headerfiles("scripting/lua_engine.hpp")
        add_defines("ATOM_ENABLE_LUA=1")
    end

    if has_config("python") then
        add_files("scripting/python_engine.cpp")
        add_headerfiles("scripting/python_engine.hpp")
        add_defines("ATOM_ENABLE_PYTHON=1")
    end

    if has_config("hot_reload") then
        add_defines("ENABLE_HOT_RELOAD=1")
    end

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("spdlog", "fmt")

    -- Add dependencies (assuming these are other xmake targets)
    add_deps("atom-error", "atom-utils")

    -- Add system libraries
    if is_plat("linux") then
        add_syslinks("pthread", "dl")
    end

    -- Enable position independent code (automatic for shared libraries)
    set_policy("build.optimization.lto", true)

    -- Set version info
    set_version("1.0.0")

    -- Set output name
    set_basename("atom-component")

    -- Set target and object directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Installation rules
    after_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        -- Install shared library
        os.cp(target:targetfile(), path.join(installdir, "lib"))

        -- Install backwards compatibility headers
        local headerdir = path.join(installdir, "include", "atom", "components")
        os.mkdir(headerdir)
        os.cp("*.hpp", headerdir)

        -- Install implementation headers in subdirectories
        os.cp("core/*.hpp", path.join(headerdir, "core"))
        os.cp("scripting/*.hpp", path.join(headerdir, "scripting"))
        os.cp("lifecycle/*.hpp", path.join(headerdir, "lifecycle"))
        os.cp("data/*.hpp", path.join(headerdir, "data"))
    end)

-- Optional: Create object library target (equivalent to CMake's object library)
target("atom-component-object")
    set_kind("object")

    -- Add source files from new structure
    add_files("core/*.cpp")
    add_files("scripting/*.cpp")
    add_files("lifecycle/*.cpp")
    add_files("data/*.cpp")

    -- Add header files from new structure
    add_headerfiles("*.hpp")  -- Backwards compatibility headers
    add_headerfiles("core/*.hpp")
    add_headerfiles("scripting/*.hpp")
    add_headerfiles("lifecycle/*.hpp")
    add_headerfiles("data/*.hpp")

    -- Conditional scripting engines
    if has_config("lua") then
        add_files("scripting/lua_engine.cpp")
        add_headerfiles("scripting/lua_engine.hpp")
        add_defines("ATOM_ENABLE_LUA=1")
    end

    if has_config("python") then
        add_files("scripting/python_engine.cpp")
        add_headerfiles("scripting/python_engine.hpp")
        add_defines("ATOM_ENABLE_PYTHON=1")
    end

    if has_config("hot_reload") then
        add_defines("ENABLE_HOT_RELOAD=1")
    end

    -- Configuration
    add_includedirs(".")
    add_packages("spdlog", "fmt")
    add_deps("atom-error", "atom-utils")
    if is_plat("linux") then
        add_syslinks("pthread", "dl")
    end

    -- Enable position independent code
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})
