-- xmake configuration for CURL HTTP client library
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-curl")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++20")

-- Add required packages
add_requires("libcurl", {optional = true})

-- Source files
local sources = {
    "cache.cpp",
    "connection_pool.cpp",
    "cookie.cpp",
    "error.cpp",
    "multi_session.cpp",
    "multipart.cpp",
    "rate_limiter.cpp",
    "request.cpp",
    "response.cpp",
    "session.cpp",
    "session_pool.cpp",
    "websocket.cpp"
}

-- Header files
local headers = {
    "cache.hpp",
    "connection_pool.hpp",
    "cookie.hpp",
    "error.hpp",
    "interceptor.hpp",
    "multi_session.hpp",
    "multipart.hpp",
    "rate_limiter.hpp",
    "request.hpp",
    "response.hpp",
    "rest_client.hpp",
    "session.hpp",
    "session_pool.hpp",
    "websocket.hpp"
}

target("atom-extra-curl")
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
    add_packages("libcurl")

    -- Set C++ standard
    set_languages("c++20")

    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        local headerdir = path.join(installdir, "include", "atom", "extra", "curl")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)
