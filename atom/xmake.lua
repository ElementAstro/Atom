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

-- Set languages (match CMake C++20/23)
set_languages("c11", "cxx20")

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

-- Module build options are inherited from parent configuration
local modules = {
    "algorithm", "async", "components", "connection", "containers",
    "error", "image", "io", "log", "memory", "meta", "search", "secret",
    "serial", "sysinfo", "system", "type", "utils", "web"
}

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
    print("Python support enabled")
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
local atom_modules = {}

-- =============================================================================
-- Module Inclusion Logic
-- =============================================================================

local valid_modules = {}

for _, module in ipairs(modules) do
    if has_config("build_" .. module) then
        if check_module_directory(module, module) then
            table.insert(valid_modules, module)
            table.insert(atom_modules, "atom-" .. module)
            print("Building " .. module .. " module")
        else
            print("Skipping " .. module .. " module due to missing or invalid directory")
        end
    end
end

-- Add tests if enabled
if has_config("build_tests") then
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
if has_config("build_tests") and os.isdir("tests") then
    includes("tests")
end

-- =============================================================================
-- Add Extra Components
-- =============================================================================

-- Add extra components directory if it exists
if os.isdir("extra") and os.isfile(path.join("extra", "xmake.lua")) then
    print("Adding extra components directory")
    includes("extra")
else
    print("Skipping extra components directory as it does not exist or does not contain xmake.lua")
end

-- =============================================================================
-- Create Combined Library
-- =============================================================================

if has_config("unified") and #atom_modules > 0 then
    target("atom-unified")
        set_kind("phony")

        -- Add all module dependencies
        for _, module in ipairs(atom_modules) do
            add_deps(module)
        end

        after_build(function (target)
            print("Created unified Atom library with modules: " .. table.concat(atom_modules, ", "))
        end)
    target_end()

    -- Create atom alias target
    target("atom")
        set_kind("phony")
        add_deps("atom-unified")
    target_end()
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
        for _, module in ipairs(atom_modules) do
            os.exec("xmake install " .. module)
        end
        print("All Atom modules installed successfully")
    end)
task_end()

-- Print configuration summary
print("Atom modules configuration completed successfully")
if #atom_modules > 0 then
    print("Active modules: " .. table.concat(atom_modules, ", "))
else
    print("No modules enabled for building")
end
