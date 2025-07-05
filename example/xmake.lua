-- filepath: d:\msys64\home\qwdma\Atom\example\xmake.lua
-- xmake configuration for Atom examples
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-examples")
set_version("1.0.0")
set_license("GPL3")

-- Set C++ standard
set_languages("c++20")

-- Define module directories with examples
local example_dirs = {
    "algorithm",
    "async",
    "components",
    "connection",
    "error",
    "extra",
    "image",
    "io",
    "log",
    "memory",
    "meta",
    "search",
    "serial",
    "system",
    "type",
    "utils",
    "web"
}

-- Function to build examples from a directory
function build_examples_from_dir(dir)
    local files = os.files(dir .. "/*.cpp")

    for _, file in ipairs(files) do
        local name = path.basename(file)
        local example_name = "example_" .. dir:gsub("/", "_") .. "_" .. name

        target(example_name)
            -- Set target kind to executable
            set_kind("binary")

            -- Add source file
            add_files(file)

            -- Add dependencies on atom libraries
            add_deps("atom")

            -- Add packages
            add_packages("loguru")

            -- Set output directory
            set_targetdir("$(buildir)/examples/" .. dir)
        target_end()
    end
end

-- Build examples from all directories
for _, dir in ipairs(example_dirs) do
    if os.isdir(dir) then
        build_examples_from_dir(dir)
    end
end
