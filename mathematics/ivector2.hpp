#pragma once

#include <cstddef>
#include "ivector.hpp"

template<typename TType, std::size_t N = 2>
struct IVector2 : IVector<TType, N, IVector2>
{
    static_assert(N == 2);

    using Base = IVector<TType, N, IVector2>;
    using Base::Base;

    TType &x = this->_data[0];
    TType &y = this->_data[1];

    IVector2() = default;

    IVector2(const IVector2 &other) : 
        Base(other), x(this->_data[0]), y(this->_data[1]) {}

    IVector2 &operator=(const IVector2 &other)
    {
        Base::operator=(other);
        return *this;
    }
};
