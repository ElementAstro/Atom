/*
 * python_engine.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "python_engine.hpp"

#if ATOM_ENABLE_PYTHON

#include <Python.h>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace atom::components::scripting {

// PyObjectWrapper implementation
PyObjectWrapper::PyObjectWrapper(PyObject* obj, bool steal_ref) : obj_(obj) {
    if (obj_ && !steal_ref) {
        Py_INCREF(obj_);
    }
}

PyObjectWrapper::PyObjectWrapper(const PyObjectWrapper& other)
    : obj_(other.obj_) {
    if (obj_) {
        Py_INCREF(obj_);
    }
}

PyObjectWrapper::PyObjectWrapper(PyObjectWrapper&& other) noexcept
    : obj_(other.obj_) {
    other.obj_ = nullptr;
}

PyObjectWrapper& PyObjectWrapper::operator=(const PyObjectWrapper& other) {
    if (this != &other) {
        if (obj_) {
            Py_DECREF(obj_);
        }
        obj_ = other.obj_;
        if (obj_) {
            Py_INCREF(obj_);
        }
    }
    return *this;
}

PyObjectWrapper& PyObjectWrapper::operator=(PyObjectWrapper&& other) noexcept {
    if (this != &other) {
        if (obj_) {
            Py_DECREF(obj_);
        }
        obj_ = other.obj_;
        other.obj_ = nullptr;
    }
    return *this;
}

PyObjectWrapper::~PyObjectWrapper() {
    if (obj_) {
        Py_DECREF(obj_);
    }
}

PyObject* PyObjectWrapper::release() {
    PyObject* obj = obj_;
    obj_ = nullptr;
    return obj;
}

void PyObjectWrapper::reset(PyObject* obj, bool steal_ref) {
    if (obj_) {
        Py_DECREF(obj_);
    }
    obj_ = obj;
    if (obj_ && !steal_ref) {
        Py_INCREF(obj_);
    }
}

// PythonEngine implementation
PythonEngine::PythonEngine(const PythonConfig& config)
    : pythonConfig_(config) {}

PythonEngine::~PythonEngine() { shutdown(); }

bool PythonEngine::initialize(const ScriptEngineConfig& config) {
    if (initialized_) {
        return true;
    }

    return initializePython();
}

bool PythonEngine::initializePython() {
    if (!Py_IsInitialized()) {
        // Set program name
        if (!pythonConfig_.programName.empty()) {
            wchar_t* programName =
                Py_DecodeLocale(pythonConfig_.programName.c_str(), nullptr);
            if (programName) {
                Py_SetProgramName(programName);
                PyMem_RawFree(programName);
            }
        }

        // Set Python home if specified
        if (!pythonConfig_.pythonHome.empty()) {
            wchar_t* pythonHome =
                Py_DecodeLocale(pythonConfig_.pythonHome.c_str(), nullptr);
            if (pythonHome) {
                Py_SetPythonHome(pythonHome);
                PyMem_RawFree(pythonHome);
            }
        }

        // Initialize Python
        Py_Initialize();
        if (!Py_IsInitialized()) {
            return false;
        }
    }

    // Get main module and globals
    mainModule_ = PyObjectWrapper(PyImport_AddModule("__main__"), false);
    if (!mainModule_) {
        return false;
    }

    globals_ = PyObjectWrapper(PyModule_GetDict(mainModule_.get()), false);
    if (!globals_) {
        return false;
    }

    // Setup Python paths
    setupPythonPaths();

    // Setup environment
    setupEnvironment();

    // Register basic functions
    registerBasicFunctions();

    initialized_ = true;
    return true;
}

ScriptResult PythonEngine::executeScript(const std::string& script,
                                         const std::string& context) {
    return executePythonCode(script, context, "exec");
}

ScriptResult PythonEngine::executeFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Cannot open file: " + filename;
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    return executePythonCode(buffer.str(), filename, "exec");
}

ScriptResult PythonEngine::executePythonCode(const std::string& code,
                                             const std::string& context,
                                             const std::string& mode) {
    if (!initialized_) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Python engine not initialized";
        return result;
    }

    const auto startTime = std::chrono::high_resolution_clock::now();
    ScriptResult result;

    // Compile the code
    PyObjectWrapper codeObj(
        Py_CompileString(code.c_str(), context.c_str(),
                         mode == "exec"   ? Py_file_input
                         : mode == "eval" ? Py_eval_input
                                          : Py_single_input),
        true);

    if (!codeObj) {
        result.success = false;
        result.errorMessage = getPythonError();
        PyErr_Clear();
        return result;
    }

    // Execute the code
    PyObjectWrapper resultObj(
        PyEval_EvalCode(codeObj.get(), globals_.get(), globals_.get()), true);

    if (!resultObj) {
        result.success = false;
        result.errorMessage = getPythonError();
        PyErr_Clear();
    } else {
        result.success = true;

        // Convert result to ScriptValue
        if (resultObj.get() != Py_None) {
            result.returnValue = pythonToScriptValue(resultObj.get());
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    // Update statistics
    statistics_.scriptsExecuted++;
    if (result.success) {
        statistics_.successfulExecutions++;
    } else {
        statistics_.failedExecutions++;
    }
    statistics_.totalExecutionTime += result.executionTime;

    return result;
}

ScriptResult PythonEngine::callFunction(const std::string& functionName,
                                        const std::vector<ScriptValue>& args) {
    if (!initialized_) {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Python engine not initialized";
        return result;
    }

    const auto startTime = std::chrono::high_resolution_clock::now();
    ScriptResult result;

    // Get the function from globals
    PyObjectWrapper func(
        PyDict_GetItemString(globals_.get(), functionName.c_str()), false);
    if (!func || !PyCallable_Check(func.get())) {
        result.success = false;
        result.errorMessage =
            "Function not found or not callable: " + functionName;
        return result;
    }

    // Call the function
    PyObjectWrapper resultObj = callPythonFunction(func.get(), args);

    if (!resultObj) {
        result.success = false;
        result.errorMessage = getPythonError();
        PyErr_Clear();
    } else {
        result.success = true;

        // Convert result to ScriptValue
        if (resultObj.get() != Py_None) {
            result.returnValue = pythonToScriptValue(resultObj.get());
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    // Update statistics
    statistics_.functionsExecuted++;
    statistics_.totalExecutionTime += result.executionTime;

    return result;
}

void PythonEngine::setGlobal(const std::string& name,
                             const ScriptValue& value) {
    if (!initialized_)
        return;

    PyObjectWrapper pyValue = scriptValueToPython(value);
    if (pyValue) {
        PyDict_SetItemString(globals_.get(), name.c_str(), pyValue.get());
    }
}

std::optional<ScriptValue> PythonEngine::getGlobal(const std::string& name) {
    if (!initialized_)
        return std::nullopt;

    PyObjectWrapper pyValue(PyDict_GetItemString(globals_.get(), name.c_str()),
                            false);
    if (!pyValue) {
        return std::nullopt;
    }

    return pythonToScriptValue(pyValue.get());
}

void PythonEngine::registerFunction(const std::string& name,
                                    ScriptFunction function) {
    if (!initialized_)
        return;

    // Store the function in our registry
    registeredFunctions_[name] = function;

    // Create Python function wrapper
    // This is a simplified implementation - a full implementation would need
    // to create proper Python function objects with closures

    // For now, we'll add a placeholder that can be called from Python
    std::string pythonCode = "def " + name + "(*args): pass";
    executePythonCode(pythonCode, "function_registration");
}

PyObjectWrapper PythonEngine::importModule(const std::string& moduleName) {
    return PyObjectWrapper(PyImport_ImportModule(moduleName.c_str()), true);
}

void PythonEngine::shutdown() {
    if (initialized_) {
        globals_.reset();
        mainModule_.reset();
        registeredFunctions_.clear();
        initialized_ = false;
    }

    // Note: We don't call Py_Finalize() here as it might be used by other parts
    // of the application. The Python interpreter should be finalized at
    // application exit.
}

ScriptValue PythonEngine::cppToScript(const std::any& value) {
    // Simplified conversion - would need more comprehensive type handling
    return ScriptValue();
}

std::any PythonEngine::scriptToCpp(const ScriptValue& value,
                                   const std::type_info& targetType) {
    // Simplified conversion - would need more comprehensive type handling
    return std::any();
}

// Helper methods
void PythonEngine::setupPythonPaths() {
    if (pythonConfig_.modulePaths.empty())
        return;

    PyObjectWrapper sysModule(PyImport_ImportModule("sys"), true);
    if (!sysModule)
        return;

    PyObjectWrapper pathList(PyObject_GetAttrString(sysModule.get(), "path"),
                             true);
    if (!pathList || !PyList_Check(pathList.get()))
        return;

    for (const auto& path : pythonConfig_.modulePaths) {
        PyObjectWrapper pathStr(PyUnicode_FromString(path.c_str()), true);
        if (pathStr) {
            PyList_Append(pathList.get(), pathStr.get());
        }
    }
}

void PythonEngine::setupEnvironment() {
    // Setup environment variables
    for (const auto& [key, value] : pythonConfig_.environmentVars) {
        PyObjectWrapper osModule(PyImport_ImportModule("os"), true);
        if (osModule) {
            PyObjectWrapper environ(
                PyObject_GetAttrString(osModule.get(), "environ"), true);
            if (environ) {
                PyObjectWrapper keyStr(PyUnicode_FromString(key.c_str()), true);
                PyObjectWrapper valueStr(PyUnicode_FromString(value.c_str()),
                                         true);
                if (keyStr && valueStr) {
                    PyObject_SetItem(environ.get(), keyStr.get(),
                                     valueStr.get());
                }
            }
        }
    }
}

void PythonEngine::registerBasicFunctions() {
    // Register basic utility functions
    // This would be implemented with proper Python C API function creation
}

PyObjectWrapper PythonEngine::scriptValueToPython(const ScriptValue& value) {
    if (value.holds<std::monostate>()) {
        Py_INCREF(Py_None);
        return PyObjectWrapper(Py_None, true);
    } else if (value.holds<bool>()) {
        return PyObjectWrapper(PyBool_FromLong(value.get<bool>() ? 1 : 0),
                               true);
    } else if (value.holds<int64_t>()) {
        return PyObjectWrapper(PyLong_FromLongLong(value.get<int64_t>()), true);
    } else if (value.holds<double>()) {
        return PyObjectWrapper(PyFloat_FromDouble(value.get<double>()), true);
    } else if (value.holds<std::string>()) {
        return PyObjectWrapper(
            PyUnicode_FromString(value.get<std::string>().c_str()), true);
    } else {
        Py_INCREF(Py_None);
        return PyObjectWrapper(Py_None, true);
    }
}

ScriptValue PythonEngine::pythonToScriptValue(PyObject* obj) {
    if (obj == Py_None) {
        return ScriptValue();
    } else if (PyBool_Check(obj)) {
        return ScriptValue(obj == Py_True);
    } else if (PyLong_Check(obj)) {
        return ScriptValue(PyLong_AsLongLong(obj));
    } else if (PyFloat_Check(obj)) {
        return ScriptValue(PyFloat_AsDouble(obj));
    } else if (PyUnicode_Check(obj)) {
        const char* str = PyUnicode_AsUTF8(obj);
        return ScriptValue(std::string(str ? str : ""));
    } else {
        return ScriptValue();  // Unsupported type
    }
}

std::string PythonEngine::getPythonError() {
    if (!PyErr_Occurred()) {
        return "Unknown Python error";
    }

    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);

    std::string error = "Python error";

    if (pvalue) {
        PyObjectWrapper strObj(PyObject_Str(pvalue), true);
        if (strObj) {
            const char* errorStr = PyUnicode_AsUTF8(strObj.get());
            if (errorStr) {
                error = errorStr;
            }
        }
    }

    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);

    return error;
}

void PythonEngine::handlePythonError(const std::string& context) {
    if (PyErr_Occurred()) {
        std::string error = getPythonError();
        PyErr_Clear();
        // Log error or handle as needed
    }
}

PyObjectWrapper PythonEngine::callPythonFunction(
    PyObject* func, const std::vector<ScriptValue>& args) {
    // Create argument tuple
    PyObjectWrapper argTuple(PyTuple_New(args.size()), true);
    if (!argTuple) {
        return PyObjectWrapper();
    }

    // Fill arguments
    for (size_t i = 0; i < args.size(); ++i) {
        PyObjectWrapper arg = scriptValueToPython(args[i]);
        if (!arg) {
            return PyObjectWrapper();
        }
        PyTuple_SetItem(argTuple.get(), i, arg.release());  // Steals reference
    }

    // Call the function
    return PyObjectWrapper(PyObject_CallObject(func, argTuple.get()), true);
}

// Static callback functions
PyObject* PythonEngine::pythonFunctionWrapper(PyObject* self, PyObject* args) {
    // This would be implemented to wrap C++ functions for Python calling
    Py_RETURN_NONE;
}

// PythonEngineFactory implementation
std::unique_ptr<PythonEngine> PythonEngineFactory::create(
    const PythonConfig& config) {
    return std::make_unique<PythonEngine>(config);
}

std::string PythonEngineFactory::getVersion() { return PY_VERSION; }

// Advanced Type Conversion System for Python

/**
 * @brief Enhanced Python type converter with STL container support
 */
