#pragma once

#include <cstddef>
#include "ivector.hpp"

template<typename TType, std::size_t N = 3>
struct IVector3 : IVector<TType, N, IVector3>
{
    static_assert(N == 3);

    using Base = IVector<TType, N, IVector3>;
    using Base::Base;

    TType &x = this->_data[0];
    TType &y = this->_data[1];
    TType &z = this->_data[2];

    IVector3() = default;

    IVector3(const IVector3 &other) : 
        Base(other), x(this->_data[0]), y(this->_data[1]), z(this->_data[2]) {}

    IVector3 &operator=(const IVector3 &other)
    {
        Base::operator=(other);
        return *this;
    }
};
