#include "random_2D_coordinate_generator.hpp"

Random2DCoordinateGenerator::Random2DCoordinateGenerator()
{
    std::random_device  rd;
    std::mt19937    gen(rd());

    std::uniform_int_distribution<long long>    dist;
    _seed = dist(gen);
}

Random2DCoordinateGenerator::Random2DCoordinateGenerator(long long seed) : _seed(seed) {};

long long   Random2DCoordinateGenerator::seed() const
{
    return (_seed);
}

long long   Random2DCoordinateGenerator::operator()(const long long& x, const long long& y) const
{
    std::seed_seq seq {
            static_cast<std::uint32_t>(_seed),
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(_seed) >> 32),
            static_cast<std::uint32_t>(x), 
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) >> 32),
            static_cast<std::uint32_t>(y),
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) >> 32)
    };
    std::mt19937_64 gen(seq);
    std::uniform_int_distribution<long long> dist(long_min, long_max);
    return (dist(gen));

}
