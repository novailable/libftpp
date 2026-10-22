#pragma once

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

// ============================================================
// IVector<TType, N, Derived>
// Generic N-dimensional vector.
// Requires C++20.
//
// Use it directly:      IVector<int, 3> v(1, 2, 3);
// Or as a CRTP base:    template<typename T, std::size_t N>
//                       struct MyVec : IVector<T, N, MyVec> { ... };
//
// Derived must be a template <typename, std::size_t> that
// publicly derives from IVector<T, N, Derived>. When left at
// its default (SelfTag), results are plain IVector objects.
// ============================================================

// Sentinel meaning "no derived class: IVector is its own Self".
// Only ever named, never instantiated.
template<typename, std::size_t>
struct	SelfTag;

template<typename TType, std::size_t N,
	template <typename, std::size_t> typename Derived = SelfTag>
struct	IVector;

namespace	type::detail
{
	template<typename T>
	inline constexpr bool is_vector = requires {
		typename	std::remove_cvref_t<T>::is_vector_tag;
	};

	template<template <typename, std::size_t> typename D>
	inline constexpr bool is_self_tag = false;

	template<>
	inline constexpr bool is_self_tag<SelfTag> = true;
}

namespace	type
{
	template<typename T>
	inline constexpr bool ivector = detail::is_vector<T>;
}

template<typename TType, std::size_t N,
	template <typename, std::size_t> typename Derived>
struct IVector
{
	static_assert(N > 0, "IVector needs at least one component");

    protected:
        std::array<TType, N> _data{};
	private:

		// Vector type of M components of U: the derived type if there
		// is one, otherwise plain IVector.
		template<typename U, std::size_t M>
		using Result = std::conditional_t<
			type::detail::is_self_tag<Derived>,
			IVector<U, M, Derived>,
			Derived<U, M>>;

		using Self = Result<TType, N>;
		using ProductType = decltype(std::declval<TType>() * std::declval<TType>());
		using CrossType = decltype(std::declval<ProductType>() - std::declval<ProductType>());

		template<typename U>
		using Rebind = Result<U, N>;

		enum struct	error_type
		{
			zero_division
		};

		[[noreturn]] static constexpr void	error(error_type errtype)
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

		constexpr Self&			self()
		{
			static_assert(std::is_base_of_v<IVector, Self>,
				"Derived<TType, N> must derive from IVector<TType, N, Derived>");
			return static_cast<Self&>(*this);
		}
		constexpr const Self&	self() const
		{
			static_assert(std::is_base_of_v<IVector, Self>,
				"Derived<TType, N> must derive from IVector<TType, N, Derived>");
			return static_cast<const Self&>(*this);
		}

	public:
		using is_vector_tag = void;
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
		constexpr	IVector(Args... args)
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
		constexpr Self& operator+=(const Self& other)
		{
			for (std::size_t i = 0; i < N; ++i)
				_data[i] += other._data[i];
			return self();
		}

		constexpr Self& operator-=(const Self& other)
		{
			for (std::size_t i = 0; i < N; ++i)
				_data[i] -= other._data[i];
			return self();
		}

		constexpr Self& operator*=(const Self& other)
		{
			for (std::size_t i = 0; i < N; ++i)
				_data[i] *= other._data[i];
			return self();
		}

		constexpr Self& operator/=(const Self& other)
		{
			if (other.zero_component())
				error(error_type::zero_division);

			for (std::size_t i = 0; i < N; ++i)
				_data[i] /= other._data[i];
			return self();
		}

		// --------------------------------------------------------
		// Vector (component-wise) arithmetic
		// --------------------------------------------------------

		constexpr Self operator+(const Self& other) const
		{
			Self result = self();
			result += other;
			return result;
		}

		constexpr Self operator-(const Self& other) const
		{
			Self result = self();
			result -= other;
			return result;
		}

		constexpr Self operator*(const Self& other) const
		{
			Self result = self();
			result *= other;
			return result;
		}

		constexpr Self operator/(const Self& other) const
		{
			Self result = self();
			result /= other;
			return result;
		}

		// --------------------------------------------------------
		// Scalar compound assignment
		// --------------------------------------------------------

		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(TType& t, const Scalar& s) { t += s; }
		constexpr Self& operator+=(const Scalar& scalar)
		{
			for (std::size_t i = 0; i < N; ++i)
				_data[i] += scalar;
			return self();
		}

		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(TType& t, const Scalar& s) { t -= s; }
		constexpr Self& operator-=(const Scalar& scalar)
		{
			for (std::size_t i = 0; i < N; ++i)
				_data[i] -= scalar;
			return self();
		}

		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(TType& t, const Scalar& s) { t *= s; }
		constexpr Self& operator*=(const Scalar& scalar)
		{
			for (std::size_t i = 0; i < N; ++i)
				_data[i] *= scalar;
			return self();
		}

		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(TType& t, const Scalar& s) {
					t /= s;
					{ s == Scalar{} } -> std::convertible_to<bool>;
				}
		constexpr Self& operator/=(const Scalar& scalar)
		{
			if (scalar == Scalar{})
				error(error_type::zero_division);
			for (std::size_t i = 0; i < N; ++i)
				_data[i] /= scalar;
			return self();
		}