class PythonTypeConverter {
public:
    explicit PythonTypeConverter(PythonEngine& engine) : engine_(engine) {}

    // Convert C++ containers to Python objects
    template <typename T>
    PyObjectWrapper vectorToPython(const std::vector<T>& vec) {
        PyObjectWrapper list(PyList_New(vec.size()), true);
        if (!list)
            return PyObjectWrapper();

        for (size_t i = 0; i < vec.size(); ++i) {
            PyObjectWrapper item = valueToPython(vec[i]);
            if (!item)
                return PyObjectWrapper();
            PyList_SetItem(list.get(), i, item.release());  // Steals reference
        }

        return list;
    }

    template <typename K, typename V>
    PyObjectWrapper mapToPython(const std::unordered_map<K, V>& map) {
        PyObjectWrapper dict(PyDict_New(), true);
        if (!dict)
            return PyObjectWrapper();

        for (const auto& [key, value] : map) {
            PyObjectWrapper pyKey = valueToPython(key);
            PyObjectWrapper pyValue = valueToPython(value);

            if (!pyKey || !pyValue)
                return PyObjectWrapper();

            if (PyDict_SetItem(dict.get(), pyKey.get(), pyValue.get()) < 0) {
                return PyObjectWrapper();
            }
        }

        return dict;
    }

