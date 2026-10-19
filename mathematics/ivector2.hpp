#pragma once

#include <cmath>
#include <stdexcept>

template<typename TType>
struct IVector2
{
    TType x;
    TType y;

    IVector2() : x(0), y(0) {}
    IVector2(TType x, TType y) : x(x), y(y) {}

    IVector2 operator+(const IVector2& other) const {
        return IVector2(x + other.x, y + other.y);
    }

    IVector2 operator-(const IVector2& other) const {
        return IVector2(x - other.x, y - other.y);
    }

    IVector2 operator*(const IVector2& other) const {
        return IVector2(x * other.x, y * other.y);
    }

    IVector2 operator/(const IVector2& other) const {
        if (other.x == TType(0) || other.y == TType(0))
            throw std::runtime_error("Division by zero in IVector2 division.");
        return IVector2(x / other.x, y / other.y);
    }

    bool operator==(const IVector2& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const IVector2& other) const {
        return !(*this == other);
    }

    float length() const {
		const float fx = static_cast<float>(x);
		const float fy = static_cast<float>(y);
        return std::sqrt(fx * fx + fy * fy);
    }

    IVector2<float> normalize() const {
        float len = length();
        if (len == 0.0f)
            return IVector2<float>(0.0f, 0.0f);
        return IVector2<float>(
            static_cast<float>(x) / len,
            static_cast<float>(y) / len
        );
    }

    float dot(const IVector2& other) const {
        return static_cast<float>(x * other.x + y * other.y);
    }

    IVector2 cross() const {
        return IVector2(-y, x);
    }

    IVector2 cross(const IVector2& other) const {
        return IVector2(TType(0), x * other.y - y * other.x);
    }
};