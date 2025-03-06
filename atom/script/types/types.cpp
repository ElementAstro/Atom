#include "types.h"

namespace tsx {

// 实现Type类的静态工厂方法
std::unique_ptr<Type> Type::createNumber() {
    return std::make_unique<PrimitiveType>(PrimitiveType::Kind::Number);
}

std::unique_ptr<Type> Type::createString() {
    return std::make_unique<PrimitiveType>(PrimitiveType::Kind::String);
}

std::unique_ptr<Type> Type::createBoolean() {
    return std::make_unique<PrimitiveType>(PrimitiveType::Kind::Boolean);
}

std::unique_ptr<Type> Type::createNull() {
    return std::make_unique<PrimitiveType>(PrimitiveType::Kind::Null);
}

std::unique_ptr<Type> Type::createUndefined() {
    return std::make_unique<PrimitiveType>(PrimitiveType::Kind::Undefined);
}

std::unique_ptr<Type> Type::createAny() {
    return std::make_unique<PrimitiveType>(PrimitiveType::Kind::Any);
}

std::unique_ptr<Type> Type::createNever() {
    return std::make_unique<PrimitiveType>(PrimitiveType::Kind::Never);
}

std::unique_ptr<Type> Type::createUnknown() {
    return std::make_unique<PrimitiveType>(PrimitiveType::Kind::Unknown);
}

// 新增: 创建类型交集
std::unique_ptr<Type> Type::createIntersection(
    std::vector<std::unique_ptr<Type>> types) {
    return std::make_unique<IntersectionType>(std::move(types));
}

}  // namespace tsx
