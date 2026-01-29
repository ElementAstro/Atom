// test_anymeta.cpp - Entry point for test_anymeta.hpp
#include "test_anymeta.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
