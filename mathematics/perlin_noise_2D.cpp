#include <cmath>
#include "perlin_noise_2D.hpp"

float   PerlinNoise2D::fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float   PerlinNoise2D::lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

auto   PerlinNoise2D::gradient(int x, int y) const
{
    long long value = _random2D(x, y), min = _random2D.long_min, max = _random2D.long_max;
    double  normalize = static_cast<double>(value - min) / static_cast<double>(max - min);

    float   angle = static_cast<float>(normalize) * 2.0f * pi;
    return IVector2<float>(static_cast<float>(std::cos(angle)),
                            static_cast<float>(std::sin(angle))
    );
}

float   PerlinNoise2D::dotGradient(int x0, int y0, int x1, int y1) const
{
    const IVector2<float> g = gradient(x0, y0);
    const IVector2<float> distance(x1 - static_cast<float>(x0), y1 - static_cast<float>(y0));

    return (g.dot(distance));
}

float   PerlinNoise2D::sample(float x, float y) const
{
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));

    int x1 = x0 + 1;
    int y1 = y1 + 1;                                                                                                                   


    float   sx = fade(x - static_cast<float>(x0));
    float   sy = fade(y - static_cast<float>(y0));

    float   bot_left = dotGradient(x0, y0, x, y);
    float   bot_right = dotGradient(x1, y0, x, y);
    float   top_left = dotGradient(x0, y1, x, y);
    float   top_right = dotGradient(x1, y1, x, y);

    float   bot = lerp(bot_left, bot_right, sx);
    float   top = lerp(top_left, top_right, sx);
    float   value = lerp(bot, top, sy);

    return (value);
}
