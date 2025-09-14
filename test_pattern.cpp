#include <iostream>
#include <string>
using namespace std;
int main() {
    string long_input;
    for (int i = 0; i < 10; i++) {
        long_input += (i % 2 == 0) ? "Aa" : "bb";
    }
    cout << "Pattern: " << long_input << endl;
    cout << "Length: " << long_input.size() << endl;
    int uppercaseCount = 0;
    for (char c : long_input) {
        if (c >= 'A' && c <= 'Z') uppercaseCount++;
    }
    cout << "Uppercase count: " << uppercaseCount << endl;
    return 0;
}
