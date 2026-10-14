#pragma once

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

// ============================================================
// IVector<TType, N>
// Generic N-dimensional vector.
// Requires C++20.
//
// Division policy: no zero checks. Integer division by zero is
// undefined behaviour; floating point follows IEEE (inf / NaN).
// ============================================================

template<typename TType, std::size_t N>
struct IVector;

namespace detail
{
    template<typename T>
    inline constexpr bool is_ivector_v = false;

    template<typename T, std::size_t N>
    inline constexpr bool is_ivector_v<IVector<T, N>> = true;
}

template<typename TType, std::size_t N>
struct IVector
{
    private:
        std::array<TType, N> _data{};

        using ProductType = decltype(std::declval<TType>() * std::declval<TType>());
        using CrossType   = decltype(std::declval<ProductType>() - std::declval<ProductType>());

    public:
        using value_type = TType;
        static constexpr std::size_t Dimension = N;

        // --------------------------------------------------------
        // Constructors
        // --------------------------------------------------------

        constexpr IVector() = default;

        // Construct from N individual components.
        template<typename... Args>
            requires (sizeof...(Args) == N
                      && sizeof...(Args) != 1
                      && (std::convertible_to<Args, TType> && ...))
        constexpr IVector(Args... args)
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

        // Unchecked.
        constexpr TType& operator[](std::size_t i)             { return _data[i]; }
        constexpr const TType& operator[](std::size_t i) const { return _data[i]; }

        // Bounds-checked (throws std::out_of_range).
        constexpr TType& at(std::size_t i)             { return _data.at(i); }
        constexpr const TType& at(std::size_t i) const { return _data.at(i); }

        constexpr std::size_t size() const { return N; }

        // --------------------------------------------------------
        // Compound assignment
        // --------------------------------------------------------

