/*
 * python_engine.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Python Script Engine with pybind11-Compatible Interface
Provides Python scripting support with pybind11-style API design,
automatic type conversion, and comprehensive binding system.

**************************************************/

#ifndef ATOM_COMPONENT_PYTHON_ENGINE_HPP
#define ATOM_COMPONENT_PYTHON_ENGINE_HPP

// Macro to enable Python support
#ifndef ATOM_ENABLE_PYTHON
#define ATOM_ENABLE_PYTHON 0
#endif

#if ATOM_ENABLE_PYTHON

#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "scripting_api.hpp"

// Forward declarations for Python
struct _object;
typedef _object PyObject;

namespace atom::components::scripting {

/**
 * @brief Python-specific configuration
 */
struct PythonConfig {
    std::string pythonHome;                // Python home directory
    std::vector<std::string> modulePaths;  // Additional module search paths
    bool enableSitePackages = true;        // Enable site-packages
    bool enableUserSite = true;            // Enable user site directory
    bool isolatedMode = false;             // Run in isolated mode
    std::string programName = "atom_component_system";
    std::unordered_map<std::string, std::string> environmentVars;
};

/**
 * @brief Python object wrapper for safe reference management
 */
class PyObjectWrapper {
public:
    PyObjectWrapper() = default;
    explicit PyObjectWrapper(PyObject* obj, bool steal_ref = false);
    PyObjectWrapper(const PyObjectWrapper& other);
    PyObjectWrapper(PyObjectWrapper&& other) noexcept;
    PyObjectWrapper& operator=(const PyObjectWrapper& other);
    PyObjectWrapper& operator=(PyObjectWrapper&& other) noexcept;
    ~PyObjectWrapper();

    PyObject* get() const { return obj_; }
    PyObject* release();
    void reset(PyObject* obj = nullptr, bool steal_ref = false);
    bool is_valid() const { return obj_ != nullptr; }

    operator bool() const { return is_valid(); }
    PyObject* operator->() const { return obj_; }

private:
    PyObject* obj_ = nullptr;
};

/**
 * @brief Python script engine implementation
 */
class PythonEngine : public IScriptEngine {
public:
    /**
     * @brief Constructs Python engine with configuration
     * @param config Python-specific configuration
     */
    explicit PythonEngine(const PythonConfig& config = {});

    /**
     * @brief Destructor
     */
    ~PythonEngine() override;

    // IScriptEngine interface implementation
    bool initialize(const ScriptEngineConfig& config) override;
    ScriptResult executeScript(const std::string& script,
                               const std::string& context = "") override;
    ScriptResult executeFile(const std::string& filename) override;
    ScriptResult callFunction(
        const std::string& functionName,
        const std::vector<ScriptValue>& args = {}) override;
    void setGlobal(const std::string& name, const ScriptValue& value) override;
    std::optional<ScriptValue> getGlobal(const std::string& name) override;
    void registerFunction(const std::string& name,
                          ScriptFunction function) override;
    ScriptLanguage getLanguage() const override {
        return ScriptLanguage::Auto;
    }  // Python not in enum
    const Statistics& getStatistics() const override { return statistics_; }
    void resetStatistics() override { statistics_ = Statistics{}; }
    void shutdown() override;

    /**
     * @brief Gets the main Python module
     * @return Main module wrapper
     */
    PyObjectWrapper getMainModule() const { return mainModule_; }

    /**
     * @brief Gets the global dictionary
     * @return Global dictionary wrapper
     */
    PyObjectWrapper getGlobals() const { return globals_; }

    /**
     * @brief Imports a Python module
     * @param moduleName Module name
     * @return Module wrapper
     */
    PyObjectWrapper importModule(const std::string& moduleName);

    /**
     * @brief Executes Python code with comprehensive error handling
     * @param code Python code to execute
     * @param context Execution context for error reporting
     * @param mode Execution mode (exec, eval, single)
     * @return Execution result
     */
    ScriptResult executePythonCode(const std::string& code,
                                   const std::string& context = "",
                                   const std::string& mode = "exec");

protected:
    ScriptValue cppToScript(const std::any& value) override;
    std::any scriptToCpp(const ScriptValue& value,
                         const std::type_info& targetType) override;

private:
    PythonConfig pythonConfig_;
    Statistics statistics_;
    PyObjectWrapper mainModule_;
    PyObjectWrapper globals_;
    std::unordered_map<std::string, ScriptFunction> registeredFunctions_;
    bool initialized_ = false;

