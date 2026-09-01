#ifndef MAT_H
#define MAT_H

#include <cassert>
#include <initializer_list>
#include <iostream>
#include "vec3.h"

template<int rows, int cols>
class mat {
  public:
    double e[rows][cols];

    mat() : e{} {}

    mat(std::initializer_list<std::initializer_list<double>> values) : e{} {
        assert(static_cast<int>(values.size()) == rows);

        int row = 0;
        for (const auto& source_row : values) {
            assert(static_cast<int>(source_row.size()) == cols);

            int col = 0;
            for (double value : source_row) {
                e[row][col] = value;
                col++;
            }
            row++;
        }
    }

    static mat identity() {
        static_assert(rows == cols);

        mat result;
        for (int i = 0; i < rows; i++) {
            result.e[i][i] = 1.0;
        }
        return result;
    }

    double determinant() const {
        static_assert((rows == 2 && cols == 2) || (rows == 3 && cols == 3));

        if constexpr (rows == 2 && cols == 2) {
            return e[0][0] * e[1][1] - e[0][1] * e[1][0];
        } else {
            return e[0][0] * (e[1][1] * e[2][2] - e[1][2] * e[2][1])
                 - e[0][1] * (e[1][0] * e[2][2] - e[1][2] * e[2][0])
                 + e[0][2] * (e[1][0] * e[2][1] - e[1][1] * e[2][0]);
        }
    }

    mat<cols, rows> transpose() const {
        mat<cols, rows> result;
        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                result[col][row] = e[row][col];
            }
        }
        return result;
    }

    mat inverse() const {
        static_assert((rows == 2 && cols == 2) || (rows == 3 && cols == 3));

        const double det = determinant();
        assert(det != 0);

        if constexpr (rows == 2 && cols == 2) {
            return mat{{
                { e[1][1] / det, -e[0][1] / det },
                { -e[1][0] / det, e[0][0] / det },
            }};
        } else {
            return mat{{
                { (e[1][1] * e[2][2] - e[1][2] * e[2][1]) / det,
                  (e[0][2] * e[2][1] - e[0][1] * e[2][2]) / det,
                  (e[0][1] * e[1][2] - e[0][2] * e[1][1]) / det },
                { (e[1][2] * e[2][0] - e[1][0] * e[2][2]) / det,
                  (e[0][0] * e[2][2] - e[0][2] * e[2][0]) / det,
                  (e[0][2] * e[1][0] - e[0][0] * e[1][2]) / det },
                { (e[1][0] * e[2][1] - e[1][1] * e[2][0]) / det,
                  (e[0][1] * e[2][0] - e[0][0] * e[2][1]) / det,
                  (e[0][0] * e[1][1] - e[0][1] * e[1][0]) / det },
            }};
        }
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
