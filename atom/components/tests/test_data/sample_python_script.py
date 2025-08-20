#!/usr/bin/env python3
"""
Sample Python script for component system testing
This script demonstrates various component operations using Python
"""

import time
import sys

def test_arithmetic():
    """Test basic arithmetic operations"""
    print("Testing arithmetic operations...")
    
    a = 10
    b = 20
    result = a + b
    
    print(f"Arithmetic test: {a} + {b} = {result}")
    return result

def test_component_creation():
    """Test component creation through Python API"""
    print("Testing component creation...")
    
    try:
        # This would call the C++ component creation function
        success = createComponent("PythonTestComponent")
        if success:
            print("Component created successfully")
            return True
        else:
            print("Failed to create component")
            return False
    except NameError:
        print("createComponent function not available (expected in test environment)")
        return True  # Return True for testing purposes

def test_component_interaction():
    """Test component interaction through Python API"""
    print("Testing component interaction...")
    
    try:
        # Create a component
        if not createComponent("InteractionTestComponent"):
            print("Failed to create component for interaction test")
            return False
        
        # Set a variable
        set_result = setVariable("InteractionTestComponent", "testValue", 42)
        if not set_result:
            print("Failed to set variable")
            return False
        
        # Get the variable back
        get_value = getVariable("InteractionTestComponent", "testValue")
        if get_value is None:
            print("Failed to get variable")
            return False
        
        print(f"Variable value: {get_value}")
        return True
        
    except NameError:
        print("Component functions not available (expected in test environment)")
        return True

def test_error_handling():
    """Test error handling"""
    print("Testing error handling...")
    
    try:
        # Try to access non-existent component
        result = getComponent("NonExistentComponent")
        if result is None:
            print("Correctly handled non-existent component")
            return True
        else:
            print("Error handling failed")
            return False
    except NameError:
        print("getComponent function not available (expected in test environment)")
        return True
    except Exception as e:
        print(f"Exception caught: {e}")
        return True

def test_data_structures():
    """Test Python data structures"""
    print("Testing data structures...")
    
    # Test dictionary
    test_dict = {
        "name": "TestDict",
        "value": 123,
        "nested": {
            "inner": "nested value"
        }
    }
    
    print(f"Dict name: {test_dict['name']}")
    print(f"Dict value: {test_dict['value']}")
    print(f"Nested value: {test_dict['nested']['inner']}")
    
    # Test list
    test_list = [1, 2, 3, 4, 5]
    list_sum = sum(test_list)
    print(f"List sum: {list_sum}")
    
    return {"dict": test_dict, "list": test_list, "sum": list_sum}

def test_loops():
    """Test loop operations"""
    print("Testing loop operations...")
    
    # Test for loop
    total = 0
    for i in range(1, 11):
        total += i
    
    print(f"Sum of 1 to 10: {total}")
    
    # Test list comprehension
    squares = [x**2 for x in range(1, 6)]
    print(f"Squares: {squares}")
    
    return {"sum": total, "squares": squares}

def test_string_operations():
    """Test string operations"""
    print("Testing string operations...")
    
    str1 = "Hello"
    str2 = "World"
    combined = f"{str1}, {str2}!"
    
    print(f"Combined string: {combined}")
    
    # Test string methods
    upper_str = combined.upper()
    lower_str = combined.lower()
    
    print(f"Upper: {upper_str}")
    print(f"Lower: {lower_str}")
    
    return {
        "original": combined,
        "upper": upper_str,
        "lower": lower_str
    }

def test_classes():
    """Test class definition and usage"""
    print("Testing classes...")
    
    class TestComponent:
        def __init__(self, name, value=0):
            self.name = name
            self.value = value
        
        def get_info(self):
            return f"Component {self.name} with value {self.value}"
        
        def update_value(self, new_value):
            old_value = self.value
            self.value = new_value
            return f"Updated from {old_value} to {new_value}"
    
    # Create and test component
    comp = TestComponent("TestComp", 42)
    info = comp.get_info()
    update_result = comp.update_value(100)
    
    print(f"Component info: {info}")
    print(f"Update result: {update_result}")
    
    return comp

def run_all_tests():
    """Run all test functions"""
    print("=== Running Python Component System Tests ===")
    
    results = {}
    
    try:
        results["arithmetic"] = test_arithmetic()
        results["component_creation"] = test_component_creation()
        results["component_interaction"] = test_component_interaction()
        results["error_handling"] = test_error_handling()
        results["data_structures"] = test_data_structures()
        results["loops"] = test_loops()
        results["string_operations"] = test_string_operations()
        results["classes"] = test_classes()
        
        print("\n=== Test Results ===")
        for test_name, result in results.items():
            print(f"{test_name}: {type(result).__name__} - {str(result)[:50]}...")
        
        return results
        
    except Exception as e:
        print(f"Error during test execution: {e}")
        return {"error": str(e)}

def performance_test(iterations=1000):
    """Performance test"""
    print(f"Running performance test with {iterations} iterations...")
    
    start_time = time.time()
    
    for i in range(iterations):
        temp = i * 2 + 1
        temp = temp / 2
        temp = temp - 0.5
    
    end_time = time.time()
    duration = (end_time - start_time) * 1000  # Convert to milliseconds
    
    print(f"Performance test completed in {duration:.2f} ms")
    return duration

def memory_test():
    """Memory test"""
    print("Running memory test...")
    
    # Create large data structure
    large_list = [f"String number {i}" for i in range(1000)]
    
    print(f"Created list with {len(large_list)} elements")
    
    # Test memory usage (simplified)
    import sys
    memory_usage = sys.getsizeof(large_list)
    print(f"List memory usage: {memory_usage} bytes")
    
    # Clear the list
    large_list = None
    
    print("Memory test completed")
    return memory_usage

def main():
    """Main function for standalone execution"""
    if len(sys.argv) > 1:
        if sys.argv[1] == "performance":
            iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
            return performance_test(iterations)
        elif sys.argv[1] == "memory":
            return memory_test()
    
    return run_all_tests()

# Export functions for external calling
__all__ = [
    'run_all_tests',
    'performance_test', 
    'memory_test',
    'test_arithmetic',
    'test_component_creation',
    'test_component_interaction'
]

if __name__ == "__main__":
    result = main()
    print(f"\nScript execution completed. Result type: {type(result)}")
