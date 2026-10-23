# IVector

`IVector` is a header-only, fixed-size N-dimensional vector base class for C++20.

```cpp
#include "ivector.hpp"
```

`IVector<TType, N, Derived>` uses the Curiously Recurring Template Pattern (CRTP) to provide common arithmetic and geometric operations for derived vector types.

It is intended to be used as a base class rather than instantiated directly. A derived class typically provides named component accessors such as `x()`, `y()`, and `z()`.

Vector-vector arithmetic returns the derived vector type. Scalar arithmetic can produce a vector with a different component type, following the result type of the corresponding C++ arithmetic expression.

## Template parameters

| Parameter | Description |
|---|---|
| `TType` | Component type, such as `int`, `float`, `double`, or a compatible custom numeric type. |
| `N` | Number of components. Must be greater than zero. |
| `Derived` | Derived class template accepting `<typename, std::size_t>`. Used to preserve the derived type in operations and to rebind the component type or dimension. |

For example, `Derived<double, 3>` is the same derived vector template with `double` components and three components.

## Requirements

- C++20 or later.
- `N > 0`.
- `Derived<T, N>` must derive from `IVector<T, N, Derived>`.
- The derived class template must support every component type and dimension that the operations you use instantiate.
- Component types must support the arithmetic, comparisons, and construction required by the operations you use.
- `length()` requires a square-root operation found through `std::sqrt` and argument-dependent lookup (ADL).
- Division checks compare against a value-initialized zero (`TType{}` or `Scalar{}`).

Not every operation needs every capability. A type that is only stored and accessed does not need division or a square root.

## Defining a derived vector

A derived class passes itself as the third template argument.

```cpp
template<typename TType, std::size_t N = 2>
struct IVector2 : IVector<TType, N, IVector2>
{
    static_assert(N == 2);

    using Base = IVector<TType, N, IVector2>;
    using Base::Base;

    constexpr TType& x() { return (*this)[0]; }
    constexpr TType& y() { return (*this)[1]; }

    constexpr const TType& x() const { return (*this)[0]; }
    constexpr const TType& y() const { return (*this)[1]; }
};
```

Usage:

```cpp
IVector2<int> a(1, 2);
IVector2<int> b(3, 4);

auto c = a + b;

int first  = c.x(); // 4
int second = c.y(); // 6
```

### Derived-class rules

- The derived class must pass itself as the `Derived` template argument.
- `using Base::Base` inherits the base class's public constructors (component list, fill, copy).
- The base default constructor is `protected`. A derived class can still be default-constructed: its implicit default constructor calls the base one.
- If the derived class adds data members, its copy and assignment semantics must handle them correctly.
- Prefer accessor functions over reference data members. Reference members complicate copying and assignment.
- Operations that change the dimension (`wedge()`) instantiate `Derived<T, M>` for a different `M`. See the limitation below.

> **Limitation:** `wedge()` returns `Derived<T, N*(N-1)/2>`. A derived template that enforces its dimension with `static_assert` (like `IVector2` above, which requires `N == 2`) cannot be instantiated with any other dimension, so `wedge()` fails to compile on it: for `N == 2` it needs `IVector2<T, 1>`. To support `wedge()`, the derived template must accept the result dimension, or the library must return a separate bivector type.

## Quick start

The examples assume a derived vector type that supports the component types and dimensions each operation needs.

```cpp
IVector2<int> a(1, 2);
IVector2<int> b(3, 4);

// Component-wise vector arithmetic
auto c = a + b; // (4, 6)
auto d = b - a; // (2, 2)
auto e = a * b; // (3, 8), not a dot product
auto f = b / a; // (3, 2), integer division

// Scalar arithmetic
auto g = a * 2.5; // IVector2<double>(2.5, 5.0)
auto h = 3 * a;   // (3, 6)

// Compound assignment
a += b;
a *= 2;

// Geometry
auto dp   = a.dot(b);
auto len  = a.length();
auto unit = a.normalize();

// Two-dimensional operations
auto perpendicular = a.cross();
auto signed_area   = a.cross(b);
```

`wedge()` is described separately below because its result has a different dimension from its inputs.

## API reference

### Construction

