#pragma once

#include <cmath>

namespace phyz::engine {

struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3d operator+(const Vec3d& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    constexpr Vec3d operator-(const Vec3d& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    constexpr Vec3d operator-() const { return {-x, -y, -z}; }
    constexpr Vec3d operator*(double scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    constexpr Vec3d operator/(double scalar) const { return {x / scalar, y / scalar, z / scalar}; }

    constexpr Vec3d& operator+=(const Vec3d& rhs) {
        x += rhs.x; y += rhs.y; z += rhs.z; return *this;
    }
    constexpr Vec3d& operator-=(const Vec3d& rhs) {
        x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this;
    }
    constexpr Vec3d& operator*=(double scalar) {
        x *= scalar; y *= scalar; z *= scalar; return *this;
    }
};

inline constexpr Vec3d operator*(double scalar, const Vec3d& value) { return value * scalar; }
inline constexpr double dot(const Vec3d& a, const Vec3d& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline constexpr Vec3d cross(const Vec3d& a, const Vec3d& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline constexpr double length_squared(const Vec3d& value) { return dot(value, value); }
inline double length(const Vec3d& value) { return std::sqrt(length_squared(value)); }
inline Vec3d normalized(const Vec3d& value) {
    const double magnitude = length(value);
    return magnitude > 1.0e-18 ? value / magnitude : Vec3d{};
}

} // namespace phyz::engine
