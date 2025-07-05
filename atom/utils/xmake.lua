-- filepath: d:\msys64\home\qwdma\Atom\atom\utils\xmake.lua
-- xmake configuration for Atom-Utils module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-utils")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local sources = {
    "aes.cpp",
    "env.cpp",
    "hash_util.cpp",
    "random.cpp",
    "string.cpp",
    "stopwatcher.cpp",
    "time.cpp",
    "uuid.cpp",
    "xml.cpp"
}

-- Define header files
local headers = {
    "aes.hpp",
    "env.hpp",
    "hash_util.hpp",
    "random.hpp",
    "refl.hpp",
    "string.hpp",
    "stopwatcher.hpp",
    "switch.hpp",
    "time.hpp",
    "uuid.hpp",
    "xml.hpp"
}

-- Object Library
target("atom-utils-object")
    set_kind("object")

    -- Add files
    add_headerfiles(table.unpack(headers))
    add_files(table.unpack(sources))

    -- Add dependencies
    add_packages("loguru", "tinyxml2")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-utils")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-utils-object")
    add_packages("loguru", "tinyxml2")

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/utils"))
    end)
target_end()
