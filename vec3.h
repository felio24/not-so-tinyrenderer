#ifndef VEC3_H
#define VEC3_H

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <type_traits>

inline double random_double() {
    // Returns a random real in [0,1).
    return std::rand() / (RAND_MAX + 1.0);
}

inline double random_double(double min, double max) {
    // Returns a random real in [min,max).
    return min + (max-min)*random_double();
}

template<int n>
class vec {
  public:
    double e[n];

    vec() : e{} {}

    template<typename... Values, typename = std::enable_if_t<sizeof...(Values) == n>>
    vec(Values... values) : e{static_cast<double>(values)...} {}

    double x() const {
        static_assert(n >= 1);
        return e[0];
    }

    double y() const {
        static_assert(n >= 2);
        return e[1];
    }

    double z() const {
        static_assert(n >= 3);
        return e[2];
    }

    double w() const {
        static_assert(n >= 4);
        return e[3];
    }

    vec<2> xy() const {
        static_assert(n >= 2);
        return vec<2>(e[0], e[1]);
    }

    vec<3> xyz() const {
        static_assert(n >= 3);
        return vec<3>(e[0], e[1], e[2]);
    }

    vec operator-() const {
        vec result;
        for (int i = 0; i < n; i++) {
            result.e[i] = -e[i];
        }
        return result;
    }

    double operator[](int i) const {
        assert(i >= 0 && i < n);
        return e[i];
    }

    double& operator[](int i) {
        assert(i >= 0 && i < n);
        return e[i];
    }

    vec& operator+=(const vec& v) {
        for (int i = 0; i < n; i++) {
            e[i] += v.e[i];
        }
        return *this;
    }

    vec& operator*=(double t) {
        for (int i = 0; i < n; i++) {
            e[i] *= t;
        }
        return *this;
    }

    vec& operator/=(double t) {
        return *this *= 1/t;
    }

    double length() const {
        return std::sqrt(length_squared());
    }

    double length_squared() const {
        double sum = 0;
        for (int i = 0; i < n; i++) {
            sum += e[i] * e[i];
        }
        return sum;
    }

    static vec random() {
        vec result;
        for (int i = 0; i < n; i++) {
            result.e[i] = random_double();
        }
        return result;
    }

    static vec random(double min, double max) {
        vec result;
        for (int i = 0; i < n; i++) {
            result.e[i] = random_double(min,max);
        }
        return result;
    }
};

using vec2 = vec<2>;
using vec3 = vec<3>;
using vec4 = vec<4>;

// point3 is just an alias for vec3, but useful for geometric clarity in the code.
using point3 = vec3;


// Vector Utility Functions

template<int n>
inline std::ostream& operator<<(std::ostream& out, const vec<n>& v) {
    for (int i = 0; i < n; i++) {
        out << v.e[i];
        if (i + 1 < n) out << ' ';
    }
    return out;
}

template<int n>
inline vec<n> operator+(const vec<n>& u, const vec<n>& v) {
    vec<n> result;
    for (int i = 0; i < n; i++) {
        result.e[i] = u.e[i] + v.e[i];
    }
    return result;
}

template<int n>
inline vec<n> operator-(const vec<n>& u, const vec<n>& v) {
    vec<n> result;
    for (int i = 0; i < n; i++) {
        result.e[i] = u.e[i] - v.e[i];
    }
    return result;
}

template<int n>
inline vec<n> operator*(const vec<n>& u, const vec<n>& v) {
    vec<n> result;
    for (int i = 0; i < n; i++) {
        result.e[i] = u.e[i] * v.e[i];
    }
    return result;
}

template<int n>
inline vec<n> operator*(double t, const vec<n>& v) {
    vec<n> result;
    for (int i = 0; i < n; i++) {
        result.e[i] = t * v.e[i];
    }
    return result;
}

template<int n>
inline vec<n> operator*(const vec<n>& v, double t) {
    return t * v;
}

template<int n>
inline vec<n> operator/(const vec<n>& v, double t) {
    return (1/t) * v;
}

template<int n>
inline double dot(const vec<n>& u, const vec<n>& v) {
    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum += u.e[i] * v.e[i];
    }
    return sum;
}

inline vec3 cross(const vec3& u, const vec3& v) {
    return vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
                u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

template<int n>
inline vec<n> unit_vector(const vec<n>& v) {
    return v / v.length();
}

inline vec3 random_unit_vector() {
    while (true) {
        auto p = vec3::random(-1,1);
        auto lensq = p.length_squared();
        if (1e-160 < lensq && lensq <= 1) // In case p has VERY small x, y & z, sqrt(lensq) might go to 0
            return p / std::sqrt(lensq);
    }
}

inline vec3 random_on_hemisphere(const vec3& normal) {
    vec3 on_unit_sphere = random_unit_vector();
    if (dot(on_unit_sphere, normal) > 0.0) // In the same hemisphere as the normal
        return on_unit_sphere;
    else
        return -on_unit_sphere;
}

inline vec3 random_in_unit_disk() {
    while (true) {
        auto p = vec3(random_double(-1,1), random_double(-1,1), 0);
        if (p.length_squared() < 1)
            return p;
    }
}

inline vec3 reflect(const vec3& v, const vec3& n) {
    return v - 2*dot(v,n)*n;
}

inline vec3 refract(const vec3& uv, const vec3& n, double etai_over_etat) {
    auto cos_theta = fmin(dot(-uv, n), 1.0); // not fully sure why we need the min here
    vec3 r_out_perp =  etai_over_etat * (uv + cos_theta*n);
    vec3 r_out_parallel = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

#endif
