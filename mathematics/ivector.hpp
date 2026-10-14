#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

// ============================================================
// IVector<TType, N>
// Generic N-dimensional vector.
// Requires C++20.
// ============================================================

template<typename TType, std::size_t N>
struct IVector
{
    private:
        std::array<TType, N> _data{};

    public:
        using value_type = TType;
        static constexpr std::size_t Dimension = N;

        // --------------------------------------------------------
        // Constructors
        // --------------------------------------------------------

        constexpr IVector() = default;

        // Construct from N individual components.
        template<typename... Args>
            requires (sizeof...(Args) == N && sizeof...(Args) != 1)
        constexpr   IVector(Args... args)
            : _data{static_cast<TType>(args)...}
        {}

        // Fill every component with the same value.
        explicit constexpr IVector(const TType& value)
        {
            _data.fill(value);
        }

        // --------------------------------------------------------
        // Access
        // --------------------------------------------------------

        TType& operator[](std::size_t i)
        {
            return _data.at(i);
        }

        const TType& operator[](std::size_t i) const
        {
            return _data.at(i);
        }

        constexpr std::size_t size() const
        {
            return N;
        }

        // --------------------------------------------------------
        // Addition
        // --------------------------------------------------------

        IVector operator+(const IVector& other) const
        {
            IVector result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] + other[i];

