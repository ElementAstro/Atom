// test_type_info.cpp - Entry point for test_type_info.hpp
#include "test_type_info.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
