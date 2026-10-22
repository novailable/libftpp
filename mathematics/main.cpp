#include <iostream>
#include "ivector2.hpp"

int main()
{
    IVector2<int>   a{1, 2};
    IVector2<int>   b{2, 1};

    auto    w = a + b;
    std::cout << w.x << w.y << std::endl;
}