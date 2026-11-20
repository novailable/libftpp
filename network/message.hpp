#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <stdexcept>
#include <vector>
#include <cstring>
#include <optional>

namespace   message_detail
{
    template <typename>
    struct  always_false : std::false_type {};

    template <typename T, typename = void>
    struct  is_sequence : std::false_type {};

    template <typename T>
    struct  is_sequence<T, std::void_t<typename T::value_type,
        decltype(std::declval<const T&>().size()),
        decltype(std::declval<const T&>().begin()),
        decltype(std::declval<const T&>().end()),
        decltype(std::declval<T&>().push_back(std::declval<typename T::value_type>()))>>
        : std::true_type {};

    template <typename T, typename = void>
    struct  is_bulk : std::false_type {};

    template <typename T>
    struct  is_bulk<T, std::void_t<
        typename T::value_type,
        decltype(std::declval<const T&>().data()),
        decltype(std::declval<T&>().resize(0)) >>
        : std::is_trivially_copyable<typename T::value_type> {};
}

class   Message
{
    public:
        using Type = int;
        static constexpr std::uint32_t  MaxSize = 64u * 1024u * 1024u;

    private:
        Type    _type;
        std::vector<char>   _data;
        mutable size_t _readPos = 0;

        size_t  remaining() const;
        void    readBytes(void* out, size_t n) const;

    public:
        Message() = delete;
        explicit Message(int type);
        int    type() const;

        template <typename T>
        Message&    operator<<(const T& data)
        {
            static_assert(!std::is_array<T>::value && !std::is_pointer<T>::value,
                            "Message: arrays, pointers and string literals cannot be serialized");
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                const char* pos = reinterpret_cast<const char*>(&data);
                _data.insert(_data.end(), pos, pos + sizeof(T));
            }
            else if constexpr (message_detail::is_sequence<T>::value)
            {
                const std::uint64_t count = data.size();
                *this << count;

                using Element = typename T::value_type;

                if constexpr (message_detail::is_bulk<T>::value)
                {
                    const char* pos = reinterpret_cast<const char*>(&data);
                    _data.insert(_data.end(), pos, pos + static_cast<size_t> (count) * sizeof(Element));
                }
                else
                    for (const Element& element : data)
                        *this << element;
            }
             else
                    static_assert(message_detail::always_false<T>::value, "Message: type is not compatiable");
            return *this;
        }

        template <typename T>
        Message&    operator>>(T& data)
        {
            static_assert(!std::is_array_v<T> && !std::is_pointer_v<T>,
                            "Message: arrays and pointers cannot be deserialized");
            if constexpr (std::is_trivially_copyable_v<T>)
                readBytes(&data, sizeof(T));
            else if constexpr (message_detail::is_sequence<T>::value)
            {
                const size_t    start = _readPos;
                try
                {
                    std::uint64_t count = 0;
                    *this >> count;
                    if (count > remaining())
                        throw   std::out_of_range("Message: sequence length exceeds message");
                    data.clear();
                    using Element = typename T::value_type;
                    if constexpr (message_detail::is_bulk<T>::value)
                    {
                        if (count > remaining() / sizeof(Element))
                            throw   std::out_of_range("Message: sequence length exceeds message");
                        data.resize(static_cast<std::size_t>(count));
                        readBytes(data.data(), static_cast<std::size_t>(count) * sizeof(Element));
                    }
                    else
                    {
                        for (uint64_t i = 0; i < count; ++i)
                        {
                            Element    element;
                            *this >> element;
                            data.push_back(std::move(element));
                        }
                    }
                }
                catch(...)
                {
                    _readPos = start;
                    throw ;
                }
            }
            else
                static_assert(message_detail::always_false<T>::value,
                            "Message: type is not compatiable");
            return *this;
        }

        std::vector<char>   encode() const;
        static std::optional<Message>   decode(std::vector<char>& buffer);
        
};

#endif
