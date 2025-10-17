-- xmake configuration for Atom Extra Components
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom Extra Components
-- Description: Additional utility libraries and wrappers for the Atom framework
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-extra")
set_version("1.0.0")
set_license("GPL-3.0")

-- Set C++ standard
set_languages("c++20")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Option to build unified extra library
option("unified_extra")
    set_default(false)
    set_description("Build a unified extra library containing all extra components")
    set_showmenu(true)
option_end()

-- List of extra component subdirectories
local extra_components = {
    "asio",
    "beast",
    "boost",
    "curl",
    "dotenv",
    "iconv",
    "inicpp",
    "injection",
    "pugixml",
    "spdlog",
    "uv"
}

-- Function to check if a subdirectory has xmake.lua
local function has_xmake_config(dir)
    return os.isfile(path.join(dir, "xmake.lua"))
end

-- Include subdirectories that have xmake.lua files
for _, component in ipairs(extra_components) do
    local component_dir = path.join(os.scriptdir(), component)
    if os.isdir(component_dir) then
        if has_xmake_config(component_dir) then
            includes(component)
            print("Including extra component: " .. component)
        else
            print("Skipping extra component (no xmake.lua): " .. component)
        end
    end
end

-- Create unified extra library if option is enabled
if has_config("unified_extra") then
    target("atom-extra-unified")
        set_kind("headeronly")
        
        -- Collect all extra targets
        local extra_targets = {
            "atom-extra-asio",
            "atom-extra-beast",
            "atom-extra-boost",
            "atom-extra-curl",
            "atom-extra-dotenv",
            "atom-extra-iconv",
            "atom-extra-inicpp",
            "atom-extra-injection",
            "atom-extra-pugixml",
            "atom-extra-spdlog",
            "atom-extra-uv"
        }
        
        -- Add dependencies on all extra components
        for _, target_name in ipairs(extra_targets) do
            add_deps(target_name, {public = true})
        end
        
        -- Set C++ standard
        set_languages("c++20")
        
        -- Installation
        on_install(function (target)
            print("Unified extra library installed")
        end)
    target_end()
end

-- Print configuration summary
after_load(function ()
    print("Atom Extra Components configuration completed")
end)

