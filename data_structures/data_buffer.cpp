#include "data_buffer.hpp"

void    DataBuffer::write(const void* data, size_t size)
{
    if (size == 0)
        return;

    const std::byte*    bytes = reinterpret_cast<const std::byte*>(data);
    _bytes.insert(_bytes.end(), bytes, bytes + size);
}

void    DataBuffer::read(void *dest, size_t size)
{
    if (size == 0)
        return ;

    if (size > _bytes.size() - _cursor)
        throw   std::out_of_range("DataBuffer: not enough data to read");
    
    std::memcpy(dest, _bytes.data() + _cursor, size);
    _cursor += size;
}

DataBuffer  &DataBuffer::operator<<(const std::string &value)
{

    *this << static_cast<std::uint64_t>(value.size());
    write(value.data(), value.size());
    return *this;
}

DataBuffer  &DataBuffer::operator>>(std::string &value)
{
    std::uint64_t   length = 0;
    *this >> length;

    if (length > _bytes.size() - _cursor)
        throw   std::out_of_range("DataBuffer: not enough data to read string");
    
    value.assign(reinterpret_cast<const char*>(_bytes.data() + _cursor),
                static_cast<size_t>(length));
    
    _cursor += static_cast<size_t>(length);
    return *this;
}

size_t DataBuffer::size() const noexcept { return _bytes.size() - _cursor; }

bool    DataBuffer::empty() const noexcept { return size() == 0; }

void    DataBuffer::reset() noexcept
{
    _bytes.clear();
    _cursor = 0;
}

void    DataBuffer::rewind() noexcept { _cursor = 0; }
