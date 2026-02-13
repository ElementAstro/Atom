// test_god.cpp - Entry point for test_god.hpp
#include "test_god.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
