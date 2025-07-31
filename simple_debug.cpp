#include <iostream>
#include <string>
#include <vector>

// Simple Boyer-Moore for debugging
std::vector<int> simpleBoyerMoore(const std::string& text, const std::string& pattern) {
    std::vector<int> result;
    int n = text.length();
    int m = pattern.length();
    
    std::cout << "Searching for '" << pattern << "' in '" << text << "'\n";
    std::cout << "Text length: " << n << ", Pattern length: " << m << "\n";
    
    if (m == 0 || n < m) {
        std::cout << "Early return: empty pattern or text too short\n";
        return result;
    }
    
    // Simple brute force for comparison
    for (int i = 0; i <= n - m; i++) {
        std::cout << "Checking position " << i << ": ";
        bool match = true;
        for (int j = 0; j < m; j++) {
            if (text[i + j] != pattern[j]) {
                std::cout << "mismatch at " << j << " ('" << text[i + j] << "' vs '" << pattern[j] << "')\n";
                match = false;
                break;
            }
        }
        if (match) {
            std::cout << "MATCH!\n";
            result.push_back(i);
        }
    }
    
    return result;
}

int main() {
    std::cout << "=== Simple Boyer-Moore Debug ===\n";
    
    // Test the failing case
    auto result1 = simpleBoyerMoore("This is a new test", "new");
    std::cout << "Results: ";
    for (int pos : result1) {
        std::cout << pos << " ";
    }
    std::cout << "\nExpected: 10\n\n";
    
    // Test another case
    auto result2 = simpleBoyerMoore("abcabcabc", "abc");
    std::cout << "Results: ";
    for (int pos : result2) {
        std::cout << pos << " ";
    }
    std::cout << "\nExpected: 0 3 6\n";
    
    return 0;
}
