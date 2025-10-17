-- xmake configuration for libuv extensions
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-uv")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")  -- C++23 required for std::expected

-- Add required packages
add_requires("libuv", {optional = true})

-- Source files
local sources = {
    "message_bus.cpp",
    "subprocess.cpp"
}

-- Header files
local headers = {
    "coro.hpp",
    "message_bus.hpp",
    "subprocess.hpp"
}

target("atom-extra-uv")
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

    -- Add packages
    add_packages("libuv")

    -- Add system libraries
    add_syslinks("pthread")

    -- Set C++ standard
    set_languages("c++23")

    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        local headerdir = path.join(installdir, "include", "atom", "extra", "uv")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)
