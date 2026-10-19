#pragma once

#include <random>
#include <array>
#include <numbers>
#include <cmath>

#include "ivector2.hpp"
#include "random_2D_coordinate_generator.hpp"

class PerlinNoise2D
{
    private:
        Random2DCoordinateGenerator _random2D;
        float   dotGradient(int gridx, int gridy, float x, float y) const;

    public:
        double pi = std::numbers::pi_v<double>;
        PerlinNoise2D() = default;
        static float    fade(float t);
        static float    lerp(float a, float b, float t);
        IVector2<float>   gradient(int x, int y) const;
        float   sample(float x, float y) const;
};
