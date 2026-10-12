#pragma once

#include <random>
#include <array>
#include "ivector2.hpp"
#include "random_2D_coordinate_generator.hpp"

class PerlinNoise2D
{
    private:
        Random2DCoordinateGenerator _random2D;
        float   gradient()
        float   dotGradient(int x0, int y0, int x1, int y1) const;

    public:

        float   sample(float x, float y) const;
};
