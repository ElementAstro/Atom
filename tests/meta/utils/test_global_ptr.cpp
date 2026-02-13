// test_global_ptr.cpp - Entry point for test_global_ptr.hpp
#include "test_global_ptr.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
