#pragma once

#include "GameObject.hpp"
#include "../Math/Matrix4x4.hpp"

namespace ST {

class Cinemachine : public GameObject {
public:
    static constexpr float ASPECT_RATIO = 16.0f / 9.0f;

    Cinemachine();
    explicit Cinemachine(const std::string& name);
    ~Cinemachine() override = default;

    void init() override;
    void start() override;
    void update(float deltaTime) override;
    void destroy() override;

    void setViewportWidth(float w);
    float getViewportWidth() const { return scale.x; }
    float getViewportHeight() const { return scale.y; }

    Matrix4x4 getViewMatrix() const { return Matrix4x4::identity(); }
    Matrix4x4 getProjectionMatrix() const;

private:
    void rebuildPreviewMesh();
};

}