    template <typename T>
    PyObjectWrapper setToPython(const std::set<T>& set) {
        PyObjectWrapper pySet(PySet_New(nullptr), true);
        if (!pySet)
            return PyObjectWrapper();

        for (const auto& item : set) {
            PyObjectWrapper pyItem = valueToPython(item);
            if (!pyItem)
                return PyObjectWrapper();

            if (PySet_Add(pySet.get(), pyItem.get()) < 0) {
                return PyObjectWrapper();
            }
        }

        return pySet;
    }

    template <typename T>
    PyObjectWrapper optionalToPython(const std::optional<T>& opt) {
        if (!opt.has_value()) {
            Py_INCREF(Py_None);
            return PyObjectWrapper(Py_None, true);
        }
        return valueToPython(opt.value());
    }

    // Convert Python objects to C++ containers
    template <typename T>
    std::optional<std::vector<T>> pythonToVector(PyObject* obj) {
        if (!PyList_Check(obj))
            return std::nullopt;

        Py_ssize_t size = PyList_Size(obj);
        std::vector<T> result;
        result.reserve(size);

        for (Py_ssize_t i = 0; i < size; ++i) {
            PyObject* item = PyList_GetItem(obj, i);  // Borrowed reference
            auto value = pythonToValue<T>(item);
            if (!value)
                return std::nullopt;
            result.push_back(*value);
        }

        return result;
    }

