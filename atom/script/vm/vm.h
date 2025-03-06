// vm.h
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tsx {

// 前向声明
class Value;
class GarbageCollector;
class Closure;
class ObjectBase;

enum class OpCode : uint8_t {
    Constant,      // 从常量池中加载常量
    Add,           // 加法
    Subtract,      // 减法
    Multiply,      // 乘法
    Divide,        // 除法
    Modulo,        // 取模运算
    Negate,        // 取反
    Not,           // 逻辑非
    Equal,         // 相等检查
    NotEqual,      // 不等检查
    Less,          // 小于检查
    LessEqual,     // 小于等于检查
    Greater,       // 大于检查
    GreaterEqual,  // 大于等于检查
    And,           // 逻辑与
    Or,            // 逻辑或
    GetLocal,      // 获取局部变量
    SetLocal,      // 设置局部变量
    GetGlobal,     // 获取全局变量
    SetGlobal,     // 设置全局变量
    GetField,      // 从对象获取字段
    SetField,      // 设置对象字段
    GetIndex,      // 通过索引获取数组值
    SetIndex,      // 通过索引设置数组值
    Array,         // 创建数组
    Object,        // 创建对象
    Call,          // 调用函数
    Return,        // 从函数返回
    Jump,          // 无条件跳转
    JumpIfFalse,   // 如果为false则跳转
    JumpIfTrue,    // 如果为true则跳转
    Pop,           // 从栈中弹出一个值
    Dup,           // 复制栈顶值
    Closure,       // 创建闭包
    GetUpvalue,    // 获取上值
    SetUpvalue,    // 设置上值
    CloseUpvalue,  // 关闭上值
    CreateClass,   // 创建类
    GetSuper,      // 获取父类方法
    Inherit,       // 从父类继承
    Method,        // 在类上定义方法
    Null,          // 空值
    True,          // 布尔值true
    False,         // 布尔值false
    Undefined      // 未定义值
};

// VM的值类型
class Value {
public:
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Object,
        Function,
        Closure,
        NativeFunction,
        Class,
        Instance,
        Array
    };

    // 默认构造函数 - 创建Null值
    Value() : type(Type::Null), asBool(false) {}

    // 显式析构函数以便规则遵循
    ~Value() {
        if (type == Type::String) {
            asString.~shared_ptr();
        }
    }

    // 移动构造函数
    Value(Value&& other) noexcept : type(Type::Null) {
        moveFrom(std::move(other));
    }

    // 移动赋值运算符
    Value& operator=(Value&& other) noexcept {
        if (this != &other) {
            clear();
            moveFrom(std::move(other));
        }
        return *this;
    }

    // 值构造函数
    explicit Value(bool b) : type(Type::Boolean), asBool(b) {}
    explicit Value(double n) : type(Type::Number), asNumber(n) {}
    explicit Value(const std::string& s)
        : type(Type::String), asString(std::make_shared<std::string>(s)) {}
    explicit Value(std::string&& s)
        : type(Type::String),
          asString(std::make_shared<std::string>(std::move(s))) {}
    explicit Value(ObjectBase* obj) : type(Type::Object), asObject(obj) {}

    // 禁用拷贝构造和拷贝赋值，因为我们不希望隐式拷贝
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;

    // 创建值的静态工厂函数
    static Value makeNull() { return Value(); }
    static Value makeBoolean(bool b) { return Value(b); }
    static Value makeNumber(double n) { return Value(n); }
    static Value makeString(const std::string& s) { return Value(s); }
    static Value makeString(std::string&& s) { return Value(std::move(s)); }
    static Value makeObject(ObjectBase* obj) { return Value(obj); }

    Type getType() const { return type; }
    bool isTruthy() const;
    std::string toString() const;
    bool equals(const Value& other) const;

    // 访问方法
    bool getBoolean() const { return asBool; }
    double getNumber() const { return asNumber; }
    const std::string& getString() const { return *asString; }
    ObjectBase* getObject() const { return asObject; }

private:
    Type type;
    union {
        bool asBool;
        double asNumber;
        std::shared_ptr<std::string> asString;  // 使用shared_ptr来管理字符串
        ObjectBase* asObject;
    };

    // 辅助函数
    void clear() {
        if (type == Type::String) {
            asString.~shared_ptr();
        }
        type = Type::Null;
    }

    void moveFrom(Value&& other) {
        type = other.type;
        switch (type) {
            case Type::Boolean:
                asBool = other.asBool;
                break;
            case Type::Number:
                asNumber = other.asNumber;
                break;
            case Type::String:
                new (&asString)
                    std::shared_ptr<std::string>(std::move(other.asString));
                break;
            case Type::Object:
            case Type::Function:
            case Type::Closure:
            case Type::NativeFunction:
            case Type::Class:
            case Type::Instance:
            case Type::Array:
                asObject = other.asObject;
                break;
            default:
                // Null值不需要特殊处理
                break;
        }
        other.type = Type::Null;
    }
};