| Expression | Description |
|---|---|
| `Derived()` | Default construction. All components are value-initialized (zero). |
| `Derived(a, b, ...)` | Exactly `N` arguments, each convertible to `TType`. |
| `Derived(value)` | Explicit. Fills every component with `value`. |
| Copy construction | Copies the components. |

In `IVector`, the default constructor is `protected`; the component-list, fill, and copy constructors are `public`.

### Access

| Expression | Description |
|---|---|
| `v[i]` | Accesses component `i` without bounds checking. |
| `v.at(i)` | Accesses component `i` with bounds checking. Throws `std::out_of_range` if `i >= N`. |
| `v.size()` | Number of components, `N`. |
| `Derived::dimension` | Compile-time constant equal to `N`. |

### Component-wise vector arithmetic

Both operands must be the same derived vector type.

| Expression | Description |
|---|---|
| `a + b` | Adds corresponding components. |
| `a - b` | Subtracts corresponding components. |
| `a * b` | Multiplies corresponding components. |
| `a / b` | Divides corresponding components. |
| `a += b` | Adds in place. |
| `a -= b` | Subtracts in place. |
| `a *= b` | Multiplies in place. |
| `a /= b` | Divides in place. |

These are not dot products or matrix multiplications.

```cpp
IVector2<int> a(2, 3);
IVector2<int> b(4, 5);

auto sum     = a + b;   // (6, 8)
auto product = a * b;   // (8, 15)
auto dot     = a.dot(b); // 23
```

#### Vector division

Before dividing, the implementation checks whether any divisor component equals `TType{}`. If so, it throws `std::runtime_error` and no component is modified.

```cpp
IVector2<double> a(10.0, 20.0);
IVector2<double> b(2.0, 0.0);

auto result = a / b; // throws std::runtime_error
```

The whole operation throws even though the first component could be divided, because the second divisor component is zero.

The check uses `==`, so for custom numeric types its behavior depends on their zero representation and equality semantics.

### Scalar arithmetic

A scalar operation applies the scalar to every component independently.

| Expression | Description |
|---|---|
| `v + s` | Adds `s` to each component. |
| `v - s` | Subtracts `s` from each component. |
| `v * s` | Multiplies each component by `s`. |
| `v / s` | Divides each component by `s`. |
| `v += s` | Adds `s` in place. |
| `v -= s` | Subtracts `s` in place. |
| `v *= s` | Multiplies in place. |
| `v /= s` | Divides in place. |
| `s * v` | Scalar multiplication from the left. |

For the non-compound forms, the result component type is deduced from the corresponding C++ expression.

```cpp
IVector2<int> v(1, 2);

auto a = v * 2;   // IVector2<int>
auto b = v * 2.5; // IVector2<double>
auto c = 2 * v;   // IVector2<int>
```

These result types assume the derived template accepts the resulting component type.

Compound assignments never change the vector's component type. `IVector2<int> v; v *= 2.5;` multiplies using the underlying `int` assignment rules; it does not turn `v` into an `IVector2<double>`.

#### Scalar division

Scalar division checks `s == Scalar{}` before touching any component. If it is true, `std::runtime_error` is thrown.

### Comparison

| Expression | Description |
|---|---|
| `a == b` | Exact equality of the base component storage. |
| `a != b` | Synthesized from `operator==` in C++20. |

Floating-point equality is exact, so mathematically equal results can compare unequal because of rounding.

The defaulted equality operator compares only the `IVector` base subobject. Extra data members added by a derived class are not included.

### Geometry

#### Dot product

```cpp
auto result = a.dot(b);
```

The sum of the component-wise products:

$$
a \cdot b = \sum_{i=0}^{N-1} a_i b_i
$$

The result type is the product type of `TType * TType`, and the accumulator is value-initialized.

#### Length

```cpp
auto result = a.length();
```

The Euclidean magnitude:

$$
\lVert a \rVert = \sqrt{\sum_{i=0}^{N-1} a_i^2}
$$

The implementation does `using std::sqrt;` and then makes an unqualified call, so ADL can find a `sqrt` for a custom numeric type.

For ordinary integer vectors the result is `double`. Custom numeric types may produce other result types.

#### Normalize

```cpp
auto result = a.normalize();
```

