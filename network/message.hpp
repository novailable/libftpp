#pragma once

template <typename T>
class   Message
{
    private:
        int _type;
        T   _msg;
    public:
        Message(int type);
        int type();
    
        operator<<();
        operator>>();
};

