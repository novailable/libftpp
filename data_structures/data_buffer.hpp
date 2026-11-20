#ifndef DATA_BUFFER_HPP
#define DATA_BUFFER_HPP

#include <cstddef>
#include <vector>
#include <iostream>
#include <type_traits>
#include <cstring>

#define COPY_ASSERT(TType) \
    static_assert(std::is_trivially_copyable_v<TType>, \
                  "DataBuffer: type must be trivially copyable")

#define POINTER_ASSERT(TType) \
    static_assert(!std::is_pointer_v<TType>, \
                  "DataBuffer: type must not be a pointer")        

class   DataBuffer
{
    private:
        std::vector<std::byte>  _bytes;
        size_t             _cursor = 0;

        void    write(const void* data, size_t size);
        void    read(void *dest, size_t size);
        

    public:
        template<typename TType>
        DataBuffer  &operator<<(const TType &value)
        {
            COPY_ASSERT(TType);
            POINTER_ASSERT(TType);

            write(&value, sizeof(TType));
            return *this;
        }

        template<typename TType>
        DataBuffer  &operator>>(TType& value)
        {
            COPY_ASSERT(TType);
            POINTER_ASSERT(TType);

            read(&value, sizeof(TType));
            return *this;
        }

        DataBuffer  &operator<<(const std::string &value);
        DataBuffer  &operator>>(std::string &value);

        size_t size() const noexcept;
        bool        empty() const noexcept;
        void        reset() noexcept;
        void        rewind() noexcept;
};

#endif
