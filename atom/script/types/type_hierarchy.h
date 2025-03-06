#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include "types.h"
#include "typechecker.h"

namespace tsx {

// 管理类型层次结构和继承关系
class TypeHierarchy {
public:
    explicit TypeHierarchy(TypeChecker& checker) : typeChecker(checker) {}
    
    // 检查一个类型是否是另一个类型的子类型
    bool isSubtypeOf(const std::string& subTypeName, const std::string& superTypeName) {
        // 如果类型名相同，直接返回true
        if (subTypeName == superTypeName) {
            return true;
        }
        
        // 查找两个类型
        Type* subType = typeChecker.lookupSymbol(subTypeName);
        Type* superType = typeChecker.lookupSymbol(superTypeName);
        
        if (!subType || !superType) {
            return false; // 类型未找到
        }
        
        // 检查子类型关系
        return subType->isAssignableTo(superType);
    }
    
    // 获取类型的所有直接父类型
    std::vector<std::string> getDirectSuperTypes(const std::string& typeName) {
        std::vector<std::string> result;
        
        // 查找类型
        Type* type = typeChecker.lookupSymbol(typeName);
        if (!type) {
            return result;
        }
        
        // 处理类类型
        if (auto classType = dynamic_cast<const ObjectType*>(type)) {
            // 查找继承关系
            for (const auto& [name, t] : inheritanceMap) {
                if (t.count(typeName) > 0) {
                    result.push_back(name);
                }
            }
        }
        
        return result;
    }
    
    // 获取类型的所有直接子类型
    std::vector<std::string> getDirectSubTypes(const std::string& typeName) {
        std::vector<std::string> result;
        
        // 查找继承映射
        auto it = inheritanceMap.find(typeName);
        if (it != inheritanceMap.end()) {
            for (const auto& subType : it->second) {
                result.push_back(subType);
            }
        }
        
        return result;
    }
    
    // 记录类型之间的继承关系
    void addInheritanceRelation(const std::string& subTypeName, const std::string& superTypeName) {
        inheritanceMap[superTypeName].insert(subTypeName);
    }
    
    // 获取类型的所有子类型（传递闭包）
    std::set<std::string> getAllSubTypes(const std::string& typeName) {
        std::set<std::string> result;
        std::set<std::string> visited;
        
        collectAllSubTypes(typeName, result, visited);
        return result;
    }
    
    // 获取类型的所有父类型（传递闭包）
    std::set<std::string> getAllSuperTypes(const std::string& typeName) {
        std::set<std::string> result;
        std::set<std::string> visited;
        
        collectAllSuperTypes(typeName, result, visited);
        return result;
    }
    
private:
    TypeChecker& typeChecker;
    
    // 存储类型继承关系（父类型 -> 子类型集合）
    std::unordered_map<std::string, std::set<std::string>> inheritanceMap;
    
    // 递归收集所有子类型
    void collectAllSubTypes(const std::string& typeName, 
                           std::set<std::string>& result, 
                           std::set<std::string>& visited) {
        if (visited.count(typeName) > 0) {
            return;
        }
        
        visited.insert(typeName);
        
        // 获取直接子类型
        auto directSubTypes = getDirectSubTypes(typeName);
        
        // 添加到结果集
        for (const auto& subType : directSubTypes) {
            result.insert(subType);
            
            // 递归添加子类型的子类型
            collectAllSubTypes(subType, result, visited);
        }
    }
    
    // 递归收集所有父类型
    void collectAllSuperTypes(const std::string& typeName, 
                             std::set<std::string>& result, 
                             std::set<std::string>& visited) {
        if (visited.count(typeName) > 0) {
            return;
        }
        
        visited.insert(typeName);
        
        // 获取直接父类型
        auto directSuperTypes = getDirectSuperTypes(typeName);
        
        // 添加到结果集
        for (const auto& superType : directSuperTypes) {
            result.insert(superType);
            
            // 递归添加父类型的父类型
            collectAllSuperTypes(superType, result, visited);
        }
    }
};

} // namespace tsx