// 引用类型的对象基类
class ObjectBase {
public:
    enum class ObjectType {
        String,
        Function,
        Closure,
        Array,
        Instance,
        Class,
        NativeFunction
    };

    explicit ObjectBase(ObjectType type) : type(type) {}
    virtual ~ObjectBase() = default;

    ObjectType getType() const { return type; }
    virtual std::string toString() const = 0;

    // 用于垃圾回收
    virtual void markReferences([[maybe_unused]] GarbageCollector& gc) {}

private:
    ObjectType type;
};

// 函数对象
class Function : public ObjectBase {
public:
    Function(std::string name, std::vector<uint8_t> bytecode,
             std::vector<Value> constants, uint8_t numParameters,
             uint8_t numLocals, uint8_t numUpvalues = 0)
        : ObjectBase(ObjectType::Function),
          name(std::move(name)),
          bytecode(std::move(bytecode)),
          // 由于我们使用了移动构造，这里可以使用std::move
          // constants现在是空的vector
          numParameters(numParameters),
          numLocals(numLocals),
          numUpvalues(numUpvalues) {
        // 手动移动constants的元素
        for (auto&& val : constants) {
            this->constants.push_back(std::move(val));
        }
    }

    const std::string& getName() const { return name; }
    const std::vector<uint8_t>& getBytecode() const { return bytecode; }
    const std::vector<Value>& getConstants() const { return constants; }
    uint8_t getNumParameters() const { return numParameters; }
    uint8_t getNumLocals() const { return numLocals; }
    uint8_t getNumUpvalues() const { return numUpvalues; }

    std::string toString() const override { return "<function " + name + ">"; }

    // 添加支持Codegen需要的方法
    void addCode(uint8_t byte) { bytecode.push_back(byte); }
    void addConstant(Value value) { constants.push_back(std::move(value)); }
    void setCode(size_t offset, uint8_t byte) { bytecode[offset] = byte; }
    size_t getCodeSize() const { return bytecode.size(); }
    size_t getConstantCount() const { return constants.size(); }
    void setNumUpvalues(uint8_t upvalues) { numUpvalues = upvalues; }

private:
    std::string name;
    std::vector<uint8_t> bytecode;
    std::vector<Value> constants;  // 现在使用可移动的Value
    uint8_t numParameters;
    uint8_t numLocals;
    uint8_t numUpvalues;
};

// 闭包的上值
class Upvalue : public ObjectBase {
public:
    explicit Upvalue(Value* location)
        : ObjectBase(ObjectType::Closure),
          location(location),
          closed() {}  // 使用默认构造函数创建null值

    Value* getLocation() const { return location; }
    Value& getClosed() { return closed; }  // 返回引用以允许修改
    const Value& getClosed() const {
        return closed;
    }  // 返回const引用以避免复制

    void setClosed(Value&& value) {
        closed = std::move(value);
        location = nullptr;
    }

    bool isClosed() const { return location == nullptr; }

    std::string toString() const override { return "<upvalue>"; }

    void markReferences([[maybe_unused]] GarbageCollector& gc) override {
        // 标记closed值
        if (isClosed()) {
            // TODO: 标记closed值
        }
    }

private:
    Value* location;  // 指向栈上的值，如果已关闭则为null
    Value closed;     // 已关闭的值
};

// 函数闭包
class Closure : public ObjectBase {
public:
    explicit Closure(Function* function)
        : ObjectBase(ObjectType::Closure), function(function) {
        upvalues.resize(function->getNumUpvalues());
    }

    Function* getFunction() const { return function; }
    std::vector<Upvalue*>& getUpvalues() { return upvalues; }

    std::string toString() const override { return function->toString(); }

    void markReferences([[maybe_unused]] GarbageCollector& gc) override {
        // 标记函数和上值
        // TODO: 标记函数
        for ([[maybe_unused]] auto upvalue : upvalues) {
            // TODO: 标记upvalue
        }
    }

private:
    Function* function;
    std::vector<Upvalue*> upvalues;
};

// 原生函数
class NativeFunction : public ObjectBase {
public:
    // 重新定义NativeFn，不使用Value返回值
    using NativeFn = std::function<void(const std::vector<Value>&, Value*)>;

    NativeFunction(std::string name, NativeFn function, uint8_t arity)
        : ObjectBase(ObjectType::NativeFunction),
          name(std::move(name)),
          function(std::move(function)),
          arity(arity) {}

    const std::string& getName() const { return name; }
    const NativeFn& getFunction() const { return function; }
    uint8_t getArity() const { return arity; }

    void call(const std::vector<Value>& args, Value* result) const {
        function(args, result);
    }

    std::string toString() const override {
        return "<native function " + name + ">";
    }

private:
    std::string name;
    NativeFn function;
    uint8_t arity;
};

