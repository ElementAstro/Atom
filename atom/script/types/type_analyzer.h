#pragma once

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "../ast/ast.h"
#include "typechecker.h"
#include "types.h"

namespace tsx {

// 类型分析工具，提供高级类型操作和分析功能
class TypeAnalyzer {
public:
    explicit TypeAnalyzer(TypeChecker& checker) : typeChecker(checker) {}

    // 类型兼容性检查，具有详细的兼容性解释
    struct CompatibilityResult {
        bool compatible;
        std::string reason;
    };

    // 检查两个类型的兼容性
    CompatibilityResult checkCompatibility(const Type* source,
                                           const Type* target) {
        if (source->isAssignableTo(target)) {
            return {true, "Types are compatible"};
        }

        // 详细解释不兼容的原因
        std::string reason = "Type '" + source->toString() +
                             "' is not assignable to type '" +
                             target->toString() + "'";

        // 检查对象类型兼容性
        if (auto sourceObj = dynamic_cast<const ObjectType*>(source)) {
            if (auto targetObj = dynamic_cast<const ObjectType*>(target)) {
                reason += ". The following properties are incompatible:";

                // 检查哪些属性不兼容
                for (const auto& [name, targetType] :
                     targetObj->getProperties()) {
                    if (!sourceObj->hasProperty(name)) {
                        reason += "\n - Property '" + name +
                                  "' is missing in source type.";
                    } else {
                        const Type* sourceType =
                            sourceObj->getPropertyType(name);
                        if (!sourceType->isAssignableTo(targetType.get())) {
                            reason += "\n - Property '" + name + "': Type '" +
                                      sourceType->toString() +
                                      "' is not assignable to type '" +
                                      targetType->toString() + "'.";
                        }
                    }
                }
            }
        }

        // 检查函数类型兼容性
        else if (auto sourceFunc = dynamic_cast<const FunctionType*>(source)) {
            if (auto targetFunc = dynamic_cast<const FunctionType*>(target)) {
                reason += ". Function types are incompatible:";

                // 参数数量不匹配
                if (sourceFunc->getParamTypes().size() !=
                    targetFunc->getParamTypes().size()) {
                    reason +=
                        "\n - Parameter count mismatch: expected " +
                        std::to_string(targetFunc->getParamTypes().size()) +
                        ", got " +
                        std::to_string(sourceFunc->getParamTypes().size());
                } else {
                    // 检查参数类型兼容性
                    for (size_t i = 0; i < targetFunc->getParamTypes().size();
                         i++) {
                        const Type* sourceParamType =
                            sourceFunc->getParamTypes()[i].get();
                        const Type* targetParamType =
                            targetFunc->getParamTypes()[i].get();

                        if (!targetParamType->isAssignableTo(sourceParamType)) {
                            reason += "\n - Parameter " +
                                      std::to_string(i + 1) + ": Type '" +
                                      targetParamType->toString() +
                                      "' is not assignable to type '" +
                                      sourceParamType->toString() + "'";
                        }
                    }
                }

                // 返回类型不兼容
                if (!sourceFunc->getReturnType()->isAssignableTo(
                        targetFunc->getReturnType())) {
                    reason += "\n - Return type: Type '" +
                              sourceFunc->getReturnType()->toString() +
                              "' is not assignable to type '" +
                              targetFunc->getReturnType()->toString() + "'";
                }
            }
        }

        return {false, reason};
    }

    // 推断函数返回值类型
    std::unique_ptr<Type> inferFunctionReturnType(
        const AST::FunctionDeclaration* func) {
        // 设置默认返回类型为never（表示函数不返回）
        std::unique_ptr<Type> returnType = Type::createNever();

        // 创建返回类型的集合
        std::vector<std::unique_ptr<Type>> returnTypes;

        // 分析函数体中的所有return语句
        if (func->getBody()) {
            collectReturnTypes(func->getBody(), returnTypes);
        }

        // 根据收集到的返回类型计算最终类型
        if (returnTypes.empty()) {
            // 没有return语句，返回void (undefined)
            return Type::createUndefined();
        } else if (returnTypes.size() == 1) {
            // 只有一种返回类型
            return std::move(returnTypes[0]);
        } else {
            // 多种返回类型，创建联合类型
            return std::make_unique<UnionType>(std::move(returnTypes));
        }
    }

