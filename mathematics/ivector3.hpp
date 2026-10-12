#pragma once

#include <cmath>
#include <stdexcept>

template<typename TType>
struct IVector3
{
    TType x;
    TType y;
    TType z;

    IVector3() : x(0), y(0), z(0) {}
    IVector3(TType x, TType y, TType z) : x(x), y(y), z(z) {}

    IVector3 operator+(const IVector3& other) const {
        return IVector3(x + other.x, y + other.y, z + other.z);
    }

    IVector3 operator-(const IVector3& other) const {
        return IVector3(x - other.x, y - other.y, z - other.z);
    }

    IVector3 operator*(const IVector3& other) const {
        return IVector3(x * other.x, y * other.y, z * other.z);
    }

    IVector3 operator/(const IVector3& other) const {
        if (other.x == TType(0) || other.y == TType(0) || other.z == TType(0))
            throw std::runtime_error("Division by zero in IVector3 division.");
        return IVector3(x / other.x, y / other.y, z / other.z);
    }

    bool operator==(const IVector3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const IVector3& other) const {
        return !(*this == other);
    }

    float length() const {
        return std::sqrt(static_cast<float>(x * x + y * y + z * z));
    }

    IVector3<float> normalize() const {
        float len = length();
        if (len == 0.0f)
            return IVector3<float>(0.0f, 0.0f, 0.0f);
        return IVector3<float>(
            static_cast<float>(x) / len,
            static_cast<float>(y) / len,
            static_cast<float>(z) / len
        );
    }

    float dot(const IVector3& other) const {
        return static_cast<float>(x * other.x + y * other.y + z * other.z);
    }

    IVector3 cross() const {
        return IVector3(-y, x, TType(0));
    }

    IVector3 cross(const IVector3& other) const {
        return IVector3(y * other.z - z * other.y,
                        z * other.x - x * other.z,
                        x * other.y - y * other.x);
    }
};