#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T, std::size_t N,
          template <typename, std::size_t> typename Derived>
struct test
{
    std::vector<T> _data{};

    template <typename... Args>
        requires (sizeof...(Args) == N
               && sizeof...(Args) > 0
               && (std::convertible_to<Args, T> && ...))
    constexpr test(Args... args)
        : _data{static_cast<T>(args)...}
    {}

    auto normalize() const
    {
        using ResultType = decltype(
            std::declval<T>() / std::declval<T>()
        );

        Derived<ResultType, N> result{};
        return result;
    }
};

template <typename T, std::size_t N>
struct shit : test<T, N, shit>
{
    using test<T, N, shit>::test;
};

template <typename... Args>
    requires (sizeof...(Args) > 0)
shit(Args...)
    -> shit<std::common_type_t<Args...>, sizeof...(Args)>;

