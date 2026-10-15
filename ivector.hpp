#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>

// ============================================================
// IVector<TType, N>
// Generic N-dimensional vector.
// Requires C++20.
// ============================================================

template<typename TType, std::size_t N>
struct	IVector;

namespace	type::detail
{
	template<typename T>
    inline constexpr bool ivector_impl = false;

    template<typename T, std::size_t N>
    inline constexpr bool ivector_impl<IVector<T, N>> = true;
}

namespace	type
{
	template<typename T>
	inline constexpr bool ivector = 
		detail::ivector_impl<std::remove_cvref_t<T>>;
}

template<typename TType, std::size_t N>
struct IVector
{
    private:
        std::array<TType, N> _data{};

		using ProductType = decltype(std::declval<TType>() * std::declval<TType>());
		using CrossType = decltype(std::declval<ProductType>() - std::declval<ProductType>());

		enum struct	error_type
		{
			zero_division
		};
	
		[[noreturn]] static void	error(error_type errtype)
		{
			switch (errtype)
			{
				case error_type::zero_division:
					throw	std::runtime_error("Error: division by zero");
			}

			throw	std::runtime_error("Error: unknown error");
		}

		constexpr bool zero_component() const
		{
			for (std::size_t i = 0; i < N; ++i)
				if (_data[i] == TType{})
					return (true);
			return (false);
		}


    public:

        using x = (*this)[0];

        using value_type = TType;
		static constexpr std::size_t dimension = N;
        // --------------------------------------------------------
        // Constructors
        // --------------------------------------------------------

        constexpr IVector() = default;

        // Construct from N individual components.
        template<typename... Args>
            requires (sizeof...(Args) == N 
					&& sizeof...(Args) > 0
					&& (std::convertible_to<Args, TType> && ...))
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

        constexpr TType& operator[](std::size_t i)				{ return _data[i]; }
        constexpr const TType& operator[](std::size_t i) const	{ return _data[i]; }

        constexpr TType& at(std::size_t i)				{ return _data.at(i); }
        constexpr const TType& at(std::size_t i) const	{ return _data.at(i); }

        constexpr std::size_t size() const	{ return N; }

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