    template <typename K, typename V>
    std::optional<std::unordered_map<K, V>> pythonToMap(PyObject* obj) {
        if (!PyDict_Check(obj))
            return std::nullopt;

        std::unordered_map<K, V> result;
        PyObject* key;
        PyObject* value;
        Py_ssize_t pos = 0;

        while (PyDict_Next(obj, &pos, &key, &value)) {
            auto cppKey = pythonToValue<K>(key);
            auto cppValue = pythonToValue<V>(value);

            if (!cppKey || !cppValue)
                return std::nullopt;

            result[*cppKey] = *cppValue;
        }

        return result;
    }

    template <typename T>
    std::optional<std::set<T>> pythonToSet(PyObject* obj) {
        if (!PySet_Check(obj))
            return std::nullopt;

        std::set<T> result;
        PyObjectWrapper iter(PyObject_GetIter(obj), true);
        if (!iter)
            return std::nullopt;

        PyObject* item;
        while ((item = PyIter_Next(iter.get())) != nullptr) {
            PyObjectWrapper itemWrapper(item, true);
            auto value = pythonToValue<T>(item);
            if (!value)
                return std::nullopt;
            result.insert(*value);
        }

        return result;
    }

    template <typename T>
    std::optional<std::optional<T>> pythonToOptional(PyObject* obj) {
        if (obj == Py_None) {
            return std::optional<T>{};  // Empty optional
        }

        auto value = pythonToValue<T>(obj);
        if (!value)
            return std::nullopt;

        return std::optional<T>{*value};
    }

private:
    PythonEngine& engine_;

    // Generic value conversion helpers
    template <typename T>
    PyObjectWrapper valueToPython(const T& value) {
        if constexpr (std::is_same_v<T, int>) {
            return PyObjectWrapper(PyLong_FromLong(value), true);
        } else if constexpr (std::is_same_v<T, double>) {
            return PyObjectWrapper(PyFloat_FromDouble(value), true);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return PyObjectWrapper(PyUnicode_FromString(value.c_str()), true);
        } else if constexpr (std::is_same_v<T, bool>) {
            return PyObjectWrapper(PyBool_FromLong(value ? 1 : 0), true);
        } else {
            // For complex types, use the engine's conversion
            return engine_.scriptValueToPython(
                engine_.cppToScript(std::any(value)));
        }
    }

