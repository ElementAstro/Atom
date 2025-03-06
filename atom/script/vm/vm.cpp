// vm.cpp
#include "vm.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "../gc/gc.h"

namespace tsx {

bool Value::isTruthy() const {
    switch (type) {
        case Type::Null:
            return false;
        case Type::Boolean:
            return asBool;
        case Type::Number:
            return asNumber != 0.0;
        case Type::String:
            return !asString->empty();
        default:
            return true;
    }
}

std::string Value::toString() const {
    switch (type) {
        case Type::Null:
            return "null";
        case Type::Boolean:
            return asBool ? "true" : "false";
        case Type::Number: {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6) << asNumber;
            std::string str = ss.str();

            // Remove trailing zeros and decimal point if not needed
            size_t pos = str.find_last_not_of('0');
            if (pos != std::string::npos && str[pos] == '.') {
                pos--;
            }
            return str.substr(0, pos + 1);
        }
        case Type::String:
            return *asString;
        case Type::Object:
        case Type::Function:
        case Type::Closure:
        case Type::NativeFunction:
        case Type::Class:
        case Type::Instance:
        case Type::Array:
            return asObject ? asObject->toString() : "null";
    }

    return "unknown";
}

bool Value::equals(const Value& other) const {
    if (type != other.type)
        return false;

    switch (type) {
        case Type::Null:
            return true;
        case Type::Boolean:
            return asBool == other.asBool;
        case Type::Number:
            return asNumber == other.asNumber;
        case Type::String:
            return *asString == *other.asString;
        case Type::Object:
            return asObject == other.asObject;
        default:
            return false;
    }
}

VirtualMachine::VirtualMachine() {
    gc = new GarbageCollector(*this);

    // Define standard library
    defineNative(
        "print",
        [](const std::vector<Value>& args, Value* result) {
            for (const auto& arg : args) {
                std::cout << arg.toString();
            }
            std::cout << std::endl;
            *result = Value::makeNull();
        },
        1);

    defineNative(
        "input",
        [](const std::vector<Value>& args, Value* result) {
            if (!args.empty()) {
                std::cout << args[0].toString();
            }
            std::string input;
            std::getline(std::cin, input);
            *result = Value::makeString(input);
        },
        0);

    defineNative(
        "clock",
        [](const std::vector<Value>& args [[maybe_unused]], Value* result) {
            *result = Value::makeNumber(static_cast<double>(std::clock()) /
                                        CLOCKS_PER_SEC);
        },
        0);
}

VirtualMachine::~VirtualMachine() { delete gc; }

