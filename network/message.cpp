#include "message.hpp"

Message::Message() : _type(0) {};

Message::Message(int type) : _type(type) {};

int     Message::type() const { return (_type); }

size_t  Message::remaining() const 
{
    return (_readPos <= _data.size() ? _data.size() - _readPos : 0);
}

void    Message::readBytes(void *out, size_t num) const
{
    if (remaining() < num)
        throw   std::out_of_range("Message: not enough data to read");

    if (num)
        std::memcpy(out, _data.data() + _readPos, num);
    _readPos += num;
}

std::vector<char>   Message::encode() const
{
    if (_data.size() > static_cast<size_t>(UINT32_MAX) - sizeof(std::int32_t))
        throw std::length_error("Message: payload exceeds wire format limit");

    const std::uint32_t len = htonl(static_cast<std::uint32_t>(sizeof(std::int32_t) + _data.size()));
    const std::uint32_t type = htonl(static_cast<std::uint32_t>(_type));

    std::vector<char>   out(sizeof(len) + sizeof(type) + _data.size());
    std::memcpy(out.data(), &len, sizeof(len));
    std::memcpy(out.data() + sizeof(len), &type, sizeof(type));
    if (!_data.empty())
        std::memcpy(out.data() + sizeof(len) + sizeof(type), _data.data(), _data.size());
    return out;
}

std::optional<Message>  Message::decode(std::vector<char>& buffer)
{
    if (buffer.size() < sizeof(std::uint32_t))
        return std::nullopt;
    
    std::uint32_t   len = 0;
    std::memcpy(&len, buffer.data(), sizeof(len));
    len = ntohl(len);

    if (len < sizeof(std::int32_t) || len > maxSize)
        throw   std::runtime_error("Message: malformed data size");
    
    if (buffer.size() < sizeof(uint32_t) + len)
        return std::nullopt;
    
    std::uint32_t   type = 0;
    std::memcpy(&type, buffer.data() + sizeof(std::uint32_t), sizeof(type));

    Message msg(static_cast<Type>(ntohl(type)));
    const char* payload = buffer.data() + sizeof(std::uint32_t) + sizeof(type);
    msg._data.assign(payload, payload + (len - sizeof(type)));

    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(sizeof(std::uint32_t) + len));
    return msg;
}