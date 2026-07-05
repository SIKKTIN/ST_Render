#include "test_camera.hpp"
#include "core/camera/Camera.hpp"

namespace ST {
namespace Test {

void testCamera() {
    std::cout << "Testing Camera..." << std::endl;

    Camera cam;
    assert(cam.m_eye == Vector3(0, 0, 5));
    assert(cam.m_target == Vector3::zero());
    assert(cam.m_up == Vector3::up());
    assert(cam.m_mode == ProjectionMode::Perspective);
    assert(cam.m_fov > 0);
    assert(cam.m_aspect > 0);
    assert(cam.m_near > 0);
    assert(cam.m_far > cam.m_near);

    Matrix4x4 viewMatrix = cam.getViewMatrix();
    assert(viewMatrix != Matrix4x4::zero());

    Matrix4x4 projMatrix = cam.getProjectionMatrix();
    assert(projMatrix != Matrix4x4::zero());

    Camera cam2(Vector3(10, 10, 10), Vector3(0, 0, 0));
    assert(cam2.m_eye == Vector3(10, 10, 10));
    assert(cam2.m_target == Vector3::zero());
    assert(cam2.m_up == Vector3::up());

    Matrix4x4 viewMatrix2 = cam2.getViewMatrix();
    assert(viewMatrix2 != cam.getViewMatrix());

    cam.m_mode = ProjectionMode::Orthographic;
    Matrix4x4 orthoMatrix = cam.getProjectionMatrix();
    assert(orthoMatrix != Matrix4x4::zero());

    std::cout << "Camera: PASSED" << std::endl;
}

} // namespace Test
} // namespace ST