    // Python utility methods
    bool initializePython();
    void setupPythonPaths();
    void setupEnvironment();
    void registerBasicFunctions();

    // Type conversion helpers
    PyObjectWrapper scriptValueToPython(const ScriptValue& value);
    ScriptValue pythonToScriptValue(PyObject* obj);

    // Error handling
    std::string getPythonError();
    void handlePythonError(const std::string& context);

    // Function call helpers
    PyObjectWrapper callPythonFunction(PyObject* func,
                                       const std::vector<ScriptValue>& args);

    // C function wrappers for Python callbacks
    static PyObject* pythonFunctionWrapper(PyObject* self, PyObject* args);
};

/**
 * @brief pybind11-compatible binding interface
 */
namespace py {

/**
 * @brief Module class for organizing bindings
 */
class module {
public:
    explicit module(PythonEngine& engine, const std::string& name = "__main__");

    /**
     * @brief Defines a function in the module
     * @tparam Func Function type
     * @param name Function name
     * @param func Function pointer
     * @param doc Optional documentation string
     * @return Reference to this module
     */
    template <typename Func>
    module& def(const std::string& name, Func func,
                const std::string& doc = "");

    /**
     * @brief Defines a class in the module
     * @tparam T Class type
     * @param name Class name
     * @param doc Optional documentation string
     * @return Class binding object
     */
    template <typename T>
    class_<T> class_(const std::string& name, const std::string& doc = "");

    /**
     * @brief Defines a constant in the module
     * @tparam T Value type
     * @param name Constant name
     * @param value Constant value
     * @return Reference to this module
     */
    template <typename T>
    module& attr(const std::string& name, const T& value);

private:
    PythonEngine& engine_;
    std::string moduleName_;
    PyObjectWrapper moduleObj_;
};

/**
 * @brief Class binding helper (pybind11-compatible)
 */
template <typename T>
class class_ {
public:
    class_(PythonEngine& engine, const std::string& name,
           const std::string& doc = "");

    /**
     * @brief Binds a constructor
     * @tparam Args Constructor argument types
     * @return Reference to this class binding
     */
    template <typename... Args>
    class_& def(const std::string& name = "__init__");

    /**
     * @brief Binds a method
     * @tparam Func Method type
     * @param name Method name
     * @param func Method pointer
     * @param doc Optional documentation
     * @return Reference to this class binding
     */
    template <typename Func>
    class_& def(const std::string& name, Func func,
                const std::string& doc = "");

    /**
     * @brief Binds a static method
     * @tparam Func Function type
     * @param name Method name
     * @param func Function pointer
     * @param doc Optional documentation
     * @return Reference to this class binding
     */
    template <typename Func>
    class_& def_static(const std::string& name, Func func,
                       const std::string& doc = "");

    /**
     * @brief Binds a property with getter and setter
     * @tparam Getter Getter type
     * @tparam Setter Setter type
     * @param name Property name
     * @param getter Getter function
     * @param setter Setter function
     * @param doc Optional documentation
     * @return Reference to this class binding
     */
    template <typename Getter, typename Setter>
    class_& def_property(const std::string& name, Getter getter, Setter setter,
                         const std::string& doc = "");

