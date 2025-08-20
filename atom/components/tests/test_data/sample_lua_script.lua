-- Sample Lua script for component system testing
-- This script demonstrates various component operations

-- Test basic arithmetic
function test_arithmetic()
    local a = 10
    local b = 20
    local result = a + b
    print("Arithmetic test: " .. a .. " + " .. b .. " = " .. result)
    return result
end

-- Test component creation
function test_component_creation()
    print("Testing component creation...")
    
    local success = createComponent("LuaTestComponent")
    if success then
        print("Component created successfully")
        return true
    else
        print("Failed to create component")
        return false
    end
end

-- Test component interaction
function test_component_interaction()
    print("Testing component interaction...")
    
    -- Create a component
    if not createComponent("InteractionTestComponent") then
        print("Failed to create component for interaction test")
        return false
    end
    
    -- Set a variable
    local setResult = setVariable("InteractionTestComponent", "testValue", 42)
    if not setResult then
        print("Failed to set variable")
        return false
    end
    
    -- Get the variable back
    local getValue = getVariable("InteractionTestComponent", "testValue")
    if getValue == nil then
        print("Failed to get variable")
        return false
    end
    
    print("Variable value: " .. tostring(getValue))
    return true
end

-- Test error handling
function test_error_handling()
    print("Testing error handling...")
    
    -- Try to access non-existent component
    local result = getComponent("NonExistentComponent")
    if result == nil then
        print("Correctly handled non-existent component")
        return true
    else
        print("Error handling failed")
        return false
    end
end

-- Test table operations
function test_table_operations()
    print("Testing table operations...")
    
    local testTable = {
        name = "TestTable",
        value = 123,
        nested = {
            inner = "nested value"
        }
    }
    
    print("Table name: " .. testTable.name)
    print("Table value: " .. testTable.value)
    print("Nested value: " .. testTable.nested.inner)
    
    return testTable
end

-- Test loop operations
function test_loops()
    print("Testing loop operations...")
    
    local sum = 0
    for i = 1, 10 do
        sum = sum + i
    end
    
    print("Sum of 1 to 10: " .. sum)
    return sum
end

-- Test string operations
function test_string_operations()
    print("Testing string operations...")
    
    local str1 = "Hello"
    local str2 = "World"
    local combined = str1 .. ", " .. str2 .. "!"
    
    print("Combined string: " .. combined)
    return combined
end

-- Main test function
function run_all_tests()
    print("=== Running Lua Component System Tests ===")
    
    local results = {}
    
    results.arithmetic = test_arithmetic()
    results.component_creation = test_component_creation()
    results.component_interaction = test_component_interaction()
    results.error_handling = test_error_handling()
    results.table_operations = test_table_operations()
    results.loops = test_loops()
    results.string_operations = test_string_operations()
    
    print("=== Test Results ===")
    for test_name, result in pairs(results) do
        print(test_name .. ": " .. tostring(result))
    end
    
    return results
end

-- Performance test
function performance_test(iterations)
    iterations = iterations or 1000
    print("Running performance test with " .. iterations .. " iterations...")
    
    local start_time = getCurrentTime()
    
    for i = 1, iterations do
        local temp = i * 2 + 1
        temp = temp / 2
        temp = temp - 0.5
    end
    
    local end_time = getCurrentTime()
    local duration = end_time - start_time
    
    print("Performance test completed in " .. duration .. " ms")
    return duration
end

-- Memory test
function memory_test()
    print("Running memory test...")
    
    local large_table = {}
    for i = 1, 1000 do
        large_table[i] = "String number " .. i
    end
    
    print("Created table with " .. #large_table .. " elements")
    
    -- Clear the table
    large_table = nil
    collectgarbage()
    
    print("Memory test completed")
    return true
end

-- Export functions for external calling
return {
    run_all_tests = run_all_tests,
    performance_test = performance_test,
    memory_test = memory_test,
    test_arithmetic = test_arithmetic,
    test_component_creation = test_component_creation,
    test_component_interaction = test_component_interaction
}
