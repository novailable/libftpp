#include "ivector.hpp"
#include <iostream>

struct vec2 : public IVector<int, 2, vec2>
{
    using IVector<int, 2, vec2>::IVector;
    int &x = (*this)[0];
    int &y = (*this)[1];
};

int main()
{
    vec2 a{2, 3};
    vec2 b{1, 2};
    
    auto c = a * b;
    
}