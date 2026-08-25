#ifndef MAT_H
#define MAT_H

#include <cassert>
#include <iostream>
#include "vec3.h"

template<int rows, int cols>
class mat {
  public:
    double e[rows][cols];

    mat() : e{} {}

    static mat identity() {
        static_assert(rows == cols);

        mat result;
        for (int i = 0; i < rows; i++) {
            result.e[i][i] = 1.0;
        }
        return result;
    }

    double* operator[](int row) {
        assert(row >= 0 && row < rows);
        return e[row];
    }

    const double* operator[](int row) const {
        assert(row >= 0 && row < rows);
        return e[row];
    }
};

using mat2 = mat<2,2>;
using mat3 = mat<3,3>;
using mat4 = mat<4,4>;

template<int rows, int cols>
inline std::ostream& operator<<(std::ostream& out, const mat<rows, cols>& m) {
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            out << m.e[row][col];
            if (col + 1 < cols) out << ' ';
        }
        if (row + 1 < rows) out << '\n';
    }
    return out;
}

template<int rows, int cols>
inline vec<rows> operator*(const mat<rows, cols>& m, const vec<cols>& v) {
    vec<rows> result;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            result[row] += m.e[row][col] * v.e[col];
        }
    }
    return result;
}

template<int rows, int inner, int cols>
inline mat<rows, cols> operator*(const mat<rows, inner>& a, const mat<inner, cols>& b) {
    mat<rows, cols> result;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            for (int k = 0; k < inner; k++) {
                result[row][col] += a.e[row][k] * b.e[k][col];
            }
        }
    }
    return result;
}

#endif