void VirtualMachine::execute(Function* function, const std::vector<Value>& args,
                             Value* result) {
    // Create a closure for the function
    auto* closure = new Closure(function);
    currentClosure = closure;

    // Setup the stack frame
    size_t stackTop = stack.size();

    // Push the closure itself (will be used for recursion)
    push(Value::makeObject(closure));

    // Push arguments
    for (const auto& arg : args) {
        // 使用临时值创建一个新的Value（使用移动语义）
        push(Value::makeObject(arg.getObject()));
    }

    // Fill missing arguments with null
    for (size_t i = args.size(); i < function->getNumParameters(); ++i) {
        push(Value::makeNull());
    }

    // Reserve space for local variables
    for (size_t i = 0; i < function->getNumLocals(); ++i) {
        push(Value::makeNull());
    }

    // Set instruction pointer to beginning of function
    ip = const_cast<uint8_t*>(function->getBytecode().data());

    try {
        // Main execution loop
        for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
            dumpStack();
            disassembleInstruction(
                currentClosure->getFunction(),
                ip - currentClosure->getFunction()->getBytecode().data());
#endif

            OpCode instruction = static_cast<OpCode>(readByte());

            switch (instruction) {
                case OpCode::Constant: {
                    // 正确处理常量读取，避免拷贝
                    Value tempValue = Value::makeNull();
                    // 使用移动语义将readConstant的结果移动到tempValue
                    tempValue = readConstant();
                    push(std::move(tempValue));
                    break;
                }

                case OpCode::Add: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        push(Value::makeNumber(a.getNumber() + b.getNumber()));
                    } else if (a.getType() == Value::Type::String &&
                               b.getType() == Value::Type::String) {
                        push(Value::makeString(a.getString() + b.getString()));
                    } else {
                        runtimeError(
                            "Operands must be two numbers or two strings");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::Subtract: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        push(Value::makeNumber(a.getNumber() - b.getNumber()));
                    } else {
                        runtimeError("Operands must be numbers");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::Multiply: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        push(Value::makeNumber(a.getNumber() * b.getNumber()));
                    } else {
                        runtimeError("Operands must be numbers");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::Divide: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        if (b.getNumber() == 0) {
                            runtimeError("Division by zero");
                            *result = Value::makeNull();
                            return;
                        }
                        push(Value::makeNumber(a.getNumber() / b.getNumber()));
                    } else {
                        runtimeError("Operands must be numbers");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::Modulo: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        if (b.getNumber() == 0) {
                            runtimeError("Modulo by zero");
                            *result = Value::makeNull();
                            return;
                        }
                        push(Value::makeNumber(
                            std::fmod(a.getNumber(), b.getNumber())));
                    } else {
                        runtimeError("Operands must be numbers");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::Negate: {
                    if (peek().getType() != Value::Type::Number) {
                        runtimeError("Operand must be a number");
                        *result = Value::makeNull();
                        return;
                    }

                    push(Value::makeNumber(-pop().getNumber()));
                    break;
                }

                case OpCode::Not: {
                    push(Value::makeBoolean(!pop().isTruthy()));
                    break;
                }

                case OpCode::Equal: {
                    Value b = pop();
                    Value a = pop();
                    push(Value::makeBoolean(a.equals(b)));
                    break;
                }

                case OpCode::NotEqual: {
                    Value b = pop();
                    Value a = pop();
                    push(Value::makeBoolean(!a.equals(b)));
                    break;
                }

                case OpCode::Less: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        push(Value::makeBoolean(a.getNumber() < b.getNumber()));
                    } else {
                        runtimeError("Operands must be numbers");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::LessEqual: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        push(
                            Value::makeBoolean(a.getNumber() <= b.getNumber()));
                    } else {
                        runtimeError("Operands must be numbers");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::Greater: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        push(Value::makeBoolean(a.getNumber() > b.getNumber()));
                    } else {
                        runtimeError("Operands must be numbers");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::GreaterEqual: {
                    Value b = pop();
                    Value a = pop();

                    if (a.getType() == Value::Type::Number &&
                        b.getType() == Value::Type::Number) {
                        push(
                            Value::makeBoolean(a.getNumber() >= b.getNumber()));
                    } else {
                        runtimeError("Operands must be numbers");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::And: {
                    Value b = pop();
                    Value a = pop();
                    push(Value::makeBoolean(a.isTruthy() && b.isTruthy()));
                    break;
                }

                case OpCode::Or: {
                    Value b = pop();
                    Value a = pop();
                    push(Value::makeBoolean(a.isTruthy() || b.isTruthy()));
                    break;
                }

                case OpCode::GetLocal: {
                    uint8_t slot = readByte();
                    // 创建一个新的值副本并移动它
                    Value temp = Value::makeNull();
                    // 使用工厂方法创建适当类型的值
                    const Value& slotValue = stack[stackTop + slot];
                    switch (slotValue.getType()) {
                        case Value::Type::Null:
                            temp = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            temp = Value::makeBoolean(slotValue.getBoolean());
                            break;
                        case Value::Type::Number:
                            temp = Value::makeNumber(slotValue.getNumber());
                            break;
                        case Value::Type::String:
                            temp = Value::makeString(slotValue.getString());
                            break;
                        default:
                            temp = Value::makeObject(slotValue.getObject());
                            break;
                    }
                    push(std::move(temp));
                    break;
                }

                case OpCode::SetLocal: {
                    uint8_t slot = readByte();
                    // 创建新的值，而不是使用赋值
                    const Value& peekValue = peek(0);
                    Value newValue = Value::makeNull();
                    switch (peekValue.getType()) {
                        case Value::Type::Null:
                            newValue = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            newValue =
                                Value::makeBoolean(peekValue.getBoolean());
                            break;
                        case Value::Type::Number:
                            newValue = Value::makeNumber(peekValue.getNumber());
                            break;
                        case Value::Type::String:
                            newValue = Value::makeString(peekValue.getString());
                            break;
                        default:
                            newValue = Value::makeObject(peekValue.getObject());
                            break;
                    }
                    stack[stackTop + slot] = std::move(newValue);
                    break;
                }

                case OpCode::GetGlobal: {
                    uint8_t index = readByte();
                    // 创建一个新的值副本
                    const Value& globalValue = globals[index];
                    Value temp = Value::makeNull();
                    switch (globalValue.getType()) {
                        case Value::Type::Null:
                            temp = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            temp = Value::makeBoolean(globalValue.getBoolean());
                            break;
                        case Value::Type::Number:
                            temp = Value::makeNumber(globalValue.getNumber());
                            break;
                        case Value::Type::String:
                            temp = Value::makeString(globalValue.getString());
                            break;
                        default:
                            temp = Value::makeObject(globalValue.getObject());
                            break;
                    }
                    push(std::move(temp));
                    break;
                }

                case OpCode::SetGlobal: {
                    uint8_t index = readByte();
                    // 创建新的值
                    const Value& peekValue = peek(0);
                    Value newValue = Value::makeNull();
                    switch (peekValue.getType()) {
                        case Value::Type::Null:
                            newValue = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            newValue =
                                Value::makeBoolean(peekValue.getBoolean());
                            break;
                        case Value::Type::Number:
                            newValue = Value::makeNumber(peekValue.getNumber());
                            break;
                        case Value::Type::String:
                            newValue = Value::makeString(peekValue.getString());
                            break;
                        default:
                            newValue = Value::makeObject(peekValue.getObject());
                            break;
                    }
                    globals[index] = std::move(newValue);
                    break;
                }

                case OpCode::GetField: {
                    Value instance = pop();
                    uint8_t fieldIndex = readByte();
                    std::string fieldName = currentClosure->getFunction()
                                                ->getConstants()[fieldIndex]
                                                .getString();

                    if (instance.getType() != Value::Type::Instance) {
                        runtimeError("Only instances have fields");
                        *result = Value::makeNull();
                        return;
                    }

                    InstanceObject* instanceObj =
                        static_cast<InstanceObject*>(instance.getObject());
                    const Value* field = instanceObj->getField(fieldName);

                    if (!field) {
                        // Try to get a method from the class
                        const Value& methodValue =
                            instanceObj->getClass()->getMethod(fieldName);

                        if (methodValue.getType() == Value::Type::Null) {
                            runtimeError("Undefined property '" + fieldName +
                                         "'");
                            *result = Value::makeNull();
                            return;
                        }

                        // 创建新值而不是拷贝
                        Value methodCopy = Value::makeNull();
                        if (methodValue.getType() == Value::Type::Object) {
                            methodCopy =
                                Value::makeObject(methodValue.getObject());
                        }
                        push(std::move(methodCopy));
                    } else {
                        // 创建新值而不是拷贝
                        Value fieldCopy = Value::makeNull();
                        switch (field->getType()) {
                            case Value::Type::Null:
                                fieldCopy = Value::makeNull();
                                break;
                            case Value::Type::Boolean:
                                fieldCopy =
                                    Value::makeBoolean(field->getBoolean());
                                break;
                            case Value::Type::Number:
                                fieldCopy =
                                    Value::makeNumber(field->getNumber());
                                break;
                            case Value::Type::String:
                                fieldCopy =
                                    Value::makeString(field->getString());
                                break;
                            default:
                                fieldCopy =
                                    Value::makeObject(field->getObject());
                                break;
                        }
                        push(std::move(fieldCopy));
                    }
                    break;
                }

                case OpCode::SetField: {
                    Value value = pop();
                    Value instance = pop();
                    uint8_t fieldIndex = readByte();
                    std::string fieldName = currentClosure->getFunction()
                                                ->getConstants()[fieldIndex]
                                                .getString();

                    if (instance.getType() != Value::Type::Instance) {
                        runtimeError("Only instances have fields");
                        *result = Value::makeNull();
                        return;
                    }

                    InstanceObject* instanceObj =
                        static_cast<InstanceObject*>(instance.getObject());

                    // 创建新值而不是移动
                    Value fieldValue = Value::makeNull();
                    switch (value.getType()) {
                        case Value::Type::Null:
                            fieldValue = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            fieldValue = Value::makeBoolean(value.getBoolean());
                            break;
                        case Value::Type::Number:
                            fieldValue = Value::makeNumber(value.getNumber());
                            break;
                        case Value::Type::String:
                            fieldValue = Value::makeString(value.getString());
                            break;
                        default:
                            fieldValue = Value::makeObject(value.getObject());
                            break;
                    }

                    instanceObj->setField(fieldName, std::move(fieldValue));
                    push(std::move(value));  // 推回值
                    break;
                }

                case OpCode::GetIndex: {
                    Value index = pop();
                    Value collection = pop();

                    if (collection.getType() == Value::Type::Array) {
                        ArrayObject* array =
                            static_cast<ArrayObject*>(collection.getObject());

                        if (index.getType() != Value::Type::Number) {
                            runtimeError("Array index must be a number");
                            *result = Value::makeNull();
                            return;
                        }

                        int i = static_cast<int>(index.getNumber());
                        if (i < 0 || i >= static_cast<int>(array->size())) {
                            runtimeError("Array index out of bounds");
                            *result = Value::makeNull();
                            return;
                        }

                        const Value& arrayValue = array->get(i);
                        // 创建副本而不是直接使用引用
                        Value arrayCopy = Value::makeNull();
                        switch (arrayValue.getType()) {
                            case Value::Type::Null:
                                arrayCopy = Value::makeNull();
                                break;
                            case Value::Type::Boolean:
                                arrayCopy =
                                    Value::makeBoolean(arrayValue.getBoolean());
                                break;
                            case Value::Type::Number:
                                arrayCopy =
                                    Value::makeNumber(arrayValue.getNumber());
                                break;
                            case Value::Type::String:
                                arrayCopy =
                                    Value::makeString(arrayValue.getString());
                                break;
                            default:
                                arrayCopy =
                                    Value::makeObject(arrayValue.getObject());
                                break;
                        }
                        push(std::move(arrayCopy));
                    } else {
                        runtimeError("Only arrays can be indexed");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::SetIndex: {
                    Value value = pop();
                    Value index = pop();
                    Value collection = pop();

                    if (collection.getType() == Value::Type::Array) {
                        ArrayObject* array =
                            static_cast<ArrayObject*>(collection.getObject());

                        if (index.getType() != Value::Type::Number) {
                            runtimeError("Array index must be a number");
                            *result = Value::makeNull();
                            return;
                        }

                        int i = static_cast<int>(index.getNumber());
                        if (i < 0) {
                            runtimeError("Array index out of bounds");
                            *result = Value::makeNull();
                            return;
                        }

                        // Resize array if needed
                        if (i >= static_cast<int>(array->size())) {
                            array->getElementsMutable().resize(i + 1);
                        }

                        // 创建新值而不是移动原始值
                        Value indexValue = Value::makeNull();
                        switch (value.getType()) {
                            case Value::Type::Null:
                                indexValue = Value::makeNull();
                                break;
                            case Value::Type::Boolean:
                                indexValue =
                                    Value::makeBoolean(value.getBoolean());
                                break;
                            case Value::Type::Number:
                                indexValue =
                                    Value::makeNumber(value.getNumber());
                                break;
                            case Value::Type::String:
                                indexValue =
                                    Value::makeString(value.getString());
                                break;
                            default:
                                indexValue =
                                    Value::makeObject(value.getObject());
                                break;
                        }

                        array->set(i, std::move(indexValue));
                        push(std::move(value));
                    } else {
                        runtimeError("Only arrays can be indexed");
                        *result = Value::makeNull();
                        return;
                    }
                    break;
                }

                case OpCode::Array: {
                    uint8_t count = readByte();
                    std::vector<Value> elements;

                    for (int i = count - 1; i >= 0; i--) {
                        const Value& stackVal = stack[stack.size() - 1 - i];
                        // 创建新值而不是移动
                        Value elemValue = Value::makeNull();
                        switch (stackVal.getType()) {
                            case Value::Type::Null:
                                elemValue = Value::makeNull();
                                break;
                            case Value::Type::Boolean:
                                elemValue =
                                    Value::makeBoolean(stackVal.getBoolean());
                                break;
                            case Value::Type::Number:
                                elemValue =
                                    Value::makeNumber(stackVal.getNumber());
                                break;
                            case Value::Type::String:
                                elemValue =
                                    Value::makeString(stackVal.getString());
                                break;
                            default:
                                elemValue =
                                    Value::makeObject(stackVal.getObject());
                                break;
                        }
                        elements.push_back(std::move(elemValue));
                    }

                    stack.resize(stack.size() - count);

                    // Reverse because we popped in reverse
                    std::reverse(elements.begin(), elements.end());
                    push(Value::makeObject(
                        new ArrayObject(std::move(elements))));
                    break;
                }

                case OpCode::Object: {
                    uint8_t count = readByte();
                    auto instance = new InstanceObject(
                        nullptr);  // Raw object without a class

                    for (int i = 0; i < count; i++) {
                        Value value = pop();
                        Value key = pop();

                        if (key.getType() != Value::Type::String) {
                            runtimeError("Object keys must be strings");
                            *result = Value::makeNull();
                            return;
                        }

                        // 创建新值而不是移动
                        Value fieldValue = Value::makeNull();
                        switch (value.getType()) {
                            case Value::Type::Null:
                                fieldValue = Value::makeNull();
                                break;
                            case Value::Type::Boolean:
                                fieldValue =
                                    Value::makeBoolean(value.getBoolean());
                                break;
                            case Value::Type::Number:
                                fieldValue =
                                    Value::makeNumber(value.getNumber());
                                break;
                            case Value::Type::String:
                                fieldValue =
                                    Value::makeString(value.getString());
                                break;
                            default:
                                fieldValue =
                                    Value::makeObject(value.getObject());
                                break;
                        }

                        instance->setField(key.getString(),
                                           std::move(fieldValue));
                    }

                    push(Value::makeObject(instance));
                    break;
                }

                case OpCode::Call: {
                    uint8_t argCount = readByte();
                    // 不要拷贝值，而是创建一个新值
                    const Value& calleeRef = peek(argCount);
                    // 创建一个新的值用于调用
                    Value calleeValue = Value::makeNull();
                    switch (calleeRef.getType()) {
                        case Value::Type::Null:
                            calleeValue = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            calleeValue = Value::makeBoolean(calleeRef.getBoolean());
                            break;
                        case Value::Type::Number:
                            calleeValue = Value::makeNumber(calleeRef.getNumber());
                            break;
                        case Value::Type::String:
                            calleeValue = Value::makeString(calleeRef.getString());
                            break;
                        default:
                            calleeValue = Value::makeObject(calleeRef.getObject());
                            break;
                    }
                    callValue(std::move(calleeValue), argCount, result);
                    break;
                }

                case OpCode::Return: {
                    Value returnValue = pop();

                    // Close any upvalues created in this function
                    closeUpvalues(&stack[stackTop]);

                    // Restore stack
                    stack.resize(stackTop);

                    // Move the result to the result parameter
                    *result = std::move(returnValue);
                    return;
                }

                case OpCode::Jump: {
                    uint16_t offset = readShort();
                    ip += offset;
                    break;
                }

                case OpCode::JumpIfFalse: {
                    uint16_t offset = readShort();
                    if (!peek(0).isTruthy()) {
                        ip += offset;
                    }
                    break;
                }

                case OpCode::JumpIfTrue: {
                    uint16_t offset = readShort();
                    if (peek(0).isTruthy()) {
                        ip += offset;
                    }
                    break;
                }

                case OpCode::Pop: {
                    pop();
                    break;
                }

                case OpCode::Dup: {
                    const Value& topValue = peek(0);
                    // 创建新的副本而不是拷贝
                    Value dupValue = Value::makeNull();
                    switch (topValue.getType()) {
                        case Value::Type::Null:
                            dupValue = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            dupValue =
                                Value::makeBoolean(topValue.getBoolean());
                            break;
                        case Value::Type::Number:
                            dupValue = Value::makeNumber(topValue.getNumber());
                            break;
                        case Value::Type::String:
                            dupValue = Value::makeString(topValue.getString());
                            break;
                        default:
                            dupValue = Value::makeObject(topValue.getObject());
                            break;
                    }
                    push(std::move(dupValue));
                    break;
                }

                case OpCode::Closure: {
                    const Value& constValue = readConstant();
                    if (constValue.getType() != Value::Type::Function) {
                        runtimeError("Expected a function");
                        *result = Value::makeNull();
                        return;
                    }

                    Function* function =
                        static_cast<Function*>(constValue.getObject());
                    auto closure = new Closure(function);

                    // Capture upvalues
                    for (uint8_t i = 0; i < function->getNumUpvalues(); i++) {
                        uint8_t isLocal = readByte();
                        uint8_t index = readByte();

                        if (isLocal) {
                            // Capture a local variable from the enclosing
                            // function
                            closure->getUpvalues()[i] =
                                captureUpvalue(&stack[stackTop + index]);
                        } else {
                            // Use an upvalue from the enclosing function
                            closure->getUpvalues()[i] =
                                currentClosure->getUpvalues()[index];
                        }
                    }

                    push(Value::makeObject(closure));
                    break;
                }

                case OpCode::GetUpvalue: {
                    uint8_t slot = readByte();
                    Upvalue* upvalue = currentClosure->getUpvalues()[slot];

                    // 创建新值而不是拷贝
                    const Value& upValue = upvalue->isClosed()
                                               ? upvalue->getClosed()
                                               : *upvalue->getLocation();

                    Value upvalueCopy = Value::makeNull();
                    switch (upValue.getType()) {
                        case Value::Type::Null:
                            upvalueCopy = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            upvalueCopy =
                                Value::makeBoolean(upValue.getBoolean());
                            break;
                        case Value::Type::Number:
                            upvalueCopy =
                                Value::makeNumber(upValue.getNumber());
                            break;
                        case Value::Type::String:
                            upvalueCopy =
                                Value::makeString(upValue.getString());
                            break;
                        default:
                            upvalueCopy =
                                Value::makeObject(upValue.getObject());
                            break;
                    }
                    push(std::move(upvalueCopy));
                    break;
                }

                case OpCode::SetUpvalue: {
                    uint8_t slot = readByte();
                    Upvalue* upvalue = currentClosure->getUpvalues()[slot];

                    const Value& peekValue = peek(0);
                    Value newValue = Value::makeNull();
                    switch (peekValue.getType()) {
                        case Value::Type::Null:
                            newValue = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            newValue =
                                Value::makeBoolean(peekValue.getBoolean());
                            break;
                        case Value::Type::Number:
                            newValue = Value::makeNumber(peekValue.getNumber());
                            break;
                        case Value::Type::String:
                            newValue = Value::makeString(peekValue.getString());
                            break;
                        default:
                            newValue = Value::makeObject(peekValue.getObject());
                            break;
                    }

                    if (upvalue->isClosed()) {
                        upvalue->setClosed(std::move(newValue));
                    } else {
                        *upvalue->getLocation() = std::move(newValue);
                    }
                    break;
                }

                case OpCode::CloseUpvalue: {
                    closeUpvalues(&stack.back());
                    pop();
                    break;
                }

                case OpCode::CreateClass: {
                    // 直接使用readConstant()返回的值，不需要std::move
                    Value className = readConstant();
                    auto superclass = peek(0).getType() == Value::Type::Class
                        ? static_cast<ClassObject*>(pop().getObject())
                        : nullptr;

                    auto classObj = new ClassObject(className.getString(), superclass);
                    push(Value::makeObject(classObj));
                    break;
                }

                case OpCode::GetSuper: {
                    uint8_t methodIndex = readByte();
                    std::string methodName = currentClosure->getFunction()
                                                ->getConstants()[methodIndex]
                                                .getString();

                    ClassObject* superclass = static_cast<ClassObject*>(pop().getObject());
                    // 获取方法并创建新值
                    const Value& methodRef = superclass->getMethod(methodName);
                    
                    if (methodRef.getType() == Value::Type::Null) {
                        runtimeError("Undefined method '" + methodName + "' in superclass");
                        *result = Value::makeNull();
                        return;
                    }

                    // 创建新值而不是拷贝
                    Value methodValue = Value::makeNull();
                    switch (methodRef.getType()) {
                        case Value::Type::Null:
                            methodValue = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            methodValue = Value::makeBoolean(methodRef.getBoolean());
                            break;
                        case Value::Type::Number:
                            methodValue = Value::makeNumber(methodRef.getNumber());
                            break;
                        case Value::Type::String:
                            methodValue = Value::makeString(methodRef.getString());
                            break;
                        default:
                            methodValue = Value::makeObject(methodRef.getObject());
                            break;
                    }
                    
                    // 使用移动语义
                    push(std::move(methodValue));
                    break;
                }

                case OpCode::Inherit: {
                    // 使用引用而不是拷贝
                    const Value& superclassRef = peek(1);

                    if (superclassRef.getType() != Value::Type::Class) {
                        runtimeError("Superclass must be a class");
                        *result = Value::makeNull();
                        return;
                    }

                    ClassObject* subclass = static_cast<ClassObject*>(peek(0).getObject());
                    ClassObject* superclassObj = static_cast<ClassObject*>(superclassRef.getObject());
                    // In a real implementation, this would iterate through
                    // methods and copy them to the subclass

                    pop();  // Subclass
                    break;
                }

                case OpCode::Method: {
                    uint8_t methodIndex = readByte();
                    std::string methodName = currentClosure->getFunction()
                                                ->getConstants()[methodIndex]
                                                .getString();
                    
                    // 使用引用获取值，然后创建新值
                    const Value& peekRef = peek(0);
                    Value methodValue = Value::makeNull();
                    switch (peekRef.getType()) {
                        case Value::Type::Null:
                            methodValue = Value::makeNull();
                            break;
                        case Value::Type::Boolean:
                            methodValue = Value::makeBoolean(peekRef.getBoolean());
                            break;
                        case Value::Type::Number:
                            methodValue = Value::makeNumber(peekRef.getNumber());
                            break;
                        case Value::Type::String:
                            methodValue = Value::makeString(peekRef.getString());
                            break;
                        default:
                            methodValue = Value::makeObject(peekRef.getObject());
                            break;
                    }

                    auto classObj = static_cast<ClassObject*>(peek(1).getObject());
                    classObj->defineMethod(methodName, std::move(methodValue));

                    pop();  // Method
                    break;
                }
            }
        }
    } catch (const std::runtime_error& e) {
        // Handle runtime errors
        std::cerr << e.what() << std::endl;
        *result = Value::makeNull();
    }
}

// 修改返回类型和签名
void VirtualMachine::executeModule(const std::string& source [[maybe_unused]], Value* result) {
    // This would compile the source code to bytecode and then execute it
    // For simplicity, we're not implementing this here
    *result = Value::makeNull();
}

void VirtualMachine::defineNative(const std::string& name,
                                  NativeFunction::NativeFn function,
                                  uint8_t arity) {
    auto nativeFn = new NativeFunction(name, function, arity);
    globals.push_back(Value(nativeFn));
}

// 修改返回类型为引用
const Value& VirtualMachine::peek(int offset) const {
    return stack[stack.size() - 1 - offset];
}

// 修改参数类型为右值引用
void VirtualMachine::push(Value&& value) { stack.push_back(std::move(value)); }

// 使用移动语义
Value VirtualMachine::pop() {
    Value value = std::move(stack.back());
    stack.pop_back();
    return value;
}

uint8_t VirtualMachine::readByte() { return *ip++; }

uint16_t VirtualMachine::readShort() {
    uint16_t value = (ip[0] << 8) | ip[1];
    ip += 2;
    return value;
}

// 使用移动语义返回值
Value VirtualMachine::readConstant() {
    uint8_t index = readByte();
    const Value& constRef = currentClosure->getFunction()->getConstants()[index];
    
    // 创建一个新值而不是尝试直接移动或复制
    Value result = Value::makeNull();
    switch (constRef.getType()) {
        case Value::Type::Null:
            result = Value::makeNull();
            break;
        case Value::Type::Boolean:
            result = Value::makeBoolean(constRef.getBoolean());
            break; 
        case Value::Type::Number:
            result = Value::makeNumber(constRef.getNumber());
            break;
        case Value::Type::String:
            result = Value::makeString(constRef.getString());
            break;
        default:
            result = Value::makeObject(constRef.getObject());
            break;
    }
    
    return result;
}

// 修改参数为右值引用
void VirtualMachine::callValue(Value&& callee, int argCount, Value* result) {
    if (callee.getType() == Value::Type::Closure) {
        call(static_cast<Closure*>(callee.getObject()), argCount, result);
    } else if (callee.getType() == Value::Type::NativeFunction) {
        auto* native = static_cast<NativeFunction*>(callee.getObject());

        if (argCount != native->getArity()) {
            runtimeError("Expected " + std::to_string(native->getArity()) +
                         " arguments but got " + std::to_string(argCount));
            *result = Value::makeNull();
            return;
        }

        std::vector<Value> args;
        args.reserve(argCount);
        for (int i = argCount - 1; i >= 0; i--) {
            // 创建新值而不是移动栈上的值
            const Value& argRef = stack[stack.size() - 1 - i];
            Value argValue = Value::makeNull();
            switch (argRef.getType()) {
                case Value::Type::Null:
                    argValue = Value::makeNull();
                    break;
                case Value::Type::Boolean:
                    argValue = Value::makeBoolean(argRef.getBoolean());
                    break;
                case Value::Type::Number:
                    argValue = Value::makeNumber(argRef.getNumber());
                    break;
                case Value::Type::String:
                    argValue = Value::makeString(argRef.getString());
                    break;
                default:
                    argValue = Value::makeObject(argRef.getObject());
                    break;
            }
            args.push_back(std::move(argValue));
        }

        stack.resize(stack.size() - argCount);
        native->call(args, result);

    } else if (callee.getType() == Value::Type::Class) {
        auto* classObj = static_cast<ClassObject*>(callee.getObject());
        auto* instance = new InstanceObject(classObj);

        stack[stack.size() - 1 - argCount] = Value::makeObject(instance);

        // 获取初始化方法并创建新值
        const Value& initRef = classObj->getMethod("constructor");
        if (initRef.getType() != Value::Type::Null) {
            Value initValue = Value::makeNull();
            switch (initRef.getType()) {
                case Value::Type::Null:
                    initValue = Value::makeNull();
                    break;
                case Value::Type::Boolean:
                    initValue = Value::makeBoolean(initRef.getBoolean());
                    break;
                case Value::Type::Number:
                    initValue = Value::makeNumber(initRef.getNumber());
                    break;
                case Value::Type::String:
                    initValue = Value::makeString(initRef.getString());
                    break;
                default:
                    initValue = Value::makeObject(initRef.getObject());
                    break;
            }
            callValue(std::move(initValue), argCount, result);
        } else if (argCount > 0) {
            runtimeError("Expected 0 arguments but got " +
                        std::to_string(argCount));
            *result = Value::makeNull();
        }
    } else {
        runtimeError("Can only call functions, classes, and objects");
        *result = Value::makeNull();
    }
}

void VirtualMachine::call(Closure* closure, int argCount, Value* result) {
    Function* function = closure->getFunction();

    if (argCount != function->getNumParameters()) {
        runtimeError("Expected " +
                     std::to_string(function->getNumParameters()) +
                     " arguments but got " + std::to_string(argCount));
        *result = Value::makeNull();
        return;
    }

    // Save current closure and IP
    Closure* oldClosure = currentClosure;
    uint8_t* oldIP = ip;

    // Set up new call frame
    currentClosure = closure;
    ip = const_cast<uint8_t*>(function->getBytecode().data());

    // Execute the function
    execute(function, {}, result);

    // Restore previous call frame
    currentClosure = oldClosure;
    ip = oldIP;
}

// 修复 closeUpvalues 中的移动语义问题:
void VirtualMachine::closeUpvalues(Value* last) {
    while (!openUpvalues.empty() &&
           openUpvalues.back()->getLocation() >= last) {
        Upvalue* upvalue = openUpvalues.back();
        openUpvalues.pop_back();

        Value temp = std::move(*upvalue->getLocation());
        upvalue->setClosed(std::move(temp));
    }
}

Upvalue* VirtualMachine::captureUpvalue(Value* local) {
    // Check if upvalue already exists
    for (auto it = openUpvalues.rbegin(); it != openUpvalues.rend(); ++it) {
        if ((*it)->getLocation() == local) {
            return *it;
        }

        if ((*it)->getLocation() < local) {
            break;
        }
    }

    // Create new upvalue
    Upvalue* upvalue = new Upvalue(local);
    openUpvalues.push_back(upvalue);
    return upvalue;
}

// 删除未使用的变量
void VirtualMachine::runtimeError(const std::string& message) {
    std::cerr << "Runtime error: " << message << std::endl;

    // Print stack trace
    while (currentClosure != nullptr) {
        Function* function = currentClosure->getFunction();

        std::cerr << "[line " << "unknown" << "] in "
                  << (function->getName().empty() ? "script"
                                                  : function->getName())
                  << std::endl;

        // TODO: Add line information for better error reporting
    }

    // Reset VM state
    stack.clear();
    currentClosure = nullptr;
    ip = nullptr;

    throw std::runtime_error(message);
}

void VirtualMachine::dumpStack() const {
    std::cout << "          ";
    for (const auto& value : stack) {
        std::cout << "[ " << value.toString() << " ]";
    }
    std::cout << std::endl;
}

std::string VirtualMachine::disassembleInstruction(const Function* function,
                                                   size_t offset) {
    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << offset << " ";

    if (offset > 0 && offset == function->getBytecode()[offset - 1]) {
        ss << "   | ";
    } else {
        ss << "   ";
    }

    uint8_t instruction = function->getBytecode()[offset];
    switch (static_cast<OpCode>(instruction)) {
        case OpCode::Constant:
            ss << "CONSTANT "
               << static_cast<int>(function->getBytecode()[offset + 1]) << " '"
               << function->getConstants()[function->getBytecode()[offset + 1]]
                      .toString()
               << "'";
            return ss.str();

        case OpCode::Add:
            return ss.str() + "ADD";
        case OpCode::Subtract:
            return ss.str() + "SUBTRACT";
        case OpCode::Multiply:
            return ss.str() + "MULTIPLY";
        case OpCode::Divide:
            return ss.str() + "DIVIDE";
        case OpCode::Modulo:
            return ss.str() + "MODULO";
        case OpCode::Negate:
            return ss.str() + "NEGATE";
        case OpCode::Not:
            return ss.str() + "NOT";
        case OpCode::Equal:
            return ss.str() + "EQUAL";
        case OpCode::NotEqual:
            return ss.str() + "NOT_EQUAL";
        case OpCode::Less:
            return ss.str() + "LESS";
        case OpCode::LessEqual:
            return ss.str() + "LESS_EQUAL";
        case OpCode::Greater:
            return ss.str() + "GREATER";
        case OpCode::GreaterEqual:
            return ss.str() + "GREATER_EQUAL";
        case OpCode::And:
            return ss.str() + "AND";
        case OpCode::Or:
            return ss.str() + "OR";

        case OpCode::GetLocal:
            ss << "GET_LOCAL "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::SetLocal:
            ss << "SET_LOCAL "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::GetGlobal:
            ss << "GET_GLOBAL "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::SetGlobal:
            ss << "SET_GLOBAL "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::GetField:
            ss << "GET_FIELD "
               << static_cast<int>(function->getBytecode()[offset + 1]) << " '"
               << function->getConstants()[function->getBytecode()[offset + 1]]
                      .toString()
               << "'";
            return ss.str();

        case OpCode::SetField:
            ss << "SET_FIELD "
               << static_cast<int>(function->getBytecode()[offset + 1]) << " '"
               << function->getConstants()[function->getBytecode()[offset + 1]]
                      .toString()
               << "'";
            return ss.str();

        case OpCode::GetIndex:
            return ss.str() + "GET_INDEX";

        case OpCode::SetIndex:
            return ss.str() + "SET_INDEX";

        case OpCode::Array:
            ss << "ARRAY "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::Object:
            ss << "OBJECT "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::Call:
            ss << "CALL "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::Return:
            return ss.str() + "RETURN";

        case OpCode::Jump:
            ss << "JUMP "
               << ((function->getBytecode()[offset + 1] << 8) |
                   function->getBytecode()[offset + 2]);
            return ss.str();

        case OpCode::JumpIfFalse:
            ss << "JUMP_IF_FALSE "
               << ((function->getBytecode()[offset + 1] << 8) |
                   function->getBytecode()[offset + 2]);
            return ss.str();

        case OpCode::JumpIfTrue:
            ss << "JUMP_IF_TRUE "
               << ((function->getBytecode()[offset + 1] << 8) |
                   function->getBytecode()[offset + 2]);
            return ss.str();

        case OpCode::Pop:
            return ss.str() + "POP";

        case OpCode::Dup:
            return ss.str() + "DUP";

        case OpCode::Closure: {
            offset++;
            uint8_t constant = function->getBytecode()[offset++];
            ss << "CLOSURE " << static_cast<int>(constant) << " "
               << function->getConstants()[constant].toString();

            Function* fn = static_cast<Function*>(
                function->getConstants()[constant].getObject());
            for (int i = 0; i < fn->getNumUpvalues(); i++) {
                uint8_t isLocal = function->getBytecode()[offset++];
                uint8_t index = function->getBytecode()[offset++];
                ss << "\n"
                   << std::setw(4) << std::setfill(' ') << "" << "   |-- "
                   << (isLocal ? "local" : "upvalue") << " "
                   << static_cast<int>(index);
            }

            return ss.str();
        }

        case OpCode::GetUpvalue:
            ss << "GET_UPVALUE "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::SetUpvalue:
            ss << "SET_UPVALUE "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::CloseUpvalue:
            return ss.str() + "CLOSE_UPVALUE";

        case OpCode::CreateClass:
            ss << "CREATE_CLASS "
               << static_cast<int>(function->getBytecode()[offset + 1]);
            return ss.str();

        case OpCode::GetSuper:
            ss << "GET_SUPER "
               << static_cast<int>(function->getBytecode()[offset + 1]) << " '"
               << function->getConstants()[function->getBytecode()[offset + 1]]
                      .toString()
               << "'";
            return ss.str();

        case OpCode::Inherit:
            return ss.str() + "INHERIT";

        case OpCode::Method:
            ss << "METHOD "
               << static_cast<int>(function->getBytecode()[offset + 1]) << " '"
               << function->getConstants()[function->getBytecode()[offset + 1]]
                      .toString()
               << "'";
            return ss.str();
    }

    ss << "UNKNOWN OPCODE " << static_cast<int>(instruction);
    return ss.str();
}

}  // namespace tsx