Divides every component by the vector's length:

$$
\hat{a} = \frac{a}{\lVert a \rVert}
$$

If the length equals its value-initialized zero, the function returns a value-initialized (zero) vector instead of dividing. It does not throw.

The result component type is deduced from `TType / length_type`. For integer vectors this is floating point.

#### Cross product

Availability depends on the dimension.

| Dimension | Expression | Result |
|---|---|---|
| 2 | `a.cross()` | Perpendicular vector $(-y, x)$. |
| 2 | `a.cross(b)` | Scalar $a_x b_y - a_y b_x$. |
| 3 | `a.cross(b)` | Right-handed 3D cross-product vector. |
| 7 | `a.cross(b)` | Seven-dimensional cross-product vector (convention below). |
| other | `a.cross(b)` | Not available (compile error). |

The 2D perpendicular rotates a vector 90 degrees counterclockwise:

$$
(x, y) \mapsto (-y, x)
$$

The 2D binary cross product is the signed area of the parallelogram spanned by the two vectors, equal to the z-component of the 3D cross product of the vectors embedded in the plane.

The 3D cross product is perpendicular to both operands, with right-handed orientation.

The 7D implementation uses the convention

$$
e_i \times e_{i+1} = e_{i+3}
$$

with indices taken modulo 7. The full sign convention is fixed by the formula in the implementation.

#### Wedge product

```cpp
auto result = a.wedge(b);
```

Available when `N >= 2`. It produces

$$
\frac{N(N-1)}{2}
$$

components, one for each index pair $i < j$:

$$
(a \wedge b)_{ij} = a_i b_j - a_j b_i
$$

Components are ordered lexicographically by index pair:

```text
(0,1), (0,2), ..., (0,N-1), (1,2), ...
```

The result is a bivector, not generally an ordinary vector. Its type is:

```cpp
Derived<CrossType, N * (N - 1) / 2>
```

where `CrossType` is the type of a product-minus-product expression.

For `N == 3` there are three components, closely related to the 3D cross product but with a different ordering and sign convention.

> **Limitation:** the result reuses the `Derived` template with a different dimension. A derived template that enforces its dimension with `static_assert` fails to instantiate it. For `N == 3` the result dimension is also 3, so `IVector3` works; for `N == 2` it is 1 and for `N == 4` it is 6, which a fixed-dimension derived template rejects.

## Errors and exceptions

| Error | Cause |
|---|---|
| `std::runtime_error` | Division by a vector with a zero component, or by a zero scalar. |
| `std::out_of_range` | `at(i)` with `i >= N`. |
| Compile-time error | `N == 0`, an unsupported cross-product dimension, a `Derived` that does not derive from `IVector<T, N, Derived>`, or an operation/type combination the component type does not support. |

The internal `error()` helper also has a fallback for an unknown error code. Currently the only defined code is division by zero.

## Constexpr support

Constructors (except the copy constructor), the assignment operator, arithmetic, comparison, `dot()`, `cross()`, and `wedge()` are `constexpr`.

These are not `constexpr` in the current implementation:

- `operator[]`
- `at()`
- `size()`
- the copy constructor
- `length()`
- `normalize()`

Whether an expression is actually usable in a constant expression also depends on the component type and the compiler.

## Type compatibility and limitations

- Binary vector operators require the same derived type. `IVector2<int> + IVector2<double>` is not supported directly.
- Scalar arithmetic can rebind the derived vector to another component type, provided the derived template accepts it.
- Left-hand scalar operators exist only for multiplication (`s * v`). `s + v`, `s - v`, and `s / v` are not provided.
- Equality is exact, not approximate.
- There is no matrix type, matrix multiplication, or broadcasting between different dimensions.
- Custom numeric types must support the operations of each method you call.
- There is no synchronization. Concurrent writes to the same object need external locking.

## Implementation notes

Components are stored in a protected `std::array<TType, N>` named `_data`.

The `type::ivector<T>` trait checks for a nested `is_vector_tag` type after removing cv-qualifiers and references. It is a structural check, not a validation that `T` implements the whole vector interface.

CRTP lets operations return the derived type, and `Derived<U, N>` lets scalar operations change the component type.