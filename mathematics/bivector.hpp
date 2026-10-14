#pragma once

#include "ivector.hpp"

template <typename T, std::size_t D>
struct  Bivector : private IVector
{
    static constexpr std::size_t size= D * (D - 1) / 2;
    
    IVector<T, size>&   first;
    IVector<T, size>&   second;
    

    constexpr   T
};
