-- xmake configuration for dotenv library
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-dotenv")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++20")

-- Source files
local sources = {
    "dotenv.cpp",
    "parser.cpp",
    "validator.cpp",
    "loader.cpp"
}

-- Header files
local headers = {
    "dotenv.hpp",
    "parser.hpp",
    "validator.hpp",
    "loader.hpp",
    "exceptions.hpp",
    "test_dotenv.hpp",
    "test_validator.hpp"
}

target("atom-extra-dotenv")
    set_kind("static")
    
    -- Add files
    for _, src in ipairs(sources) do
        add_files(src)
    end
    
    for _, hdr in ipairs(headers) do
        add_headerfiles(hdr)
    end
    
    -- Include directories
    add_includedirs(".", {public = true})
    
    -- Add system libraries
    add_syslinks("pthread")
    
    -- Windows-specific libraries
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
    
    -- Set C++ standard
    set_languages("c++20")
    
    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        local headerdir = path.join(installdir, "include", "atom", "extra", "dotenv")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)

