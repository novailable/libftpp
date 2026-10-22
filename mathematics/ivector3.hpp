#pragma once

#include <cstddef>
#include "ivector.hpp"

template<typename TType, std::size_t N = 3>
struct  IVector3 : public IVector<TType, N, IVector3>
{
    using Base = IVector<TType, N, IVector3>;
    using Base::Base;

    TType& x = (*this)[0];
    TType& y = (*this)[1];
    TType& z = (*this)[2];
};
