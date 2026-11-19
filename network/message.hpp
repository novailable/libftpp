#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <sstream>
#include <stdexcept>
#include <vector>

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
        : std::is_trivially_copyable_v<T> {};
}

class   Message
{
    public:
        using Type = int;
        static constexpr std::uint32_t  MaxSize = 64u * 1024u * 1024u;
    
    private:
        Type    _type;
        std::vector<char>   _data;

    public:
        Message() = delete;
        explicit Message(int type);
        int    type() const;

        template <typename T>
        Message&    operator<<(const T& data)
        {
            static_assert(!std::is_array<T>::value && !std::is_pointer<T>::value,
                            "Message: arrays, pointers and string literals cannot be serialized");
            if constexpr (std::is_trivially_copyable<T>::value)
            {
                const char* pos = reinterpret_cast<const char*>(&value);
                _data.insert(_data.end(), pos, pos + sizeof(T));
            }
            return (*this);
        }

        template <typename T>
        Message&    operator>>(const T& data)
        {
            static_assert(!std::is_array<T>::value && !std::is_pointer<T>::value,
                            "Message: arrays and pointers cannot be deserialized");
        }
};

#endif