		// --------------------------------------------------------
		// Scalar arithmetic
		// The result component type follows normal C++ promotion,
		// e.g. Vector<int, 3> * 2.5 -> Vector<double, 3>.
		// --------------------------------------------------------

		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(const TType& t, const Scalar& s) { t + s; }
		constexpr auto operator+(const Scalar& scalar) const
		{
			using ResultType = decltype(
				std::declval<const TType&>() + std::declval<const Scalar&>()
			);

			Rebind<ResultType> result;

			for (std::size_t i = 0; i < N; ++i)
				result[i] = _data[i] + scalar;

			return result;
		}

		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(const TType& t, const Scalar& s) { t - s; }
		constexpr auto operator-(const Scalar& scalar) const
		{
			using ResultType = decltype(
				std::declval<const TType&>() - std::declval<const Scalar&>()
			);

			Rebind<ResultType> result;

			for (std::size_t i = 0; i < N; ++i)
				result[i] = _data[i] - scalar;

			return result;
		}

		template<typename Scalar>
			requires (!type::ivector<Scalar>)
				&& requires(const TType& t, const Scalar& s) { t * s; }
		constexpr auto operator*(const Scalar& scalar) const
		{
			using ResultType = decltype(
				std::declval<const TType&>() * std::declval<const Scalar&>()
			);

			Rebind<ResultType> result;

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

			Rebind<ResultType> result;

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
		// Returns a zero vector if the length is zero.
		// --------------------------------------------------------

		auto normalize() const
		{
			const auto len = length();
			using len_type = std::remove_cvref_t<decltype(len)>;
			using ResultType = decltype(
				std::declval<TType>() / std::declval<len_type>());

			Rebind<ResultType>	result{};

			if (len != len_type{})
				for (std::size_t i = 0; i < N; ++i)
					result[i] = _data[i] / len;
			return result;
		}

		// --------------------------------------------------------
		// 2D perpendicular vector: rotate 90 degrees counterclockwise
		// --------------------------------------------------------

		constexpr Self cross() const requires (N == 2)
		{
			return Self(-_data[1], _data[0]);
		}

		// --------------------------------------------------------
		// 2D cross product: returns a scalar
		// --------------------------------------------------------

		constexpr auto cross(const Self& other) const requires (N == 2)
		{
			return _data[0] * other._data[1]
				- _data[1] * other._data[0];
		}

		// --------------------------------------------------------
		// 3D cross product: returns a vector
		// --------------------------------------------------------

		constexpr auto cross(const Self& other) const requires (N == 3)
		{
			return Rebind<CrossType>(
				_data[1] * other._data[2] - _data[2] * other._data[1],
				_data[2] * other._data[0] - _data[0] * other._data[2],
				_data[0] * other._data[1] - _data[1] * other._data[0]
			);
		}

		// --------------------------------------------------------
		// 7D cross product: returns a vector
		// Convention: e_i x e_{i+1} = e_{i+3} (indices mod 7).
		// --------------------------------------------------------

		constexpr auto cross(const Self& other) const requires (N == 7)
		{
			auto	w = [&](std::size_t i, std::size_t j) {
				return _data[i % 7] * other._data[j % 7]
					- _data[j % 7] * other._data[i % 7];
			};

			Rebind<CrossType>	result;

			for (std::size_t k = 0; k < 7; ++k)
				result[k] = w(k + 1, k + 3) + w(k + 2, k + 6) + w(k + 4, k + 5);

			return result;
		}

		// --------------------------------------------------------
		// Wedge product: works in any dimension >= 2.
		//
		// Returns N * (N - 1) / 2 components.
		// This is a bivector, not generally an ordinary vector.
		// --------------------------------------------------------

		constexpr auto wedge(const Self& other) const requires (N >= 2)
		{
			constexpr std::size_t Count = N * (N - 1) / 2;

			Result<CrossType, Count> result{};

			std::size_t k = 0;

			for (std::size_t i = 0; i < N; ++i)
				for (std::size_t j = i + 1; j < N; ++j)
					result[k++] = _data[i] * other._data[j]
								- _data[j] * other._data[i];

			return result;
		}
};

// ============================================================
// Scalar multiplication from the left
//
// 3 * vector
// ============================================================

template<typename Scalar, typename TType, std::size_t N,
	template <typename, std::size_t> typename Derived>
	requires (!type::ivector<Scalar>)
		&& requires(const TType& t, const Scalar& s) { t * s; }
constexpr auto operator*(const Scalar& scalar, const IVector<TType, N, Derived>& vector)
{
	return vector * scalar;
}

// ============================================================
// Class template argument deduction
//
// IVector a(1, 2, 3)   -> IVector<int, 3>
// IVector b(1.0, 2.0)  -> IVector<double, 2>
// ============================================================

template<typename... Args>
IVector(Args...) -> IVector<std::common_type_t<Args...>, sizeof...(Args)>;