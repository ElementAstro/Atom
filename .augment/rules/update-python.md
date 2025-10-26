---
type: "manual"
---

# Update Python Bindings

I will provide you with two folders shortly. I need you to systematically
update Python bindings in the second folder based on the C++ modules in the
first folder. For each C++ module, please:

1. **Complete Interface Exposure**: Ensure every public class, method,
   function, property, and enum from the C++ module is properly exposed in
   the corresponding Python binding file
2. **Functional Completeness**: Verify that all C++ functionality is
   accessible from Python, including:
   - All public methods and their overloads
   - All constructors and destructors
   - All static methods and properties
   - All enums and constants
   - All operator overloads where applicable
3. **Comprehensive English Documentation**: Add complete English docstrings
   for:
   - Every exposed class with description of its purpose
   - Every method with parameter descriptions and return value descriptions
   - Every property with description of what it represents
   - Every enum value with its meaning
4. **Module-by-Module Processing**: Process each C++ module individually and
   update its corresponding Python binding file
5. **Consistency**: Ensure naming conventions and documentation style are
   consistent across all binding files
6. **Error Handling**: Properly handle C++ exceptions and convert them to
   appropriate Python exceptions

Please work through each module systematically, and let me know when you've
completed each one so I can review the changes before proceeding to the next
module.
