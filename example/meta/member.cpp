#include "atom/meta/member.hpp"
#include <iostream>
#include <string>
#include <vector>

// Sample structures for demonstration
struct Point {
    int x;
    int y;
};

struct Rectangle {
    Point topLeft;
    Point bottomRight;
    std::string name;
};

struct Node {
    int value;
    Node* next;
};

struct ComplexObject {
    int id;
    std::string name;
    std::vector<int> data;
    Point position;
};

// Base class for inheritance example
struct Base {
    int baseValue;
    virtual ~Base() = default;
};

struct Derived : public Base {
    int derivedValue;
    Point position;
};

// Simple tuple-like structure for get_member_by_index example
struct TupleLike {
    int first;
    double second;
    std::string third;
};

// 为TupleLike提供正确的tuple-like接口
namespace std {
template <>
struct tuple_size<TupleLike> : std::integral_constant<std::size_t, 3> {};

template <>
struct tuple_element<0, TupleLike> {
    using type = int;
};
template <>
struct tuple_element<1, TupleLike> {
    using type = double;
};
template <>
struct tuple_element<2, TupleLike> {
    using type = std::string;
};

template <std::size_t I>
typename tuple_element<I, TupleLike>::type& get(TupleLike& t) {
    if constexpr (I == 0)
        return t.first;
    else if constexpr (I == 1)
        return t.second;
    else if constexpr (I == 2)
        return t.third;
}

template <std::size_t I>
const typename tuple_element<I, TupleLike>::type& get(const TupleLike& t) {
    if constexpr (I == 0)
        return t.first;
    else if constexpr (I == 1)
        return t.second;
    else if constexpr (I == 2)
        return t.third;
}
}  // namespace std

