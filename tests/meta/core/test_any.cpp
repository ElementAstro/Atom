// test_any.cpp - Entry point for test_any.hpp
#include "test_any.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
