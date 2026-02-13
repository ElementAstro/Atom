// test_proxy.cpp - Entry point for test_proxy.hpp
#include "test_proxy.hpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
