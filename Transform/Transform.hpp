#pragma once

#include "Matrix4x4.hpp"
#include "Vector3.hpp"

namespace ST {
struct Transform {
    Vector3 m_position;
    Vector3 m_rotation;
    Vector3 m_scale;
    Transform() : m_position(0, 0, 0), m_rotation(0, 0, 0), m_scale(1, 1, 1) {}

    Matrix4x4 getModelMatrix() const {
        Matrix4x4 translationMatrix = Matrix4x4::translation(m_position);
        Matrix4x4 rotationXMatrix = Matrix4x4::rotationX(m_rotation.x);
        Matrix4x4 rotationYMatrix = Matrix4x4::rotationY(m_rotation.y);
        Matrix4x4 rotationZMatrix = Matrix4x4::rotationZ(m_rotation.z);
        Matrix4x4 scaleMatrix = Matrix4x4::scale(m_scale.x, m_scale.y, m_scale.z);
        return translationMatrix * rotationZMatrix * rotationYMatrix * rotationXMatrix * scaleMatrix;
    }

    Matrix4x4 getNormalMatrix() const {
        return getModelMatrix().inverse().transpose();
    }
};

inline Matrix4x4 getMVPMatrix(const Transform& transform, const Matrix4x4& view, const Matrix4x4& projection) {
    return projection * view * transform.getModelMatrix();
}

inline Matrix4x4 getViewportMatrix(int width, int height) {
    Matrix4x4 result = Matrix4x4::identity();
    result.m[0][0] = static_cast<float>(width) / 2.0f;
    result.m[1][1] = static_cast<float>(-height) / 2.0f;
    result.m[0][3] = static_cast<float>(width) / 2.0f;
    result.m[1][3] = static_cast<float>(height) / 2.0f;
    return result;
}
}
