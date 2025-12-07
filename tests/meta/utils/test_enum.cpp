// test_enum.cpp - Entry point for test_enum.hpp
#include "test_enum.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
