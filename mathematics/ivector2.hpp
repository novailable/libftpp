#pragma once

#include "ivector.hpp"

template<typename TType>
struct  IVector2 : public IVector<TType, 2, IVector2<TType>>
{
    using Base = IVector<TType, 2, IVector2<TType>>;
    using Base::Base;

    TType &x = (*this)[0];
    TType &y = (*this)[1];
};