    // 寻找所有使用某类型的位置
    struct TypeUsage {
        enum class UsageKind {
            Variable,
            Parameter,
            ReturnType,
            PropertyType,
            TypeArgument
        };

        UsageKind kind;
        std::string location;  // 如: "function foo, parameter bar"
        AST::Position position;
    };

    std::vector<TypeUsage> findTypeUsages(const std::string& typeName,
                                          const AST::Program* program) {
        std::vector<TypeUsage> usages;

        for (const auto& stmt : program->getStatements()) {
            collectTypeUsages(stmt.get(), typeName, usages);
        }

        return usages;
    }

    // 检查类型循环引用
    bool detectCircularReferences(const std::string& startType) {
        std::set<std::string> visited;
        std::set<std::string> recursionStack;

        return hasCycle(startType, visited, recursionStack);
    }

    // 简化类型表示（压缩联合类型、消除never等）
    std::unique_ptr<Type> simplifyType(std::unique_ptr<Type> type) {
        // 消除never类型
        if (auto unionType = dynamic_cast<UnionType*>(type.get())) {
            std::vector<std::unique_ptr<Type>> simplifiedTypes;
            bool hasAny = false;

            for (const auto& t : unionType->getTypes()) {
                // 检查是否为Any类型
                if (auto primType =
                        dynamic_cast<const PrimitiveType*>(t.get())) {
                    if (primType->getKind() == PrimitiveType::Kind::Any) {
                        hasAny = true;
                        break;
                    }
                }

                // 跳过Never类型
                if (auto primType =
                        dynamic_cast<const PrimitiveType*>(t.get())) {
                    if (primType->getKind() == PrimitiveType::Kind::Never) {
                        continue;
                    }
                }

                simplifiedTypes.push_back(t->clone());
            }

            // 如果联合中有Any，整个类型就是Any
            if (hasAny) {
                return Type::createAny();
            }

            // 如果简化后为空，则为never
            if (simplifiedTypes.empty()) {
                return Type::createNever();
            }

            // 如果只有一个类型，返回该类型
            if (simplifiedTypes.size() == 1) {
                return std::move(simplifiedTypes[0]);
            }

            // 返回简化后的联合类型
            return std::make_unique<UnionType>(std::move(simplifiedTypes));
        }

        // 对其他类型直接返回
        return type->clone();
    }

    // 推断对象文字量的类型，可选地进行结构化类型推断
    std::unique_ptr<Type> inferObjectLiteralType(
        const AST::ObjectLiteralExpression* obj,
        bool useStructuralTyping = true) {
        auto objType = std::make_unique<ObjectType>();

        for (const auto& prop : obj->getProperties()) {
            auto propType = typeChecker.getExpressionType(prop.value.get());

            // 对于方法，进行特殊处理
            if (auto funcType = dynamic_cast<FunctionType*>(propType.get())) {
                // 处理方法的this绑定等
            }

            objType->addProperty(prop.key, std::move(propType));
        }

        // 如果启用结构化类型推断，尝试匹配已有接口
        if (useStructuralTyping) {
            // TODO: 实现结构化类型推断
        }

        return objType;
    }

