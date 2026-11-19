#include "message.hpp"

Message::Message(int type) : _type(type) {};

int    Message::type() const { return (_type); }

