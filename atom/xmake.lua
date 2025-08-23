-- xmake script for Atom
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom
-- Description: Atom Library for all of the Element Astro Project
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom")
set_version("1.0.0")
set_license("GPL-3.0")

-- Set languages
set_languages("c11", "cxx17")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- =============================================================================
-- Configuration Options
-- =============================================================================

-- Python support option
option("python")
    set_default(false)
    set_description("Build Atom with Python support")
    set_showmenu(true)
option_end()

-- Module build options
local modules = {
    "algorithm", "async", "components", "connection", "containers",
    "error", "io", "log", "memory", "meta", "search", "secret",
    "serial", "sysinfo", "system", "type", "utils", "web"
}

for _, module in ipairs(modules) do
    option(module)
        set_default(false)
        set_description("Build " .. module .. " module")
        set_showmenu(true)
    option_end()
end

-- Tests option
option("tests")
    set_default(false)
    set_description("Build tests")
    set_showmenu(true)
option_end()

-- Unified library option
option("unified")
    set_default(true)
    set_description("Build a unified Atom library containing all modules")
    set_showmenu(true)
option_end()

-- =============================================================================
-- Python Support Configuration
-- =============================================================================

if has_config("python") then
    add_requires("python3", "pybind11")

    after_load(function ()
        local python = find_tool("python3")
        if python then
            print("Found Python: " .. python.program)
        else
            raise("Python not found")
        end
    end)
end

-- =============================================================================
-- Platform-Specific Dependencies
-- =============================================================================

if is_plat("linux") then
    -- Linux-specific dependencies
    add_requires("pkgconfig::libsystemd", {optional = true})
end

-- =============================================================================
-- Module Management Functions
-- =============================================================================

-- Function to check if a module directory is valid
function check_module_directory(name, dir_name)
    local module_path = path.join(".", dir_name)
    local xmake_file = path.join(module_path, "xmake.lua")

    if os.isdir(module_path) and os.isfile(xmake_file) then
        return true
    else
        if not os.isdir(module_path) then
            print("Module directory for '" .. name .. "' does not exist: " .. module_path)
        elseif not os.isfile(xmake_file) then
            print("Module directory '" .. module_path .. "' exists but lacks xmake.lua")
        end
        return false
    end
end

-- Global module registry
if not _G.ATOM_MODULES then
    _G.ATOM_MODULES = {}
end

-- =============================================================================
-- Module Inclusion Logic
-- =============================================================================

local valid_modules = {}

for _, module in ipairs(modules) do
    if has_config(module) then
        if check_module_directory(module, module) then
            table.insert(valid_modules, module)
            table.insert(_G.ATOM_MODULES, "atom-" .. module)
            print("Building " .. module .. " module")
        else
            print("Skipping " .. module .. " module due to missing or invalid directory")
        end
    end
end

-- Add tests if enabled
if has_config("tests") then
    if os.isdir("tests") then
        table.insert(valid_modules, "tests")
        print("Building tests")
    end
end

-- =============================================================================
-- Include Subdirectories
-- =============================================================================

-- Include all valid modules
for _, module in ipairs(valid_modules) do
    if module ~= "tests" then
        includes(module)
    end
end

-- Include tests separately if needed
if has_config("tests") and os.isdir("tests") then
    includes("tests")
end

-- =============================================================================
-- Create Combined Library
-- =============================================================================

if has_config("unified") and #_G.ATOM_MODULES > 0 then
    target("atom-unified")
        set_kind("phony")

        -- Add all module dependencies
        for _, module in ipairs(_G.ATOM_MODULES) do
            add_deps(module)
        end

        after_build(function (target)
            print("Created unified Atom library with modules: " .. table.concat(_G.ATOM_MODULES, ", "))
        end)

    -- Create atom alias target
    target("atom")
        set_kind("phony")
        add_deps("atom-unified")
end

-- =============================================================================
-- Installation Rules
-- =============================================================================

-- Global installation task
task("install-all")
    set_menu {
        usage = "xmake install-all",
        description = "Install all Atom modules"
    }

    on_run(function ()
        for _, module in ipairs(_G.ATOM_MODULES) do
            os.exec("xmake install " .. module)
        end
        print("All Atom modules installed successfully")
    end)

after_load(function ()
    print("Atom modules configuration completed successfully")
    if #_G.ATOM_MODULES > 0 then
        print("Active modules: " .. table.concat(_G.ATOM_MODULES, ", "))
    else
        print("No modules enabled for building")
    end
end)