        constexpr IVector& operator+=(const IVector& other)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] += other._data[i];
            return *this;
        }

        constexpr IVector& operator-=(const IVector& other)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] -= other._data[i];
            return *this;
        }

        // Component-wise.
        constexpr IVector& operator*=(const IVector& other)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] *= other._data[i];
            return *this;
        }

        // Component-wise.
        constexpr IVector& operator/=(const IVector& other)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] /= other._data[i];
            return *this;
        }

        template<typename Scalar>
            requires (!detail::is_ivector_v<Scalar>)
                  && requires(TType& t, const Scalar& s) { t *= s; }
        constexpr IVector& operator*=(const Scalar& scalar)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] *= scalar;
            return *this;
        }

        template<typename Scalar>
            requires (!detail::is_ivector_v<Scalar>)
                  && requires(TType& t, const Scalar& s) { t /= s; }
        constexpr IVector& operator/=(const Scalar& scalar)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] /= scalar;
            return *this;
        }

        // --------------------------------------------------------
        // Arithmetic
        // --------------------------------------------------------

        constexpr IVector operator-() const
        {
            IVector result;
            for (std::size_t i = 0; i < N; ++i)
                result._data[i] = static_cast<TType>(-_data[i]);
            return result;
        }

        constexpr IVector operator+(const IVector& other) const
        {
            IVector result = *this;
            result += other;
            return result;
        }

        constexpr IVector operator-(const IVector& other) const
        {
            IVector result = *this;
            result -= other;
            return result;
        }

        // Component-wise multiplication.
        constexpr IVector operator*(const IVector& other) const
        {
            IVector result = *this;
            result *= other;
            return result;
        }

        // Component-wise division.
        constexpr IVector operator/(const IVector& other) const
        {
            IVector result = *this;
            result /= other;
            return result;
        }

        // Scalar multiplication. Result type follows TType * Scalar.
        template<typename Scalar>
            requires (!detail::is_ivector_v<Scalar>)
                  && requires(const TType& t, const Scalar& s) { t * s; }
        constexpr auto operator*(const Scalar& scalar) const
        {
            using ResultType = decltype(
                std::declval<const TType&>() * std::declval<const Scalar&>()
            );

            IVector<ResultType, N> result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] * scalar;

            return result;
        }

        // Scalar division. Result type follows TType / Scalar.
        template<typename Scalar>
            requires (!detail::is_ivector_v<Scalar>)
                  && requires(const TType& t, const Scalar& s) { t / s; }
        constexpr auto operator/(const Scalar& scalar) const
        {
            using ResultType = decltype(
                std::declval<const TType&>() / std::declval<const Scalar&>()
            );

            IVector<ResultType, N> result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] / scalar;

            return result;
        }

        // --------------------------------------------------------
        // Equality (also provides !=)
        // --------------------------------------------------------

        constexpr bool operator==(const IVector&) const = default;

        // --------------------------------------------------------
        // Dot product
        // --------------------------------------------------------

        constexpr auto dot(const IVector& other) const
        {
            ProductType sum{};

            for (std::size_t i = 0; i < N; ++i)
                sum += _data[i] * other._data[i];

            return sum;
        }

        // --------------------------------------------------------
        // Length / magnitude
        //
        // Uses ADL so a custom numeric type can provide its own sqrt.
        // Not constexpr: std::sqrt is not constexpr before C++26.
        // --------------------------------------------------------

        auto length() const
        {
            using std::sqrt;
            return sqrt(dot(*this));
        }

        // --------------------------------------------------------
        // Normalize
        //
        // Returns a zero vector if the length is zero.
        // Component type is TType / length type (double for int input).
        // --------------------------------------------------------

        auto normalize() const
        {
            using LengthType = decltype(length());
            using ResultType = decltype(
                std::declval<const TType&>() / std::declval<const LengthType&>()
            );

            const LengthType len = length();

            if (len == LengthType{})
                return IVector<ResultType, N>{};

            return IVector<ResultType, N>(*this / len);
        }

        // --------------------------------------------------------
        // 2D perpendicular vector: rotate 90 degrees counterclockwise
        // --------------------------------------------------------

        constexpr IVector perpendicular() const requires (N == 2)
        {
            return IVector(-_data[1], _data[0]);
        }

        // --------------------------------------------------------
        // 2D cross product: returns a scalar
        // --------------------------------------------------------

        constexpr auto cross(const IVector& other) const requires (N == 2)
        {
            return _data[0] * other._data[1]
                 - _data[1] * other._data[0];
        }

        // --------------------------------------------------------
        // 3D cross product: returns a vector
        // --------------------------------------------------------

        constexpr auto cross(const IVector& other) const requires (N == 3)
        {
            return IVector<CrossType, 3>(
                _data[1] * other._data[2] - _data[2] * other._data[1],
                _data[2] * other._data[0] - _data[0] * other._data[2],
                _data[0] * other._data[1] - _data[1] * other._data[0]
            );
        }

        // --------------------------------------------------------
        // 7D cross product: returns a vector
        //
        // Convention: e_k = e_(k+1) x e_(k+3), indices mod 7
        // (Fano plane lines {k, k+1, k+3}). Other octonion
        // multiplication tables give different, equally valid, results.
        //
        //   result[k] = w(k+1, k+3) + w(k+2, k+6) + w(k+4, k+5)
        //   where w(i, j) = a[i]*b[j] - a[j]*b[i]
        // --------------------------------------------------------

        constexpr auto cross(const IVector& other) const requires (N == 7)
        {
            auto w = [&](std::size_t i, std::size_t j) {
                return _data[i % 7] * other._data[j % 7]
                     - _data[j % 7] * other._data[i % 7];
            };

            IVector<CrossType, 7> result;

            for (std::size_t k = 0; k < 7; ++k)
                result[k] = w(k + 1, k + 3) + w(k + 2, k + 6) + w(k + 4, k + 5);

            return result;
        }

        // --------------------------------------------------------
        // Wedge product: works in any dimension.
        //
        // Returns N * (N - 1) / 2 components.
        // This is a bivector, not generally an ordinary vector.
        // --------------------------------------------------------

        constexpr auto wedge(const IVector& other) const
        {
            constexpr std::size_t Count = N * (N - 1) / 2;

            std::array<CrossType, Count> result{};

            std::size_t k = 0;

            for (std::size_t i = 0; i < N; ++i)
                for (std::size_t j = i + 1; j < N; ++j)
                    result[k++] = _data[i] * other._data[j]
                                - _data[j] * other._data[i];

            return result;
        }
};

// ============================================================
// Class template argument deduction
//
// IVector a(1, 2, 3) -> IVector<int, 3>
// IVector b(1.0, 2.0) -> IVector<double, 2>
// ============================================================

template<typename... Args>
IVector(Args...)
    -> IVector<std::common_type_t<Args...>, sizeof...(Args)>;

// ============================================================
// Scalar multiplication from the left
//
// 3 * vector
// ============================================================

template<typename Scalar, typename TType, std::size_t N>
    requires (!detail::is_ivector_v<Scalar>)
          && requires(const TType& t, const Scalar& s) { t * s; }
constexpr auto operator*(const Scalar& scalar, const IVector<TType, N>& vector)
{
    return vector * scalar;
}