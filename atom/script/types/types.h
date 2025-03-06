#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tsx {

// Forward declarations
class TypeChecker;
class Value;

// Base type class
class Type {
public:
    virtual ~Type() = default;
    virtual std::string toString() const = 0;
    virtual bool isAssignableTo(const Type* other) const = 0;
    virtual bool equals(const Type* other) const = 0;
    virtual std::unique_ptr<Type> clone() const = 0;  // 添加克隆接口

    // Factory methods for common types
    static std::unique_ptr<Type> createNumber();
    static std::unique_ptr<Type> createString();
    static std::unique_ptr<Type> createBoolean();
    static std::unique_ptr<Type> createNull();
    static std::unique_ptr<Type> createUndefined();
    static std::unique_ptr<Type> createAny();
    static std::unique_ptr<Type> createNever();
    static std::unique_ptr<Type> createUnknown();

    // 新增: 创建类型交集
    static std::unique_ptr<Type> createIntersection(
        std::vector<std::unique_ptr<Type>> types);
};

// Primitive types
class PrimitiveType : public Type {
public:
    enum class Kind {
        Number,
        String,
        Boolean,
        Null,
        Undefined,
        Any,
        Never,
        Unknown
    };

private:
    Kind kind;

public:
    explicit PrimitiveType(Kind kind) : kind(kind) {}

    std::string toString() const override {
        switch (kind) {
            case Kind::Number:
                return "number";
            case Kind::String:
                return "string";
            case Kind::Boolean:
                return "boolean";
            case Kind::Null:
                return "null";
            case Kind::Undefined:
                return "undefined";
            case Kind::Any:
                return "any";
            case Kind::Never:
                return "never";
            case Kind::Unknown:
                return "unknown";
        }
        return "unknown";  // To satisfy compiler
    }

    bool isAssignableTo(const Type* other) const override {
        // Any type can be assigned to any
        if (auto otherPrim = dynamic_cast<const PrimitiveType*>(other)) {
            if (otherPrim->kind == Kind::Any)
                return true;
            // Never is assignable to any type
            if (kind == Kind::Never)
                return true;
            // Same types are assignable to each other
            return kind == otherPrim->kind;
        }
        return false;
    }

    bool equals(const Type* other) const override {
        if (auto otherPrim = dynamic_cast<const PrimitiveType*>(other)) {
            return kind == otherPrim->kind;
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        return std::make_unique<PrimitiveType>(kind);
    }

    Kind getKind() const { return kind; }
};

// Object types
class ObjectType : public Type {
private:
    std::unordered_map<std::string, std::unique_ptr<Type>> properties;
    std::optional<std::unique_ptr<Type>> indexSignature;
    bool isInterface = false;

public:
    void addProperty(const std::string& name, std::unique_ptr<Type> type) {
        properties[name] = std::move(type);
    }

    void setIndexSignature(std::unique_ptr<Type> type) {
        indexSignature = std::move(type);
    }

    void setIsInterface(bool isIntf) { isInterface = isIntf; }

    bool hasProperty(const std::string& name) const {
        return properties.find(name) != properties.end();
    }

    const Type* getPropertyType(const std::string& name) const {
        auto it = properties.find(name);
        return it != properties.end() ? it->second.get() : nullptr;
    }

    std::string toString() const override {
        std::string result = "{";
        bool first = true;

        for (const auto& [name, type] : properties) {
            if (!first)
                result += ", ";
            result += name + ": " + type->toString();
            first = false;
        }

        if (indexSignature) {
            if (!first)
                result += ", ";
            result += "[index: string]: " + (*indexSignature)->toString();
        }

        return result + "}";
    }

