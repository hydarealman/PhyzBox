#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace phyz {

constexpr double Pi = 3.1415926535897932384626433832795;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3() = default;
    constexpr Vec3(double xIn, double yIn, double zIn) : x(xIn), y(yIn), z(zIn) {}

    constexpr Vec3 operator+(const Vec3& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Vec3 operator-(const Vec3& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }

    constexpr Vec3 operator-() const {
        return {-x, -y, -z};
    }

    constexpr Vec3 operator*(double scalar) const {
        return {x * scalar, y * scalar, z * scalar};
    }

    constexpr Vec3 operator/(double scalar) const {
        return {x / scalar, y / scalar, z / scalar};
    }

    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vec3& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
};

inline constexpr Vec3 operator*(double scalar, const Vec3& value) {
    return value * scalar;
}

inline constexpr double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline double lengthSquared(const Vec3& value) {
    return dot(value, value);
}

inline double length(const Vec3& value) {
    return std::sqrt(lengthSquared(value));
}

inline Vec3 normalized(const Vec3& value) {
    const double len = length(value);
    if (len <= 1.0e-12) {
        return {0.0, 0.0, 0.0};
    }
    return value / len;
}

inline double clamp(double value, double low, double high) {
    return std::max(low, std::min(value, high));
}

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

inline std::array<float, 16> perspective(float fovYRadians, float aspect, float zNear, float zFar) {
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    std::array<float, 16> m{};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0f;
    m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return m;
}

inline std::array<float, 16> lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 forward = normalized(center - eye);
    const Vec3 side = normalized(cross(forward, up));
    const Vec3 trueUp = cross(side, forward);

    std::array<float, 16> m{};
    m[0] = static_cast<float>(side.x);
    m[4] = static_cast<float>(side.y);
    m[8] = static_cast<float>(side.z);
    m[12] = static_cast<float>(-dot(side, eye));

    m[1] = static_cast<float>(trueUp.x);
    m[5] = static_cast<float>(trueUp.y);
    m[9] = static_cast<float>(trueUp.z);
    m[13] = static_cast<float>(-dot(trueUp, eye));

    m[2] = static_cast<float>(-forward.x);
    m[6] = static_cast<float>(-forward.y);
    m[10] = static_cast<float>(-forward.z);
    m[14] = static_cast<float>(dot(forward, eye));

    m[3] = 0.0f;
    m[7] = 0.0f;
    m[11] = 0.0f;
    m[15] = 1.0f;
    return m;
}

} // namespace phyz