    // 获取两个类型的交集（如string & number -> never）
    std::unique_ptr<Type> getTypeIntersection(const Type* t1, const Type* t2) {
        // 处理基本类型
        if (auto prim1 = dynamic_cast<const PrimitiveType*>(t1)) {
            if (auto prim2 = dynamic_cast<const PrimitiveType*>(t2)) {
                // 相同类型的交集是其本身
                if (prim1->getKind() == prim2->getKind()) {
                    return t1->clone();
                }

                // Any类型与任何类型的交集是该类型
                if (prim1->getKind() == PrimitiveType::Kind::Any) {
                    return t2->clone();
                }
                if (prim2->getKind() == PrimitiveType::Kind::Any) {
                    return t1->clone();
                }

                // 不同基本类型的交集是never
                return Type::createNever();
            }
        }

        // 对象类型的交集是属性的并集
        if (auto obj1 = dynamic_cast<const ObjectType*>(t1)) {
            if (auto obj2 = dynamic_cast<const ObjectType*>(t2)) {
                auto result = std::make_unique<ObjectType>();

                // 复制第一个对象的所有属性
                for (const auto& [name, type] : obj1->getProperties()) {
                    result->addProperty(name, type->clone());
                }

                // 添加或合并第二个对象的属性
                for (const auto& [name, type] : obj2->getProperties()) {
                    if (result->hasProperty(name)) {
                        // 如果属性已存在，取交集
                        auto existingType = result->getPropertyType(name);
                        auto intersection =
                            getTypeIntersection(existingType, type.get());
                        result->addProperty(name, std::move(intersection));
                    } else {
                        // 否则添加新属性
                        result->addProperty(name, type->clone());
                    }
                }

                return result;
            }
        }

        // 创建交集类型
        std::vector<std::unique_ptr<Type>> types;
        types.push_back(t1->clone());
        types.push_back(t2->clone());
        return Type::createIntersection(std::move(types));
    }

private:
    TypeChecker& typeChecker;

    // 递归收集函数中的所有返回类型
    void collectReturnTypes(const AST::BlockStatement* block,
                            std::vector<std::unique_ptr<Type>>& returnTypes) {
        for (const auto& stmt : block->getStatements()) {
            // 如果是return语句
            if (auto returnStmt =
                    dynamic_cast<const AST::ReturnStatement*>(stmt.get())) {
                if (returnStmt->getValue()) {
                    returnTypes.push_back(
                        typeChecker.getExpressionType(returnStmt->getValue()));
                } else {
                    returnTypes.push_back(Type::createUndefined());
                }
            }

            // 如果是块语句，递归检查
            if (auto blockStmt =
                    dynamic_cast<const AST::BlockStatement*>(stmt.get())) {
                collectReturnTypes(blockStmt, returnTypes);
            }

            // 如果是if语句，检查两个分支
            if (auto ifStmt =
                    dynamic_cast<const AST::IfStatement*>(stmt.get())) {
                if (auto thenBlock = dynamic_cast<const AST::BlockStatement*>(
                        ifStmt->getThenBranch())) {
                    collectReturnTypes(thenBlock, returnTypes);
                }

                if (ifStmt->getElseBranch()) {
                    if (auto elseBlock =
                            dynamic_cast<const AST::BlockStatement*>(
                                ifStmt->getElseBranch())) {
                        collectReturnTypes(elseBlock, returnTypes);
                    }
                }
            }
        }
    }

    // 收集类型使用位置
    void collectTypeUsages(const AST::Node* node, const std::string& typeName,
                           std::vector<TypeUsage>& usages) {
        // 检查变量声明
        if (auto varDecl =
                dynamic_cast<const AST::VariableDeclaration*>(node)) {
            for (const auto& decl : varDecl->getDeclarations()) {
                if (decl.typeAnnotation &&
                    decl.typeAnnotation->toString() == typeName) {
                    usages.push_back({TypeUsage::UsageKind::Variable,
                                      "Variable " + decl.name,
                                      node->getLocation()});
                }
            }
        }

        // 检查函数声明
        else if (auto funcDecl =
                     dynamic_cast<const AST::FunctionDeclaration*>(node)) {
            // 检查返回类型
            if (funcDecl->getReturnType() &&
                funcDecl->getReturnType()->toString() == typeName) {
                usages.push_back(
                    {TypeUsage::UsageKind::ReturnType,
                     "Function " + funcDecl->getName() + " return type",
                     node->getLocation()});
            }

            // 检查参数类型
            for (const auto& param : funcDecl->getParameters()) {
                if (param.typeAnnotation &&
                    param.typeAnnotation->toString() == typeName) {
                    usages.push_back({TypeUsage::UsageKind::Parameter,
                                      "Function " + funcDecl->getName() +
                                          ", parameter " + param.name,
                                      node->getLocation()});
                }
            }

            // 递归检查函数体
            if (funcDecl->getBody()) {
                collectTypeUsages(funcDecl->getBody(), typeName, usages);
            }
        }

        // 检查类声明
        else if (auto classDecl =
                     dynamic_cast<const AST::ClassDeclaration*>(node)) {
            // 检查属性类型
            for (const auto& member : classDecl->getMembers()) {
                if (member.kind ==
                    AST::ClassDeclaration::MemberKind::Property) {
                    if (member.propertyType &&
                        member.propertyType->toString() == typeName) {
                        usages.push_back({TypeUsage::UsageKind::PropertyType,
                                          "Class " + classDecl->getName() +
                                              ", property " + member.name,
                                          node->getLocation()});
                    }
                }
            }
        }

        // 递归检查子节点
        // TODO: 递归遍历更多节点类型
    }

