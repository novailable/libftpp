#pragma once

#include <random>
#include <limits>
#include <cstdint>


class Random2DCoordinateGenerator
{
    private:
        long long   _seed;

    public:

        const long long long_min = std::numeric_limits<long long>::min();
        const long long long_max = std::numeric_limits<long long>::max();
        Random2DCoordinateGenerator();
        Random2DCoordinateGenerator(long long seed);

        long long   seed() const;
        long long   operator()(const long long& x, const long long& y) const;
};
