//
// Created by eternal on 2024/5/5.
//
#include <cmath>
#include "MathUtils.hh"

namespace math_utils {
    glm::mat4x4 scale2D(const glm::vec2 &s) {
        return {{s.x, 0,   0, 0},
                {0,   s.y, 0, 0},
                {0,   0,   1, 0},
                {0,   0,   0, 1}};
    }

    glm::mat4x4 rotateZ(const float zRadians) {
        const float s = std::sin(zRadians);
        const float c = std::cos(zRadians);
        return {{c,  s, 0, 0},
                {-s, c, 0, 0},
                {0,  0, 1, 0},
                {0,  0, 0, 1}};
    }

    glm::mat4x4 translate2D(const glm::vec2 &t) {
        return {{1,   0,   0, 0},
                {0,   1,   0, 0},
                {0,   0,   1, 0},
                {t.x, t.y, 0, 1}};
    }

    glm::mat4x4
    orthographicProjection(const float left, const float top, const float right, const float bottom,
                           const float near, const float far) {
        const float sx = 2.0f / (right - left);
        const float sy = 2.0f / (top - bottom);
        const float sz = 1 / (near - far);
        const float tx = (left + right) / (left - right);
        const float ty = (top + bottom) / (bottom - top);
        const float tz = near / (near - far);
        return {{sx, 0,  0,  0},
                {0,  sy, 0,  0},
                {0,  0,  sz, 0},
                {tx, ty, tz, 1}};
    }
}
