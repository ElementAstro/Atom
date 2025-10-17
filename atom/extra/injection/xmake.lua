-- xmake configuration for Dependency Injection library
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-injection")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++20")

-- Header files
local headers = {
    "all.hpp",
    "binding.hpp",
    "common.hpp",
    "container.hpp",
    "inject.hpp",
    "resolver.hpp",
    "test_all.hpp"
}

target("atom-extra-injection")
    set_kind("headeronly")
    
    -- Add headers
    for _, hdr in ipairs(headers) do
        add_headerfiles(hdr)
    end
    
    -- Include directories
    add_includedirs(".", {public = true})
    
    -- Set C++ standard
    set_languages("c++20")
    
    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        local headerdir = path.join(installdir, "include", "atom", "extra", "injection")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)