    // 检测类型循环引用
    bool hasCycle(const std::string& typeName, std::set<std::string>& visited,
                  std::set<std::string>& recursionStack) {
        // 已经检查过，不在当前递归栈中
        if (visited.count(typeName) > 0 &&
            recursionStack.count(typeName) == 0) {
            return false;
        }

        // 已在递归栈中，发现循环
        if (recursionStack.count(typeName) > 0) {
            return true;
        }

        // 标记为已访问并加入递归栈
        visited.insert(typeName);
        recursionStack.insert(typeName);

        // 获取类型定义
        Type* type = typeChecker.lookupSymbol(typeName);
        if (!type) {
            // 类型未找到，移出递归栈
            recursionStack.erase(typeName);
            return false;
        }

        // 检查引用的类型
        std::vector<std::string> referencedTypes = getReferencedTypes(type);
        for (const auto& ref : referencedTypes) {
            if (hasCycle(ref, visited, recursionStack)) {
                return true;
            }
        }

        // 处理完成，移出递归栈
        recursionStack.erase(typeName);
        return false;
    }

    // 获取类型中引用的所有其他类型名称
    // 获取类型中引用的所有其他类型名称
    std::vector<std::string> getReferencedTypes(const Type* type) {
        std::vector<std::string> result;
        if (!type)
            return result;

        // 处理对象类型中的属性类型
        if (auto objType = dynamic_cast<const ObjectType*>(type)) {
            for (const auto& [name, propType] : objType->getProperties()) {
                auto propRefs = getReferencedTypes(propType.get());
                result.insert(result.end(), propRefs.begin(), propRefs.end());
            }
        }

        // 处理数组类型
        if (auto arrayType = dynamic_cast<const ArrayType*>(type)) {
            auto elemRefs = getReferencedTypes(arrayType->getElementType());
            result.insert(result.end(), elemRefs.begin(), elemRefs.end());
        }

        // 处理函数类型
        if (auto funcType = dynamic_cast<const FunctionType*>(type)) {
            // 处理参数类型
            for (const auto& paramType : funcType->getParamTypes()) {
                auto paramRefs = getReferencedTypes(paramType.get());
                result.insert(result.end(), paramRefs.begin(), paramRefs.end());
            }

            // 处理返回类型
            auto returnRefs = getReferencedTypes(funcType->getReturnType());
            result.insert(result.end(), returnRefs.begin(), returnRefs.end());
        }

        // 处理联合类型
        if (auto unionType = dynamic_cast<const UnionType*>(type)) {
            for (const auto& memberType : unionType->getTypes()) {
                auto memberRefs = getReferencedTypes(memberType.get());
                result.insert(result.end(), memberRefs.begin(),
                              memberRefs.end());
            }
        }

        // 处理交叉类型
        if (auto intersectionType =
                dynamic_cast<const IntersectionType*>(type)) {
            for (const auto& memberType : intersectionType->getTypes()) {
                auto memberRefs = getReferencedTypes(memberType.get());
                result.insert(result.end(), memberRefs.begin(),
                              memberRefs.end());
            }
        }

        // 移除重复项
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());

        return result;
    }
};

}  // namespace tsx
