-- filepath: d:\msys64\home\qwdma\Atom\tests\xmake.lua
-- xmake configuration for Atom tests
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-tests")
set_version("1.0.0")
set_license("GPL3")

-- Set C++ standard
set_languages("c++20")

-- Add required packages
add_requires("gtest", {system = false})

-- Helper function to create test targets
function add_atom_test(name, files)
    target("test_" .. name)
        -- Set target kind to executable
        set_kind("binary")
        
        -- Add group for testing
        set_group("tests")
        
        -- Add source files
        add_files(files)
        
        -- Add dependencies
        add_deps("atom")
        
        -- Add packages
        add_packages("gtest", "loguru")
        
        -- Output directory
        set_targetdir("$(buildir)/tests")
        
        -- Set test target attributes
        on_run(function(target)
            os.execv(target:targetfile())
        end)
    target_end()
end

-- Find and add test files automatically
local test_files = os.files("*.cpp")
for _, file in ipairs(test_files) do
    local base_name = path.basename(file)
    add_atom_test(base_name, file)
end

-- Create a test group target
target("test")
    set_kind("phony")
    set_group("tests")
    
    on_run(function(target)
        -- Run all test targets
        local test_targets = target.project.targets
        for name, target in pairs(test_targets) do
            if name:startswith("test_") and target:get("kind") == "binary" then
                os.execv(target:targetfile())
            end
        end
    end)
target_end()
