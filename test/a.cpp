#include <stacktrace>
#include <iostream>

int main() {
    std::stacktrace st = std::stacktrace::current();
    std::cout << st;
    return 0;
}