        constexpr IVector& operator*=(const IVector& other)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] *= other._data[i];
            return *this;
        }

        constexpr IVector& operator/=(const IVector& other)
        {
			if (other.zero_component())
				error(error_type::zero_division);

            for (std::size_t i = 0; i < N; ++i)
                _data[i] /= other._data[i];
            return *this;
        }

        // --------------------------------------------------------
        // Addition
        // --------------------------------------------------------

        constexpr IVector operator+(const IVector& other) const
        {
            IVector result = *this;
			result += other;
            return result;
        }

        // --------------------------------------------------------
        // Subtraction
        // --------------------------------------------------------

        constexpr IVector operator-(const IVector& other) const
        {
            IVector result = *this;
			result -= other;
            return result;
        }

        // --------------------------------------------------------
        // Multiplication
        // --------------------------------------------------------

        constexpr IVector operator*(const IVector& other) const
        {
            IVector result = *this;
			result *= other;
            return result;
        }

		// --------------------------------------------------------
        // Division
        // --------------------------------------------------------

        constexpr IVector operator/(const IVector& other) const
        {
            IVector result;

            for (std::size_t i = 0; i < N; ++i)
            {
                if (other._data[i] == TType{})
                    error(error_type::zero_division);
                result[i] = _data[i] / other._data[i];
            }

            return result;
        }

        // --------------------------------------------------------
        // Scalar Compound assignment
        // --------------------------------------------------------

		template<typename Scalar>
            requires (!type::ivector<Scalar>)
				&& requires(TType& t, const Scalar& s) { t += s; }
        constexpr IVector& operator+=(const Scalar& scalar)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] += scalar;
            return *this;
        }

        template<typename Scalar>
            requires (!type::ivector<Scalar>)
				&& requires(TType& t, const Scalar& s) { t -= s; }
        constexpr IVector& operator-=(const Scalar& scalar)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] -= scalar;
            return *this;
        }
		template<typename Scalar>
            requires (!type::ivector<Scalar>)
				&& requires(TType& t, const Scalar& s) { t *= s; }
        constexpr IVector& operator*=(const Scalar& scalar)
        {
            for (std::size_t i = 0; i < N; ++i)
                _data[i] *= scalar;
            return *this;
        }

        template<typename Scalar>
            requires (!type::ivector<Scalar>)
				&& requires(TType& t, const Scalar& s) { 
					t /= s;
					{ s == Scalar{} } -> std::convertible_to<bool>;
				}
        constexpr IVector& operator/=(const Scalar& scalar)
        {
			if (scalar == Scalar{})
				error(error_type::zero_division);
            for (std::size_t i = 0; i < N; ++i)
                _data[i] /= scalar;
            return *this;
        }

		// --------------------------------------------------------
        // Scalar Arithmetic
        // --------------------------------------------------------
        template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(const TType& t, const Scalar& s) { t + s; }
        constexpr auto operator+(const Scalar& scalar) const
        {
            using ResultType = decltype(
                std::declval<const TType&>() + std::declval<const Scalar&>()
            );

            IVector<ResultType, N> result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] + scalar;

            return result;
        }
		
		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(const TType& t, const Scalar& s) {t - s;}
        constexpr auto operator-(const Scalar& scalar) const
        {
            using ResultType = decltype(
                std::declval<const TType&>() - std::declval<const Scalar&>()
            );

            IVector<ResultType, N> result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] - scalar;

            return result;
        }

		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(const TType& t, const Scalar& s) {t * s;}
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

		template<typename Scalar>
			requires (!type::ivector<Scalar>) 
				&& requires(const TType& t, const Scalar& s) {
					t / s;
					{ s == Scalar{} } -> std::convertible_to<bool>;
				}
        constexpr auto operator/(const Scalar& scalar) const
        {
            if (scalar == Scalar{})
                error(error_type::zero_division);

            using ResultType = decltype(
                std::declval<const TType&>() / std::declval<const Scalar&>()
            );

            IVector<ResultType, N> result;

            for (std::size_t i = 0; i < N; ++i)
                result[i] = _data[i] / scalar;

            return result;
        }

        // --------------------------------------------------------
        // Equality
        // --------------------------------------------------------

        constexpr bool operator==(const IVector& other) const = default;


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
			using ResultType = decltype(
				std::declval<TType>() / std::declval<decltype(len)>());
            
			IVector<ResultType, N>	result{};
			
			if (len != decltype(len){})
            	for (std::size_t i = 0; i < N; ++i)
					result[i] = _data[i] / len;
            return result;
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
            return _data[0] * other._data[1]
                - _data[1] * other._data[0];
        }

        // --------------------------------------------------------
        // 3D cross product: returns a vector
        // --------------------------------------------------------

        auto cross(const IVector& other) const requires (N == 3)
        {
            using ResultType = decltype(
                _data[1] * other._data[2] - _data[2] * other._data[1]
            );

            return IVector<ResultType, 3>(
                _data[1] * other._data[2] - _data[2] * other._data[1],
                _data[2] * other._data[0] - _data[0] * other._data[2],
                _data[0] * other._data[1] - _data[1] * other._data[0]
            );
        }

        // --------------------------------------------------------
        // 7D cross product: returns a vector
        // --------------------------------------------------------

        auto cross(const IVector& other) const requires (N == 7)
        {
			auto	w = [&](std::size_t i, std::size_t j) {
				return _data[i % 7] * other._data[j % 7] - _data[j % 7] * other._data[i % 7];
			};

            IVector<CrossType, 7>	result;
			
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

        auto wedge(const IVector& other) const
        {
            constexpr std::size_t Count = N * (N - 1) / 2;

            IVector<CrossType, Count> result{};

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
IVector(Args...) -> IVector<std::common_type_t<Args...>, sizeof...(Args)>;

// ============================================================
// Scalar multiplication from the left
//
// 3 * vector
// ============================================================

template<typename Scalar, typename TType, std::size_t N>
	requires (!type::ivector<Scalar>)
		&& requires(const TType& t, const Scalar& s) { t * s; }
constexpr auto operator*(const Scalar& scalar, const IVector<TType, N>& vector)
{
    return vector * scalar;
}
