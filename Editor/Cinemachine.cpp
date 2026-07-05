#include "Cinemachine.hpp"
#include <cstdio>

ST::Cinemachine::Cinemachine()
    : GameObject("Cinemachine") {
    shapeType = ShapeType::Quad;
    setViewportWidth(1280.0f);
}

ST::Cinemachine::Cinemachine(const std::string& name)
    : GameObject(name) {
    shapeType = ShapeType::Quad;
    setViewportWidth(1280.0f);
}

void ST::Cinemachine::rebuildPreviewMesh() {
    m_mesh.clear();
    float hw = 0.5f;
    float hh = 0.5f;
    m_mesh.addVertex(Vertex(Vector3(-hw, -hh, 0.0f), Vector3(0, 0, 1), Vector2(0, 0), color));
    m_mesh.addVertex(Vertex(Vector3( hw, -hh, 0.0f), Vector3(0, 0, 1), Vector2(1, 0), color));
    m_mesh.addVertex(Vertex(Vector3( hw,  hh, 0.0f), Vector3(0, 0, 1), Vector2(1, 1), color));
    m_mesh.addVertex(Vertex(Vector3(-hw,  hh, 0.0f), Vector3(0, 0, 1), Vector2(0, 1), color));
    m_mesh.addTriangle(0, 1, 2);
    m_mesh.addTriangle(0, 2, 3);
    m_dirty = false;
}

void ST::Cinemachine::init() {
}

void ST::Cinemachine::start() {
    std::printf("[Cinemachine] start complete: %s\n", name.c_str());
}

void ST::Cinemachine::update(float deltaTime) {
    (void)deltaTime;
}

void ST::Cinemachine::destroy() {
}

void ST::Cinemachine::setViewportWidth(float w) {
    scale.x = w;
    scale.y = w / ASPECT_RATIO;
    scale.z = 1.0f;
    rebuildPreviewMesh();
}

ST::Matrix4x4 ST::Cinemachine::getProjectionMatrix() const {
    float halfW = getViewportWidth() * 0.5f;
    float halfH = getViewportHeight() * 0.5f;
    return Matrix4x4::orthographic(position.x - halfW, position.x + halfW,
                                   position.y - halfH, position.y + halfH, -1.0f, 1.0f);
}
