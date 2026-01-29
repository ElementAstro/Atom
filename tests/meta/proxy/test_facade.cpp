// test_facade.cpp - Entry point for test_facade.hpp
#include "test_facade.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
