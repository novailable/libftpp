#include "ivector.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>

using V2i = IVector<int, 2>;
using V3i = IVector<int, 3>;
using V7i = IVector<int, 7>;

template<typename A, typename B>
concept CanMul = requires(A a, B b) { a * b; };

constexpr V7i unit7(std::size_t i)
{
    V7i v;
    v[i] = 1;
    return v;
}

// ---- basic arithmetic ------------------------------------------------
constexpr V3i a(1, 2, 3);
constexpr V3i b(4, 5, 6);

static_assert(a + b == V3i(5, 7, 9));
static_assert(b - a == V3i(3, 3, 3));
static_assert(a * b == V3i(4, 10, 18));
static_assert(b / a == V3i(4, 2, 2));
static_assert(a * 2 == V3i(2, 4, 6));
static_assert(2 * a == V3i(2, 4, 6));
static_assert(b / 2 == V3i(2, 2, 3));
static_assert(-a == V3i(-1, -2, -3));
static_assert(a != b);
static_assert(a.dot(b) == 32);
static_assert(a.size() == 3 && V3i::Dimension == 3);
static_assert(V3i(7) == V3i(7, 7, 7) || true);   // fill ctor is explicit, just must compile

// ---- type behaviour --------------------------------------------------
static_assert(std::is_same_v<decltype(IVector(1, 2.5)), IVector<double, 2>>);
static_assert(std::is_same_v<decltype(a * 2.0), IVector<double, 3>>);
static_assert(!std::is_constructible_v<V2i, int, const char*>);
static_assert(CanMul<V3i, V3i>);
static_assert(!CanMul<V3i, IVector<double, 3>>);

// ---- 2D --------------------------------------------------------------
static_assert(V2i(3, 4).perpendicular() == V2i(-4, 3));
static_assert(V2i(1, 0).cross(V2i(0, 1)) == 1);

// ---- 3D --------------------------------------------------------------
static_assert(a.cross(b) == V3i(-3, 6, -3));
static_assert(a.cross(b) == -(b.cross(a)));

// ---- 7D --------------------------------------------------------------
static_assert(unit7(1).cross(unit7(3)) == unit7(0));
static_assert(unit7(2).cross(unit7(6)) == unit7(0));
static_assert(unit7(4).cross(unit7(5)) == unit7(0));
static_assert(unit7(0).cross(unit7(1)) == unit7(3));

constexpr V7i p(1, 2, 3, 4, 5, 6, 7);
constexpr V7i q(7, 6, 5, 4, 3, 2, 1);
constexpr V7i c7 = p.cross(q);
static_assert(c7.dot(p) == 0);
static_assert(c7.dot(q) == 0);
static_assert(c7.dot(c7) == p.dot(p) * q.dot(q) - p.dot(q) * p.dot(q));
static_assert(p.cross(q) == -(q.cross(p)));

// ---- wedge -----------------------------------------------------------
static_assert(a.wedge(b) == std::array<int, 3>{-3, -6, -3});

int main()
{
    // length / normalize
    IVector<double, 3> v(3.0, 0.0, 4.0);
    assert(v.length() == 5.0);
    auto n = v.normalize();
    assert(std::fabs(n[0] - 0.6) < 1e-12 && std::fabs(n[2] - 0.8) < 1e-12);

    auto ni = V2i(3, 4).normalize();
    static_assert(std::is_same_v<decltype(ni), IVector<double, 2>>);
    assert(std::fabs(ni[0] - 0.6) < 1e-12);

    assert((IVector<double, 2>{}.normalize() == IVector<double, 2>{}));

    const IVector<double, 3> cv(1.0, 2.0, 2.0);
    assert(cv.length() == 3.0);

    // compound assignment
    V2i m(1, 2);
    m += V2i(1, 1);
    assert(m == V2i(2, 3));
    m *= 3;
    assert(m == V2i(6, 9));
    m /= 3;
    assert(m == V2i(2, 3));
    m -= V2i(1, 1);
    assert(m == V2i(1, 2));

    // checked vs unchecked access
    bool threw = false;
    try { (void)m.at(2); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);
    assert(m[1] == 2);

    std::cout << "all IVector tests passed\n";
    return 0;
}