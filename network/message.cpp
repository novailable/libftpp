#include "message.hpp"

Message::Message(int type) : _type(type) {};

int     Message::type() const { return (_type); }

size_t  Message::remaining() const { return _data.size() - _readPos; }

void    Message::readBytes(void *out, size_t num) const
{
    if (remaining() < num)
        throw   std::out_of_range("Message: not enough data to read");

    if (num)
        std::memcpy(out, _data.data() + _readPos, num);
    _readPos += num;
}

// std::vector<char>   Message::encode() const
// {
//     const std::uint32_t len = htonl(stat)
// }