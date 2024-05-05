//
// Created by eternal on 2024/5/5.
//

#ifndef LEARNINGVULKAN_MATHUTILS_HH
#define LEARNINGVULKAN_MATHUTILS_HH
#include <glm/glm.hpp>

namespace math_utils {
    glm::mat4x4 scale2D(const glm::vec2& s);

    glm::mat4x4 rotateZ(float zRadians);

    glm::mat4x4 translate2D(const glm::vec2& t);

    glm::mat4x4 orthographicProjection(float left, float top, float right, float bottom, float near,
                                       float far);
}

#endif //LEARNINGVULKAN_MATHUTILS_HH
