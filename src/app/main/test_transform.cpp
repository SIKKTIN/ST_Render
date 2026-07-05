#include "test_transform.hpp"
#include "renderer/transform/Transform.hpp"

namespace ST {
namespace Test {

void testTransform() {
    std::cout << "Testing Transform..." << std::endl;

    Transform t;
    assert(t.m_position == Vector3::zero());
    assert(t.m_rotation == Vector3::zero());
    assert(t.m_scale == Vector3(1, 1, 1));

    t.m_position = Vector3(1, 2, 3);
    t.m_rotation = Vector3(0, 90, 0);
    t.m_scale = Vector3(2, 2, 2);

    Matrix4x4 modelMatrix = t.getModelMatrix();
    Matrix4x4 normalMatrix = t.getNormalMatrix();

    assert(modelMatrix != Matrix4x4::identity());

    std::cout << "Transform: PASSED" << std::endl;
}

void testGetMVPMatrix() {
    std::cout << "Testing getMVPMatrix..." << std::endl;

    Transform transform;
    Matrix4x4 view = Matrix4x4::identity();
    Matrix4x4 projection = Matrix4x4::identity();

    Matrix4x4 mvp = getMVPMatrix(transform, view, projection);

    assert(mvp == projection * view * transform.getModelMatrix());

    std::cout << "getMVPMatrix: PASSED" << std::endl;
}

void testGetViewportMatrix() {
    std::cout << "Testing getViewportMatrix..." << std::endl;

    Matrix4x4 viewport = getViewportMatrix(800, 600);

    assert(viewport.m[0][0] == 400.0f);   // width / 2
    assert(viewport.m[1][1] == -300.0f);   // -height / 2
    assert(viewport.m[0][3] == 400.0f);   // width / 2
    assert(viewport.m[1][3] == 300.0f);   // height / 2

    Matrix4x4 viewport2 = getViewportMatrix(1920, 1080);
    assert(viewport2.m[0][0] == 960.0f);
    assert(viewport2.m[1][1] == -540.0f);

    std::cout << "getViewportMatrix: PASSED" << std::endl;
}

} // namespace Test
} // namespace ST
