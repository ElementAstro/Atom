-- xmake configuration for iconv C++ wrapper
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-iconv")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++20")

-- Add required packages
local use_system_packages = has_config("use_system_packages")
add_requires("libiconv", {optional = true, system = use_system_packages})

-- Header files
local headers = {
    "iconv_cpp.hpp"
}

target("atom-extra-iconv")
    set_kind("headeronly")

    -- Add headers
    for _, hdr in ipairs(headers) do
        add_headerfiles(hdr)
    end

    -- Include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("libiconv")

    -- Set C++ standard
    set_languages("c++20")

    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        local headerdir = path.join(installdir, "include", "atom", "extra", "iconv")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)
