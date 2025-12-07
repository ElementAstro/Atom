-- filepath: d:\msys64\home\qwdma\Atom\atom\web\xmake.lua
-- xmake configuration for Atom-Web module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-web")
set_version("1.0.0")
set_license("GPL3")

-- Include time subdirectory
includes("time/xmake.lua")

-- Define source files from new structure
local sources = {
    -- Address functionality
    "address/address.cpp",
    "address/ipv4.cpp",
    "address/ipv6.cpp",
    "address/unix_domain.cpp",

    -- HTTP functionality
    "http/curl.cpp",
    "http/downloader.cpp",
    "http/httpparser.cpp",

    -- MIME type functionality
    "mime/minetype.cpp",

    -- Utility functionality
    "utils/addr_info.cpp",
    "utils/dns.cpp",
    "utils/ip.cpp",
    "utils/network.cpp",
    "utils/port.cpp",
    "utils/socket.cpp"
}

-- Add time module sources
for _, src in ipairs(get_time_sources()) do
    table.insert(sources, "time/" .. src)
end

-- Define header files from new structure
local headers = {
    -- Backwards compatibility headers
    "address.hpp",
    "curl.hpp",
    "downloader.hpp",
    "httpparser.hpp",
    "minetype.hpp",
    "time.hpp",
    "utils.hpp",

    -- Address implementation headers
    "address/address.hpp",
    "address/ipv4.hpp",
    "address/ipv6.hpp",
    "address/main.hpp",
    "address/unix_domain.hpp",

    -- HTTP implementation headers
    "http/curl.hpp",
    "http/downloader.hpp",
    "http/httpparser.hpp",

    -- MIME implementation headers
    "mime/minetype.hpp",

    -- Utils implementation headers
    "utils/common.hpp",
    "utils/addr_info.hpp",
    "utils/dns.hpp",
    "utils/ip.hpp",
    "utils/network.hpp",
    "utils/port.hpp",
    "utils/socket.hpp"
}

-- Add time module headers
for _, hdr in ipairs(get_time_headers()) do
    table.insert(headers, "time/" .. hdr)
end

-- Object Library
target("atom-web-object")
    set_kind("object")

    -- Add files
    add_headerfiles(table.unpack(headers))
    add_files(table.unpack(sources))

    -- Add dependencies
    add_packages("loguru")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-web")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-web-object")
    add_packages("loguru", "cpp-httplib")

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("wsock32", "ws2_32")
    end

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/web"))
        os.cp("http/*.hpp", path.join(target:installdir(), "include/atom/web/http"))
        os.cp("mime/*.hpp", path.join(target:installdir(), "include/atom/web/mime"))
        os.cp("utils/*.hpp", path.join(target:installdir(), "include/atom/web/utils"))
        os.cp("time/*.hpp", path.join(target:installdir(), "include/atom/web/time"))
        os.cp("address/*.hpp", path.join(target:installdir(), "include/atom/web/address"))
    end)
target_end()
