#include "network/message.hpp"
#include <iostream>
int main()
{
    Message msg(1);
    msg << 42;
    
    int doubledValue;
    msg >> doubledValue;

    std::cout << doubledValue;
}