#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "types.h"

namespace tsx {

// 类型注册表，用于全局管理类型定义
class TypeRegistry {
public:
    TypeRegistry() {
        // 注册内置类型
        registerPrimitiveTypes();
    }

    // 注册一个类型
    void registerType(const std::string& name, std::unique_ptr<Type> type) {
        types[name] = std::move(type);
    }

    // 查找一个类型
    Type* lookupType(const std::string& name) const {
        auto it = types.find(name);
        return it != types.end() ? it->second.get() : nullptr;
    }

    // 查找类型，如果不存在则创建
    Type* getOrCreateType(const std::string& name) {
        auto it = types.find(name);
        if (it != types.end()) {
            return it->second.get();
        }

        // 创建一个占位符类型
        auto placeholder = std::make_unique<PlaceholderType>(name);
        auto* result = placeholder.get();
        types[name] = std::move(placeholder);
        return result;
    }

    // 获取所有注册的类型名称
    std::vector<std::string> getRegisteredTypeNames() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : types) {
            names.push_back(name);
        }
        return names;
    }

    // 清除所有注册的类型
    void clear() {
        types.clear();
        registerPrimitiveTypes();  // 重新注册内置类型
    }

private:
    // 存储所有类型定义
    std::unordered_map<std::string, std::unique_ptr<Type>> types;

    // 注册内置的原始类型
    void registerPrimitiveTypes() {
        registerType("number", Type::createNumber());
        registerType("string", Type::createString());
        registerType("boolean", Type::createBoolean());
        registerType("null", Type::createNull());
        registerType("undefined", Type::createUndefined());
        registerType("any", Type::createAny());
        registerType("never", Type::createNever());
        registerType("unknown", Type::createUnknown());

        // 注册内置对象类型
        auto objectType = std::make_unique<ObjectType>();
        registerType("Object", std::move(objectType));

        // 注册数组类型
        auto arrayType = std::make_unique<ObjectType>();
        // TODO: 添加数组的内置方法
        registerType("Array", std::move(arrayType));

        // 注册函数类型
        auto functionType = std::make_unique<ObjectType>();
        // TODO: 添加函数的内置方法
        registerType("Function", std::move(functionType));
    }

    // 占位符类型，用于处理循环引用
    class PlaceholderType : public Type {
    public:
        explicit PlaceholderType(std::string n) : name(std::move(n)) {}

        std::string toString() const override {
            return name + " (placeholder)";
        }

        bool isAssignableTo(const Type* other) const override {
            return other->equals(this) ||
                   (dynamic_cast<const PrimitiveType*>(other) &&
                    dynamic_cast<const PrimitiveType*>(other)->getKind() ==
                        PrimitiveType::Kind::Any);
        }

        bool equals(const Type* other) const override {
            if (auto otherPlaceholder =
                    dynamic_cast<const PlaceholderType*>(other)) {
                return name == otherPlaceholder->name;
            }
            return false;
        }

        std::unique_ptr<Type> clone() const override {
            return std::make_unique<PlaceholderType>(name);
        }

    private:
        std::string name;
    };
};

}  // namespace tsx