    template <typename T>
    std::optional<T> pythonToValue(PyObject* obj) {
        if constexpr (std::is_same_v<T, int>) {
            if (!PyLong_Check(obj))
                return std::nullopt;
            return static_cast<int>(PyLong_AsLong(obj));
        } else if constexpr (std::is_same_v<T, double>) {
            if (!PyFloat_Check(obj))
                return std::nullopt;
            return PyFloat_AsDouble(obj);
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (!PyUnicode_Check(obj))
                return std::nullopt;
            const char* str = PyUnicode_AsUTF8(obj);
            return str ? std::string(str) : std::string();
        } else if constexpr (std::is_same_v<T, bool>) {
            if (!PyBool_Check(obj))
                return std::nullopt;
            return obj == Py_True;
        } else {
            // For complex types, use the engine's conversion
            ScriptValue scriptVal = engine_.pythonToScriptValue(obj);
            std::any anyVal = engine_.scriptToCpp(scriptVal, typeid(T));
            try {
                return std::any_cast<T>(anyVal);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
    }
};

// Enhanced pybind11-compatible binding implementation
namespace py {

template <typename T>
class class_ {
public:
    class_(PythonEngine& engine, const std::string& name,
           const std::string& doc = "")
        : engine_(engine), className_(name) {
        // Create Python class object
        // This is a simplified implementation - full pybind11 compatibility
        // would require more work
    }

    template <typename... Args>
    class_& def(const std::string& name = "__init__") {
        // Register constructor
        return *this;
    }

    template <typename Func>
    class_& def(const std::string& name, Func func,
                const std::string& doc = "") {
        // Register method
        engine_.registerFunction(
            className_ + "." + name,
            [func](const std::vector<ScriptValue>& args) -> ScriptValue {
                // Convert arguments and call function
                // This is a simplified implementation
                return ScriptValue();
            });
        return *this;
    }

    template <typename Func>
    class_& def_static(const std::string& name, Func func,
                       const std::string& doc = "") {
        // Register static method
        return *this;
    }

    template <typename Getter, typename Setter>
    class_& def_property(const std::string& name, Getter getter, Setter setter,
                         const std::string& doc = "") {
        // Register property with getter and setter
        return *this;
    }

    template <typename Getter>
    class_& def_property_readonly(const std::string& name, Getter getter,
                                  const std::string& doc = "") {
        // Register read-only property
        return *this;
    }

private:
    PythonEngine& engine_;
    std::string className_;
    PyObjectWrapper classObj_;
};

// Type caster implementations
template <>
PyObjectWrapper type_caster<int>::cast(int value) {
    return PyObjectWrapper(PyLong_FromLong(value), true);
}

template <>
int type_caster<int>::cast(PyObject* obj) {
    return static_cast<int>(PyLong_AsLong(obj));
}

template <>
bool type_caster<int>::check(PyObject* obj) {
    return PyLong_Check(obj);
}

template <>
PyObjectWrapper type_caster<double>::cast(double value) {
    return PyObjectWrapper(PyFloat_FromDouble(value), true);
}

template <>
double type_caster<double>::cast(PyObject* obj) {
    return PyFloat_AsDouble(obj);
}

template <>
bool type_caster<double>::check(PyObject* obj) {
    return PyFloat_Check(obj);
}

template <>
PyObjectWrapper type_caster<std::string>::cast(const std::string& value) {
    return PyObjectWrapper(PyUnicode_FromString(value.c_str()), true);
}

template <>
std::string type_caster<std::string>::cast(PyObject* obj) {
    const char* str = PyUnicode_AsUTF8(obj);
    return str ? std::string(str) : std::string();
}

template <>
bool type_caster<std::string>::check(PyObject* obj) {
    return PyUnicode_Check(obj);
}

template <>
PyObjectWrapper type_caster<bool>::cast(bool value) {
    return PyObjectWrapper(PyBool_FromLong(value ? 1 : 0), true);
}

template <>
bool type_caster<bool>::cast(PyObject* obj) {
    return obj == Py_True;
}

template <>
bool type_caster<bool>::check(PyObject* obj) {
    return PyBool_Check(obj);
}

}  // namespace py

}  // namespace atom::components::scripting

#endif  // ATOM_ENABLE_PYTHON
