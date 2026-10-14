#include <cmath>

float   PerlinNoise2D::dotGradient(int x0, int y0, int x1, int x1) const
{
    long long value = _random2D(x, y);
    const double    normalized = static_cast<double>(static_cast<std::uint64_t>(value))
}

float   PerlinNoise2D::sample(float x, float y) const
{
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));

    int x1 = x0 + 1;
    int y1 = y1 + 1;                                                                                                                   



    // Fractional position within the cell.
    float local_x = x - grid_x;
    float local_y = y - grid_y;

    Random2DCoordinateGenerator
    // Continue with gradient vectors, dot products,
    // and smooth interpolation.

    return (12.2);
}