            return result;
        }

        // --------------------------------------------------------
        // Subtraction
        // --------------------------------------------------------

        IVector operator-(const IVector& other) const
        {
            IVector result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] - other[i];

            return result;
        }

        // --------------------------------------------------------
        // Component-wise multiplication
        // --------------------------------------------------------

        IVector operator*(const IVector& other) const
        {
            IVector result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] * other[i];

            return result;
        }

        // --------------------------------------------------------
        // Scalar multiplication
        // Result type follows TType * Scalar.
        // --------------------------------------------------------

        template<typename Scalar>
        auto operator*(const Scalar& scalar) const
        {
            using ResultType = decltype(
                std::declval<TType>() * std::declval<Scalar>()
            );

            IVector<ResultType, N> result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] * scalar;

            return result;
        }

        // --------------------------------------------------------
        // Component-wise division
        // --------------------------------------------------------

        IVector operator/(const IVector& other) const
        {
            IVector result;

            for (std::size_t i = 0; i < N; ++i)
            {
                if (other[i] == TType{})
                    throw std::runtime_error(
                        "Division by zero in IVector division."
                    );

                result[i] = _data[i] / other[i];
            }

            return result;
        }

        // --------------------------------------------------------
        // Scalar division
        // Result type follows TType / Scalar.
        // --------------------------------------------------------

        template<typename Scalar>
        auto operator/(const Scalar& scalar) const
        {
            if (scalar == Scalar{})
                throw std::runtime_error(
                    "Division by zero in IVector division."
                );

            using ResultType = decltype(
                std::declval<TType>() / std::declval<Scalar>()
            );

            IVector<ResultType, N> result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] / scalar;

            return result;
        }

        // --------------------------------------------------------
        // Equality
        // --------------------------------------------------------

        bool operator==(const IVector& other) const
        {
            return _data == other._data;
        }

        bool operator!=(const IVector& other) const
        {
            return !(*this == other);
        }

        // --------------------------------------------------------
        // Dot product
        // --------------------------------------------------------

        auto dot(const IVector& other) const
        {
            using ResultType = decltype(
                std::declval<TType>() * std::declval<TType>()
            );

            ResultType sum{};

            for (std::size_t i = 0; i < N; ++i)
                sum += _data[i] * other[i];

            return sum;
        }

        // --------------------------------------------------------
        // Length / magnitude
        //
        // Uses ADL so a custom numeric type can provide its own sqrt.
        // --------------------------------------------------------

        auto length() const
        {
            using std::sqrt;
            return sqrt(dot(*this));
        }

        // --------------------------------------------------------
        // Normalize
        //
        // Result component type is deduced from TType / length type.
        // --------------------------------------------------------

        auto normalize() const
        {
            const auto len = length();

            if (len == decltype(len){})
            {
                using ResultType = decltype(
                    std::declval<TType>() / std::declval<decltype(len)>()
                );

                return IVector<ResultType, N>{};
            }

            return *this / len;
        }

        // --------------------------------------------------------
        // 2D perpendicular vector: rotate 90 degrees counterclockwise
        // --------------------------------------------------------

        IVector perpendicular() const requires (N == 2)
        {
            return IVector(-_data[1], _data[0]);
        }

        // --------------------------------------------------------
        // 2D cross product: returns a scalar
        // --------------------------------------------------------

        auto cross(const IVector& other) const requires (N == 2)
        {
            return _data[0] * other[1]
                - _data[1] * other[0];
        }

        // --------------------------------------------------------
        // 3D cross product: returns a vector
        // --------------------------------------------------------

        auto cross(const IVector& other) const requires (N == 3)
        {
            using ResultType = decltype(
                _data[1] * other[2] - _data[2] * other[1]
            );

            return IVector<ResultType, 3>(
                _data[1] * other[2] - _data[2] * other[1],
                _data[2] * other[0] - _data[0] * other[2],
                _data[0] * other[1] - _data[1] * other[0]
            );
        }

        // --------------------------------------------------------
        // 3D cross product: returns a vector
        // --------------------------------------------------------

        auto cross(const IVector& other) const requires (N == 7)
        {
            using ResultType = decltype(
                _data[0] * other[0] - _data[0] * other[0]
            );

            return IVector<ResultType, 7>(
                _data[1] * other[3] - _data[3] * other[1]
                    + _data[2] * other[6] - _data[6] * other[2]
                    + _data[4] * other[5] - _data[5] * other[4],

                _data[2] * other[4] - _data[4] * other[2]
                    + _data[3] * other[0] - _data[0] * other[3]
                    + _data[5] * other[6] - _data[6] * other[5],

                _data[3] * other[5] - _data[5] * other[3]
                    + _data[4] * other[1] - _data[1] * other[4]
                    + _data[6] * other[0] - _data[0] * other[6],

                _data[4] * other[6] - _data[6] * other[4]
                    + _data[5] * other[2] - _data[2] * other[5]
                    + _data[0] * other[1] - _data[1] * other[0],

                _data[5] * other[0] - _data[0] * other[5]
                    + _data[6] * other[3] - _data[3] * other[6]
                    + _data[1] * other[2] - _data[2] * other[1],

                _data[6] * other[1] - _data[1] * other[6]
                    + _data[0] * other[4] - _data[4] * other[0]
                    + _data[2] * other[3] - _data[3] * other[2],

                _data[0] * other[2] - _data[2] * other[0]
                    + _data[1] * other[5] - _data[5] * other[1]
                    + _data[3] * other[4] - _data[4] * other[3]
            );
        }

        // --------------------------------------------------------
        // Wedge product: works in any dimension.
        //
        // Returns N * (N - 1) / 2 components.
        // This is a bivector, not generally an ordinary vector.
        // --------------------------------------------------------

        auto wedge(const IVector& other) const
        {
            constexpr std::size_t Count = N * (N - 1) / 2;

            using ResultType = decltype(
                std::declval<TType>() * std::declval<TType>()
                - std::declval<TType>() * std::declval<TType>()
            );

            IVector<ResultType, Count> result{};

            std::size_t k = 0;

            for (std::size_t i = 0; i < N; ++i)
            {
                for (std::size_t j = i + 1; j < N; ++j)
                {
                    result[k++] =
                        _data[i] * other[j]
                        - _data[j] * other[i];
                }
            }

            return result;
        }

        static constexpr std::size_t    dimension() {
            return N;
        }
};

// ============================================================
// Class template argument deduction
//
// IVector a(1, 2, 3) -> IVector<int, 3>
// IVector b(1.0, 2.0) -> IVector<double, 2>
// ============================================================

template<typename... Args>
IVector(Args...) -> IVector<std::common_type_t<Args...>, sizeof...(Args)>;

// ============================================================
// Scalar multiplication from the left
//
// 3 * vector
// ============================================================

template<typename Scalar, typename TType, std::size_t N>
auto operator*(const Scalar& scalar, const IVector<TType, N>& vector)
{
    return vector * scalar;
}
