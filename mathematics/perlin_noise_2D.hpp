#pragma once

#include <random>
#include <array>
#include <numbers>

#include "ivector2.hpp"
#include "random_2D_coordinate_generator.hpp"

class PerlinNoise2D
{
    private:
        Random2DCoordinateGenerator _random2D;
        float   dotGradient(int x0, int y0, int x1, int y1) const;

    public:
        double pi = std::numbers::pi_v<double>;
        PerlinNoise2D() = default;
        static float    fade(float t);
        static float    lerp(float a, float b, float t);
        auto   gradient(int x, int y) const;
        float   sample(float x, float y) const;
};
