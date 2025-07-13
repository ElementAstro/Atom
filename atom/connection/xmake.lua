-- xmake script for Atom-Connection
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom-Connection
-- Description: Connection Between Lithium Drivers, TCP and IPC
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-connection")
set_version("1.0.0", {build = "%Y%m%d%H%M"})
set_license("GPL-3.0")

-- Set languages
set_languages("c11", "cxx17")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Add required packages
add_requires("loguru", "openssl")

-- Add optional packages
if has_config("enable-libssh") then
    add_requires("libssh")
end

-- Define configuration options
option("enable-libssh")
    set_default(false)
    set_description("Enable LibSSH support")
    set_showmenu(true)
option_end()

option("enable-ssh")
    set_default(false)
    set_description("Enable SSH support")
    set_showmenu(true)
option_end()

-- Define base sources and headers
local base_sources = {
    "async_fifoclient.cpp",
    "async_fifoserver.cpp",
    "async_sockethub.cpp",
    "async_tcpclient.cpp",
    "async_udpclient.cpp",
    "async_udpserver.cpp",
    "fifoclient.cpp",
    "fifoserver.cpp",
    "sockethub.cpp",
    "tcpclient.cpp",
    "udpclient.cpp",
    "udpserver.cpp"
}

local base_headers = {
    "async_fifoclient.hpp",
    "async_fifoserver.hpp",
    "async_sockethub.hpp",
    "async_tcpclient.hpp",
    "async_udpclient.hpp",
    "async_udpserver.hpp",
    "fifoclient.hpp",
    "fifoserver.hpp",
    "sockethub.hpp",
    "tcpclient.hpp",
    "udpclient.hpp",
    "udpserver.hpp"
}

-- SSH-related files (conditional)
local ssh_sources = {
    "sshclient.cpp",
    "sshserver.cpp"
}

local ssh_headers = {
    "sshclient.hpp",
    "sshserver.hpp"
}

-- Main static library target
target("atom-connection")
    -- Set target kind
    set_kind("static")

    -- Add base source files
    add_files(base_sources)
    add_headerfiles(base_headers)

    -- Add SSH files conditionally
    if has_config("enable-libssh") then
        add_files(ssh_sources)
        add_headerfiles(ssh_headers)
    end

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("loguru", "openssl")

    -- Add SSH package conditionally
    if has_config("enable-ssh") then
        add_packages("libssh")
    end

    -- Add system libraries
    add_syslinks("pthread")

    -- Windows-specific libraries
    if is_plat("windows") then
        add_syslinks("ws2_32", "mswsock")
    end

    -- Enable position independent code
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})

    -- Set version info
    set_version("1.0.0")

    -- Set output name
    set_basename("atom-connection")

    -- Set directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Installation rules
    after_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        -- Install static library
        os.cp(target:targetfile(), path.join(installdir, "lib"))

        -- Install headers
        local headerdir = path.join(installdir, "include", "atom-connection")
        os.mkdir(headerdir)

        -- Install base headers
        for _, header in ipairs(base_headers) do
            os.cp(header, headerdir)
        end

        -- Install SSH headers conditionally
        if has_config("enable-libssh") then
            for _, header in ipairs(ssh_headers) do
                os.cp(header, headerdir)
            end
        end
    end)

-- Optional: Create object library target
target("atom-connection-object")
    set_kind("object")

    -- Add base files
    add_files(base_sources)
    add_headerfiles(base_headers)

    -- Add SSH files conditionally
    if has_config("enable-libssh") then
        add_files(ssh_sources)
        add_headerfiles(ssh_headers)
    end

    -- Configuration
    add_includedirs(".")
    add_packages("loguru", "openssl")

    if has_config("enable-ssh") then
        add_packages("libssh")
    end

    add_syslinks("pthread")

    if is_plat("windows") then
        add_syslinks("ws2_32", "mswsock")
    end

    -- Enable PIC
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})
