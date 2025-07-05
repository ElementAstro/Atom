-- filepath: d:\msys64\home\qwdma\Atom\atom\search\xmake.lua
-- xmake configuration for Atom-Search module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-search")
set_version("1.0.0")
set_description("Search Engine for Element Astro Project")
set_license("GPL3")

-- Object Library
target("atom-search-object")
    set_kind("object")

    -- Add source files
    add_files("*.cpp")

    -- Add header files
    add_headerfiles("*.hpp")

    -- Add dependencies
    add_packages("loguru")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("pthread")
    end

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-search")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-search-object")
    add_packages("loguru")

    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("pthread")
    end

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/search"))
    end)
target_end()
