#include <iostream>
#include <limits>

int main() {
    volatile int maximum = std::numeric_limits<int>::max();
    const int overflow = maximum + 1;
    std::cout << overflow << '\n';
    return 0;
}
