#include "atom/io/pushd.hpp"

#include <iostream>
#include <filesystem>

int main() {
    ::atom::io::DirectoryStack dirStack; // Uses default (synchronous) constructor

    std::cout << "Current directory: " << std::filesystem::current_path() << "\n";

    // Pushd to a new directory (use current path for a safe demo)
    std::filesystem::path newDir = std::filesystem::current_path();
    dirStack.pushd(newDir);
    std::cout << "Changed to: " << std::filesystem::current_path() << "\n";

    // Peek top
    auto top = dirStack.peek();
    std::cout << "Top of stack: " << top << "\n";

    // Show stack size and list
    auto list = dirStack.dirs();
    std::cout << "Stack has " << list.size() << " entries\n";

    // Popd back
    dirStack.popd();
    std::cout << "Back to: " << std::filesystem::current_path() << "\n";

    // Save/load stack demo
    std::string filename = "dir_stack.txt";
    dirStack.saveStackToFile(filename);
    std::cout << "Saved stack to: " << filename << "\n";

    dirStack.loadStackFromFile(filename);
    std::cout << "Loaded stack from: " << filename << "\n";

    std::cout << "Is empty? " << (dirStack.isEmpty() ? "yes" : "no") << "\n";
    std::cout << "Size: " << dirStack.size() << "\n";

    return 0;
}