    bool isAssignableTo(const Type* other) const override {
        if (dynamic_cast<const PrimitiveType*>(other)) {
            auto primOther = dynamic_cast<const PrimitiveType*>(other);
            return primOther->getKind() == PrimitiveType::Kind::Any;
        }

        if (auto otherObj = dynamic_cast<const ObjectType*>(other)) {
            // Check if this object has at least all properties of the other
            // object
            for (const auto& [name, otherType] : otherObj->properties) {
                auto it = properties.find(name);
                if (it == properties.end())
                    return false;

                if (!it->second->isAssignableTo(otherType.get())) {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    bool equals(const Type* other) const override {
        if (auto otherObj = dynamic_cast<const ObjectType*>(other)) {
            if (properties.size() != otherObj->properties.size())
                return false;

            for (const auto& [name, type] : properties) {
                auto it = otherObj->properties.find(name);
                if (it == otherObj->properties.end())
                    return false;
                if (!type->equals(it->second.get()))
                    return false;
            }

            return true;
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        auto cloned = std::make_unique<ObjectType>();

        // 克隆所有属性
        for (const auto& [name, type] : properties) {
            cloned->addProperty(name, type->clone());
        }

        // 克隆索引签名
        if (indexSignature) {
            cloned->setIndexSignature((*indexSignature)->clone());
        }

        cloned->setIsInterface(isInterface);
        return cloned;
    }

    auto getProperties() const { return properties; }
};

// Function types
class FunctionType : public Type {
private:
    std::vector<std::unique_ptr<Type>> paramTypes;
    std::unique_ptr<Type> returnType;

public:
    FunctionType(std::vector<std::unique_ptr<Type>> params,
                 std::unique_ptr<Type> ret)
        : paramTypes(std::move(params)), returnType(std::move(ret)) {}

    std::string toString() const override {
        std::string result = "(";
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += paramTypes[i]->toString();
        }
        return result + ") => " + returnType->toString();
    }

    bool isAssignableTo(const Type* other) const override {
        if (auto otherFn = dynamic_cast<const FunctionType*>(other)) {
            // Functions are contravariant in parameter types and covariant in
            // return type
            if (paramTypes.size() != otherFn->paramTypes.size())
                return false;

            for (size_t i = 0; i < paramTypes.size(); ++i) {
                // Contravariant: other parameter must be assignable to this
                // parameter
                if (!otherFn->paramTypes[i]->isAssignableTo(
                        paramTypes[i].get())) {
                    return false;
                }
            }

            // Covariant: this return type must be assignable to other return
            // type
            return returnType->isAssignableTo(otherFn->returnType.get());
        }

        // Check if other is any
        if (auto primOther = dynamic_cast<const PrimitiveType*>(other)) {
            return primOther->getKind() == PrimitiveType::Kind::Any;
        }

        return false;
    }

    bool equals(const Type* other) const override {
        if (auto otherFn = dynamic_cast<const FunctionType*>(other)) {
            if (paramTypes.size() != otherFn->paramTypes.size())
                return false;

            for (size_t i = 0; i < paramTypes.size(); ++i) {
                if (!paramTypes[i]->equals(otherFn->paramTypes[i].get())) {
                    return false;
                }
            }

            return returnType->equals(otherFn->returnType.get());
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        std::vector<std::unique_ptr<Type>> clonedParams;
        for (const auto& param : paramTypes) {
            clonedParams.push_back(param->clone());
        }

        return std::make_unique<FunctionType>(std::move(clonedParams),
                                              returnType->clone());
    }

    const Type* getReturnType() const { return returnType.get(); }
    const std::vector<std::unique_ptr<Type>>& getParamTypes() const {
        return paramTypes;
    }
};

// Array types
class ArrayType : public Type {
private:
    std::unique_ptr<Type> elementType;

public:
    explicit ArrayType(std::unique_ptr<Type> elemType)
        : elementType(std::move(elemType)) {}

    std::string toString() const override {
        return elementType->toString() + "[]";
    }

    bool isAssignableTo(const Type* other) const override {
        if (auto otherArray = dynamic_cast<const ArrayType*>(other)) {
            return elementType->isAssignableTo(otherArray->elementType.get());
        }

        if (auto primOther = dynamic_cast<const PrimitiveType*>(other)) {
            return primOther->getKind() == PrimitiveType::Kind::Any;
        }

        return false;
    }

    bool equals(const Type* other) const override {
        if (auto otherArray = dynamic_cast<const ArrayType*>(other)) {
            return elementType->equals(otherArray->elementType.get());
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        return std::make_unique<ArrayType>(elementType->clone());
    }

    const Type* getElementType() const { return elementType.get(); }
};

// Union types
class UnionType : public Type {
private:
    std::vector<std::unique_ptr<Type>> types;

public:
    explicit UnionType(std::vector<std::unique_ptr<Type>> types)
        : types(std::move(types)) {}

    std::string toString() const override {
        std::string result;
        for (size_t i = 0; i < types.size(); ++i) {
            if (i > 0)
                result += " | ";
            result += types[i]->toString();
        }
        return result;
    }

    bool isAssignableTo(const Type* other) const override {
        // A union is assignable to another type if all its variants are
        for (const auto& type : types) {
            if (!type->isAssignableTo(other)) {
                return false;
            }
        }
        return true;
    }

    bool equals(const Type* other) const override {
        if (auto otherUnion = dynamic_cast<const UnionType*>(other)) {
            if (types.size() != otherUnion->types.size())
                return false;

            // Check if every type in this union has an equivalent in the other
            // union
            for (const auto& type : types) {
                bool found = false;
                for (const auto& otherType : otherUnion->types) {
                    if (type->equals(otherType.get())) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return false;
            }

            // Check if every type in the other union has an equivalent in this
            // union
            for (const auto& otherType : otherUnion->types) {
                bool found = false;
                for (const auto& type : types) {
                    if (otherType->equals(type.get())) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return false;
            }

            return true;
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        std::vector<std::unique_ptr<Type>> clonedTypes;
        for (const auto& type : types) {
            clonedTypes.push_back(type->clone());
        }

        return std::make_unique<UnionType>(std::move(clonedTypes));
    }

    const std::vector<std::unique_ptr<Type>>& getTypes() const { return types; }
};

// Generic type parameters
class GenericTypeParameter : public Type {
private:
    std::string name;
    std::unique_ptr<Type> constraint;

public:
    explicit GenericTypeParameter(std::string n,
                                  std::unique_ptr<Type> c = nullptr)
        : name(std::move(n)), constraint(std::move(c)) {}

    std::string toString() const override {
        if (constraint) {
            return name + " extends " + constraint->toString();
        }
        return name;
    }

    bool isAssignableTo(const Type* other) const override {
        if (constraint) {
            return constraint->isAssignableTo(other);
        }
        return true;  // No constraint means assignable to anything
    }

    bool equals(const Type* other) const override {
        if (auto otherParam =
                dynamic_cast<const GenericTypeParameter*>(other)) {
            return name == otherParam->name &&
                   ((!constraint && !otherParam->constraint) ||
                    (constraint && otherParam->constraint &&
                     constraint->equals(otherParam->constraint.get())));
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        if (constraint) {
            return std::make_unique<GenericTypeParameter>(name,
                                                          constraint->clone());
        }
        return std::make_unique<GenericTypeParameter>(name);
    }

    const std::string& getName() const { return name; }
    const Type* getConstraint() const { return constraint.get(); }
};

// 新增: 类型断言类
class TypeAssertion : public Type {
private:
    std::unique_ptr<Type> sourceType;
    std::unique_ptr<Type> targetType;

public:
    TypeAssertion(std::unique_ptr<Type> source, std::unique_ptr<Type> target)
        : sourceType(std::move(source)), targetType(std::move(target)) {}

    std::string toString() const override {
        return sourceType->toString() + " as " + targetType->toString();
    }

    bool isAssignableTo(const Type* other) const override {
        // 类型断言结果是目标类型
        return targetType->isAssignableTo(other);
    }

    bool equals(const Type* other) const override {
        if (auto otherAssertion = dynamic_cast<const TypeAssertion*>(other)) {
            return sourceType->equals(otherAssertion->sourceType.get()) &&
                   targetType->equals(otherAssertion->targetType.get());
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        return std::make_unique<TypeAssertion>(sourceType->clone(),
                                               targetType->clone());
    }

    const Type* getSourceType() const { return sourceType.get(); }
    const Type* getTargetType() const { return targetType.get(); }
};

// 新增: 泛型实例化类型
class GenericInstanceType : public Type {
private:
    std::string baseTypeName;
    std::vector<std::unique_ptr<Type>> typeArguments;

public:
    GenericInstanceType(std::string name,
                        std::vector<std::unique_ptr<Type>> args)
        : baseTypeName(std::move(name)), typeArguments(std::move(args)) {}

    std::string toString() const override {
        std::string result = baseTypeName + "<";
        for (size_t i = 0; i < typeArguments.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += typeArguments[i]->toString();
        }
        return result + ">";
    }

    bool isAssignableTo(const Type* other) const override {
        // 简单实现：检查基础类型名称是否相同
        if (auto otherInstance =
                dynamic_cast<const GenericInstanceType*>(other)) {
            if (baseTypeName != otherInstance->baseTypeName)
                return false;

            // 检查类型参数是否可赋值
            if (typeArguments.size() != otherInstance->typeArguments.size())
                return false;

            for (size_t i = 0; i < typeArguments.size(); ++i) {
                if (!typeArguments[i]->isAssignableTo(
                        otherInstance->typeArguments[i].get()))
                    return false;
            }
            return true;
        }
        return false;
    }

    bool equals(const Type* other) const override {
        if (auto otherInstance =
                dynamic_cast<const GenericInstanceType*>(other)) {
            if (baseTypeName != otherInstance->baseTypeName)
                return false;

            if (typeArguments.size() != otherInstance->typeArguments.size())
                return false;

            for (size_t i = 0; i < typeArguments.size(); ++i) {
                if (!typeArguments[i]->equals(
                        otherInstance->typeArguments[i].get()))
                    return false;
            }
            return true;
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        std::vector<std::unique_ptr<Type>> clonedArgs;
        for (const auto& arg : typeArguments) {
            clonedArgs.push_back(arg->clone());
        }
        return std::make_unique<GenericInstanceType>(baseTypeName,
                                                     std::move(clonedArgs));
    }

    const std::string& getBaseTypeName() const { return baseTypeName; }
    const std::vector<std::unique_ptr<Type>>& getTypeArguments() const {
        return typeArguments;
    }
};

// 新增: 类型交集
class IntersectionType : public Type {
private:
    std::vector<std::unique_ptr<Type>> types;

public:
    explicit IntersectionType(std::vector<std::unique_ptr<Type>> types)
        : types(std::move(types)) {}

    std::string toString() const override {
        std::string result;
        for (size_t i = 0; i < types.size(); ++i) {
            if (i > 0)
                result += " & ";
            result += types[i]->toString();
        }
        return result;
    }

    bool isAssignableTo(const Type* other) const override {
        // 只要交集中任意一个类型可赋值给other，即可赋值
        for (const auto& type : types) {
            if (type->isAssignableTo(other)) {
                return true;
            }
        }
        return false;
    }

    bool equals(const Type* other) const override {
        if (auto otherIntersection =
                dynamic_cast<const IntersectionType*>(other)) {
            if (types.size() != otherIntersection->types.size())
                return false;

            // 检查每个类型是否都有对应的等价类型
            for (const auto& type : types) {
                bool found = false;
                for (const auto& otherType : otherIntersection->types) {
                    if (type->equals(otherType.get())) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return false;
            }
            return true;
        }
        return false;
    }

    std::unique_ptr<Type> clone() const override {
        std::vector<std::unique_ptr<Type>> clonedTypes;
        for (const auto& type : types) {
            clonedTypes.push_back(type->clone());
        }
        return std::make_unique<IntersectionType>(std::move(clonedTypes));
    }

    const std::vector<std::unique_ptr<Type>>& getTypes() const { return types; }
};

}  // namespace tsx