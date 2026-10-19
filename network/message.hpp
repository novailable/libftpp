#pragma once

#include <iostream>

template <typename T>
class   Message
{
    private:
        int _type;
        T   _msg;
    public:
        Message(int type);
        int type();
    
		template <typename output>
        operator<<();
        operator>>();
};

