#include "network/message.hpp"
#include <iostream>
#include <string>

int main()
{
    Message msg(1);

    int age = 25;
    float height = 1.75f;
    std::string word = "this is bitch";

    msg << age << height << word;

    int a;
    int b, c;
    std::string d;

    // msg >> d;
    // msg >> c;
    std::cout << a << ", " << b << ", " << c << ", " << d << std::endl;
}