int main() {
    std::cout << "=============================================\n";
    std::cout << "Atom Meta Member Library Usage Examples\n";
    std::cout << "=============================================\n\n";

    // 1. Basic member offset and size examples
    std::cout << "1. BASIC MEMBER INFORMATION\n";
    std::cout << "-------------------------------------------\n";

    std::cout << "Point struct size: " << atom::meta::struct_size<Point>()
              << "\n";

    // 修复：不使用constexpr，因为member_offset是consteval函数
    auto pointXOffset = atom::meta::offset_of(&Point::x);
    auto pointYOffset = atom::meta::offset_of(&Point::y);
    auto pointXSize = atom::meta::member_size(&Point::x);
    auto pointXAlign = atom::meta::member_alignment(&Point::x);

    std::cout << "Point::x offset: " << pointXOffset << "\n";
    std::cout << "Point::y offset: " << pointYOffset << "\n";
    std::cout << "Point::x size: " << pointXSize << "\n";
    std::cout << "Point::x alignment: " << pointXAlign << "\n\n";

    constexpr auto rectSize = atom::meta::struct_size<Rectangle>();
    // 修复：使用offset_of而不是member_offset
    auto rectTopLeftOffset = atom::meta::offset_of(&Rectangle::topLeft);
    auto rectBottomRightOffset = atom::meta::offset_of(&Rectangle::bottomRight);
    auto rectNameOffset = atom::meta::offset_of(&Rectangle::name);

    std::cout << "Rectangle struct size: " << rectSize << "\n";
    std::cout << "Rectangle::topLeft offset: " << rectTopLeftOffset << "\n";
    std::cout << "Rectangle::bottomRight offset: " << rectBottomRightOffset
              << "\n";
    std::cout << "Rectangle::name offset: " << rectNameOffset << "\n\n";

    // 2. Print member info (only enabled when ATOM_ENABLE_DEBUG is defined)
    std::cout << "2. PRINT MEMBER INFO (WHEN DEBUG ENABLED)\n";
    std::cout << "-------------------------------------------\n";

#if ATOM_ENABLE_DEBUG
    atom::meta::print_member_info<Point>("Point", &Point::x, &Point::y);
    atom::meta::print_member_info<Rectangle>("Rectangle", &Rectangle::topLeft,
                                             &Rectangle::bottomRight,
                                             &Rectangle::name);
#else
    std::cout
        << "ATOM_ENABLE_DEBUG not defined. print_member_info not available.\n";
#endif
    std::cout << "\n";

    // 3. offset_of with validation
    std::cout << "3. OFFSET_OF WITH VALIDATION\n";
    std::cout << "-------------------------------------------\n";

    try {
        std::size_t offset = atom::meta::offset_of(&Point::y);
        std::cout << "offset_of(Point::y): " << offset << "\n";

        // This would throw an exception if member_ptr was null:
        // atom::meta::offset_of<Point, int>(nullptr);
    } catch (const atom::meta::member_pointer_error& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
    std::cout << "\n";

    // 4. container_of examples
    std::cout << "4. CONTAINER_OF EXAMPLES\n";
    std::cout << "-------------------------------------------\n";

    Rectangle rect{Point{10, 20}, Point{30, 40}, "Example Rectangle"};
    const Point* pointPtr = &rect.topLeft;

    try {
        // 使用const_cast处理const到非const的转换
        Rectangle* recoveredRect = const_cast<Rectangle*>(
            atom::meta::container_of(pointPtr, &Rectangle::topLeft));
        std::cout << "Recovered rectangle name: " << recoveredRect->name
                  << "\n";

        // For const ptr
        const Rectangle* constRecoveredRect =
            atom::meta::container_of(pointPtr, &Rectangle::topLeft);
        std::cout << "Const recovered rectangle name: "
                  << constRecoveredRect->name << "\n";
    } catch (const atom::meta::member_pointer_error& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
    std::cout << "\n";

    // 5. safe_container_of example
    std::cout << "5. SAFE_CONTAINER_OF EXAMPLE\n";
    std::cout << "-------------------------------------------\n";

    // 使用正确的模板参数数量
    auto result = atom::meta::safe_container_of<Rectangle, Point>(
        &rect.bottomRight, &Rectangle::bottomRight);
    if (result) {
        std::cout << "Safe container_of succeeded: " << result.value()->name
                  << "\n";
    } else {
        // 修复：直接使用error()而不是调用what()
        std::cout << "Safe container_of failed: " << result.error() << "\n";
    }

    // Example with null pointer
    Point* nullPtr = nullptr;
    auto nullResult =
        atom::meta::safe_container_of<Rectangle, Point>(nullPtr, &Rectangle::topLeft);
    if (!nullResult) {
        // 修复：直接使用error()而不是调用what()
        std::cout << "Expected error with null pointer: "
                  << nullResult.error() << "\n";
    }
    std::cout << "\n";

    // 6. pointer_to_object examples
    std::cout << "6. POINTER_TO_OBJECT EXAMPLES\n";
    std::cout << "-------------------------------------------\n";

    try {
        Rectangle anotherRect{Point{5, 5}, Point{15, 15}, "Another Rectangle"};
        Point* pointMember = &anotherRect.topLeft;

        Rectangle* recoveredObject =
            atom::meta::pointer_to_object(&Rectangle::topLeft, pointMember);
        std::cout << "Recovered object name: " << recoveredObject->name << "\n";

        // Const version
        const Rectangle constRect{Point{50, 50}, Point{100, 100},
                                  "Const Rectangle"};
        const Point* constPointMember = &constRect.bottomRight;

        const Rectangle* constRecoveredObject = atom::meta::pointer_to_object(
            &Rectangle::bottomRight, constPointMember);
        std::cout << "Const recovered object name: "
                  << constRecoveredObject->name << "\n";
    } catch (const atom::meta::member_pointer_error& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
    std::cout << "\n";

    // 7. container_of with inheritance
    std::cout << "7. CONTAINER_OF WITH INHERITANCE\n";
    std::cout << "-------------------------------------------\n";

    try {
        Derived derived;
        derived.baseValue = 100;
        derived.derivedValue = 200;
        derived.position = {5, 10};

        Point* posPtr = &derived.position;

        Derived* recoveredDerived =
            atom::meta::container_of<Derived>(posPtr, &Derived::position);
        std::cout << "Recovered derived value: "
                  << recoveredDerived->derivedValue << "\n";

        Base* recoveredBase =
            atom::meta::container_of<Base, Derived>(posPtr, &Derived::position);
        std::cout << "Recovered base value: " << recoveredBase->baseValue
                  << "\n";

        // Const version
        const Derived constDerived{};
        const Point* constPosPtr = &constDerived.position;

        [[maybe_unused]] const Base* constRecoveredBase =
            atom::meta::container_of<Base, Derived>(constPosPtr,
                                                    &Derived::position);
        std::cout << "Const recovered base object accessed\n";
    } catch (const atom::meta::member_pointer_error& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
    std::cout << "\n";

    // 8. container_of_range examples
    std::cout << "8. CONTAINER_OF_RANGE EXAMPLES\n";
    std::cout << "-------------------------------------------\n";

    std::vector<Point> points{{1, 1}, {2, 2}, {3, 3}, {4, 4}};
    Point searchPoint{3, 3};

    auto rangeResult = atom::meta::container_of_range(points, &searchPoint);
    if (rangeResult) {
        const Point* foundPoint = rangeResult.value();
        std::cout << "Found point in container: (" << foundPoint->x << ", "
                  << foundPoint->y << ")\n";
    } else {
        // 修复：直接使用error()而不是调用what()
        std::cout << "Point not found: " << rangeResult.error() << "\n";
    }

    // Point not in the container
    Point notInContainer{9, 9};
    auto notFoundResult =
        atom::meta::container_of_range(points, &notInContainer);
    if (!notFoundResult) {
        // 修复：直接使用error()而不是调用what()
        std::cout << "Expected error for point not in container: "
                  << notFoundResult.error() << "\n";
    }
    std::cout << "\n";

    // 9. container_of_if_range examples
    std::cout << "9. CONTAINER_OF_IF_RANGE EXAMPLES\n";
    std::cout << "-------------------------------------------\n";

    auto predResult = atom::meta::container_of_if_range(
        points, [](const Point& p) { return p.x == 2 && p.y == 2; });
    if (predResult) {
        const Point* foundPoint = predResult.value();
        std::cout << "Found point with predicate: (" << foundPoint->x << ", "
                  << foundPoint->y << ")\n";
    } else {
        // 修复：直接使用error()而不是调用what()
        std::cout << "No point matching predicate: "
                  << predResult.error() << "\n";
    }

    // No match for predicate
    auto noMatchResult = atom::meta::container_of_if_range(
        points, [](const Point& p) { return p.x > 10; });
    if (!noMatchResult) {
        // 修复：直接使用error()而不是调用what()
        std::cout << "Expected error for no matching predicate: "
                  << noMatchResult.error() << "\n";
    }
    std::cout << "\n";

    // 10. is_member_of examples
    std::cout << "10. IS_MEMBER_OF EXAMPLES\n";
    std::cout << "-------------------------------------------\n";

    Rectangle testRect{Point{1, 2}, Point{3, 4}, "Test Rectangle"};
    Point* topLeftPtr = &testRect.topLeft;
    Point* bottomRightPtr = &testRect.bottomRight;
    Point unrelatedPoint{5, 6};

    bool isTopLeft =
        atom::meta::is_member_of(&testRect, topLeftPtr, &Rectangle::topLeft);
    bool isBottomRight = atom::meta::is_member_of(&testRect, bottomRightPtr,
                                                  &Rectangle::topLeft);
    bool isUnrelated = atom::meta::is_member_of(&testRect, &unrelatedPoint,
                                                &Rectangle::topLeft);

    std::cout << "topLeftPtr is member topLeft of testRect: " << std::boolalpha
              << isTopLeft << "\n";
    std::cout << "bottomRightPtr is member topLeft of testRect: "
              << isBottomRight << "\n";
    std::cout << "unrelatedPoint is member topLeft of testRect: " << isUnrelated
              << "\n";
    std::cout << "\n";

    // 11. get_member_by_index example
    std::cout << "11. GET_MEMBER_BY_INDEX EXAMPLE\n";
    std::cout << "-------------------------------------------\n";

    TupleLike tupleLike{42, 3.14, "hello"};

    auto& first = std::get<0>(tupleLike);
    auto& second = std::get<1>(tupleLike);
    auto& third = std::get<2>(tupleLike);

    std::cout << "First member: " << first << "\n";
    std::cout << "Second member: " << second << "\n";
    std::cout << "Third member: " << third << "\n";
    std::cout << "\n";

    // 12. for_each_member example
    std::cout << "12. FOR_EACH_MEMBER EXAMPLE\n";
    std::cout << "-------------------------------------------\n";

    Point p{10, 20};
    std::cout << "Processing each member of Point:\n";
    atom::meta::for_each_member(
        p,
        [](auto& member) {
            std::cout << "  Member value: " << member << "\n";
            // Double each member value
            member *= 2;
        },
        &Point::x, &Point::y);

    std::cout << "After processing: Point(" << p.x << ", " << p.y << ")\n";
    std::cout << "\n";

    // 13. memory_layout_stats example
    std::cout << "13. MEMORY_LAYOUT_STATS EXAMPLE\n";
    std::cout << "-------------------------------------------\n";

    constexpr auto pointStats =
        atom::meta::memory_layout_stats<Point>::compute();
    std::cout << "Point layout stats:\n";
    std::cout << "  Size: " << pointStats.size << "\n";
    std::cout << "  Alignment: " << pointStats.alignment << "\n";
    std::cout << "  Potential padding: " << pointStats.potential_padding
              << "\n\n";

    constexpr auto complexStats =
        atom::meta::memory_layout_stats<ComplexObject>::compute();
    std::cout << "ComplexObject layout stats:\n";
    std::cout << "  Size: " << complexStats.size << "\n";
    std::cout << "  Alignment: " << complexStats.alignment << "\n";
    std::cout << "  Potential padding: " << complexStats.potential_padding
              << "\n";
    std::cout << "\n";

    return 0;
}