// 数组实现
class ArrayObject : public ObjectBase {
public:
    ArrayObject() : ObjectBase(ObjectType::Array) {}

    // 使用移动构造
    explicit ArrayObject(std::vector<Value>&& elements)
        : ObjectBase(ObjectType::Array), elements() {
        // 手动移动每个元素
        for (auto&& val : elements) {
            this->elements.push_back(std::move(val));
        }
    }

    const std::vector<Value>& getElements() const { return elements; }
    std::vector<Value>& getElementsMutable() { return elements; }

    void push(Value&& value) { elements.push_back(std::move(value)); }

    // 返回引用以避免复制
    const Value& get(size_t index) const { return elements[index]; }

    void set(size_t index, Value&& value) {
        elements[index] = std::move(value);
    }

    size_t size() const { return elements.size(); }

    std::string toString() const override {
        std::string result = "[";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += elements[i].toString();
        }
        return result + "]";
    }

    void markReferences([[maybe_unused]] GarbageCollector& gc) override {
        // 标记所有元素
        for ([[maybe_unused]] const auto& element : elements) {
            // TODO: 标记元素
        }
    }

private:
    std::vector<Value> elements;
};

// 类实现
class ClassObject : public ObjectBase {
public:
    explicit ClassObject(std::string name, ClassObject* superclass = nullptr)
        : ObjectBase(ObjectType::Class),
          name(std::move(name)),
          superclass(superclass) {}

    const std::string& getName() const { return name; }
    ClassObject* getSuperclass() const { return superclass; }

    void defineMethod(const std::string& name, Value&& method) {
        methods[name] = std::move(method);
    }

    // 返回引用以避免复制
    const Value& getMethod(const std::string& name) const {
        static Value nullValue = Value();  // 静态null值

        auto it = methods.find(name);
        if (it != methods.end()) {
            return it->second;
        }

        if (superclass) {
            return superclass->getMethod(name);
        }

        return nullValue;
    }

    std::string toString() const override { return "<class " + name + ">"; }

    void markReferences([[maybe_unused]] GarbageCollector& gc) override {
        // 标记父类
        if (superclass) {
            // TODO: 标记父类
        }

        // 标记方法
        for ([[maybe_unused]] const auto& [methodName, methodValue] : methods) {
            // TODO: 标记方法
        }
    }

private:
    std::string name;
    ClassObject* superclass;
    std::unordered_map<std::string, Value> methods;
};

// 实例实现
class InstanceObject : public ObjectBase {
public:
    explicit InstanceObject(ClassObject* classObj)
        : ObjectBase(ObjectType::Instance), classObj(classObj) {}

    ClassObject* getClass() const { return classObj; }

    void setField(const std::string& name, Value&& value) {
        fields[name] = std::move(value);
    }

    // 返回一个指向字段的指针而不是可选值
    const Value* getField(const std::string& name) const {
        auto it = fields.find(name);
        if (it != fields.end()) {
            return &it->second;
        }
        return nullptr;
    }

    std::string toString() const override {
        return "<instance of " + classObj->getName() + ">";
    }

    void markReferences([[maybe_unused]] GarbageCollector& gc) override {
        // 标记类
        // TODO: 标记类

        // 标记字段
        for ([[maybe_unused]] const auto& [fieldName, fieldValue] : fields) {
            // TODO: 标记值
        }
    }

private:
    ClassObject* classObj;  // 使用classObj替换klass避免关键字冲突
    std::unordered_map<std::string, Value> fields;
};

// VM实现
class VirtualMachine {
public:
    VirtualMachine();
    ~VirtualMachine();

    // 执行函数并返回结果
    void execute(Function* function, const std::vector<Value>& args,
                 Value* result);

    // 执行模块并返回结果
    void executeModule(const std::string& source, Value* result);

    // 原生函数注册
    void defineNative(const std::string& name,
                      NativeFunction::NativeFn function, uint8_t arity);

    std::vector<Value> stack;
    std::vector<Value> globals;
    std::vector<Upvalue*> openUpvalues;

    Closure* currentClosure = nullptr;
    uint8_t* ip = nullptr;  // 指令指针

    // 垃圾收集器
    GarbageCollector* gc;

    // 辅助方法
    const Value& peek(int offset = 0) const;
    void push(Value&& value);
    Value pop();

    uint8_t readByte();
    uint16_t readShort();
    Value readConstant();

    void callValue(Value&& callee, int argCount, Value* result);
    void call(Closure* closure, int argCount, Value* result);
    void closeUpvalues(Value* last);
    Upvalue* captureUpvalue(Value* local);

    void runtimeError(const std::string& message);

    // 调试辅助
    void dumpStack() const;
    static std::string disassembleInstruction(const Function* function,
                                              size_t offset);
};

}  // namespace tsx