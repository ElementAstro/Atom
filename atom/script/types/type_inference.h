#pragma once

#include <memory>
#include <vector>
#include "../ast/ast.h"
#include "typechecker.h"
#include "types.h"

namespace tsx {

// 高级类型推导引擎，支持上下文相关的类型推导
class TypeInference {
public:
    explicit TypeInference(TypeChecker& checker) : typeChecker(checker) {}

    // 从上下文推导表达式类型
    std::unique_ptr<Type> inferTypeFromContext(
        const AST::Expression* expr, const Type* contextType = nullptr) {
        // 如果没有上下文类型，直接使用表达式的类型
        if (!contextType) {
            return typeChecker.getExpressionType(expr);
        }

        // 对于不同类型的表达式，应用不同的上下文推导规则
        if (auto arrayLiteral =
                dynamic_cast<const AST::ArrayLiteralExpression*>(expr)) {
            return inferArrayLiteralType(arrayLiteral, contextType);
        } else if (auto objLiteral =
                       dynamic_cast<const AST::ObjectLiteralExpression*>(
                           expr)) {
            return inferObjectLiteralType(objLiteral, contextType);
        } else if (auto funcExpr =
                       dynamic_cast<const AST::FunctionExpression*>(expr)) {
            return inferFunctionExpressionType(funcExpr, contextType);
        }

        // 默认使用表达式自己的类型
        return typeChecker.getExpressionType(expr);
    }

    // 推导对象字面量的类型，使用上下文类型改进推导结果
    std::unique_ptr<Type> inferObjectLiteralType(
        const AST::ObjectLiteralExpression* expr, const Type* contextType) {
        // 如果上下文类型是对象类型，使用它的属性类型来推导
        if (auto objContextType =
                dynamic_cast<const ObjectType*>(contextType)) {
            auto result = std::make_unique<ObjectType>();

            // 遍历对象字面量的所有属性
            for (const auto& prop : expr->getProperties()) {
                // 查找上下文中的属性类型
                const Type* propContextType = nullptr;
                if (objContextType->hasProperty(prop.key)) {
                    propContextType = objContextType->getPropertyType(prop.key);
                }

                // 使用上下文推导属性值的类型
                auto propType =
                    inferTypeFromContext(prop.value.get(), propContextType);
                result->addProperty(prop.key, std::move(propType));
            }

            return result;
        }

        // 如果上下文类型不是对象类型，使用默认的推导
        return typeChecker.getExpressionType(expr);
    }

    // 推导数组字面量的类型，使用上下文类型改进推导结果
    std::unique_ptr<Type> inferArrayLiteralType(
        const AST::ArrayLiteralExpression* expr, const Type* contextType) {
        // 如果上下文类型是数组类型，使用其元素类型作为上下文
        if (auto arrayContextType =
                dynamic_cast<const ArrayType*>(contextType)) {
            const Type* elemContextType = arrayContextType->getElementType();

            // 创建一个新数组类型，元素类型根据上下文推导
            std::unique_ptr<Type> elementType;

            // 如果数组为空，直接使用上下文元素类型
            const auto& elements = expr->getElements();
            if (elements.empty()) {
                elementType = elemContextType->clone();
            } else {
                // 尝试使用上下文为每个元素推导类型
                std::vector<std::unique_ptr<Type>> elementTypes;
                for (const auto& elem : elements) {
                    elementTypes.push_back(
                        inferTypeFromContext(elem.get(), elemContextType));
                }

                // 尝试找到最佳共同类型
                elementType = findBestCommonType(elementTypes);
            }

            return std::make_unique<ArrayType>(std::move(elementType));
        }

        // 如果上下文类型不是数组类型，使用默认的推导
        return typeChecker.getExpressionType(expr);
    }

    // 推导函数表达式的类型，使用上下文类型改进推导结果
    std::unique_ptr<Type> inferFunctionExpressionType(
        const AST::FunctionExpression* expr, const Type* contextType) {
        // 如果上下文类型是函数类型，使用其参数类型和返回类型作为上下文
        if (auto funcContextType =
                dynamic_cast<const FunctionType*>(contextType)) {
            const auto& contextParamTypes = funcContextType->getParamTypes();
            const Type* contextReturnType = funcContextType->getReturnType();

            // 创建参数类型数组
            std::vector<std::unique_ptr<Type>> paramTypes;
            const auto& params = expr->getParameters();

            // 使用上下文类型为参数提供类型
            for (size_t i = 0; i < params.size(); ++i) {
                if (i < contextParamTypes.size()) {
                    paramTypes.push_back(contextParamTypes[i]->clone());
                } else {
                    // 如果超出上下文参数数量，使用any
                    paramTypes.push_back(Type::createAny());
                }
            }

            // 使用上下文返回类型
            std::unique_ptr<Type> returnType;
            if (expr->getBody()) {
                // TODO: 分析函数体以推断返回类型
                returnType = contextReturnType->clone();
            } else {
                returnType = contextReturnType->clone();
            }

            return std::make_unique<FunctionType>(std::move(paramTypes),
                                                  std::move(returnType));
        }

        // 如果上下文类型不是函数类型，使用默认的推导
        return typeChecker.getExpressionType(expr);
    }

    // 查找最佳共同类型（用于联合类型推导）
    std::unique_ptr<Type> findBestCommonType(
        const std::vector<std::unique_ptr<Type>>& types) {
        if (types.empty()) {
            return Type::createAny();
        }

        if (types.size() == 1) {
            return types[0]->clone();
        }

        // 检查所有类型是否相同
        bool allSame = true;
        for (size_t i = 1; i < types.size(); ++i) {
            if (!types[i]->equals(types[0].get())) {
                allSame = false;
                break;
            }
        }

        if (allSame) {
            return types[0]->clone();
        }

        // 检查是否所有类型都可分配给某一个类型
        for (size_t i = 0; i < types.size(); ++i) {
            bool allAssignable = true;
            for (size_t j = 0; j < types.size(); ++j) {
                if (i != j && !types[j]->isAssignableTo(types[i].get())) {
                    allAssignable = false;
                    break;
                }
            }

            if (allAssignable) {
                return types[i]->clone();
            }
        }

        // 创建联合类型
        std::vector<std::unique_ptr<Type>> unionTypes;
        for (const auto& type : types) {
            unionTypes.push_back(type->clone());
        }

        return std::make_unique<UnionType>(std::move(unionTypes));
    }

private:
    TypeChecker& typeChecker;
};

}  // namespace tsx
