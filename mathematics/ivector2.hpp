#pragma once

#include <cstddef>
#include "ivector.hpp"

template<typename TType, std::size_t N = 2>
struct  IVector2 : public IVector<TType, 2, IVector2>
{
    using Base = IVector<TType, 2, IVector2>;
    using Base::Base;

    
};