    /**
     * @brief Binds a read-only property
     * @tparam Getter Getter type
     * @param name Property name
     * @param getter Getter function
     * @param doc Optional documentation
     * @return Reference to this class binding
     */
    template <typename Getter>
    class_& def_property_readonly(const std::string& name, Getter getter,
                                  const std::string& doc = "");

private:
    PythonEngine& engine_;
    std::string className_;
    PyObjectWrapper classObj_;
};

/**
 * @brief Type caster for automatic type conversion
 */
template <typename T>
struct type_caster {
    static PyObjectWrapper cast(const T& value);
    static T cast(PyObject* obj);
    static bool check(PyObject* obj);
};

/**
 * @brief Specializations for common types
 */
template <>
struct type_caster<int> {
    static PyObjectWrapper cast(int value);
    static int cast(PyObject* obj);
    static bool check(PyObject* obj);
};

template <>
struct type_caster<double> {
    static PyObjectWrapper cast(double value);
    static double cast(PyObject* obj);
    static bool check(PyObject* obj);
};

template <>
struct type_caster<std::string> {
    static PyObjectWrapper cast(const std::string& value);
    static std::string cast(PyObject* obj);
    static bool check(PyObject* obj);
};

template <>
struct type_caster<bool> {
    static PyObjectWrapper cast(bool value);
    static bool cast(PyObject* obj);
    static bool check(PyObject* obj);
};

}  // namespace py

/**
 * @brief Python binding macros for easy integration
 */
#define ATOM_PYTHON_MODULE(engine, name) \
    atom::components::scripting::py::module(engine, name)

#define ATOM_PYTHON_CLASS(engine, className) \
    atom::components::scripting::py::class_<className>(engine, #className)

#define ATOM_PYTHON_BIND_COMPONENT(engine, ComponentType) \
    ATOM_PYTHON_CLASS(engine, ComponentType)              \
        .def("getName", &ComponentType::getName)          \
        .def("getState", &ComponentType::getState)        \
        .def("setState", &ComponentType::setState)

/**
 * @brief Python engine factory
 */
class PythonEngineFactory {
public:
    /**
     * @brief Creates a Python engine instance
     * @param config Python configuration
     * @return Unique pointer to Python engine
     */
    static std::unique_ptr<PythonEngine> create(
        const PythonConfig& config = {});

    /**
     * @brief Checks if Python is available
     * @return True if Python support is compiled in
     */
    static bool isAvailable() { return true; }

    /**
     * @brief Gets Python version information
     * @return Version string
     */
    static std::string getVersion();

    /**
     * @brief Initializes the Python interpreter (global)
     * @param config Python configuration
     * @return True if successful
     */
    static bool initializeInterpreter(const PythonConfig& config = {});

    /**
     * @brief Finalizes the Python interpreter (global)
     */
    static void finalizeInterpreter();
};

}  // namespace atom::components::scripting

#else  // ATOM_ENABLE_PYTHON

namespace atom::components::scripting {

// Stub implementations when Python is disabled
class PythonEngine : public IScriptEngine {
public:
    explicit PythonEngine(const void* = nullptr) {}
    bool initialize(const ScriptEngineConfig&) override { return false; }
    ScriptResult executeScript(const std::string&,
                               const std::string& = "") override {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Python support not compiled in";
        return result;
    }
    ScriptResult executeFile(const std::string&) override {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Python support not compiled in";
        return result;
    }
    ScriptResult callFunction(const std::string&,
                              const std::vector<ScriptValue>& = {}) override {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Python support not compiled in";
        return result;
    }
    void setGlobal(const std::string&, const ScriptValue&) override {}
    std::optional<ScriptValue> getGlobal(const std::string&) override {
        return std::nullopt;
    }
    void registerFunction(const std::string&, ScriptFunction) override {}
    ScriptLanguage getLanguage() const override { return ScriptLanguage::Auto; }
    const Statistics& getStatistics() const override {
        static Statistics s;
        return s;
    }
    void resetStatistics() override {}
    void shutdown() override {}

protected:
    ScriptValue cppToScript(const std::any&) override { return ScriptValue(); }
    std::any scriptToCpp(const ScriptValue&, const std::type_info&) override {
        return std::any();
    }
};

class PythonEngineFactory {
public:
    static std::unique_ptr<PythonEngine> create(const void* = nullptr) {
        return nullptr;
    }
    static bool isAvailable() { return false; }
    static std::string getVersion() { return "Python support not compiled in"; }
    static bool initializeInterpreter(const void* = nullptr) { return false; }
    static void finalizeInterpreter() {}
};

namespace py {
class module {
public:
    explicit module(const void*, const std::string& = "") {}
    template <typename Func>
    module& def(const std::string&, Func, const std::string& = "") {
        return *this;
    }
};
}  // namespace py

}  // namespace atom::components::scripting

#endif  // ATOM_ENABLE_PYTHON

#endif  // ATOM_COMPONENT_PYTHON_ENGINE_HPP
