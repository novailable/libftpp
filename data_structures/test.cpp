#include <iostream>
using namespace std;

template<typename ... TArgs>
void print(TArgs... args) {
    (cout << ... << args << " t") << '\n';
}

int main() {
    print(10);
    print(10, "Hello", 3.14);
}