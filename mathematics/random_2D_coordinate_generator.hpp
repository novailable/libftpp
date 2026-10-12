#pragma once

#include <random>

class Random2DCoordinateGenerator
{
    private:
        long long   _seed;

    public:
        Random2DCoordinateGenerator();
        Random2DCoordinateGenerator(long long seed);

        long long   seed() const;
        long long   operator()(const long long& x, const long long& y) const;
};
