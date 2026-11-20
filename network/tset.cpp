#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// A typed, serializable message exchanged between Client and Server.
//
// Data is appended with operator<< and read back, in the same order, with
// operator>>. operator>> is const (the read position is mutable) so actions
// can read from the `const Message&` they receive.
//
// Wire format (see encode()/tryDecode()):
//   uint32 length (big endian, = 4 + payload size) | int32 type (big endian) | payload
class Message
{
public:
    using Type = int;

    explicit Message(Type type);

    Type type() const;

    // ---- writing -----------------------------------------------------------
    template <typename T>
    Message& operator<<(const T& value)
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Message: only trivially copyable types can be serialized");
        const char* p = reinterpret_cast<const char*>(&value);
        _data.insert(_data.end(), p, p + sizeof(T));
        return *this;
    }
    Message& operator<<(const std::string& value);
    Message& operator<<(const char* value);

    // ---- reading (throws std::out_of_range if the message is exhausted) -----
    template <typename T>
    const Message& operator>>(T& value) const
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Message: only trivially copyable types can be deserialized");
        readBytes(&value, sizeof(T));
        return *this;
    }
    const Message& operator>>(std::string& value) const;

    // ---- transport ----------------------------------------------------------
    std::vector<char> encode() const;

    // Extracts one complete message from the front of `buffer` (and erases it
    // from there). Returns nullopt if the frame is not complete yet.
    // Throws std::runtime_error on a malformed frame.
    static std::optional<Message> tryDecode(std::vector<char>& buffer);

private:
    void readBytes(void* out, size_t n) const;

    Type              _type;
    std::vector<char> _data;
    mutable size_t _readPos = 0;
};

#endif // MESSAGE_HPP



#include "message.hpp"

#include <arpa/inet.h>

#include <cstdint>
#include <cstring>

namespace
{
    constexpr std::uint32_t kMaxFrame = 64u * 1024u * 1024u;   // sanity limit
}

Message::Message(Type type) : _type(type) {}

Message::Type Message::type() const
{
    return _type;
}

Message& Message::operator<<(const std::string& value)
{
    const std::uint64_t len = value.size();
    *this << len;
    _data.insert(_data.end(), value.begin(), value.end());
    return *this;
}

Message& Message::operator<<(const char* value)
{
    return *this << std::string(value ? value : "");
}

void Message::readBytes(void* out, size_t n) const
{
    if (_data.size() - _readPos < n)
        throw std::out_of_range("Message: not enough data to read");
    std::memcpy(out, _data.data() + _readPos, n);
    _readPos += n;
}

const Message& Message::operator>>(std::string& value) const
{
    std::uint64_t len = 0;
    readBytes(&len, sizeof(len));
    if (_data.size() - _readPos < len)
    {
        _readPos -= sizeof(len);   // leave the message unchanged on failure
        throw std::out_of_range("Message: string length exceeds message");
    }
    value.assign(_data.data() + _readPos, static_cast<size_t>(len));
    _readPos += static_cast<size_t>(len);
    return *this;
}

std::vector<char> Message::encode() const
{
    const std::uint32_t len = htonl(static_cast<std::uint32_t>(sizeof(std::int32_t) + _data.size()));
    const std::uint32_t typ = htonl(static_cast<std::uint32_t>(_type));

    std::vector<char> out(sizeof(len) + sizeof(typ) + _data.size());
    std::memcpy(out.data(), &len, sizeof(len));
    std::memcpy(out.data() + sizeof(len), &typ, sizeof(typ));
    if (!_data.empty())
        std::memcpy(out.data() + sizeof(len) + sizeof(typ), _data.data(), _data.size());
    return out;
}

std::optional<Message> Message::tryDecode(std::vector<char>& buffer)
{
    if (buffer.size() < sizeof(std::uint32_t))
        return std::nullopt;

    std::uint32_t len = 0;
    std::memcpy(&len, buffer.data(), sizeof(len));
    len = ntohl(len);

    if (len < sizeof(std::int32_t) || len > kMaxFrame)
        throw std::runtime_error("Message: malformed frame length");

    if (buffer.size() < sizeof(std::uint32_t) + len)
        return std::nullopt;

    std::uint32_t typ = 0;
    std::memcpy(&typ, buffer.data() + sizeof(std::uint32_t), sizeof(typ));

    Message msg(static_cast<Type>(ntohl(typ)));
    const char* payload = buffer.data() + sizeof(std::uint32_t) + sizeof(typ);
    msg._data.assign(payload, payload + (len - sizeof(typ)));

    buffer.erase(buffer.begin(),
                 buffer.begin() + static_cast<std::ptrdiff_t>(sizeof(std::uint32_t) + len));
    return msg;
}
