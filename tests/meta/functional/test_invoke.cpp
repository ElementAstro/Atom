// test_invoke.cpp - Entry point for test_invoke.hpp
#include "test_invoke.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
